
#pragma once
#ifndef ARENA_H
#define ARENA_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "scache.h"
#include    "bcache.h"
#include    "arenaconfig.h"


typedef struct global_arena glob_arena_t;


struct arena
{
    const glob_arena_t         *glob;
    const glob_alloc_state_t   *glob_state;
    const arena_cfg_t          *arena_cfg;

    tsalloc_errctx_t   *error_ctx;
    scache_t            scache;
    bcache_t            bcache;
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
    arena_t            *arena,
    byte_t            **batch,
    size_t              nblocks
){
    tsalloc_err_t   ret1, ret2;

    ret2    = TSALLOC_SUCCESS;
    for (int i = 0; i < nblocks; i++)
    {
        ret1    = bcache_put_block(
            arena->error_ctx, 
            arena->arena_cfg, 
            &(arena->bcache), 
            batch[i]
        );
        if (ret1 != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(arena->error_ctx);
            ret2    = ret1;
        }
    }

    return ret2;
}

static inline tsalloc_err_t
arena_put_span(
    arena_t    *arena,
    span_t     *span
){
    tsalloc_err_t   ret;

    ret = scache_put_span(
        arena->error_ctx, 
        arena->arena_cfg, 
        &(arena->scache), 
        span
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
arena_get_batch(
    arena_t        *arena,
    span_t        **dest,
    ts_szclass_t    szclass,
    size_t          nblocks
){
    tsalloc_err_t   ret;

    ret = bcache_get_batch(
        arena->error_ctx, 
        arena->arena_cfg, 
        &(arena->bcache), 
        dest, 
        szclass, 
        nblocks
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
arena_get_span(
    arena_t        *arena,
    span_t        **dest,
    ts_szclass_t    szclass
){
    tsalloc_err_t   ret;
    
    ret = scache_get_span(
        nullptr, 
        arena->error_ctx, 
        arena->arena_cfg, 
        &(arena->scache), 
        dest, 
        szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
arena_decay(
    arena_t    *arena
){
    tsalloc_err_t   ret;

    ret = scache_decay(arena->error_ctx, arena->arena_cfg, &(arena->scache));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline span_t*
arena_mapto_span(
    arena_t    *arena,
    byte_t     *addr
){
    return scache_mapto_span(&(arena->scache), addr);
}


tsalloc_err_t
arena_init(
    const glob_arena_t         *glob,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    byte_t                     *auxil_mem,
    arena_t                    *arena
);

tsalloc_err_t
arena_deinit(
    arena_t    *arena
);


#endif  //ARENA_H