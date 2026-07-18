
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/span.h"

#include    "internal/objpool.h"
#include    "internal/arenaconfig.h"


tsalloc_err_t
slab_init(
    const tsalloc_config_t     *cfg,
    const tsalloc_slab_info_t  *slabinfo,
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *slabpool,
    span_t             *span
){
    slab_t         *metadata;
    tsalloc_err_t   ret;

    ret = objpool_alloc(error_ctx, slabpool, ((void*)(&metadata)));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    byte_t *bitmap;

    bitmap      = ((byte_t*)metadata) + (2 * sizeof(uint16_t));
    *metadata   = (slab_t){
        .nbytes_block   = slabinfo->block_size,
        .nblocks_free   = slabinfo->nblocks,
        .bitmap         = bitmap
    };

    span->flags.is_slab = true;
    span->slab_metadata = metadata;

    return TSALLOC_SUCCESS;
}

inline void
slab_deinit(
    objpool_t  *slabpool,
    span_t     *span
){
    span->flags.is_slab = false;
    objpool_free(slabpool, ((void*)(span->slab_metadata)));
}


tsalloc_err_t
span_create(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    span_t            **dest,
    tsalloc_szclass_t   szclass,
    size_t              _align
){
    span_t         *span;
    tsalloc_err_t   ret;

    ret = objpool_alloc(error_ctx, spanpool, ((void*)(&span)));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    void   *mem;
    size_t  nbytes;

    nbytes  = tsalloc_szclass_span_size((arena_cfg->tsalloc_cfg), szclass);
    mem     = arena_cfg->auxil_map(
        arena_cfg->extra, 
        _align? _align : arena_cfg->auxil_align, 
        nbytes
    );
    if (!mem)
    {
        set_tsalloc_error
        (
            error_ctx,
            "span_create::span.c auxilliary mapper could not allocate memory",
            TSALLOC_AUXIL_MAP_ERR
        );
        return TSALLOC_AUXIL_MAP_ERR;
    }

    *span   = (span_t){
        .flags.szclass  = szclass,
        .addr           = mem,
        .nbytes         = nbytes
    };

    *dest   = span;

    return TSALLOC_SUCCESS;
}

inline tsalloc_err_t
span_destroy(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_config,
    objpool_t          *spanpool,
    span_t             *span
){
    int ret;

    ret = arena_config->auxil_unmap(
        arena_config->extra,
        ((void*)(span->addr)),
        span->nbytes
    );
    if (ret)
    {
        set_tsalloc_error(
            error_ctx,
            "span_destroy::span.c auxilliary unmapper could not free memory",
            TSALLOC_AUXIL_UNMAP_ERR
        );
        return TSALLOC_AUXIL_UNMAP_ERR;
    }

    if (spanpool)
    {
        objpool_free(spanpool, ((void*)span));
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
span_split(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    span_t            **origin,
    span_t            **dest,
    tsalloc_szclass_t   szclass
){
    span_t *split;
    size_t  split_nbytes;
    size_t  origin_nbytes;

    split_nbytes    = tsalloc_szclass_span_size((arena_cfg->tsalloc_cfg), szclass);
    origin_nbytes   = ((*origin)->nbytes) - split_nbytes;
    if (((*origin)->nbytes) < split_nbytes)
    {
        set_tsalloc_error(
            error_ctx,
            "span_split::span.c origin span too small to split",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS; 
    }
    
    if (origin_nbytes == 0) 
    {
        split                       = *origin;
        split->flags.is_slab        = false;
        split->flags.is_dumpable    = false;

        *origin = nullptr;
        *dest   = split;

        return TSALLOC_SUCCESS;
    }
    
    tsalloc_err_t   ret;

    ret = objpool_alloc(error_ctx, spanpool, ((void*)(&split)));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    *split          = (span_t){0};
    split->flags    = (*origin)->flags;
    split->addr     = ((*origin)->addr) + origin_nbytes;
    split->nbytes   = split_nbytes;

    (*origin)->nbytes   = origin_nbytes;
    
    *dest   = split;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
span_coalesce(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    span_t             *lspan,
    span_t             *rspan,
    span_t            **dest
){
    if (!lspan)
    {
        *dest   = rspan;
        return TSALLOC_SUCCESS;
    }
    if (!rspan)
    {
        *dest   = lspan;
        return TSALLOC_SUCCESS;
    }

    if ((lspan->addr + lspan->nbytes) != (rspan->addr))
    {
        set_tsalloc_error(
            error_ctx,
            "span_coalesce::span.c memory address of lspan must precede that of rspan",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS; 
    }

    span_t             *span;
    size_t              nbytes;
    tsalloc_szclass_t   szclass; 

    nbytes  = (lspan->nbytes) + (rspan->nbytes);
    szclass = tsalloc_get_szclass((arena_cfg->tsalloc_cfg), nbytes);

    span    = lspan;
    *span   = (span_t){
        .flags.age      = MIN((lspan->flags.age), (rspan->flags.age)),
        .flags.szclass  = szclass,
        .flags.arena    = lspan->flags.arena,
        .flags.state    = MAX((lspan->flags.state), (rspan->flags.state)),
        .addr           = lspan->addr,
        .nbytes         = nbytes
    };
    objpool_free(spanpool, ((void*)rspan));

    *dest   = span;

    return TSALLOC_SUCCESS;
}

