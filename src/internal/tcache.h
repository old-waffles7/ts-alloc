
#pragma once
#ifndef TCACHE_H
#define TCACHE_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "glob.h"
#include    "arena.h"
#include    "column.h"
#include    "ledger.h"
#include    "arenaconfig.h"


struct thread_loc_cache
{
    col_t          *columns;
    arena_t        *loc_arena;
    ledger_coord_t  coord;
    size_t          nszclasses; 
};
typedef struct thread_loc_cache tcache_t;

static inline size_t
tcache_auxil_mem_size(
    const tsalloc_cfg_t    *tsalloc_cfg
){
    static size_t   nbytes;

    if (nbytes)
    {
        return nbytes;
    }

    uint16_t    nszclasses;

    nszclasses  = tsalloc_cfg->nszclasses_span;
    for (uint16_t i = 0; i < nszclasses; i++)
    {
        nbytes += col_auxil_mem_size(tsalloc_cfg->tcache_info[i]) + sizeof(col_t);
    }

    return nbytes;
}

static inline tsalloc_err_t
tcache_init(
    const tsalloc_cfg_t    *tsalloc_cfg,
    tsalloc_errctx_t   *error_ctx,
    byte_t             *auxil_mem,
    glob_arena_t       *global,
    tcache_t           *cache
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "tcache_init::tcache.h nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    col_t          *column;
    col_t          *columns;
    byte_t         *col_auxil_mem;
    byte_t         *auxil_mem_addr;
    size_t          nszclasses;
    tsalloc_err_t   ret;

    nszclasses      = tsalloc_cfg->nszclasses_span;
    columns         = (col_t*)auxil_mem;
    auxil_mem_addr  = auxil_mem + nszclasses * sizeof(col_t);
    for (int i = 0; i < nszclasses; i++)
    {
        column          = columns + i;
        auxil_mem_addr += col_auxil_mem_size(tsalloc_cfg->tcache_info[i]);
        col_auxil_mem   = auxil_mem_addr;

        ret = col_init(
            tsalloc_cfg,
            error_ctx, 
            col_auxil_mem, 
            column, 
            i
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    *cache  = (tcache_t){
        .columns    = columns,
        .loc_arena  = glob_claim(global),
        .nszclasses = nszclasses
    };

    return TSALLOC_SUCCESS;
}

static inline void
tcache_flush(
    tcache_t   *cache
){
    col_t              *column;
    tsalloc_szclass_t   nszclasses;
    
    nszclasses  = cache->nszclasses;
    for (tsalloc_szclass_t i = 0; i < nszclasses; i++)
    {
        column  = cache->columns + i;
        (void)col_flush(nullptr, cache->loc_arena, column);
    }
}

static inline tsalloc_err_t
tcache_get_block(
    tsalloc_errctx_t   *error_ctx,
    tcache_t           *cache,
    byte_t            **dest,
    tsalloc_szclass_t   szclass
){
    if (szclass > cache->nszclasses)
    {
        set_tsalloc_error(
            error_ctx,
            "tcache_get_block::tcache.h invalide szclass arguemnt",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    tsalloc_err_t   ret;

    ret = col_get_block(
        error_ctx, 
        cache->loc_arena, 
        cache->columns + szclass, 
        dest
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
tcache_put_block(
    tsalloc_errctx_t   *error_ctx,
    tcache_t           *cache,
    byte_t             *block,
    tsalloc_szclass_t   szclass
){
    if (szclass > cache->nszclasses)
    {
        set_tsalloc_error(
            error_ctx,
            "tcache_put_block::tcache.h invalide szclass arguemnt",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    tsalloc_err_t   ret;

    ret = col_put_block(
        error_ctx, 
        cache->loc_arena, 
        cache->columns + szclass, 
        block
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
tcache_decay(
    tsalloc_errctx_t   *error_ctx,
    tcache_t           *cache
){
    tsalloc_err_t   ret1, ret2;
    size_t          nszclasses;

    ret2    = TSALLOC_SUCCESS;
    nszclasses  = cache->nszclasses;
    for (int i = 0; i < nszclasses; i++)
    {
        ret1    = col_decay(error_ctx, cache->loc_arena, cache->columns + i);
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2    = ret1;
        }
    }

    return ret2;
}


#endif  //TCACHE_H