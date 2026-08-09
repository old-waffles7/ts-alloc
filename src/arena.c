
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/arena.h"

#include    "internal/scache.h"
#include    "internal/bcache.h"
#include    "internal/arenaconfig.h"

#include    <stdatomic.h>


tsalloc_err_t
arena_init(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    byte_t             *auxil_mem,
    arena_t            *arena
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

    byte_t         *bcache_addr;
    tsalloc_err_t   ret;

    bcache_addr = auxil_mem + scache_auxil_mem_size(arena_cfg);
    
    *arena = (arena_t){0};

    ret = scache_init(
        error_ctx, 
        arena_cfg, 
        auxil_mem, 
        &(arena->scache)
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = bcache_init(
        error_ctx, 
        arena_cfg, 
        &(arena->scache), 
        bcache_addr, 
        &(arena->bcache)
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)scache_deinit(error_ctx, &(arena->scache));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    arena->cfg = arena_cfg;
    atomic_init(&(arena->nthreads), 0);

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
arena_deinit(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena
){
    tsalloc_err_t   ret;

    if (arena->cfg->unmap_on_termination)
    {
        ret = scache_destroy(error_ctx, arena->cfg, &(arena->scache));
    }
    else
    {
        ret = scache_deinit(error_ctx, &(arena->scache));
    }
    if (ret != TSALLOC_SUCCESS)
    {
        (void)bcache_deinit(error_ctx, &(arena->bcache));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = bcache_deinit(error_ctx, &(arena->bcache));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}