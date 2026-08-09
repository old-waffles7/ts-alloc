
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
    size_t          nszclasses;
    tsalloc_err_t   ret;

    pails       = (pail_t*)auxil_mem;
    nszclasses  = arena_cfg->tsalloc_cfg->nszclasses_slab;
    for (uint16_t i = 0; i < nszclasses; i++)
    {
        ret = pail_init(error_ctx, arena_cfg, &(pails[i]), i);
        if (ret != TSALLOC_SUCCESS)
        {
            for (uint16_t j = 0; j < i; j++) 
            {
                (void)pail_deinit(error_ctx, &(pails[j]));
            }
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    cache->pails    = pails;
    cache->macro    = macro;
    cache->nclasses = nszclasses;

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
    
    slab = scache_mapto_span(cache->macro, ((void*)block));
    if (!slab)
    {
        set_tsalloc_error(
            error_ctx,
            "bcache_put_block::bache.c block not allocated from this cache",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    pail_t         *pail;
    tsalloc_err_t   ret;
    
    pail    = &(cache->pails[slab->flags.szclass]);
    ret     = pail_put_block(
                error_ctx, 
                arena_cfg, 
                cache->macro, 
                pail, 
                block
            );
    
    return ret;
}