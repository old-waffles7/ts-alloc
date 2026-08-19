
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

    pagetrie_t          pagetrie;
    ledger_t            ledger;
    mutex_t             ledger_lock;
    _Atomic(uint32_t)   arena_idx;  //  uint16_t upsized to 32 bit-width for costless casting
    uint32_t            narenas;    //  uint16_t upsized to 32 bit-width for costless casting
    int32_t             glob_uid;
};
typedef struct tsalloc_global_arena glob_t;

static inline arena_t*
glob_claim_arena(
    glob_t *glob
){
    uint32_t current_idx;
    uint32_t next_idx;

    current_idx = atomic_load_explicit(&(glob->arena_idx), memory_order_relaxed);
    do 
    {
        next_idx    = (current_idx + 1 >= glob->narenas)? 0 : current_idx + 1;
    } while (!atomic_compare_exchange_weak_explicit(
                &(glob->arena_idx), 
                &current_idx, 
                next_idx, 
                memory_order_release, 
                memory_order_relaxed));

    return glob->arenas + current_idx;
}

//  warning blocks must be valid. i.e they must be cut from spans initialized to slabs
static inline ts_err_t 
glob_put_batch_inarena(
    glob_t     *glob,
    byte_t    **batch,
    size_t      nblocks
){
    arena_t    *arena;
    span_t     *slab;
    ts_err_t    ret;

    for (size_t i = 0; i < nblocks; i++)
    {
        slab    = (span_t*)pagetrie_lookup(&(glob->pagetrie), batch[i]);
        arena   = glob->arenas + ((uint16_t)slab->flags.arena_uid);
        ret     = arena_put_block(arena, slab, batch[i]);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob->glob_uid);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

//  assume align > pagesize
static inline ts_err_t
glob_alloc_spc_aligned_bulk(
    glob_t *glob,
    void  **dest,
    size_t  align,
    size_t  nbytes
){
    ts_szclass_t    szclass;

    //  force nbytes to be a span allocation
    nbytes  = MAX(glob->glob_state->page_size, nbytes);
    szclass = tsconfig_get_span_szclass(glob->glob_state, nbytes);
    if (szclass == (-1))
    {
        set_tsalloc_error(
            glob->glob_uid,
            "glob_alloc::glob.c invalid nbytes argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    span_t     *span;
    arena_t    *arena;
    ts_err_t    ret;

    arena   = glob_claim_arena(glob);
    ret     = arena_get_span_aligned(
        arena, 
        &span, 
        align, 
        szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob->glob_uid);
        return ret;
    }

    *dest   = (void*)span->addr;

    return TSALLOC_SUCCESS;
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


ts_err_t
glob_create(
    const arena_cfg_t  *arena_cfg,
    glob_t            **dest
);

//  undefined behavior if called in thread A while thread B is using
ts_err_t
glob_destroy(
    glob_t *glob
);

ts_err_t
glob_alloc(
    glob_t *glob,
    void  **dest,
    size_t  nbytes
);

ts_err_t
glob_free(
    glob_t *glob,
    void   *addr
);

//  allocator must still be valid otherwise undefined behavior

void 
glob_turnon_tcache(
    glob_t *glob
);

//  allocator must still be valid otherwise undefined behavior
ts_err_t 
glob_turnoff_tcache(
    glob_t *glob
);

/*
size_t
glob_nfails_tcleanup(
    glob_t *glob
);
*/


#endif  //GLOB_H