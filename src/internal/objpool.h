/*
 * @file    objpool.h
 * @brief   fixed-size object pool implemented using an intrusive free list
 */


#pragma once
#ifndef OBJPOOL_H
#define OBJPOOL_H


#include    <stddef.h>

#include    "error.h"


/*
 * @struct  iobjpool
 * @brief   fixed-size intrusive object pool
 */
struct iobjpool
{
    void   *chunk_stack;    ///< pointer to the most recently allocated chunk
    void   *slab_stack;     ///< pointer to the head of the intrusive free list
    size_t  align;          ///< alignment requirement for the objects
    size_t  nbytes_slab;    ///< size of an individual object slab
    size_t  nbytes_chunk;   ///< total size of a newly allocated chunk
};

typedef struct iobjpool objpool_t;


/*
 * @brief   initializes a new object pool
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   objpool     pointer to the object pool to initialize
 * @param   align       memory alignment requirement (must be power of 2), defaults to 8 on 0
 * @param   nbytes_obj  size of the object in bytes
 * @param   nobjs_chunk minimum number of objects to allocate per chunk, defaults to 256 on 0
 * 
 * @return  status code representing success or failure
 */
tsalloc_err_t
objpool_init(
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *objpool,
    size_t              align,
    size_t              nbytes_obj,
    size_t              nobjs_chunk
);

/*
 * @brief   destroys an object pool and frees all associated chunks
 * 
 * @param   objpool     pointer to the object pool to deinitialize
 */
inline void 
objpool_deinit(
    objpool_t  *objpool
);

/*
 * @brief   allocates a single object from the pool
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   objpool     pointer to the object pool
 * @param   dest        pointer to store the allocated memory address
 * 
 * @return  status code representing success or failure
 */
tsalloc_err_t
objpool_alloc(
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *objpool,
    void              **dest
);

/*
 * @brief   returns an object to the pool's free list
 * 
 * @param   objpool     pointer to the object pool
 * @param   ptr         pointer to the memory to free
 */
inline void
objpool_free(
    objpool_t  *objpool,
    void       *ptr
);


#endif  //OBJPOOL_H