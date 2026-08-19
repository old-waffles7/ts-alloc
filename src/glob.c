
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/glob.h"

#include    "config/tsalloc_config.h"

#include    "internal/os.h"
#include    "internal/mutex.h"
#include    "internal/arena.h"
#include    "internal/ledger.h"
#include    "internal/tcache.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"

#include    <unistd.h>
#include    <pthread.h>
#include    <stdatomic.h>


_Thread_local static tcache_t  *tlocal_bcache[TSALLOC_MAXN_GLOBS];
_Thread_local static bool       tlocal_bcache_isoff[TSALLOC_MAXN_GLOBS];


static pthread_key_t    tsalloc_tcleanup_key;
static bool             tsalloc_tcleanup_ready;

static _Atomic(bool)    allocator_isactive[TSALLOC_MAXN_GLOBS];
static _Atomic(size_t)  tsalloc_tcleanup_nfails;
static inline void      tsalloc_tcleanup(void *arg);

static inline void
tsalloc_tcleanup(
    void *arg
){
    tcache_t   *cache;
    ts_err_t    ret;

    for (uint16_t i = 0; i < TSALLOC_MAXN_GLOBS; i++)
    {
        if(!atomic_load_explicit(allocator_isactive + i, memory_order_acquire))
        {
            tlocal_bcache[i]    = nullptr;
            continue;
        }

        if (tlocal_bcache[i] == nullptr)
        {
            continue;
        }

        cache   = tlocal_bcache[i];        
        ret     = tcache_destroy(cache, true);
        if (ret != TSALLOC_SUCCESS)
        {
            atomic_fetch_add_explicit(&tsalloc_tcleanup_nfails, 1, memory_order_release);
        }
    }
}

static inline bool
glob_ctx_not_valid(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg
){
    if ((!glob_state) || (!arena_cfg))
    {
        return true;
    }
    if (!arena_cfg->auxil_map)
    {
        return true;
    }
    if (!arena_cfg->auxil_unmap)
    {
        return true;
    }
    if (!IS_POWER_OF_TWO(arena_cfg->pagesize))
    {
        return true;
    }
    if (arena_cfg->default_new_span_szclass > glob_state->nszclasses_span)
    {
        return true;
    }
    if (arena_cfg->lcpu_arena_count == 0)
    {
        return true;
    }
    return false;
}

static ts_err_t
glob_alloc_block(
    glob_t         *glob,
    void          **dest,
    ts_szclass_t    szclass,
    int32_t         glob_uid
){
    ts_err_t    ret;

    if (tlocal_bcache_isoff[glob_uid])
    {
        arena_t    *arena;

        arena   = glob_claim_arena(glob);
        ret     = arena_get_batch(
            arena, 
            ((byte_t**)dest), 
            szclass, 
            1
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }

        return TSALLOC_SUCCESS;
    }

    if (tlocal_bcache[glob_uid] == nullptr)
    {
        tcache_t   *cache;

        ret = tcache_create(glob, &cache);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }

        tlocal_bcache[glob_uid] = cache;
    }

    ret = tcache_get_block(
        tlocal_bcache[glob_uid], 
        ((byte_t**)dest), 
        szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob->glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline ts_err_t
glob_alloc_span(
    glob_t         *glob,
    void          **dest,
    ts_szclass_t    szclass
){
    arena_t    *arena;
    span_t     *span;
    ts_err_t    ret;

    arena   = glob_claim_arena(glob);
    ret     = arena_get_span(arena, &span, szclass);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob->glob_uid);
        return ret;
    }

    *dest   = ((void*)span->addr);

    return TSALLOC_SUCCESS;
}

static inline ts_err_t
glob_free_block(
    glob_t         *glob,
    span_t         *slab,
    void           *addr,
    ts_szclass_t    szclass,
    int32_t         glob_uid
){
    if (tlocal_bcache_isoff[glob_uid])
    {
        glob_put_batch_inarena(glob, ((byte_t**)&addr), 1);

        return TSALLOC_SUCCESS;
    }

    ts_err_t    ret;

    if (tlocal_bcache[glob_uid] == nullptr)
    {
        tcache_t   *cache;

        ret = tcache_create(glob, &cache);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }

        tlocal_bcache[glob_uid] = cache;
    }

    tcache_put_block(tlocal_bcache[glob_uid], ((byte_t*)addr), szclass);

    return TSALLOC_SUCCESS;
}


