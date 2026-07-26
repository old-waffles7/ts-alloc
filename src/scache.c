
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/scache.h"

#include    "internal/bin.h"
#include    "internal/span.h"
#include    "internal/mutex.h"
#include    "internal/objpool.h"
#include    "internal/arenaconfig.h"


static inline bool
scache_find_nonempty_bin(
    scache_t               *cache,
    bin_t                 **dest,
    tsalloc_szclass_t       szclass
){
    bin_t      *bin;
    uint64_t   *bitmap;
    uint64_t    idx;
    uint64_t    word;
    uint64_t    bit_idx;
    uint64_t    word_idx;
    uint64_t    max_word_idx;
    bool        isszclass;

    bitmap          = ((uint64_t*)(cache->bitmap));
    word_idx        = szclass / 64;
    bit_idx         = szclass % 64;
    max_word_idx    = ((cache->nclasses) + 63) / 64;

    word    = bitmap[word_idx] & (~0ULL << bit_idx);
    if (word != 0)
    {
        idx         = (word_idx * 64) + __builtin_ctzll(word);
        isszclass   = true;
    }
    else
    {
        for (uint64_t i = (word_idx + 1); i < max_word_idx; i++)
        {
            word    = bitmap[i];
            if (word != 0)
            {
                idx         = (i * 64) + __builtin_ctzll(word);
                isszclass   = false;
                break;
            }
        }
    }

    if (idx >= cache->nclasses)
    {
        bin = nullptr;
    }
    else 
    {
        bin = &(cache->bins[idx]);
    }

    *dest   = bin;

    return isszclass;
}


tsalloc_err_t
scache_init(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    byte_t             *auxil_mem,
    scache_t           *cache
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "scache_init::scache.c nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    tsalloc_err_t   ret;

    ret = mutex_init(error_ctx, &(cache->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    byte_t *bitmap_addr;
    size_t  nclasses;
    size_t  bitmap_bytes;
    
    nclasses        = (arena_cfg->tsalloc_cfg->nszclasses);
    cache->bins     = (bin_t*)auxil_mem;    
    bitmap_addr     = auxil_mem + (sizeof(bin_t) * nclasses);
    bitmap_bytes    = ((nclasses + 63) / 64) * 8;

    cache->bitmap   = bitmap_addr;
    cache->nclasses = nclasses;
    memset(cache->bitmap, 0, bitmap_bytes);

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
scache_deinit(
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
){
    tsalloc_err_t   ret;

    ret = mutex_deinit(error_ctx, &(cache->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
scache_get_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   szclass
){
    span_t         *span;
    bin_t          *bin;
    bool            isszclass;
    tsalloc_err_t   ret;

    isszclass   = scache_find_nonempty_bin(cache, &bin, szclass);
    if (!bin)
    {
        ret = span_create(
            error_ctx, 
            arena_cfg, 
            spanpool, 
            &span, 
            szclass, 
            TSALLOC_DEFAULT_ARG
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        *dest   = span;
    }
    else 
    {
        span    = bin_get_span(bin);
    }
    
    *dest   = span;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
scache_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    scache_t           *cache,
    span_t             *span
){
    tsalloc_szclass_t   szclass;
    tsalloc_err_t       ret;

    szclass = ((tsalloc_szclass_t)span->flags.szclass);
    ret     = bin_put_span(error_ctx, arena_cfg, &(cache->bins[szclass]), span);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

