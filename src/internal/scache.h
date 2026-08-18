
/*
 * @file    scache.h
 * @brief   definitions of functionalities for managing the span cache
 */


#pragma once
#ifndef SCACHE_H
#define SCACHE_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "bin.h"
#include    "span.h"
#include    "mutex.h"
#include    "records.h"
#include    "pagetrie.h"
#include    "arenaconfig.h"


/*
 * @struct  span_cache
 * @brief   metadata and resources for a span cache
 */
struct span_cache
{
    bin_t          *bins;       
    byte_t         *bitmap;     
    pagetrie_t     *pagetrie;
    mutex_t         lock;    
    objpool_t       spanpool; 
    objpool_t       slabpool;  
    records_t       origins;    
    ts_szclass_t    nszclasses;
    uint32_t        epoch;     
    uint16_t        arena_uid; 
};
typedef struct span_cache   scache_t;

/*
 * @brief   calculates the required auxiliary memory size for the span cache
 *
 * @param   global_config  pointer to the global allocation state
 *
 * @return  number of bytes required for the span cache's auxiliary memory
 */
static inline size_t
scache_auxil_mem_size(
    const glob_alloc_state_t   *global_config
){
    size_t  nbytes;
    size_t  nszclasses;

    nszclasses = global_config->nszclasses_span;

    nbytes  = sizeof(bin_t) * nszclasses;
    nbytes += 8 + ((nszclasses + 63) / 64) * 8;

    return nbytes;
}

/*
 * @brief   queries the pagetrie for the span containing a given address
 *
 * @param   cache   pointer to the span cache
 * @param   block   address to query
 *
 * @return  pointer to the span covering @p block, or `nullptr` if no span
 *          is registered for the address
 */
static inline span_t*
scache_query(
    const scache_t *cache,
    const byte_t   *block
){
    return (span_t*)pagetrie_lookup(cache->pagetrie, block);
}


/*
 * @brief   initializes a span cache
 *
 * @param   global_state    pointer to the global allocation state
 * @param   arena_cfg       pointer to the arena configuration
 * @param   error_ctx       pointer to the error context used to report failure
 * @param   pagetrie        pagetrie used to register and query cached spans
 * @param   auxil_mem       pre-allocated auxiliary memory for bins and bitmap
 * @param   cache           pointer to the span cache being initialized
 * @param   arena_uid       unique identifier of the arena owning the cache
 *
 * @return  `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
 */
tsalloc_err_t
scache_init(
    const glob_alloc_state_t   *global_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pagetrie_t                 *pagetrie,
    byte_t                     *auxil_mem,
    scache_t                   *cache,
    uint16_t                    arena_uid
);

/*
 * @brief   deinitializes a span cache without explicitly unmapping memory
 *
 * @param   error_ctx  pointer to the error context used to report failure
 * @param   cache      pointer to the span cache being deinitialized
 *
 * @return  `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
 *
 * @warning this function does not explicitly unmap any underlying span mappings
 */
tsalloc_err_t
scache_deinit(
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
);

/*
 * @brief   destroys a span cache and explicitly unmaps its origin mappings
 *
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context used to report failure
 * @param   cache       pointer to the span cache being destroyed
 *
 * @return  `TSALLOC_SUCCESS` if all tracked mappings and resources were
 *          successfully destroyed, otherwise an appropriate error code
 *
 * @warning this function requires `arena_cfg->unmap_on_termination` to be enabled 
 */
tsalloc_err_t
scache_destroy(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
);

/*
 * @brief   returns an allocated span to the span cache
 *
 * @param   glob_state  pointer to the global allocation state
 * @param   arena_cfg   pointer to the arena configuration
 * @param   cache       pointer to the target span cache
 * @param   span        span being returned to the cache
 *
 * @warning the span must already be registered in the cache's pagetrie
 */
tsalloc_err_t
scache_put_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                     *span
);

/*
 * @brief   retrieves a span suitable for a requested size class
 *
 * @param   slab_init_info  optional slab initialization information; when
 *                          non-null, the returned span is initialized as a slab
 * @param   glob_state      pointer to the global allocation state
 * @param   arena_cfg       pointer to the arena configuration
 * @param   error_ctx       pointer to the error context used to report failure
 * @param   cache           pointer to the target span cache
 * @param   dest            pointer receiving the retrieved span
 * @param   szclass         requested span size class
 *
 * @return  `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
 */
tsalloc_err_t
scache_get_span(
    const tsalloc_slab_info_t  *slab_init_info,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                    **dest,
    ts_szclass_t                szclass
);


/*
 * @brief   retrieves a specially aligned span suitable for a requested size class
 *
 * @param   slab_init_info  optional slab initialization information; when
 *                          non-null, the returned span is initialized as a slab
 * @param   glob_state      pointer to the global allocation state
 * @param   arena_cfg       pointer to the arena configuration
 * @param   error_ctx       pointer to the error context used to report failure
 * @param   cache           pointer to the target span cache
 * @param   dest            pointer receiving the retrieved span
 * @param   align           requested alignment
 * @param   szclass         requested span size class
 *
 * @return  `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
 *
 * @warning @p align must be strictly greater than pagesize configured by @p glob_state
 */
tsalloc_err_t
scache_get_span_aligned(
    const tsalloc_slab_info_t  *slab_init_info,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                    **dest,
    size_t                      align,
    ts_szclass_t                szclass
);

/*
 * @brief   decays cached spans across all size-class bins
 *
 * @param   arena_cfg   pointer to the arena configuration
 * @param   error_ctx   pointer to the error context used to report failure
 * @param   cache       pointer to the span cache being decayed
 *
 * @return  `TSALLOC_SUCCESS` if all bins decay successfully, otherwise the
 *          most recent error encountered while processing the bins
 */
tsalloc_err_t
scache_decay(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
);


#endif  //SCACHE_H