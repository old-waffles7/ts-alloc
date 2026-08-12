
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/glob.h"

#include    "config/tsalloc_config.h"

#include    "internal/os.h"
#include    "internal/arena.h"
#include    "internal/ledger.h"
#include    "internal/tcache.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"

#include    <unistd.h>
#include    <pthread.h>
#include    <stdatomic.h>


/*
TODO

_Thread_local static tcache_t  *tcaches[TSALLOC_MAXN_GLOBS];
static bool                     tcache_isoff[TSALLOC_MAXN_GLOBS];

static pthread_key_t    tsalloc_thread_cleanup_key;
static bool             cleanup_key_isinit

static inline void
tsalloc_thread_cleanup(
    void   *arg
);
*/  


tsalloc_err_t
glob_create(
    const arena_cfg_t  *arena_cfg,
    glob_t            **dest
){
    static _Atomic(uint16_t)    glob_epoch;
    uint16_t                    glob_uid;

    glob_uid    = atomic_fetch_add(&glob_epoch, 1);
    if (glob_uid >= TSALLOC_MAXN_GLOBS - 1)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    const glob_alloc_state_t   *glob_state;
    arena_cfg_t                 _arena_cfg;

    if (arena_cfg == TSALLOC_DEFAULT_ARG)
    {
        glob_state  = tsconfig_get_cfg(sys_page_size());
        if (glob_state == nullptr)
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }

        ts_szclass_t    def_szclass;
        tsalloc_szreq_t req;

        req = tsconfig_get_szclass(glob_state, (1024ULL * 1024ULL * 2ULL));
        if ((req.isslab == -1) || (req.isslab == true))
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }
        _arena_cfg  = (arena_cfg_t){
            .auxil_map                  = &def_auxil_map,
            .auxil_unmap                = &def_auxil_unmap,
            .auxil_madvise              = &def_auxil_madvise,
            .pagesize                   = glob_state->page_size,
            .def_alloc_align            = 16,
            .default_new_span_szclass   = req.szclass,
            .lcpu_arena_count           = 4,
            .unmap_on_termination       = false,
            .default_turnoff_tcaches    = false,
            .allow_cross_origin_merge   = true
        };
    }
    else 
    {
        glob_state  = tsconfig_get_cfg(arena_cfg->pagesize);
        if (glob_state == nullptr)
        {
            return TSALLOC_UNTRACKED_FAILURE;
        }

        _arena_cfg  = *arena_cfg;
    }

    size_t  narenas;

    narenas = _arena_cfg.lcpu_arena_count * sysconf(_SC_NPROCESSORS_ONLN);
    if (narenas > MAXN_ARENAS)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }

    byte_t         *raw;
    arena_t        *arena_addr;
    glob_t         *glob;    
    size_t          nbytes_req;
    size_t          nbytes_arena_auxil_mem;

    nbytes_arena_auxil_mem  = arena_auxil_mem_size(&_arena_cfg);
    nbytes_req              = sizeof(glob_arena_t) 
                              + narenas * (sizeof(arena_t) + nbytes_arena_auxil_mem);
    raw     = sys_map(nbytes_req);
    if (raw == nullptr)
    {
        return TSALLOC_UNTRACKED_FAILURE;
    }
    glob        = (glob_arena_t*)raw;
    arena_addr  = (arena_t*)(raw + sizeof(glob_arena_t));
    
    tsalloc_err_t   ret;

    ret = pagetrie_init(nullptr, &(glob->pagetrie));
    if (ret != TSALLOC_SUCCESS)
    {
        (void)sys_unmap(((void*)raw), nbytes_req);
        return TSALLOC_UNTRACKED_FAILURE;
    }

    *glob   = (glob_arena_t){
        .arenas         = arena_addr,
        .glob_state     = glob_state,
        .arena_cfg      = _arena_cfg,
        .glob_uid       = glob_uid,
        .narenas        = narenas
    };

    byte_t     *auxil_mem;
    
    auxil_mem   = (raw + sizeof(glob_arena_t) + narenas * sizeof(arena_t));
    for (ts_szclass_t i = 0; i < narenas; i++)
    {
        auxil_mem  += nbytes_arena_auxil_mem;

        ret = arena_init(
            nullptr, 
            &_arena_cfg, 
            auxil_mem, 
            glob, 
            arena_addr + i
        );
        if (ret != TSALLOC_SUCCESS)
        {
            (void)pagetrie_deinit(&(glob->pagetrie));
            (void)sys_unmap(((void*)raw), nbytes_req);
            return TSALLOC_UNTRACKED_FAILURE;
        }
    }

    *dest   = glob;

    return TSALLOC_SUCCESS;
}



