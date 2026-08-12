
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
#include    "pagetrie.h"
#include    "arenaconfig.h"

#include    <stdatomic.h>


struct global_arena
{
    arena_t    *arenas;

    struct
    {
        size_t  free;
        size_t  alloc;
    } epoch;

    const glob_alloc_state_t   *glob_state;
    arena_cfg_t                 arena_cfg;

    tsalloc_errctx_t    error_ctx;
    pagetrie            pagetrie;
    ledger_t            ledger;
    _Atomic(uint16_t)   arena_idx;
    uint16_t            glob_uid;
    uint16_t            narenas;
};
typedef struct global_arena glob_t;

tsalloc_err_t
glob_create(
    const arena_cfg_t  *arena_cfg,
    glob_arena_t      **dest
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