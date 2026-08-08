
#pragma once
#ifndef BIN_H
#define BIN_H


#include    "common.h"
#include    "error.h"

#include    "span.h"
#include    "mutex.h"
#include    "bucket.h"
#include    "arenaconfig.h"


struct bin_stats
{
    size_t  nspans_cached[SPAN_NSTATES];
};
typedef struct bin_stats    bin_stats_t;


struct bin 
{
    #ifdef  OPT_TRACK_STATS
        bin_stats_t stats;
    #endif  //OPT_TRACK_STATS

    bucket_t            buckets[SPAN_NSTATES];
    tsalloc_szclass_t   szclass;
    uint32_t            nspans;
    uint32_t            epoch_min_nspans;
    byte_t              bitmap; // bit length >= SPAN_NSTATES
};
typedef struct bin  bin_t;

static inline void
bin_init(
    bin_t              *bin,
    tsalloc_szclass_t   szclass
){
    *bin    = (bin_t){0};
}

static inline bool 
bin_isempty(
    bin_t  *bin
){
    return (bin->bitmap == 0);
}

static inline int16_t
bin_first_nonempty_bucket(
    uint8_t bitmap
){
    if (bitmap == 0) 
    {
        return -1; 
    }
    
    return __builtin_ctz(((uint32_t)bitmap));
}

static inline span_t*
bin_get_span(
    bin_t  *bin
){
    int16_t idx;

    idx = bin_first_nonempty_bucket(bin->bitmap);
    if (idx < 0)
    {
        return nullptr;
    }

    span_t *span;

    span    = bucket_pop(&(bin->buckets[idx]));
    if (bin->buckets[idx].root == nullptr) 
    {
        bin->bitmap    &= ~(1 << idx);
    }

    bin->nspans--;
    if (bin->epoch_min_nspans > bin->nspans)
    {
        bin->epoch_min_nspans   = bin->nspans;
    }

    return span;
}

static inline tsalloc_err_t
bin_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    bin_t              *bin,
    span_t             *span
){
    if ((span->flags.szclass) != (bin->szclass))
    {
        set_tsalloc_error(
            error_ctx,
            "bin_put_span::bin.h input span has incorrect size-class",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    if (span->flags.state == TSALLOC_SPAN_RETAINED)
    {
        bucket_insert(&(bin->buckets[TSALLOC_SPAN_RETAINED]), span);
        bin->bitmap    |= (1 << TSALLOC_SPAN_RETAINED);
    }
    else 
    {
        tsalloc_err_t   ret;

        ret = span_set_state(
            error_ctx,
            arena_cfg,
            span,
            TSALLOC_SPAN_DIRTY
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        bucket_insert(&(bin->buckets[TSALLOC_SPAN_DIRTY]), span);
        bin->bitmap    |= (1 << TSALLOC_SPAN_DIRTY);
    }
    bin->nspans++;

    return TSALLOC_SUCCESS;
}

static inline void
bin_remove_span(
    bin_t  *bin,
    span_t *span
){
    tsalloc_span_state_t    state;

    state   = (tsalloc_span_state_t)span->flags.state;
    bucket_remove(&(bin->buckets[state]), span);
    if (bin->buckets[state].root == nullptr) 
    {
        bin->bitmap    &= ~(1 << state);
    }
}
static inline tsalloc_err_t
bin_decay(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    bin_t              *bin
){
    if (!arena_cfg->auxil_madvise)
    {
        return TSALLOC_SUCCESS;
    }

    span_t             *span;
    auxil_madvise_fn    auxil_madvise;
    tsalloc_err_t       ret1, ret2;
    uint32_t            nspans_decay;
    
    auxil_madvise   = arena_cfg->auxil_madvise;
    nspans_decay    = bin->epoch_min_nspans; 
    ret2            = TSALLOC_SUCCESS;

    for (uint32_t i = 0; i < nspans_decay; i++)
    {
        if (bin_first_nonempty_bucket(bin->bitmap) == TSALLOC_SPAN_RETAINED)
        {
            break;
        }
        
        span = bin_get_span(bin);
        if (!span)
        {
            break;
        }

        ret1    = span_set_state(
            error_ctx,
            arena_cfg,
            span,
            TSALLOC_SPAN_RETAINED
        );
        if (ret1 != TSALLOC_SUCCESS)
        {
            bin->epoch_min_nspans   = bin->nspans;
            append_tsalloc_error_trace(error_ctx);
            return ret1;
        }

        ret1 = bin_put_span(
            error_ctx, 
            arena_cfg, 
            bin, 
            span
        );
        if (ret1 != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            ret2    = ret1;
        }
    }
    bin->epoch_min_nspans   = bin->nspans; 

    return ret2;
}


#endif  //BIN_H