#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/scache.h"

#include    "internal/bin.h"
#include    "internal/span.h"
#include    "internal/slab.h"
#include    "internal/mutex.h"
#include    "internal/records.h"
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

static inline tsalloc_err_t
scache_merge_and_update(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t             *lspan,
    span_t             *rspan,
    span_t            **dest
){
    void           *cached_addr;
    size_t          cached_nbytes;
    tsalloc_err_t   ret;

    cached_addr     = rspan->addr;
    cached_nbytes   = rspan->nbytes;
    ret = span_coalesce(
        error_ctx, 
        arena_cfg, 
        &(cache->spanpool),
        &(cache->origins),
        lspan, 
        rspan, 
        dest
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
        *dest, 
        cached_nbytes
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline bool
scache_can_merge(
    arena_cfg_t *arena_cfg,
    span_t       *lspan,
    span_t       *rspan
){
    if ((!lspan) || (!rspan))
    {
        return false;
    }
    if ((!lspan->flags.is_alloc) || ((!rspan->flags.is_alloc)))
    {
        return false;
    }
    return arena_cfg->allow_cross_origin_merge || (lspan->flags.age == rspan->flags.age);
}

static inline tsalloc_err_t
scache_mint_span(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   req_szclass
){
    span_t             *span;
    tsalloc_err_t       ret;
    tsalloc_szclass_t   szclass;

    szclass = (arena_cfg->default_new_span_szclass > req_szclass) ? arena_cfg->default_new_span_szclass : req_szclass;

    ret = span_create(
        error_ctx, 
        arena_cfg, 
        &(cache->spanpool), 
        &span,
        &(cache->epoch),
        szclass, 
        TSALLOC_DEFAULT_ARG,
        true
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = pagetrie_insert(error_ctx, &(cache->pagetrie), span->addr, span, span->nbytes);
    if (ret != TSALLOC_SUCCESS)
    {
        span_destroy(error_ctx, arena_cfg, &(cache->spanpool), span);
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    records_push(&(cache->origins), span);

    *dest   = span;

    return TSALLOC_SUCCESS;
}


tsalloc_err_t
scache_init(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
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
        0,                  
        (arena_cfg->unmap_on_termination)? (sizeof(span_t) + sizeof(record_t)):sizeof(span_t),
        64                  
    );
    if (ret != TSALLOC_SUCCESS)
    {
        mutex_deinit(error_ctx, &(cache->lock));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = objpool_init(
        error_ctx, 
        &(cache->slabpool), 
        0,                  
        sizeof(slab_t) + (arena_cfg->tsalloc_cfg->nbytes_bitmap),
        64                  
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
    bitmap_addr     = auxil_mem + (sizeof(bin_t) * nclasses);
    bitmap_bytes    = ((nclasses + 63) / 64) * 8;

    cache->bins     = (bin_t*)auxil_mem;
    cache->origins  = (records_t){0};
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
    objpool_deinit(&(cache->spanpool));
    objpool_deinit(&(cache->slabpool));

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
scache_destroy(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache
){
    if (!arena_cfg->unmap_on_termination)
    {
        set_tsalloc_error(
            error_ctx,
            "scache_destroy::scache.c spans missing record instances",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    tsalloc_err_t   ret;

    ret = mutex_deinit(error_ctx, &(cache->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    auxil_unmap_fn  unmap;
    span_t         *span;
    size_t          nbytes;
    int             nfail_unmap;
    
    unmap       = arena_cfg->auxil_unmap;
    nfail_unmap = 0;
    while (!records_isempty(&(cache->origins)))
    {
        span    = records_pop(&(cache->origins));
        nbytes  = span->record->nbytes;
        if (unmap(arena_cfg->extra, ((void*)span->addr), nbytes))
        {
            set_tsalloc_error(
                error_ctx,
                "scache_destroy::scache.c unmap failure",
                TSALLOC_INVALID_ARGS
            );
            nfail_unmap++;
        }
    }
    objpool_deinit(&(cache->spanpool));
    objpool_deinit(&(cache->slabpool));

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
_scache_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t             *span,
    bool                isslab
){
    if (isslab)
    {
        slab_deinit(&cache->slabpool, span);
    }

    span_t             *lspan;
    span_t             *rspan;
    span_t             *_lspan;
    tsalloc_err_t       ret;
    tsalloc_szclass_t   szclass;

    span->flags.is_alloc    = false;
    span_get_adj(&(cache->pagetrie), span, &lspan, &rspan);

    if (scache_can_merge(arena_cfg, span, lspan))
    {
        ret = scache_merge_and_update(
            error_ctx, arena_cfg, cache, 
            lspan, span, 
            &_lspan
        );
        if (ret != TSALLOC_SUCCESS)
        {
            return ret;
        }
        span = _lspan;
    }

    if (scache_can_merge(arena_cfg, span, rspan))
    {
        ret = scache_merge_and_update(
            error_ctx, arena_cfg, cache, 
            span, rspan, 
            &_lspan
        );
        if (ret != TSALLOC_SUCCESS)
        {
            return ret;
        }
        span = _lspan;
    }

    szclass = (tsalloc_szclass_t)span->flags.szclass;
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
scache_put_span(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t             *span,
    bool                isslab
){
    tsalloc_err_t ret;
    
    mutex_lock(&(cache->lock));
    ret = _scache_put_span(error_ctx, arena_cfg, cache, span, isslab);
    mutex_unlock(&(cache->lock));
    
    return ret;
}

tsalloc_err_t
scache_get_span(
    const tsalloc_slab_info_t  *slab_init_info,
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   szclass
){
    span_t             *span;
    tsalloc_szclass_t   bin_szclass;
    tsalloc_err_t       ret;
    bool                isoverfit;

    mutex_lock(&(cache->lock));

    isoverfit = scache_find_nonempty_bin_idx(cache, &bin_szclass, szclass);
    if (bin_szclass >= cache->nclasses)
    {
        ret = scache_mint_span(error_ctx, arena_cfg, cache, &span, szclass);
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&(cache->lock));
            return ret;
        }
        isoverfit = span->flags.szclass > szclass;
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

        ret = span_split(error_ctx, arena_cfg, &(cache->spanpool), &span, &cut, szclass);
        if (ret != TSALLOC_SUCCESS)
        {
            (void)_scache_put_span(error_ctx, arena_cfg, cache, span, false);
            append_tsalloc_error_trace(error_ctx);
            mutex_unlock(&(cache->lock));
            return ret;
        }

        ret = pagetrie_insert(error_ctx, &(cache->pagetrie), cut->addr, cut, cut->nbytes);
        if (ret != TSALLOC_SUCCESS)
        {
            (void)_scache_put_span(error_ctx, arena_cfg, cache, span, false);
            (void)_scache_put_span(error_ctx, arena_cfg, cache, cut, false);
            append_tsalloc_error_trace(error_ctx);
            mutex_unlock(&(cache->lock));
            return ret;
        }

        ret = _scache_put_span(error_ctx, arena_cfg, cache, span, false); 
        if (ret != TSALLOC_SUCCESS)
        {
            span_t *_span;
            if (span_coalesce(error_ctx, arena_cfg, &(cache->spanpool), &(cache->origins), span, cut, &_span) == TSALLOC_SUCCESS) 
            {
                (void)_scache_put_span(error_ctx, arena_cfg, cache, _span, false);
            }
            append_tsalloc_error_trace(error_ctx);
            mutex_unlock(&(cache->lock));
            return ret;
        }
        span = cut;
    }

    if (slab_init_info)
    {
        ret = slab_init(
            slab_init_info, 
            error_ctx, 
            &(cache->slabpool), 
            span
        );
        if (ret != TSALLOC_SUCCESS)
        {
            (void)_scache_put_span(error_ctx, arena_cfg, cache, span, false);
            append_tsalloc_error_trace(error_ctx);
            mutex_unlock(&(cache->lock));
            return ret;
        }
    }

    mutex_unlock(&(cache->lock));

    span->flags.is_alloc    = true;
    *dest                   = span;

    return TSALLOC_SUCCESS;
}