/*
 * @file    slab.h
 * @brief   definitions of functionalities for managing memory slabs
 */


#pragma once
#ifndef SLAB_H
#define SLAB_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "objpool.h"


typedef struct span span_t;


/*
 * @struct  slab
 * @brief   metadata for a memory slab
 */
struct slab
{
    byte_t             *bitmap;         ///< pointer to the bitmap tracking block allocation
    uint16_t            nbytes_block;   ///< size of an individual block within the slab
    uint16_t            nblocks_free;   ///< number of currently free blocks in the slab
    tsalloc_szclass_t   szclass;        ///< szclass of slab blocks
};
typedef struct slab slab_t;

/*
 * @brief   initializes a new memory slab
 *
 * @param   cfg         pointer to the allocator configuration
 * @param   slabinfo    pointer to the slab information/layout configuration
 * @param   error_ctx   pointer to the error context struct
 * @param   slabpool    pointer to the object pool used for slab metadata allocation
 * @param   span        pointer to the span being formatted as a slab
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t slab_init(
    const tsalloc_slab_info_t  *slabinfo,
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *slabpool,
    span_t             *span
);

/*
 * @brief   deinitializes a memory slab and returns its metadata to the pool
 *
 * @param   slabpool    pointer to the object pool used for slab metadata
 * @param   span        pointer to the span formatted as a slab
 */
void
slab_deinit(
    objpool_t  *slabpool,
    span_t     *span
);

/*
 * @brief   retrieves a free block from a slab using its bitmap
 *
 * @param   span    pointer to the span containing the slab metadata and memory
 *
 * @return  pointer to the allocated block
 */
byte_t*
slab_get_block(
    span_t *span
);

/*
 * @brief   frees a block back to a slab by clearing its bitmap entry
 *
 * @param   span    pointer to the span containing the slab metadata and memory
 * @param   block   pointer to the memory block being freed
 */
void
slab_put_block(
    span_t *span,
    void   *block
);


#endif  //SLAB_H