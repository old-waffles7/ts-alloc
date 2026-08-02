
/**
 * @file    bcache.h
 * @brief   block cache definitions and interface for small memory allocations
 */


#pragma once
#ifndef bcache_h
#define bcache_h

#include    "common.h"
#include    "error.h"

#include    "span.h"
#include    "slab.h"
#include    "pail.h"
#include    "arenaconfig.h"

/**
 * @brief   cache for small memory blocks.
 */
struct block_cache
{
    pail_t     *pails;      /**< array of pails for block management */
    scache_t   *macro;      /**< pointer to the backend span cache */
    size_t      nclasses;   /**< number of supported size classes */
};
typedef struct block_cache  bcache_t;

/**
 * @brief   calculates required auxiliary memory size for the block cache
 *
 * @param   arena_cfg   arena configuration context
 *
 * @return  required memory size in bytes
 */
static inline size_t
bcache_auxil_mem_size(
    arena_cfg_t        *arena_cfg
){
    size_t  nbytes;
    size_t  nclasses;

    nclasses    = arena_cfg->tsalloc_cfg->nszclasses_slab;
    nbytes      = sizeof(pail_t) * nclasses;

    return nbytes;
}

/**
 * @brief   retrieves a batch of memory blocks
 *
 * @param   error_ctx   error handling context
 * @param   arena_cfg   arena configuration context
 * @param   cache       block cache instance
 * @param   dest        pointer to store the batch array pointer
 * @param   szclass     size class index of the requested blocks
 * @param   nblocks     number of blocks to retrieve
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
bcache_get_batch(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    bcache_t           *cache,
    byte_t            **dest,
    tsalloc_szclass_t   szclass,
    size_t              nblocks
){
    tsalloc_err_t   ret;

    ret = pail_get_batch(
        error_ctx, 
        arena_cfg, 
        cache->macro, 
        &(cache->pails[szclass]), 
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
 * @brief   initializes a block cache
 *
 * @param   error_ctx   error handling context
 * @param   arena_cfg   arena configuration context
 * @param   macro       backend span cache for fulfilling slab allocations
 * @param   auxil_mem   pre-allocated memory buffer for internal structures
 * @param   cache       block cache instance to initialize
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t   
bcache_init(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *macro,
    byte_t             *auxil_mem,
    bcache_t           *cache
);

/**
 * @brief   deinitializes a block cache.
 *
 * @param   error_ctx   error handling context
 * @param   cache       block cache instance to deinitialize
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
bcache_deinit(
    tsalloc_errctx_t   *error_ctx,
    bcache_t           *cache
);

/**
 * @brief   frees a memory block back to the block cache
 *
 * @param   error_ctx   error handling context
 * @param   arena_cfg   arena configuration context
 * @param   cache       block cache instance
 * @param   block       pointer to the memory block being freed
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
bcache_put_block(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    bcache_t           *cache,
    byte_t             *block
);


#endif  //bcache_h