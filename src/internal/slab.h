
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
    byte_t         *bitmap;      
    ts_szclass_t    szclass; 
    uint16_t        nbytes_block; 
    uint16_t        nblocks_free;
};
typedef struct slab slab_t;

/*
 * @brief   initializes a new memory slab
 *
 * @param   slabinfo    pointer to the slab information/layout configuration
 * @param   slabpool    pointer to the object pool used for slab metadata allocation
 * @param   span        pointer to the span being formatted as a slab
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
ts_err_t 
slab_init(
    const tsalloc_slab_info_t  *slabinfo,
    objpool_t                  *slabpool,
    span_t                     *span,
    int32_t                     glob_uid
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