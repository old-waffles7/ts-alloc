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


/*
 * @struct  span_cache
 * @brief   metadata and resources for a span cache
 */
struct span_cache
{
    bin_t      *bins;       ///< pointer to the array of bins
    byte_t     *bitmap;     ///< pointer to the bitmap tracking non-empty bins
    size_t      nclasses;   ///< number of size classes managed by the cache
    mutex_t     lock;       ///< mutex for thread-safe access
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
    arena_conf_t   *arena_cfg
){
    size_t  nbytes;
    size_t  nclasses;

    nclasses    = (arena_cfg->tsalloc_cfg->nszclasses);
    nbytes      = sizeof(bin_t) * nclasses;
    nbytes     += ((nclasses + 63) / 64) * 8;

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
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
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
 */
tsalloc_err_t
scache_deinit(
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
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    scache_t           *cache,
    span_t             *span
);

/*
 * @brief   retrieves a span from the cache for a given size class
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration
 * @param   spanpool    pointer to the object pool used for span allocation
 * @param   cache       pointer to the target span cache
 * @param   dest        pointer to store the retrieved span
 * @param   szclass     size class index of the requested span
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
scache_get_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   szclass
);


#endif  //SCACHE_H