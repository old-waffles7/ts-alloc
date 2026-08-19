
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
 * @param   macro       pointer to backup span cache
 * @param   auxil_mem   pointer to pre-allocated memory buffer for internal structures
 * @param   cache       pointer to block cache instance to initialize
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
static inline ts_err_t   
bcache_init(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *macro,
    byte_t                     *auxil_mem,
    bcache_t                   *cache,
    int32_t                     glob_uid
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            glob_uid,
            "bcache_init::bcache.h nullptr auxil_mem argument",
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

    ts_err_t   ret;

    for (ts_szclass_t i = 0; i < nszclasses; i++)
    {
        ret = pail_init(
            glob_state, 
            arena_cfg, 
            macro, 
            pails + i, 
            i,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            for (ts_szclass_t j = 0; j < i; j++)
            {
                (void)pail_deinit(pails + j, TSALLOC_NO_ERROR_CONTEXT);
            }
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

/**
 * @brief   deinitializes a block cache
 *
 * @param   cache       pointer to block cache instance to deinitialize
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
static inline ts_err_t
bcache_deinit(
    bcache_t   *cache,
    int32_t     glob_uid
){
    ts_err_t   ret1, ret2;

    ret2    = TSALLOC_SUCCESS;
    for (ts_szclass_t i = 0; i < cache->nszclasses; i++)
    {
        ret1    = pail_deinit(cache->pails + i, glob_uid);
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2    = ret1;
            append_tsalloc_error_trace(glob_uid);
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
 * @param   cache       pointer to block cache 
 * @param   dest        array receiving the block addresses
 * @param   szclass     size class of requested blocks
 * @param   nblocks     number of blocks to retrieve
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 *
 * @warning szclass must be a validated by caller
 */
static inline ts_err_t
bcache_get_batch(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    bcache_t                   *cache,
    byte_t                    **dest,
    ts_szclass_t                szclass,
    size_t                      nblocks,
    int32_t                     glob_uid
){    
    ts_err_t   ret;

    ret = pail_get_batch(
        glob_state, 
        arena_cfg, 
        cache->pails + szclass, 
        dest, 
        nblocks,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

/**
 * @brief   returns a memory block to the block cache
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   cache       pointer to block cache
 * @param   slab        slab containing the block
 * @param   block       pointer to the memory block being returned
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
static inline ts_err_t 
bcache_put_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    bcache_t                   *cache,
    span_t                     *slab,
    byte_t                     *block,
    int32_t                     glob_uid
){
    ts_err_t   ret;
    
    ret = pail_put_block(
        glob_state, 
        arena_cfg, 
        cache->pails + ((ts_szclass_t)slab->slabmeta->szclass), 
        slab, 
        block,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


#endif  //bcache_h