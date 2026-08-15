
/**
 * @file    column.h
 * @brief   thread-local cache column definitions and interface
 */


#pragma once
#ifndef COLUMN_H
#define COLUMN_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "slab.h"
#include    "span.h"
#include    "glob.h"
#include    "arena.h"
#include    "pagetrie.h"
#include    "arenaconfig.h"


/**
 * @struct  column
 * @brief   represents a cache for blocks of a single size class
 */
struct column
{
    byte_t        **blocks;
    size_t          nblocks;
    size_t          capacity;
    size_t          epoch_min_nblocks;
    ts_szclass_t    szclass;
};
typedef struct column   col_t;


/**
 * @brief   refills a column with blocks from an arena
 *
 * @param   glob    pointer to global allocator
 * @param   col     pointer to column being refilled
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
_col_refill(
    glob_t *glob,
    col_t  *col
){
    size_t          nblocks;
    tsalloc_err_t   ret;

    nblocks = MAX(1, (col->capacity >> 1));
    ret = arena_get_batch(
        glob_claim_arena(glob), 
        col->blocks, 
        col->szclass, 
        nblocks
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(&(glob->error_ctx));
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   calculates the auxiliary memory size required for a column
 *
 * @param   nblocks number of block pointers required
 *
 * @return  required auxiliary memory size in bytes
 */
static inline size_t
col_auxil_mem_size(
    size_t  nblocks
){
    return nblocks * sizeof(void*);
}


/**
 * @brief   initializes a column
 *
 * @param   glob_state  pointer to global allocation state
 * @param   error_ctx   pointer to error handling context
 * @param   auxil_mem   pointer to pre-allocated memory for the column
 * @param   col         pointer to column being initialized
 * @param   szclass     size class associated with the column
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
col_init(
    const glob_alloc_state_t   *glob_state,
    tsalloc_errctx_t           *error_ctx,
    byte_t                     *auxil_mem,
    col_t                      *col,
    ts_szclass_t                szclass
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "col_init::column.h nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    if (szclass > glob_state->nszclasses_slab)
    {
        set_tsalloc_error(
            error_ctx,
            "col_init::column.h invalide szclass arguemnt",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    size_t    capacity;

    capacity    = glob_state->tcache_info[szclass];
    *col        = (col_t){
        .blocks             = (byte_t**)auxil_mem,
        .capacity           = capacity,
        .epoch_min_nblocks  = capacity,
        .szclass            = szclass
    };

    return TSALLOC_SUCCESS;
}


/**
 * @brief   flushes cached blocks from a column
 *
 * @param   glob    pointer to global allocator
 * @param   col     pointer to column being flushed
 */
static inline void
col_flush(
    glob_t     *glob,
    col_t      *col
){
    glob_put_batch_inarena(glob, col->blocks, col->nblocks);
}


/**
 * @brief   retrieves a block from a column
 *
 * @param   glob    pointer to global allocator
 * @param   col     pointer to column supplying the block
 * @param   dest    pointer to destination of block pointer
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
col_get_block(
    glob_t             *glob,
    col_t              *col,
    byte_t            **dest
){
    if (col->nblocks == 0)
    {
        tsalloc_err_t   ret;

        ret = _col_refill(glob, col);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(&(glob->error_ctx));
            return ret;
        }
    }

    *dest   = col->blocks[col->nblocks - 1];
    col->nblocks--;
    if (col->nblocks < col->epoch_min_nblocks)
    {
        col->epoch_min_nblocks  = col->nblocks;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   returns a block to a column
 *
 * @param   glob    pointer to global allocator
 * @param   col     pointer to column receiving the block
 * @param   block   pointer to block being returned
 */
static inline void
col_put_block(
    glob_t *glob,
    col_t  *col,
    byte_t *block
){
    if (col->nblocks == col->capacity)
    {
        byte_t    **batch;
        size_t      nblocks_put;
        size_t      nblocks_keep;

        nblocks_keep    = MAX(1, 3 * (col->capacity >> 2));
        nblocks_put     = col->capacity - nblocks_keep;
        batch           = col->blocks + nblocks_keep;
        col->nblocks   -= nblocks_put;
        glob_put_batch_inarena(glob, batch, nblocks_put);
    }

    col->blocks[col->nblocks]   = block;
    col->nblocks++;
}


/**
 * @brief   decays cached blocks from a column
 *
 * @param   glob    pointer to global allocator
 * @param   col     pointer to column being decayed
 */
static inline void
col_decay(
    glob_t *glob,
    col_t  *col
){
    byte_t    **batch;

    batch   = col->blocks + (col->nblocks - col->epoch_min_nblocks);
    glob_put_batch_inarena(glob, batch, col->epoch_min_nblocks);

    col->nblocks           -= col->epoch_min_nblocks;
    col->epoch_min_nblocks  = col->nblocks;
}


#endif  //COLUMN_H