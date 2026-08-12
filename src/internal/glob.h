
#pragma once
#ifndef GLOB_H
#define GLOB_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "os.h"
#include    "mutex.h"
#include    "arena.h"
#include    "ledger.h"
#include    "arenaconfig.h"

#include    <stdatomic.h>


/*
struct global_stats
{
    size_t *ncached_spans;          // need to add nelements to genheap?
    size_t *ncached_blocks;         // add nblocks to stats struct for pail_t
    size_t *nallocs_spans;          // lifetime
    size_t *nallocs_blocks;         // lifetime
    size_t  nbytes_alloc_lftime;
    size_t  nbytes_freed_lftime;
    size_t  nbytes_alloc_c_epoch;
    size_t  nbytes_freed_c_epoch;
    size_t  nbytes_infrastructure;
};
typedef struct global_stats global_stats_t
*/

struct global_arena
{
    arena_t    *loc_arenas;

    struct
    {
        size_t  free;
        size_t  alloc;
    } epoch;

    tsalloc_errctx_t    error_ctx;
    arena_cfg_t         cfg;
    ledger_t            ledger;
    mutex_t             lock;
    size_t              uid;
    uint16_t            narenas;
    uint16_t            arena_idx;
};
typedef struct global_arena glob_arena_t;

static inline arena_t*
glob_claim(
    glob_arena_t   *arena
){
    arena_t    *loc_arena;

    arena->arena_idx++;
    if (arena->arena_idx == arena->narenas)
    {
        arena->arena_idx    = 0;
    }
    loc_arena   = arena->loc_arenas + arena->arena_idx;
    (void)atomic_fetch_add(&loc_arena->nthreads, 1);
    arena_claim(loc_arena);

    return loc_arena;
}


tsalloc_err_t
glob_create(
    glob_arena_t  **dest,
    arena_cfg_t     cfg,
    uint16_t        narenas,
    bool            nouse_tcache
);

tsalloc_err_t
glob_destroy(
    glob_arena_t   *arena
);

tsalloc_err_t
glob_alloc(
    glob_arena_t   *arena,
    void          **dest,
    size_t          nbytes
);

tsalloc_err_t
glob_free(
    glob_arena_t   *arena,
    void           *addr
);


#endif  //GLOB_H