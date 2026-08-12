
/**
 * @file    bin.h
 * @brief   definitions of functionalities for managing memory bins
 */

 
#pragma once
#ifndef BIN_H
#define BIN_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "span.h"
#include    "mutex.h"
#include    "bucket.h"
#include    "arenaconfig.h"


/**
 * @struct  bin
 * @brief   represents a collection of spans belonging to the same size class
 */
struct bin
{
    bucket_t        buckets[SPAN_NSTATES];
    ts_szclass_t    szclass;
    size_t          nspans;
    size_t          epoch_min_nspans;
    byte_t          bitmap; // bit length >= SPAN_NSTATES
};
typedef struct bin  bin_t;

/**
 * @brief   initializes a bin for a given size class
 *
 * @param   bin     pointer to the bin to initialize
 * @param   szclass size class associated with the bin
 */
static inline void
bin_init(
    bin_t          *bin,
    ts_szclass_t    szclass
){
    *bin    = (bin_t){
        .szclass    = szclass
    };
}

/**
 * @brief   checks whether the bin contains no spans
 *
 * @param   bin pointer to the bin to check
 *
 * @return  `true` if the bin contains no spans, otherwise false
 */
static inline bool
bin_isempty(
    bin_t  *bin
){
    return (bin->bitmap == 0);
}

/**
 * @brief   retrieves the index of the first non-empty bucket
 *
 * @param   bitmap  bitmap representing non-empty buckets
 *
 * @return  index of the first non-empty bucket, or `-1` if all buckets are empty
 */
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

/**
 * @brief   removes and returns a span from the first non-empty bucket
 *
 * @param   bin pointer to the bin from which to retrieve the span
 *
 * @return  pointer to the retrieved span, or `nullptr` if the bin is empty
 */
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

/**
 * @brief   inserts a span into the appropriate bucket in the bin
 *
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   error_ctx   pointer to the error context struct
 * @param   bin         pointer to the bin receiving the span
 * @param   span        pointer to the span being inserted
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
bin_put_span(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
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

        // Changed to match the current span_set_state() signature.
        ret = span_set_state(
            arena_cfg,
            error_ctx,
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

/**
 * @brief   removes a span from its current state bucket
 *
 * @param   bin     pointer to the bin containing the span
 * @param   span    pointer to the span being removed
 */
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

/**
 * @brief   decays free spans in the bin by marking them as retained
 *
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   error_ctx   pointer to the error context struct
 * @param   bin         pointer to the bin being decayed
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
bin_decay(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
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

        // Changed to match the current span_set_state() signature.
        ret1    = span_set_state(
            arena_cfg,
            error_ctx,
            span,
            TSALLOC_SPAN_RETAINED
        );
        if (ret1 != TSALLOC_SUCCESS)
        {
            bin->epoch_min_nspans   = bin->nspans;
            append_tsalloc_error_trace(error_ctx);
            return ret1;
        }

        // Changed to match the current bin_put_span() signature.
        ret1 = bin_put_span(
            arena_cfg,
            error_ctx, 
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