ts_err_t
glob_create(
    const arena_cfg_t  *arena_cfg,
    glob_t            **dest
){
    if (!tsalloc_tcleanup_ready)
    {
        if (pthread_key_create(&tsalloc_tcleanup_key, tsalloc_tcleanup)
            || pthread_setspecific(tsalloc_tcleanup_key, nullptr))
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }
        
        tsalloc_tcleanup_ready = true;
    }

    static _Atomic(uint16_t) glob_epoch;
    int32_t glob_uid;

    glob_uid = atomic_load_explicit(&glob_epoch, memory_order_acquire);
    if (glob_uid >= TSALLOC_MAXN_GLOBS - 1)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    const glob_alloc_state_t *glob_state;
    arena_cfg_t               _arena_cfg;

    if (arena_cfg == TSALLOC_DEFAULT_ARG)
    {
        glob_state = tsconfig_get_cfg(sys_page_size());
        if (glob_state == nullptr)
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }

        tsalloc_szreq_t req;

        req = tsconfig_get_szclass(glob_state, (1024ULL * 1024ULL * 2ULL));
        if ((req.isslab == -1) || (req.isslab == true))
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }
        _arena_cfg = (arena_cfg_t){
            .auxil_map                = &def_auxil_map,
            .auxil_unmap              = &def_auxil_unmap,
            .auxil_madvise            = &def_auxil_madvise,
            .pagesize                 = glob_state->page_size,
            .default_new_span_szclass = req.szclass,
            .lcpu_arena_count         = 4,
            .unmap_on_termination     = false,
            .allow_cross_origin_merge = true
        };
    }
    else 
    {
        glob_state = tsconfig_get_cfg(arena_cfg->pagesize);
        if (glob_state == nullptr)
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }

        _arena_cfg = *arena_cfg;
    }
    if (glob_ctx_not_valid(glob_state, &_arena_cfg))
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    size_t narenas;

    narenas = _arena_cfg.lcpu_arena_count * sysconf(_SC_NPROCESSORS_ONLN);
    if (narenas > MAXN_ARENAS)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    byte_t  *raw;
    arena_t *arena_addr;
    glob_t  *glob;    
    size_t   nbytes_req;
    size_t   nbytes_arena_auxil_mem;

    nbytes_arena_auxil_mem = arena_auxil_mem_size(glob_state);
    nbytes_req             = sizeof(glob_t) 
                             + narenas * (sizeof(arena_t) + nbytes_arena_auxil_mem);
    raw = sys_map(nbytes_req);
    if (raw == nullptr)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }
    arena_addr = (arena_t*)(raw + sizeof(glob_t));
    glob       = (glob_t*)raw;
    *glob      = (glob_t){
        .arenas     = arena_addr,
        .glob_state = glob_state,
        .arena_cfg  = _arena_cfg,
        .glob_uid   = glob_uid,
        .narenas    = narenas
    };
    
    ts_err_t ret;

    ret = pagetrie_init(&(glob->pagetrie), TSALLOC_NO_ERROR_CONTEXT);
    if (ret != TSALLOC_SUCCESS)
    {
        (void)sys_unmap(((void*)raw), nbytes_req);
        return TSALLOC_UNTRACKED_FAILURE;
    }

    ret = mutex_init(&(glob->ledger_lock), TSALLOC_NO_ERROR_CONTEXT);
    if (ret != TSALLOC_SUCCESS)
    {
        (void)pagetrie_deinit(&(glob->pagetrie), TSALLOC_NO_ERROR_CONTEXT);
        (void)sys_unmap(((void*)raw), nbytes_req);
        return TSALLOC_UNTRACKED_FAILURE;
    }

    byte_t *auxil_mem;
    
    auxil_mem = (raw + sizeof(glob_t) + narenas * sizeof(arena_t));
    for (uint16_t i = 0; i < narenas; i++)
    {
        ret = arena_init(
            glob,
            glob_state,
            &(glob->arena_cfg), 
            &(glob->pagetrie), 
            auxil_mem,
            arena_addr + i,
            i
        );
        if (ret != TSALLOC_SUCCESS)
        {
            (void)mutex_deinit(&(glob->ledger_lock), TSALLOC_NO_ERROR_CONTEXT);
            (void)pagetrie_deinit(&(glob->pagetrie), TSALLOC_NO_ERROR_CONTEXT);
            (void)sys_unmap(((void*)raw), nbytes_req);
            return TSALLOC_UNTRACKED_FAILURE;
        }
        auxil_mem += nbytes_arena_auxil_mem;
    }

    atomic_store_explicit(allocator_isactive + glob->glob_uid, true, memory_order_release);
    atomic_store_explicit(&glob_epoch, glob_uid + 1, memory_order_release);
    *dest = glob;

    return TSALLOC_SUCCESS;
}

