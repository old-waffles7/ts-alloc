
/**
 * @file    bcache.h
 * @brief   block cache definitions and interface for small memory allocations
 */


#pragma once
#ifndef bcache_h
#define bcache_h


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "span.h"
#include    "slab.h"
#include    "pail.h"
#include    "arenaconfig.h"


/**
 * @brief   cache for small memory blocks
 */
struct block_cache
{
    pail_t         *pails;     
    scache_t       *macro;    
    ts_szclass_t    nszclasses; 
};
typedef struct block_cache  bcache_t;

/**
 * @brief   initializes a block cache
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context
 * @param   macro       pointer to backup span cache
 * @param   auxil_mem   pointer to pre-allocated memory buffer for internal structures
 * @param   cache       pointer to block cache instance to initialize
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t   
bcache_init(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *macro,
    byte_t                     *auxil_mem,
    bcache_t                   *cache
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "scache_init::scache.c nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    pail_t         *pails;
    ts_szclass_t    nszclasses;

    pails       = (pail_t*)auxil_mem;
    nszclasses  = glob_state->nszclasses_slab;
    *cache      = (bcache_t){
            .pails      = pails,
            .macro      = macro,
            .nszclasses = nszclasses
    };

    tsalloc_err_t   ret;

    for (ts_szclass_t i = 0; i < nszclasses; i++)
    {
        ret = pail_init(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            macro, 
            pails + i, 
            i
        );
        if (ret != TSALLOC_SUCCESS)
        {
            for (ts_szclass_t j = 0; j < i; j++)
            {
                (void)pail_deinit(nullptr, pails + j);
            }
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

/**
 * @brief   deinitializes a block cache
 *
 * @param   error_ctx   pointer to the error context
 * @param   cache       pointer to block cache instance to deinitialize
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
bcache_deinit(
    tsalloc_errctx_t   *error_ctx,
    bcache_t           *cache
){
    tsalloc_err_t   ret1, ret2;

    ret2    = TSALLOC_SUCCESS;
    for (ts_szclass_t i = 0; i < cache->nszclasses; i++)
    {
        ret1    = pail_deinit(error_ctx, cache->pails + i);
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2    = ret1;
            append_tsalloc_error_trace(error_ctx);
        }
    }

    return ret2;
}

/**
 * @brief   calculates required auxiliary memory size for the block cache
 *
 * @param   glob_state  pointer to the global allocation state
 *
 * @return  required memory size in bytes
 */
static inline size_t
bcache_auxil_mem_size(
    const glob_alloc_state_t   *glob_state
){
    size_t          nbytes;
    ts_szclass_t    nszclasses;

    nszclasses  = glob_state->nszclasses_slab;
    nbytes      = sizeof(pail_t) * nszclasses;

    return nbytes;
}

/**
 * @brief   retrieves a batch of memory blocks
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context
 * @param   cache       pointer to block cache 
 * @param   dest        array receiving the block addresses
 * @param   szclass     size class of requested blocks
 * @param   nblocks     number of blocks to retrieve
 *
 * @return  status code representing success or failure
 *
 * @warning szclass must be a validated by caller
 */
static inline tsalloc_err_t
bcache_get_batch(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    bcache_t                   *cache,
    byte_t                    **dest,
    ts_szclass_t                szclass,
    size_t                      nblocks
){    
    tsalloc_err_t   ret;

    ret = pail_get_batch(
        glob_state, 
        arena_cfg, 
        error_ctx, 
        cache->pails + szclass, 
        dest, 
        nblocks
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

/**
 * @brief   returns a memory block to the block cache
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context
 * @param   cache       pointer to block cache
 * @param   slab        slab containing the block
 * @param   block       pointer to the memory block being returned
 */
static inline void 
bcache_put_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    bcache_t                   *cache,
    span_t                     *slab,
    byte_t                     *block
){
    pail_put_block(
        glob_state, 
        arena_cfg, 
        cache->pails + ((ts_szclass_t)slab->slabmeta->szclass), 
        slab, 
        block
    );
}


#endif  //BCACHE_H