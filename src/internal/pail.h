
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
#include    "pagetrie.h"
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
 * @param   error_ctx   pointer to the error context
 * @param   pail        pointer to the pail receiving the new slab
 *
 * @return  status code representing the outcome of the operation
 */
static tsalloc_err_t
_pail_mint_slab(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pail_t                     *pail
){
    span_t         *slab;
    tsalloc_err_t   ret;

    ret = scache_get_span(
        pail->init_info, 
        glob_state, 
        arena_cfg, 
        error_ctx, 
        pail->macro, 
        &slab, 
        pail->szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
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
 * @param   error_ctx   pointer to the error context
 * @param   pail        pointer to the pail supplying the block
 * @param   dest        pointer to the destination block address
 *
 * @return  status code representing the outcome of the operation
 */
static inline tsalloc_err_t
_pail_get_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pail_t                     *pail,
    byte_t                    **dest
){
    if (!pail->slabs.head)
    {
        tsalloc_err_t   ret;

        ret = _pail_mint_slab(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            pail
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
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
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   pail        pointer to the pail receiving the block
 * @param   slab        pointer to the slab containing the block
 * @param   block       pointer to the block being returned
 *
 * @warning @p slab must be registered in global pagetrie
 */
static inline void
_pail_put_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    span_t                     *slab,
    byte_t                     *block
){
    slab_put_block(slab, ((void*)block));

    bool    isfull;

    isfull  = (slab->slabmeta->nblocks_free == pail->init_info->nblocks);
    if (isfull && (pail->nslabs > 1))
    {
        registry_remove(&(pail->slabs), slab);
        //  slab must be pagetrie
        scache_put_span(
            glob_state, 
            arena_cfg, 
            pail->macro, 
            slab
        );
        pail->nslabs--;
    }
    else if (slab->slabmeta->nblocks_free == 1)
    {
        registry_push(&(pail->slabs), slab);
    }
}


/**
 * @brief   initializes a pail for a given size class
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context
 * @param   spancache   pointer to the span cache used by the pail
 * @param   pail        pointer to the pail being initialized
 * @param   szclass     size class associated with the pail
 *
 * @return  status code representing the outcome of the operation
 */
static inline tsalloc_err_t
pail_init(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *macro,
    pail_t                     *pail,
    ts_szclass_t                szclass
){
    if (szclass >= glob_state->nszclasses_slab)
    {
        set_tsalloc_error(
            error_ctx,
            "pail_init::pail.h invalide szclass argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    *pail   = (pail_t){
        .init_info  = tsconfig_get_slabinfo(glob_state, szclass),
        .macro      = macro,
        .szclass    = szclass
    };

    tsalloc_err_t   ret;

    ret = mutex_init(error_ctx, &(pail->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   deinitializes a pail
 *
 * @param   error_ctx   pointer to the error context
 * @param   pail        pointer to the pail being deinitialized
 *
 * @return  status code representing the outcome of the operation
 */
static inline tsalloc_err_t
pail_deinit(
    tsalloc_errctx_t   *error_ctx,
    pail_t             *pail
){
    tsalloc_err_t   ret;

    ret = mutex_deinit(error_ctx, &(pail->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   retrieves a batch of free blocks from the pail
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context
 * @param   pail        pointer to the pail supplying the blocks
 * @param   dest        array receiving the block addresses
 * @param   nblocks     number of blocks to retrieve
 *
 * @return  status code representing the outcome of the operation
 */
static tsalloc_err_t
pail_get_batch(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pail_t                     *pail,
    byte_t                    **dest,
    size_t                      nblocks
){
    tsalloc_err_t   ret;

    mutex_lock(&(pail->lock));
    
        for (int i = 0; i < nblocks; i++)
        {
            ret = _pail_get_block(
                glob_state, 
                arena_cfg, 
                error_ctx, 
                pail, 
                (dest + i)
            );
            if (ret != TSALLOC_SUCCESS) 
            {
                span_t *slab;

                for (int j = 0; j < i; j++) 
                {
                    slab    = scache_query(pail->macro, dest[j]);
                    _pail_put_block(
                        glob_state, 
                        arena_cfg, 
                        pail, 
                        slab,
                        dest[j]
                    );
                }
                mutex_unlock(&(pail->lock));
                append_tsalloc_error_trace(error_ctx);
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
 */
static inline void
pail_put_block(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    pail_t                     *pail,
    span_t                     *slab,
    byte_t                     *block
){    
    mutex_lock(&(pail->lock));

    _pail_put_block(
        glob_state, 
        arena_cfg, 
        pail, 
        slab, 
        block
    );
    
    mutex_unlock(&(pail->lock));
}


#endif  //PAIL_H