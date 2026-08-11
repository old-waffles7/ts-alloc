
#pragma once
#ifndef COLUMN_H
#define COLUMN_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "arena.h"
#include    "arenaconfig.h"


struct column
{
    byte_t            **blocks;
    tsalloc_szclass_t   szclass;
    size_t              nblocks;
    size_t              capacity;
    size_t              epoch_min_nblocks;
};
typedef struct column   col_t;

static inline size_t
col_auxil_mem_size(
    size_t  nblocks
){
    return nblocks * sizeof(void*);
}

static inline tsalloc_err_t
col_init(
    tsalloc_errctx_t   *error_ctx,
    tsalloc_cfg_t      *tsalloc_cfg,
    byte_t             *auxil_mem,
    col_t              *col,
    tsalloc_szclass_t   szclass
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

    if (szclass > tsalloc_cfg->nszclasses_slab)
    {
        set_tsalloc_error(
            error_ctx,
            "col_init::column.h invalide szclass arguemnt",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    uint32_t    capacity;

    capacity    = tsalloc_cfg->tcache_info[szclass];
    *col        = (col_t){
        .blocks             = (byte_t**)auxil_mem,
        .szclass            = szclass,
        .capacity           = capacity,
        .epoch_min_nblocks  = capacity
    };

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
col_get_block(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    col_t              *col,
    byte_t            **dest
){
    if (col->nblocks ==  0)
    {
        tsalloc_err_t   ret;

        ret = arena_get_batch(
            arena, 
            col->blocks, 
            col->szclass, 
            col->capacity
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    *dest   = col->blocks[--col->nblocks];
    if (col->nblocks < col->epoch_min_nblocks)
    {
        col->epoch_min_nblocks = col->nblocks;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
col_put_block(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    col_t              *col,
    byte_t             *block
){
    if (col->nblocks == col->capacity)
    {
        byte_t        **batch;
        tsalloc_err_t   ret;
        uint16_t        nblocks_put;
        uint16_t        nblocks_keep;

        nblocks_keep    = MAX(1, 3 * (col->capacity >> 2));
        nblocks_put     = col->capacity - nblocks_keep;
        batch           = col->blocks + nblocks_keep;
        ret = arena_put_batch(arena, batch, nblocks_put);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        col->nblocks   -= nblocks_put;
    }

    col->blocks[col->nblocks]   = block;
    col->nblocks++;

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
col_decay(
    tsalloc_errctx_t   *error_ctx,
    arena_t            *arena,
    col_t              *col
){
    byte_t        **batch;
    tsalloc_err_t   ret;

    ret = arena_put_batch(arena, batch, col->epoch_min_nblocks);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    col->nblocks           -= col->epoch_min_nblocks;
    col->epoch_min_nblocks  = col->nblocks;

    return TSALLOC_SUCCESS;
}


#endif  //COLUMN_H