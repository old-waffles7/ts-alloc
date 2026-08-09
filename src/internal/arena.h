
#pragma once
#ifndef ARENA_H
#define ARENA_H


#include    "common.h"
#include    "error.h"

#include    "scache.h"
#include    "bcache.h"
#include    "arenaconfig.h"


struct arena
{
    arena_cfg_t    *cfg;
    scache_t        scache;
    bcache_t        bcache;
    _Atomic(size_t) nthreads;
};
typedef struct arena    arena_t;

static inline size_t
arena_auxil_mem_size(
    arena_cfg_t    *arena_cfg
){
    static size_t   nbytes;

    if (nbytes)
    {
        return nbytes;
    }

    nbytes  = scache_auxil_mem_size(arena_cfg) + bcache_auxil_mem_size(arena_cfg);

    return nbytes;
}

static inline tsalloc_err_t
arena_put_batch(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    byte_t            **batch,
    size_t              nblocks
){
    tsalloc_err_t   ret1, ret2;

    ret2    = TSALLOC_SUCCESS;
    for (int i = 0; i < nblocks; i++)
    {
        ret1    = bcache_put_block(
            error_ctx, 
            arena->cfg, 
            &(arena->bcache), 
            batch[i]
        );
        if (ret1 != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            ret2    = ret1;
        }
    }

    return ret2;
}

static inline tsalloc_err_t
arena_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    span_t             *span
){
    tsalloc_err_t   ret;

    ret = scache_put_span(
        error_ctx, 
        arena->cfg, 
        &(arena->scache), 
        span
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
arena_get_batch(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    byte_t            **dest,
    tsalloc_szclass_t   szclass,
    size_t              nblocks
){
    tsalloc_err_t   ret;

    ret = bcache_get_batch(
        error_ctx, 
        arena->cfg, 
        &(arena->bcache), 
        dest, 
        szclass, 
        nblocks
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
arena_get_span(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    span_t            **dest,
    tsalloc_szclass_t   szclass
){
    tsalloc_err_t   ret;

    ret = scache_get_span(
        nullptr, 
        error_ctx, 
        arena->cfg, 
        &(arena->scache), 
        dest, 
        szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


tsalloc_err_t
arena_init(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    byte_t             *auxil_mem,
    arena_t            *arena
);

tsalloc_err_t
arena_deinit(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena
);

static inline void
arena_claim(
    arena_t    *arena
){
    (void)atomic_fetch_add(&(arena->nthreads), 1);
}


#endif  //ARENA_H