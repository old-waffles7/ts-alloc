
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/arena.h"

#include    "internal/glob.h"
#include    "internal/scache.h"
#include    "internal/bcache.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"

#include    <stdatomic.h>


tsalloc_err_t
arena_init(
    const glob_arena_t         *glob,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pagetrie_t                 *pagetrie,
    byte_t                     *auxil_mem,
    arena_t                    *arena,
    uint16_t                    arena_uid
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "arena_init::arena.h nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    
    tsalloc_err_t   ret;
    
    *arena = (arena_t){
        .glob       = glob,
        .glob_state = glob_state,
        .arena_cfg  = arena_cfg,
        .error_ctx  = error_ctx,
        .arena_uid  = arena_uid
    };

    ret = scache_init(
        glob_state, 
        arena_cfg, 
        error_ctx, 
        pagetrie, 
        auxil_mem, 
        &(arena->scache), 
        arena_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    byte_t         *bcache_auxil_mem_addr;

    bcache_auxil_mem_addr   = auxil_mem + scache_auxil_mem_size(glob_state);
    ret = bcache_init(
        glob_state, 
        arena_cfg, 
        error_ctx, 
        &(arena->scache), 
        bcache_auxil_mem_addr, 
        &(arena->bcache)
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)scache_deinit(nullptr, &(arena->scache));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
arena_deinit(
    arena_t    *arena
){
    tsalloc_err_t   ret;

    if (arena->arena_cfg->unmap_on_termination)
    {
        ret = scache_destroy(arena->arena_cfg, arena->error_ctx, &(arena->scache));
    }
    else
    {
        ret = scache_deinit(arena->error_ctx, &(arena->scache));
    }
    if (ret != TSALLOC_SUCCESS)
    {
        (void)bcache_deinit(arena->error_ctx, &(arena->bcache));
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    ret = bcache_deinit(arena->error_ctx, &(arena->bcache));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}