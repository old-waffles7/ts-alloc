
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
    uint8_t idx;

    idx = bin_first_nonempty_bucket(bin->bitmap);
    if (idx < 0)
    {
        return nullptr;
    }

    span_t *span;

    span    = bucket_pop(&(bin->buckets[idx]));
    return span;
}

static inline tsalloc_err_t
bin_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
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

    span_set_state(
        error_ctx,
        arena_cfg,
        span,
        TSALLOC_SPAN_DIRTY
    );
    bucket_insert(&(bin->buckets[TSALLOC_SPAN_DIRTY]), span);

    return TSALLOC_SUCCESS;
}


#endif  //BIN_H