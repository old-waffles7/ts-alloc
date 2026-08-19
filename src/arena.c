
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/arena.h"

#include    "internal/glob.h"
#include    "internal/scache.h"
#include    "internal/bcache.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"

#include    <stdatomic.h>


ts_err_t
arena_init(
    const glob_t               *glob,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pagetrie_t                 *pagetrie,
    byte_t                     *auxil_mem,
    arena_t                    *arena,
    uint16_t                    arena_uid
){
    int32_t glob_uid = glob->glob_uid;

    if (!auxil_mem)
    {
        set_tsalloc_error(
            glob_uid,
            "arena_init::arena.c nullptr auxil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    ts_err_t    ret;
    
    *arena = (arena_t){
        .glob       = glob,
        .glob_state = glob_state,
        .arena_cfg  = arena_cfg,
        .epoch.max  = glob_state->epoch_max,
        .glob_uid   = glob_uid,
        .arena_uid  = arena_uid
    };

    ret = scache_init(
        glob_state, 
        arena_cfg, 
        pagetrie, 
        auxil_mem, 
        &(arena->scache), 
        arena_uid,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    byte_t         *bcache_auxil_mem_addr;

    bcache_auxil_mem_addr   = auxil_mem + scache_auxil_mem_size(glob_state);
    ret = bcache_init(
        glob_state, 
        arena_cfg, 
        &(arena->scache), 
        bcache_auxil_mem_addr, 
        &(arena->bcache),
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)scache_deinit(&(arena->scache), TSALLOC_NO_ERROR_CONTEXT);
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

ts_err_t
arena_deinit(
    arena_t    *arena
){
    ts_err_t    ret;

    if (arena->arena_cfg->unmap_on_termination)
    {
        ret = scache_destroy(arena->arena_cfg, &(arena->scache), arena->glob_uid);
    }
    else
    {
        ret = scache_deinit(&(arena->scache), arena->glob_uid);
    }
    if (ret != TSALLOC_SUCCESS)
    {
        (void)bcache_deinit(&(arena->bcache), arena->glob_uid);
        append_tsalloc_error_trace(arena->glob_uid);
        return ret;
    }

    ret = bcache_deinit(&(arena->bcache), arena->glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}