ts_err_t
glob_destroy(
    glob_t *glob
){
    atomic_store_explicit(allocator_isactive + glob->glob_uid, false, memory_order_release);

    tcache_t   *cache;
    ts_err_t    ret1, ret2;

    ret2 = TSALLOC_SUCCESS;

    while (!ledger_isempty(&(glob->ledger)))
    {
        cache   = ledger_pop(&(glob->ledger));
        ret1    = tcache_destroy(cache, false);
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2 = ret1;
        }
    }

    for (uint32_t i = 0; i < glob->narenas; i++)
    {
        ret1 = arena_deinit(glob->arenas + i);
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2 = ret1;
        }
    }

    int     ret3;
    size_t  nbytes;

    nbytes  = glob->narenas * (sizeof(arena_t) + arena_auxil_mem_size(glob->glob_state))
              + sizeof(glob_t);
    ret3    = sys_unmap(((void*)glob), nbytes);
    if (ret3 != 0)
    {
        ret2 = TSALLOC_OS_ERR;
    }

    return ret2;
}

ts_err_t
glob_alloc(
    glob_t *glob,
    void  **dest,
    size_t  nbytes
){
    tsalloc_szreq_t szreq;

    szreq   = tsconfig_get_szclass(glob->glob_state, nbytes);
    if (szreq.isslab == (-1))
    {
        set_tsalloc_error(
            glob->glob_uid,
            "glob_alloc::glob.c invalid nbytes argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    ts_err_t    ret;

    if (szreq.isslab == true)
    {
        ret = glob_alloc_block(glob, dest, szreq.szclass, glob->glob_uid);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }
    }
    else 
    {
        ret = glob_alloc_span(glob, dest, szreq.szclass);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

ts_err_t
glob_free(
    glob_t *glob,
    void   *addr
){
    span_t *origin;

    origin  = (span_t*)pagetrie_lookup(&(glob->pagetrie), ((byte_t*)addr));
    if (origin == nullptr)
    {
        set_tsalloc_error(
            glob->glob_uid,
            "glob_free::glob.c invalid addr argument, memory does not belong to this arena",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    ts_err_t    ret;

    if (origin->flags.is_slab)
    {
        ret = glob_free_block(
            glob, 
            origin, 
            addr, 
            origin->slabmeta->szclass, 
            glob->glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }
    }
    else
    {
        arena_t    *arena;

        arena   = glob->arenas + ((uint16_t)origin->flags.arena_uid);
        ret     = arena_put_span(arena, origin);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

void 
glob_turnon_tcache(
    glob_t *glob
){
    tlocal_bcache_isoff[glob->glob_uid] = false;
}

ts_err_t 
glob_turnoff_tcache(
    glob_t *glob
){
    ts_err_t    ret;
    int32_t     glob_uid;

    glob_uid    = glob->glob_uid;

    if (tlocal_bcache[glob_uid])
    {
        ret = tcache_destroy(tlocal_bcache[glob->glob_uid], true);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }

        tlocal_bcache[glob_uid] = nullptr;
    }

    tlocal_bcache_isoff[glob->glob_uid] = true;

    return TSALLOC_SUCCESS;
}