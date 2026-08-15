
#pragma once
#ifndef GLOB_H
#define GLOB_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "mutex.h"
#include    "arena.h"
#include    "ledger.h"
#include    "pagetrie.h"
#include    "arenaconfig.h"

#include    <stdatomic.h>


struct tsalloc_global_arena
{
    arena_t    *arenas;

    const glob_alloc_state_t   *glob_state;
    arena_cfg_t                 arena_cfg;

    tsalloc_errctx_t    error_ctx;
    pagetrie_t          pagetrie;
    ledger_t            ledger;
    mutex_t             ledger_lock;
    _Atomic(uint32_t)   arena_idx;  //  uint16_t upsized to 32 bit-width for costless casting
    uint32_t            narenas;    //  uint16_t upsized to 32 bit-width for costless casting
    uint16_t            glob_uid;
};
typedef struct tsalloc_global_arena glob_t;

static inline arena_t*
glob_claim_arena(
    glob_t *glob
){
    arena_t    *arena;
    uint32_t    arena_idx;

    (void)atomic_compare_exchange_strong_explicit(
        &(glob->arena_idx), 
        &(glob->narenas), 
        0, 
        memory_order_release, 
        memory_order_relaxed
    );

    arena_idx   = atomic_fetch_add_explicit(&(glob->arena_idx), 1, memory_order_release);

    arena   = glob->arenas + arena_idx;

    return arena;
}

//  warning blocks must be valid. i.e they must be cut from spans initialized to slabs
static inline void 
glob_put_batch_inarena(
    glob_t     *glob,
    byte_t    **batch,
    size_t      nblocks
){
    arena_t    *arena;
    span_t     *slab;

    for (size_t i = 0; i < nblocks; i++)
    {
        slab    = (span_t*)pagetrie_lookup(&(glob->pagetrie), batch[i]);
        arena   = glob->arenas + ((uint16_t)slab->flags.arena_uid);
        arena_put_block(arena, slab, batch[i]);
    }
}

static inline void
glob_register_tcache
(
    glob_t     *glob,
    tcache_t   *cache
){
    mutex_lock(&(glob->ledger_lock));
        ledger_push(&(glob->ledger), cache);
    mutex_unlock(&(glob->ledger_lock));
}

static inline void
glob_deregister_tcache
(
    glob_t     *glob,
    tcache_t   *cache
){
    mutex_lock(&(glob->ledger_lock));
        ledger_remove(&(glob->ledger), cache);
    mutex_unlock(&(glob->ledger_lock));
}


tsalloc_err_t
glob_create(
    const arena_cfg_t  *arena_cfg,
    glob_t            **dest
);

//  undefined behavior if called in thread A while thread B is using
tsalloc_err_t
glob_destroy(
    glob_t *glob
);

tsalloc_err_t
glob_alloc(
    glob_t *glob,
    void  **dest,
    size_t  nbytes
);

tsalloc_err_t
glob_free(
    glob_t *glob,
    void   *addr
);

void 
glob_turnon_tcache(
    glob_t *glob
);

tsalloc_err_t 
glob_turnoff_tcache(
    glob_t *glob
);


#endif  //GLOB_H