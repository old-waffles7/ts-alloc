
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
    arena_config   *cfg;
    scache_t        scache;
    bcache_t        bcache;
    _Atomic(size_t) nthreads;
};
typedef struct arena    arena_t;

static inline size_t
arena_auxil_mem_size(
    arena_config   *arena_cfg
){
    return scache_auxil_mem_size(arena_cfg) + bcache_auxil_mem_size(arena_cfg);
}


static inline tsalloc_err_t
arena_init(
    tsalloc_errctx_t   *error_ctx,
    arena_config       *arena_cfg,
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

    byte_t         *bache_addr;
    tsalloc_err_t   ret;

    bache_addr  = auxil_mem + sizeof(scache_t);
    *arena      = (arena_t){0};

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
        bache_addr, 
        &(arena->bcache)
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    arena->cfg  = arena_cfg;

    return TSALLOC_SUCCESS;
}


#endif  //ARENA_H