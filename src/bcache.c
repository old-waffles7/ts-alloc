
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/bcache.h"

#include    "internal/span.h"
#include    "internal/scache.h"
#include    "internal/arenaconfig.h"


tsalloc_err_t   
bcache_init(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *macro,
    byte_t             *auxil_mem,
    bcache_t           *cache
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "scache_init::scache.c nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    pail_t         *pails;
    size_t          nclasses;
    tsalloc_err_t   ret;

    pails       = (pail_t*)auxil_mem;
    nclasses    = arena_cfg->tsalloc_cfg->nszclasses_slab;
    for (uint16_t i = 0; i < nclasses; i++)
    {
        ret = pail_init(
            error_ctx, 
            arena_cfg, 
            &(pails[i]), 
            i
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    cache->macro    = macro;
    cache->nclasses = nclasses;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
bcache_deinit(
    tsalloc_errctx_t   *error_ctx,
    bcache_t           *cache
){
    tsalloc_err_t   ret;

    for (uint16_t i = 0; i < cache->nclasses; i++)
    {
        ret = pail_deinit(error_ctx, &(cache->pails[i]));
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
bcache_put_block(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    bcache_t           *cache,
    byte_t             *block
){
    span_t *slab;

    slab    = scache_mapto_span(cache->macro, ((void*)block));
    if (!slab)
    {
        set_tsalloc_error(
            error_ctx,
            "bcache_put_block::bache.c block not allocated from cache",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    if (slab->slab_metadata->nblocks_free == 0)
    {
        pail_put_slab(&(cache->pails[slab->flags.szclass]), slab);
    }
    slab_put_block(slab, ((void*)block));

    return TSALLOC_SUCCESS;
}