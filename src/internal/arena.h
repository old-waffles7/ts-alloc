
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
    return scache_auxil_mem_size(arena_cfg) + bcache_auxil_mem_size(arena_cfg);
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


#endif  //ARENA_H