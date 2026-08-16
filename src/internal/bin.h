
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
static inline int8_t
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
static inline tsalloc_err_t
bin_get_span(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    span_t            **dest,
    bin_t              *bin
){
    int8_t  idx;

    idx = bin_first_nonempty_bucket(bin->bitmap);
    if (idx < 0)
    {
        *dest   = nullptr;
        return TSALLOC_SUCCESS;
    }

    span_t *span;

    span    = bucket_pop(&(bin->buckets[idx]));
    if (idx == TSALLOC_SPAN_RETAINED)
    {
        tsalloc_err_t   ret;

        ret = span_set_state(
            arena_cfg, 
            error_ctx, 
            span, 
            TSALLOC_SPAN_UNRETAINED
        );
        if (ret != TSALLOC_SUCCESS)
        {
            bucket_insert(&(bin->buckets[idx]), span);
            append_tsalloc_error_trace(error_ctx);
            return TSALLOC_AUXIL_MADVISE_ERR;
        }
    }

    bin->nspans--;
    if (bin->buckets[idx].root == nullptr) 
    {
        bin->bitmap    &= ~(1 << idx);
    }
    if (bin->epoch_min_nspans > bin->nspans)
    {
        bin->epoch_min_nspans   = bin->nspans;
    }

    *dest   = span;

    return TSALLOC_SUCCESS;
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
    tsalloc_err_t   ret;

    if (span->flags.state == TSALLOC_SPAN_RETAINED)
    {
        bucket_insert(&(bin->buckets[TSALLOC_SPAN_RETAINED]), span);
        bin->bitmap    |= (1 << TSALLOC_SPAN_RETAINED);
    }
    else 
    {
        ret = span_set_state(
            arena_cfg,
            error_ctx,
            span,
            TSALLOC_SPAN_DIRTY
        );
        if (ret == TSALLOC_AUXIL_MADVISE_ERR)
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
static inline tsalloc_err_t
bin_remove_span(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    bin_t              *bin,
    span_t             *span
){
    tsalloc_span_state_t    state;

    state   = (tsalloc_span_state_t)span->flags.state;
    bucket_remove(&(bin->buckets[state]), span);

    if (state == TSALLOC_SPAN_RETAINED)
    {
        tsalloc_err_t   ret;

        ret = span_set_state(
            arena_cfg, 
            error_ctx, 
            span, 
            TSALLOC_SPAN_UNRETAINED
        );
        if (ret != TSALLOC_SUCCESS)
        {
            bucket_insert(&(bin->buckets[state]), span);
            append_tsalloc_error_trace(error_ctx);
            return TSALLOC_AUXIL_MADVISE_ERR;
        }
    }

    if (bin->buckets[state].root == nullptr) 
    {
        bin->bitmap    &= ~(1 << state);
    }
    bin->nspans--;

    return TSALLOC_SUCCESS;
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
        
        //  cannot fail as loop breaks if only spans left are retained
        (void)bin_get_span(
            arena_cfg, 
            error_ctx, 
            &span, 
            bin
        );
        if (!span)
        {
            break;
        }

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

        //  spans with retained state cannot trigger failure of this function
        (void)bin_put_span(
            arena_cfg,
            error_ctx, 
            bin, 
            span
        );
    }
    bin->epoch_min_nspans   = bin->nspans; 

    return ret2;
}


#endif  //BIN_H