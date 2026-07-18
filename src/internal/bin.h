
#pragma once
#ifndef BIN_H
#define BIN_H


#include    "common.h"
#include    "error.h"

#include    "span.h"
#include    "mutex.h"
#include    "bucket.h"
#include    "arenaconfig.h"

#include    <string.h>


struct bin_stats
{
    size_t  nspans_cached[SPAN_NSTATES];
};
typedef struct bin_stats    bin_stats_t;


struct bin 
{
    bucket_t            buckets[SPAN_NSTATES];
    mutex_t             mutexes[SPAN_NSTATES];
    tsalloc_szclass_t   szclass;

    #ifdef  OPT_TRACK_STATS
        bin_stats_t stats;
    #endif  //OPT_TRACK_STATS
};
typedef struct bin  bin_t;

static inline tsalloc_err_t
bin_init(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    bin_t              *bin,
    tsalloc_szclass_t   szclass
){
    tsalloc_err_t   ret;

    for (int i = 0; i < SPAN_NSTATES; i++)
    {
        ret = mutex_init(error_ctx, &(bin->mutexes[i]));
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }
    bin->szclass    = 0;
    memset(&(bin->buckets), 0, (sizeof(bucket_t) * SPAN_NSTATES));

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
bin_deinit(
    tsalloc_errctx_t   *error_ctx,
    bin_t              *bin
){
    if (!bin)
    {
        return TSALLOC_SUCCESS;
    }

    tsalloc_err_t   ret;

    for (int i = 0; i < SPAN_NSTATES; i++)
    {
        ret = mutex_deinit(error_ctx, &(bin->mutexes[i]));
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

static tsalloc_err_t
bin_get_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    bin_t              *bin,
    bool                is_clean
){

    //  if taking to long to lock mutex on a larger bucket to split slab, then
    //  after sleep for a while wake up and ask OS for new memory?
}


#endif  //BIN_H