
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/scache.h"

#include    "internal/bin.h"
#include    "internal/span.h"
#include    "internal/mutex.h"
#include    "internal/objpool.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"


static inline bool
scache_find_nonempty_bin_idx(
    scache_t               *cache,
    tsalloc_szclass_t      *dest,
    tsalloc_szclass_t       szclass
){
    uint64_t   *bitmap;
    uint64_t    idx;
    uint64_t    word;
    uint64_t    bit_idx;
    uint64_t    word_idx;
    uint64_t    max_word_idx;
    bool        isoverfit;

    idx             = cache->nclasses;                
    isoverfit       = true;                                  
    bitmap          = ((uint64_t*)(cache->bitmap));
    word_idx        = szclass / 64;
    bit_idx         = szclass % 64;
    max_word_idx    = ((cache->nclasses) + 63) / 64;

    word    = bitmap[word_idx] & (~0ULL << bit_idx);
    if (word != 0)
    {
        idx         = (word_idx * 64) + __builtin_ctzll(word);
        isoverfit   = (idx > szclass);
    }
    else
    {
        for (uint64_t i = (word_idx + 1); i < max_word_idx; i++)
        {
            word    = bitmap[i];
            if (word != 0)
            {
                idx         = (i * 64) + __builtin_ctzll(word);
                isoverfit   = true;
                break;
            }
        }
    }

    *dest   = (tsalloc_szclass_t)idx;

    return isoverfit;
}

static inline void 
scache_set_bitmap(
    scache_t           *cache,
    tsalloc_szclass_t   szclass,
    bool                val
){
    uint64_t   *bitmap;
    uint64_t   *mapword;
    uint64_t    bit_idx;
    uint64_t    word_idx;

    word_idx    = szclass / 64;
    bit_idx     = szclass % 64;
    bitmap      = ((uint64_t*)cache->bitmap);
    mapword     = &(bitmap[word_idx]);
    
    if (val)
    {
        *mapword   |= (1ULL << bit_idx);
    }
    else
    {
        *mapword   &= ~(1ULL << bit_idx);
    }
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

    ret = pagetrie_init(error_ctx, &(cache->pagetrie));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = objpool_init(
        error_ctx, 
        &(cache->spanpool), 
        0,                  // !later implement TSALLOCDEFAULTARG
        sizeof(span_t), 
        64                  // !replace later with macro dependent on pagesize? maybe remove arg
    );
    if (ret != TSALLOC_SUCCESS)
    {
        mutex_deinit(error_ctx, &(cache->lock));
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

    cache->origins  = (registry_t){0};
    cache->epoch    = 0;
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
scache_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    scache_t           *cache,
    span_t             *span
){
    span_t         *lspan;
    span_t         *rspan;
    span_t         *_lspan;
    void           *cached_addr;
    size_t          cached_nbytes;
    tsalloc_err_t   ret;

    span_get_adj(&(cache->pagetrie), span, &lspan, &rspan);
    
    if (lspan
        && (((!arena_cfg->allow_cross_origin_merge) && (lspan->flags.age == span->flags.age))
        || ((arena_cfg->allow_cross_origin_merge))))
    {
        cached_addr     = span->addr;
        cached_nbytes   = span->nbytes;

        //  lspan descriptor augmented, placed in _lspan
        ret = span_coalesce(
            error_ctx, 
            arena_cfg, 
            &(cache->spanpool), 
            lspan, 
            span, 
            &_lspan
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        //  need only overwrite only pagetrie entries for rightmost span
        ret = pagetrie_insert(
            error_ctx, 
            &(cache->pagetrie), 
            cached_addr, 
            _lspan, 
            cached_nbytes
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        span    = _lspan;
    }

    if (rspan
        && (((!arena_cfg->allow_cross_origin_merge) && (span->flags.age == rspan->flags.age))
        || ((arena_cfg->allow_cross_origin_merge))))
    {
        cached_addr     = rspan->addr;
        cached_nbytes   = rspan->nbytes;

        ret = span_coalesce(
            error_ctx, 
            arena_cfg, 
            &(cache->spanpool), 
            span, 
            rspan, 
            &_lspan
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        
        ret = pagetrie_insert(
            error_ctx, 
            &(cache->pagetrie), 
            cached_addr, 
            _lspan, 
            cached_nbytes
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        span    = _lspan;
    }

    tsalloc_szclass_t   szclass;

    szclass = ((tsalloc_szclass_t)span->flags.szclass);
    ret     = bin_put_span(error_ctx, arena_cfg, &(cache->bins[szclass]), span);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    scache_set_bitmap(cache, szclass, true); 
    
    return TSALLOC_SUCCESS;
}

tsalloc_err_t
scache_get_span(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   szclass
){
    span_t             *span;
    tsalloc_szclass_t   bin_szclass;
    tsalloc_err_t       ret;
    bool                isoverfit;

    isoverfit   = scache_find_nonempty_bin_idx(cache, &bin_szclass, szclass);
    if (bin_szclass >= cache->nclasses)
    {
        ret = span_create(
            error_ctx, 
            arena_cfg, 
            &(cache->spanpool), 
            &span,
            &(cache->epoch),
            (arena_cfg->default_new_span_szclass > szclass)? arena_cfg->default_new_span_szclass : szclass, 
            TSALLOC_DEFAULT_ARG
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        ret = pagetrie_insert(
            error_ctx, 
            &(cache->pagetrie), 
            span->addr, 
            span, 
            span->nbytes
        );
        if (ret != TSALLOC_SUCCESS)
        {
            span_destroy(
                error_ctx, 
                arena_cfg, 
                &(cache->spanpool),  
                span
            );
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        registry_push(&(cache->origins), span);
        isoverfit   = span->flags.szclass > szclass;
    }
    else 
    {
        bin_t  *bin;

        bin     = &(cache->bins[bin_szclass]);
        span    = bin_get_span(bin);
        if (bin_isempty(bin))
        {
            scache_set_bitmap(cache, bin_szclass, false); 
        }
    }

    if (isoverfit)
    {
        span_t *cut;

        ret = span_split(
            error_ctx, 
            arena_cfg, 
            &(cache->spanpool), 
            &span, 
            &cut, 
            szclass
        );
        if (ret != TSALLOC_SUCCESS){
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        ret = scache_put_span(
            error_ctx,
            arena_cfg,
            cache,
            span
        );
        if (ret != TSALLOC_SUCCESS){
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        span    = cut;
    }
    
    *dest   = span;

    return TSALLOC_SUCCESS;
}