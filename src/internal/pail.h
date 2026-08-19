
/**
 * @file    pail.h
 * @brief   definitions of functionalities for managing pails
 */


#pragma once
#ifndef PAIL_H
#define PAIL_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "slab.h"
#include    "span.h"
#include    "mutex.h"
#include    "scache.h"
#include    "registry.h"
#include    "arenaconfig.h"


/**
 * @struct  pail
 * @brief   represents a collection of slabs for a single size class
 */
struct pail
{
    const tsalloc_slab_info_t  *init_info;
    scache_t                   *macro;
    registry_t                  slabs;
    mutex_t                     lock;
    ts_szclass_t                szclass;
    size_t                      nslabs;
};
typedef struct pail pail_t;


/**
 * @brief   creates and registers a new slab for the pail
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   pail        pointer to the pail receiving the new slab
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 */
static ts_err_t
_pail_mint_slab(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    int32_t                     glob_uid
){
    span_t     *slab;
    ts_err_t    ret;

    ret = scache_get_span(
        pail->init_info, 
        glob_state, 
        arena_cfg, 
        pail->macro, 
        &slab, 
        pail->szclass,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    registry_push(&(pail->slabs), slab);
    pail->nslabs++;

    return TSALLOC_SUCCESS;
}


/**
 * @brief   retrieves a free block from the pail
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   pail        pointer to the pail supplying the block
 * @param   dest        pointer to the destination block address
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 */
static inline ts_err_t
_pail_get_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    byte_t                    **dest,
    int32_t                     glob_uid
){
    if (!pail->slabs.head)
    {
        ts_err_t    ret;

        ret = _pail_mint_slab(
            glob_state, 
            arena_cfg, 
            pail,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }
    }

    span_t *slab;
    byte_t *block;

    slab    = pail->slabs.head;
    block   = slab_get_block(slab);
    if (slab->slabmeta->nblocks_free == 0)
    {
        registry_pop(&(pail->slabs));
    }

    *dest   = block;

    return TSALLOC_SUCCESS;
}

/**
 * @brief   returns a block to its slab and updates the pail's slab registry
 *
 * @param   glob_state      pointer to the global allocation state
 * @param   arena_cfg       pointer to the arena configuration
 * @param   pail            pointer to the pail receiving the block
 * @param   slab            pointer to the slab containing the block
 * @param   block           pointer to the block being returned
 * @param   flush_iffull    flag indicating whether to flush fully freed slabs
 * @param   glob_uid        global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 *
 * @warning @p slab must be registered in global pagetrie
 */
static inline ts_err_t
_pail_put_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    span_t                     *slab,
    byte_t                     *block,
    bool                        flush_iffull,
    int32_t                     glob_uid
){
    slab_put_block(slab, ((void*)block));

    bool        isfull;
    ts_err_t    ret;

    isfull  = (slab->slabmeta->nblocks_free == pail->init_info->nblocks);
    if (flush_iffull && isfull && (pail->nslabs > 1))
    {
        registry_remove(&(pail->slabs), slab);
        
        //  slab must be pagetrie
        ret = scache_put_span(
            glob_state, 
            arena_cfg, 
            pail->macro, 
            slab,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            registry_push(&(pail->slabs), slab);
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }

        pail->nslabs--;
    }
    else if (slab->slabmeta->nblocks_free == 1)
    {
        registry_push(&(pail->slabs), slab);
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   initializes a pail for a given size class
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   macro       pointer to the span cache used by the pail
 * @param   pail        pointer to the pail being initialized
 * @param   szclass     size class associated with the pail
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 */
static inline ts_err_t
pail_init(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *macro,
    pail_t                     *pail,
    ts_szclass_t                szclass,
    int32_t                     glob_uid
){
    if (szclass >= glob_state->nszclasses_slab)
    {
        set_tsalloc_error(
            glob_uid,
            "pail_init::pail.h invalid szclass argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    *pail   = (pail_t){
        .init_info  = tsconfig_get_slabinfo(glob_state, szclass),
        .macro      = macro,
        .szclass    = szclass
    };

    ts_err_t    ret;

    ret = mutex_init(&(pail->lock), glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   deinitializes a pail
 *
 * @param   pail        pointer to the pail being deinitialized
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 */
static inline ts_err_t
pail_deinit(
    pail_t     *pail,
    int32_t     glob_uid
){
    ts_err_t    ret;

    ret = mutex_deinit(&(pail->lock), glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   retrieves a batch of free blocks from the pail
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   pail        pointer to the pail supplying the blocks
 * @param   dest        array receiving the block addresses
 * @param   nblocks     number of blocks to retrieve
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 */
static ts_err_t
pail_get_batch(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    byte_t                    **dest,
    size_t                      nblocks,
    int32_t                     glob_uid
){
    ts_err_t    ret;

    mutex_lock(&(pail->lock));
    
        for (size_t i = 0; i < nblocks; i++)
        {
            ret = _pail_get_block(
                glob_state, 
                arena_cfg, 
                pail, 
                (dest + i),
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS) 
            {
                span_t *slab;

                for (size_t j = 0; j < i; j++) 
                {
                    slab    = scache_query(pail->macro, dest[j]);
                    //  cannot fail as flush can never be procced
                    (void)_pail_put_block(
                        glob_state, 
                        arena_cfg, 
                        pail, 
                        slab, 
                        dest[j],
                        false,
                        glob_uid
                    );
                }
                mutex_unlock(&(pail->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }
        }
    
    mutex_unlock(&(pail->lock));

    return TSALLOC_SUCCESS;
}

/**
 * @brief   returns a block to the pail
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   pail        pointer to the pail receiving the block
 * @param   slab        pointer to the slab containing the block
 * @param   block       pointer to the block being returned
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing the outcome of the operation
 */
static inline ts_err_t
pail_put_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    span_t                     *slab,
    byte_t                     *block,
    int32_t                     glob_uid
){    
    ts_err_t    ret;

    mutex_lock(&(pail->lock));

        ret = _pail_put_block(
            glob_state, 
            arena_cfg, 
            pail, 
            slab, 
            block, 
            true,
            glob_uid
        );
    
    mutex_unlock(&(pail->lock));

    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


#endif  //PAIL_H