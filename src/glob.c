
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/glob.h"

#include    "config/tsalloc_config.h"

#include    "internal/os.h"
#include    "internal/arena.h"
#include    "internal/ledger.h"
#include    "internal/tcache.h"
#include    "internal/arenaconfig.h"

#include    <pthread.h>


//  can be arena* or tcache*
_Thread_local static void  *tloc_cache[TSALLOC_MAXN_GLOBS];
static bool                 _nouse_tcache[TSALLOC_MAXN_GLOBS];

static pthread_key_t        tcache_cleanup_key;
static bool                 tcache_key_initialized;

static inline void
thread_tcache_destructor(
    void   *arg
){
    tcache_t   *cache;
    
    for (uint16_t i = 0; i < TSALLOC_MAXN_GLOBS; i++)
    {
        if (_nouse_tcache[i])
        {
            continue;
        }
        cache   = ((tcache_t*)arg);
        if (cache->loc_arena)
        {
            tcache_flush(cache);
        }
        sys_unmap(
            ((void*)cache), 
            sizeof(tcache_t) + tcache_auxil_mem_size(cache->loc_arena->glob->cfg.tsalloc_cfg)
        );
    }
}


static tsalloc_err_t
_glob_alloc(
    glob_arena_t   *glob,
    void          **dest,
    size_t          uid,
    size_t          nbytes,
    bool            bootstrap
){
    tsalloc_szreq_t req;

    req = tsconfig_get_szclass(glob->cfg.tsalloc_cfg, nbytes);
    if (req.isslab < 0)
    {
        set_tsalloc_error(
            &(glob->error_ctx),
            "_glob_alloc::glob.c invalid argument nbytes",
            TSALLOC_INVALID_ARGS
        );
        *dest   = nullptr;
        return TSALLOC_INVALID_ARGS;
    }

    tsalloc_err_t   ret;

    if (req.isslab)
    {
        if (bootstrap)
        {
            arena_t    *loc_arena;

            loc_arena   = _nouse_tcache[uid]? ((arena_t*)tloc_cache[uid]) : (glob->loc_arenas + glob->arena_idx);
            ret = arena_get_batch(
                loc_arena, 
                ((byte_t**)dest), 
                req.szclass, 
                1
            );
            if (ret != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(&(glob->error_ctx));
                return ret;
            }
        }
        else
        {
            ret = tcache_get_block(
                &(glob->error_ctx), 
                ((tcache_t*)tloc_cache[uid]), 
                ((byte_t**)dest), 
                req.szclass
            );
            if (ret != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(&(glob->error_ctx));
                return ret;
            }
        }
    }
    else
    {
        arena_t    *loc_arena;
        span_t     *span;

        if (bootstrap)
        {
            loc_arena   = glob->loc_arenas + glob->arena_idx;
        }
        else if (_nouse_tcache[uid])
        {
            loc_arena   = (arena_t*)tloc_cache[uid];
        }
        else 
        {
            loc_arena   = ((tcache_t*)tloc_cache[uid])->loc_arena;
        }

        ret = arena_get_span(loc_arena, &span, req.szclass);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(&(glob->error_ctx));
            return ret;
        }

        *dest   = ((void*)(span->addr));
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
glob_init_tloc_cache(
    glob_arena_t   *glob,
    size_t          uid
){
    arena_t        *loc_arena;
    
    loc_arena   = glob_claim(glob);
    if (_nouse_tcache[uid])
    {
        tloc_cache[uid] = (void*)loc_arena;
    }
    else 
    {
        byte_t *raw_mem;
        size_t  nbytes;

        nbytes  = sizeof(tcache_t) + tcache_auxil_mem_size(glob->cfg.tsalloc_cfg);
        raw_mem = (byte_t*)sys_map(nbytes);
        if (raw_mem == nullptr)
        {
            set_tsalloc_error(
                &(glob->error_ctx),
                "glob_init_tloc_cache::glob.c cannot allocate memory for tcache",
                TSALLOC_OS_ERR
            );
            return TSALLOC_OS_ERR;
        }

        byte_t         *auxil_mem_addr;
        tsalloc_err_t   ret;

        auxil_mem_addr  = raw_mem + sizeof(tcache_t);
        tcache_init(
            glob->cfg.tsalloc_cfg, 
            &(glob->error_ctx), 
            auxil_mem_addr, 
            glob, 
            ((tcache_t*)raw_mem)
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(&(glob->error_ctx));
            return ret;
        }

        tloc_cache[uid] = (void*)raw_mem;
        ledger_push(&(glob->ledger), tloc_cache[uid]);
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
glob_deinit_tloc_cache(
    const tsalloc_cfg_t    *tsalloc_cfg,
    tcache_t   *cache    
){
    if (cache == nullptr)
    {
        return TSALLOC_SUCCESS;
    }

    size_t  nbytes;
    int     ret;

    nbytes  = sizeof(tcache_t) + tcache_auxil_mem_size(tsalloc_cfg);
    ret     = sys_unmap(((void*)cache), nbytes);
    if (ret == (-1))
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    return TSALLOC_SUCCESS;
}


tsalloc_err_t
glob_create(
    glob_arena_t  **dest,
    arena_cfg_t     cfg,
    uint16_t        narenas,
    bool            nouse_tcache
){
    static size_t   uid_g;

    if (uid_g == TSALLOC_MAXN_GLOBS)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    if (cfg.auxil_map == TSALLOC_DEFAULT_ARG)
    {
        const tsalloc_cfg_t    *tsalloc_cfg;

        tsalloc_cfg = tsconfig_get_cfg(sys_page_size());
        if (!tsalloc_cfg)
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }

        tsalloc_szreq_t req;

        req = tsconfig_get_szclass(tsalloc_cfg, (1024 * 1024 * 2)); // 2 MiB
        if (req.isslab < 0)
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }

        cfg = (arena_cfg_t){
            .auxil_map                  = &def_auxil_map,
            .auxil_unmap                = &def_auxil_unmap,
            .auxil_madvise              = &def_auxil_madvise,
            .tsalloc_cfg                = tsalloc_cfg,
            .default_new_span_szclass   = req.szclass,
            .auxil_align                = 16,
            .unmap_on_termination       = false,
            .allow_cross_origin_merge   = true
        };
    }

    byte_t *raw_mem;
    size_t  nbytes_req;
    size_t  nbytes_loc_arena_auxil_mem;

    nbytes_loc_arena_auxil_mem  = arena_auxil_mem_size(&cfg);
    nbytes_req                  = sizeof(glob_arena_t) + narenas * (sizeof(arena_t) + nbytes_loc_arena_auxil_mem);
    raw_mem                     = (byte_t*)sys_map(nbytes_req);
    if (raw_mem == nullptr)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    glob_arena_t   *glob;
    arena_t        *loc_arena;
    byte_t         *auxil_mem;
    byte_t         *arenas_addr;
    byte_t         *auxil_mems_addr;
    tsalloc_err_t   ret;

    glob            = (glob_arena_t*)raw_mem;
    arenas_addr     = raw_mem + sizeof(glob_arena_t);
    auxil_mems_addr = arenas_addr + narenas * sizeof(arena_t);
    for (int i = 0; i < narenas; i++)
    {
        loc_arena   = ((arena_t*)arenas_addr) + i;
        auxil_mem   = auxil_mems_addr + nbytes_loc_arena_auxil_mem;
        
        ret = arena_init(
            nullptr, 
            &cfg, 
            auxil_mem, 
            glob,
            loc_arena
        );
        if (ret != TSALLOC_SUCCESS)
        {
            for (int j = 0; j < i; j++)
            {
                (void)arena_deinit(nullptr, ((arena_t*)arenas_addr) + j);
            }
            return TSALLOC_UNTRACKED_FAILURE;
        }
    }

    _nouse_tcache[glob->uid]   = nouse_tcache;
    if (!tcache_key_initialized) 
    {
        pthread_key_create(&tcache_cleanup_key, thread_tcache_destructor);
        pthread_setspecific(tcache_cleanup_key, nullptr);
        tcache_key_initialized  = true;
    }

    *glob   = (glob_arena_t){
        .loc_arenas = (arena_t*)arenas_addr,
        .cfg        = cfg,
        .uid        = uid_g++,
        .narenas    = narenas
    };
    *dest   = glob;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
glob_destroy(
    glob_arena_t   *glob
){
    arena_t        *loc_arena;
    tsalloc_err_t   ret1, ret2;
    size_t          narenas;

    ret2    = TSALLOC_SUCCESS;
    narenas = glob->narenas;
    for (int i = 0; i < narenas; i++)
    {
        ret1    = arena_deinit(nullptr, &(glob->loc_arenas[i]));
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2    = ret1;
        }
    }

    ledger_t       *ledger;
    tcache_t       *cache;

    ledger  = &(glob->ledger);
    while (!ledger_isempty(ledger))
    {
        cache               = ledger_pop(ledger);
        cache->loc_arena    = nullptr;
    }

    size_t  nbytes_req;

    nbytes_req  = sizeof(glob_arena_t) + narenas * (sizeof(arena_t) + arena_auxil_mem_size(&(glob->cfg)));
    sys_unmap(((void*)glob), nbytes_req);

    return ret2;
}

tsalloc_err_t
glob_alloc(
    glob_arena_t   *glob,
    void          **dest,
    size_t          nbytes
){
    tsalloc_err_t   ret;

    if ((glob->cfg.tsalloc_cfg->epoch_max - glob->epoch.alloc) <= nbytes)
    {
        arena_t    *arena;
        uint16_t    narenas;

        narenas = glob->narenas;
        for (uint16_t i = 0; i < narenas; i++)
        {
            arena   = glob->loc_arenas + i;
            ret     = arena_decay(arena);
            if (ret != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(&(glob->error_ctx));
                return ret;
            }
        }
        glob->epoch.alloc   = 0;
    }

    size_t          uid;

    uid = glob->uid;
    if (tloc_cache[uid] == nullptr)
    {
        ret = glob_init_tloc_cache(glob, uid);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(&(glob->error_ctx));
            return ret;
        }
    }
    
    ret = _glob_alloc(
        glob, 
        dest, 
        uid, 
        nbytes, 
        false
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(&(glob->error_ctx));
        return ret;
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
glob_free(
    glob_arena_t   *glob,
    void           *addr
){
    span_t         *span;
    arena_t        *loc_arena;
    tsalloc_err_t   ret;
    size_t          uid;
    size_t          nbytes;

    uid         = glob->uid;
    loc_arena   = _nouse_tcache[uid]? ((arena_t*)tloc_cache[uid]) : ((tcache_t*)tloc_cache[uid])->loc_arena;
    span        = arena_mapto_span(loc_arena, (byte_t*)addr);
    if (span == nullptr)
    {
        set_tsalloc_error(
            &(glob->error_ctx),
            "glob_free::glob.c unable to map address to span, wrong arena",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    if (span->flags.is_slab)
    {
        nbytes  = tsconfig_get_nbytes_szclass(
            glob->cfg.tsalloc_cfg, 
            span->slab_metadata->szclass, 
            true
        );
    }
    else 
    {
        nbytes  = span->nbytes;
    }
    if ((glob->cfg.tsalloc_cfg->epoch_max - glob->epoch.free) <= nbytes)
    {
        arena_t    *arena;
        uint16_t    narenas;

        narenas = glob->narenas;
        for (uint16_t i = 0; i < narenas; i++)
        {
            arena   = glob->loc_arenas + i;
            ret     = arena_decay(arena);
            if (ret != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(&(glob->error_ctx));
                return ret;
            }
        }
        glob->epoch.free   = 0;
    }

    if (span->flags.is_slab)
    {
        if (_nouse_tcache[uid])
        {
            ret = arena_put_batch(loc_arena, (byte_t**)(&addr), 1);
        }
        else
        {
            ret = tcache_put_block(
                &(glob->error_ctx), 
                ((tcache_t*)tloc_cache[uid]), 
                ((byte_t*)addr), 
                span->slab_metadata->szclass
            );
            if (ret != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(&(glob->error_ctx));
                return ret;
            }
        }
    }
    else 
    {
        ret = arena_put_span(loc_arena, span);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(&(glob->error_ctx));
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}