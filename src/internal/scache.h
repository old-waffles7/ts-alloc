
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
 * @param   global_config   pointer to the global allocation state
 *
 * @return  size of auxiliary memory in bytes
 */
static inline size_t
scache_auxil_mem_size(
    const glob_alloc_state_t   *global_config
){
    static size_t   nbytes;
    size_t          nszclasses;

    if (nbytes)
    {
        return nbytes;
    }

    nszclasses  = (global_config->nszclasses_span);
    nbytes      = sizeof(bin_t) * nszclasses;
    nbytes     += 8 + ((nszclasses + 63) / 64) * 8;

    return nbytes;
}

/*
 * @brief   initializes a new span cache
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration
 * @param   auxil_mem   pointer to the pre-allocated auxiliary memory
 * @param   cache       pointer to the span cache being initialized
 *
 * @return  status code representing success or failure
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
 * @brief   deinitializes a span cache
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   cache       pointer to the span cache being deinitialized
 *
 * @return  status code representing success or failure
 * 
 * @warning does not unmap any memory, allocated or cached
 */
tsalloc_err_t
scache_deinit(
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
);

/*
 * @brief   deinitializes a span cache, unconditionally attempts to unmap all cached and allocated 
 *          memory
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration
 * @param   cache       pointer to the span cache being deinitialized
 *
 * @return  status code representing success or failure 
 */
tsalloc_err_t
scache_destroy(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
);

/*
 * @brief   returns a span to the cache
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration
 * @param   cache       pointer to the target span cache
 * @param   span        pointer to the span being returned
 *
 * @return  status code representing success or failure
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
 * @brief   retrieves a span from the cache for a given size class
 *
 * @param   slab_init_info  pointer to info struct for slab initialization
 * @param   error_ctx       pointer to the error context struct
 * @param   arena_cfg       pointer to the arena configuration
 * @param   spanpool        pointer to the object pool used for span allocation
 * @param   cache           pointer to the target span cache
 * @param   dest            pointer to store the retrieved span
 * @param   szclass         size class index of the requested span
 *
 * @return  status code representing success or failure
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

tsalloc_err_t
scache_decay(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
);


#endif  //SCACHE_H