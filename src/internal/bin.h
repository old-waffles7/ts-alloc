
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
 * @param   arena_cfg   pointer to the arena configuration
 * @param   dest        pointer to store the retrieved span
 * @param   bin         pointer to the bin from which to retrieve the span
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
static inline ts_err_t
bin_get_span(
    const arena_cfg_t  *arena_cfg,
    span_t            **dest,
    bin_t              *bin,
    int32_t             glob_uid
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
        ts_err_t   ret;

        ret = span_set_state(
            arena_cfg,
            span,
            TSALLOC_SPAN_UNRETAINED,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            bucket_insert(&(bin->buckets[idx]), span);
            append_tsalloc_error_trace(glob_uid);
            return ret;
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
 * @param   bin         pointer to the bin receiving the span
 * @param   span        pointer to the span being inserted
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
static inline ts_err_t
bin_put_span(
    const arena_cfg_t  *arena_cfg,
    bin_t              *bin,
    span_t             *span,
    int32_t             glob_uid
){
    ts_err_t   ret;

    if (span->flags.state == TSALLOC_SPAN_RETAINED)
    {
        bucket_insert(&(bin->buckets[TSALLOC_SPAN_RETAINED]), span);
        bin->bitmap    |= (1 << TSALLOC_SPAN_RETAINED);
    }
    else 
    {
        ret = span_set_state(
            arena_cfg,
            span,
            TSALLOC_SPAN_DIRTY,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob_uid);
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
 * @param   arena_cfg   pointer to the arena configuration
 * @param   bin         pointer to the bin containing the span
 * @param   span        pointer to the span being removed
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 */
static inline ts_err_t
bin_remove_span(
    const arena_cfg_t  *arena_cfg,
    bin_t              *bin,
    span_t             *span,
    int32_t             glob_uid
){
    tsalloc_span_state_t    state;

    state   = (tsalloc_span_state_t)span->flags.state;
    bucket_remove(&(bin->buckets[state]), span);

    if (state == TSALLOC_SPAN_RETAINED)
    {
        ts_err_t   ret;

        ret = span_set_state(
            arena_cfg,
            span,
            TSALLOC_SPAN_UNRETAINED,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            bucket_insert(&(bin->buckets[state]), span);
            append_tsalloc_error_trace(glob_uid);
            return ret;
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
 * @param   bin         pointer to the bin being decayed
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
static inline ts_err_t
bin_decay(
    const arena_cfg_t  *arena_cfg,
    bin_t              *bin,
    int32_t             glob_uid
){
    if (!arena_cfg->auxil_madvise)
    {
        return TSALLOC_SUCCESS;
    }

    span_t             *span;
    auxil_madvise_fn    auxil_madvise;
    ts_err_t            ret1, ret2;
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
            &span,
            bin,
            glob_uid
        );
        if (!span)
        {
            break;
        }

        ret1    = span_set_state(
            arena_cfg,
            span,
            TSALLOC_SPAN_RETAINED,
            glob_uid
        );
        if (ret1 != TSALLOC_SUCCESS)
        {
            bin->epoch_min_nspans   = bin->nspans;
            append_tsalloc_error_trace(glob_uid);
            return ret1;
        }

        //  spans with retained state cannot trigger failure of this function
        (void)bin_put_span(
            arena_cfg,
            bin,
            span,
            glob_uid
        );
    }
    bin->epoch_min_nspans   = bin->nspans; 

    return ret2;
}


#endif  //BIN_H