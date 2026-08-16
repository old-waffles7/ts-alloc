
/**
 * @file    arena.h
 * @brief   arena definitions and interface
 */


#pragma once
#ifndef ARENA_H
#define ARENA_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "scache.h"
#include    "bcache.h"
#include    "arenaconfig.h"


typedef struct tsalloc_global_arena glob_t;


/**
 * @struct  arena
 * @brief   represents an allocation arena and its associated caches
 */
struct arena
{
    const glob_t               *glob;
    const glob_alloc_state_t   *glob_state;
    const arena_cfg_t          *arena_cfg;

    struct 
    {
        size_t  max;
        size_t  alloc;
    } epoch;

    tsalloc_errctx_t   *error_ctx;
    scache_t            scache;
    bcache_t            bcache;
    uint16_t            arena_uid;
};
typedef struct arena    arena_t;

/**
 * @brief   decays the arena's span cache
 *
 * @param   arena   pointer to arena to have memory decayed
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
_arena_decay(
    arena_t    *arena
){
    tsalloc_err_t   ret;

    ret = scache_decay(arena->arena_cfg, arena->error_ctx, &(arena->scache));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   calculates the required auxiliary memory size for an arena
 *
 * @param   glob_state  pointer to global allocation state
 *
 * @return  required auxiliary memory size in bytes
 */
static inline size_t
arena_auxil_mem_size(
    const glob_alloc_state_t   *glob_state
){
    static size_t   nbytes;

    if (nbytes)
    {
        return nbytes;
    }

    nbytes  = scache_auxil_mem_size(glob_state) + bcache_auxil_mem_size(glob_state);

    return nbytes;
}

/**
 * @brief   returns a block to the arena's block cache
 *
 * @param   arena   pointer to arena receiving the block
 * @param   slab    pointer to slab containing the block
 * @param   block   block being returned
 */
static inline void
arena_put_block(
    arena_t    *arena,
    span_t     *slab,
    byte_t     *block
){
    bcache_put_block(
        arena->glob_state, 
        arena->arena_cfg, 
        &(arena->bcache), 
        slab, 
        block
    );
}

/**
 * @brief   returns a span to the arena's span cache
 *
 * @param   arena   pointer to arena receiving the span
 * @param   span    pointer to span being returned
 */
static inline void
arena_put_span(
    arena_t    *arena,
    span_t     *span
){
    scache_put_span(
        arena->glob_state, 
        arena->arena_cfg, 
        &(arena->scache), 
        span
    );
}

/**
 * @brief   retrieves a batch of memory blocks from the arena
 *
 * @param   arena   pointer to arena supplying the blocks
 * @param   dest    array receiving the block addresses
 * @param   szclass size class of the requested blocks
 * @param   nblocks number of blocks to retrieve
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
arena_get_batch(
    arena_t        *arena,
    byte_t        **dest,
    ts_szclass_t    szclass,
    size_t          nblocks
){
    tsalloc_err_t   ret;

    ret = bcache_get_batch(
        arena->glob_state, 
        arena->arena_cfg, 
        arena->error_ctx, 
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


/**
 * @brief   retrieves a span from the arena's span cache
 *
 * @param   arena   pointer to arena supplying the span
 * @param   dest    pointer to destination of pointer to recieved span
 * @param   szclass size class of the requested span
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
arena_get_span(
    arena_t        *arena,
    span_t        **dest,
    ts_szclass_t    szclass
){
    tsalloc_err_t   ret;

    ret = scache_get_span(
        nullptr, 
        arena->glob_state, 
        arena->arena_cfg, 
        arena->error_ctx, 
        &(arena->scache), 
        dest, 
        szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(arena->error_ctx);
        return ret;
    }

    size_t  nbytes;

    nbytes  = (*dest)->nbytes;
    if ((arena->epoch.max - arena->epoch.alloc) <= nbytes)
    {
        ret = _arena_decay(arena);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(arena->error_ctx);
            return ret;
        }
        arena->epoch.alloc  = 0;
    }
    else
    {
        arena->epoch.alloc += nbytes;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   initializes an arena
 *
 * @param   glob        pointer to global arena
 * @param   glob_state  pointer to global allocation state
 * @param   arena_cfg   pointer to arena configuration
 * @param   error_ctx   pointer to error handling context
 * @param   pagetrie    pointer to pagetrie used by the arena
 * @param   auxil_mem   pointer to pre-allocated auxiliary memory for the arena
 * @param   arena       pointer to arena instance to initialize
 * @param   arena_uid   unique identifier of the arena
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
arena_init(
    const glob_t               *glob,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pagetrie_t                 *pagetrie,
    byte_t                     *auxil_mem,
    arena_t                    *arena,
    uint16_t                    arena_uid
);


/**
 * @brief   deinitializes an arena
 *
 * @param   arena   pointer to arena instance to deinitialize
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
arena_deinit(
    arena_t    *arena
);


#endif  //ARENA_H