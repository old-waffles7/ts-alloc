
/*
 * @file    scache.h
 * @brief   definitions of functionalities for managing the span cache
 */


#pragma once
#ifndef SCACHE_H
#define SCACHE_H


#include    "common.h"
#include    "error.h"

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
    bin_t      *bins;       ///< pointer to the array of bins
    byte_t     *bitmap;     ///< pointer to the bitmap tracking non-empty bins
    pagetrie_t  pagetrie;   ///< pagetrie for tracking span descriptors, facillitates coalescion
    records_t   origins;    ///< linked-list for tracking origin span descriptors, facillitates explicit global destruction
    objpool_t   spanpool;   ///< object-pool for span descriptors
    objpool_t   slabpool;   ///< object-pool for slab descriptors
    mutex_t     lock;       ///< mutex for thread-safe access
    size_t      nclasses;   ///< number of size classes managed by the cache
    uint32_t    epoch;      ///< cache-global epoch for initializing ages of newly minteed spans
};
typedef struct span_cache   scache_t;

/*
 * @brief   calculates the required auxiliary memory size for the span cache
 *
 * @param   arena_cfg   pointer to the arena configuration
 *
 * @return  size of auxiliary memory in bytes
 */
static inline size_t
scache_auxil_mem_size(
    arena_cfg_t        *arena_cfg
){
    static size_t   nbytes;
    size_t          nszclasses;

    if (nbytes)
    {
        return nbytes;
    }

    nszclasses  = (arena_cfg->tsalloc_cfg->nszclasses_span);
    nbytes      = sizeof(bin_t) * nszclasses;
    nbytes     += 8 + ((nszclasses + 63) / 64) * 8;

    return nbytes;
}

/*
 * @brief   returns pointer to span descriptor associated with specified memory address
 *
 * @param   cache   pointer to the span cache being searched
 * @param   addr    memory address to be used as a key
 *
 * @return  pointer to the assocated span if it exists, otherwise `nullptr`
 */
static inline span_t*
scache_mapto_span(
    scache_t   *cache,
    void       *addr
){
    return ((span_t*)pagetrie_lookup(&(cache->pagetrie), addr));
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
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    byte_t             *auxil_mem,
    scache_t           *cache
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
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
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
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t             *span
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
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   szclass
);

tsalloc_err_t
scache_decay(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache
);


#endif  //SCACHE_H