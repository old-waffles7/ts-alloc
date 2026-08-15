#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/scache.h"

#include    "config/tsalloc_config.h"

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
    const scache_t   *cache,
    ts_szclass_t     *dest,
    ts_szclass_t      szclass
){
    uint64_t   *bitmap;
    uint64_t    idx;
    uint64_t    word;
    uint64_t    bit_idx;
    uint64_t    word_idx;
    uint64_t    max_word_idx;
    uint64_t    last_word_bits;
    bool        isoverfit;

    idx             = (-1);                
    isoverfit       = true;                                  
    bitmap          = ((uint64_t*)(cache->bitmap));
    word_idx        = szclass / 64;
    bit_idx         = szclass % 64;
    max_word_idx    = ((cache->nszclasses) + 63) / 64;

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

            if (i == (max_word_idx - 1))
            {
                last_word_bits = cache->nszclasses % 64;

                if (last_word_bits != 0)
                {
                    word &= ((1ULL << last_word_bits) - 1);
                }
            }

            if (word != 0)
            {
                idx         = (i * 64) + __builtin_ctzll(word);
                isoverfit   = true;
                break;
            }
        }
    }

    *dest   = (ts_szclass_t)idx;

    return isoverfit;
}


static inline void
scache_set_bitmap(
    scache_t       *cache,
    ts_szclass_t    szclass,
    bool            val
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


static inline void
scache_merge_and_update(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                     *lspan,
    span_t                     *rspan,
    span_t                    **dest
){
    void           *cached_addr;
    size_t          cached_nbytes;

    cached_addr     = rspan->addr;
    cached_nbytes   = rspan->nbytes;

    span_coalesce(
        glob_state, 
        arena_cfg, 
        error_ctx, 
        &(cache->spanpool), 
        &(cache->origins), 
        lspan, 
        rspan, 
        dest
    );

    // expect both spans to have been in pagetrie already, cannot fail
    (void)pagetrie_insert(
        error_ctx, 
        cache->pagetrie, 
        cached_addr, 
        *dest, 
        cached_nbytes
    );
}


static inline tsalloc_err_t
scache_mint_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                    **dest,
    ts_szclass_t                req_szclass
){
    span_t             *span;
    ts_szclass_t        szclass;
    tsalloc_err_t       ret;

    if (arena_cfg->default_new_span_szclass > req_szclass)
    {
        szclass = arena_cfg->default_new_span_szclass;
    }
    else 
    {
        szclass = req_szclass;
    }

    ret = span_create(
        glob_state, 
        arena_cfg, 
        error_ctx, 
        &(cache->spanpool), 
        &span, 
        szclass, 
        &(cache->epoch), 
        cache->arena_uid, 
        TSALLOC_DEFAULT_ARG
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = pagetrie_insert(
        error_ctx, 
        cache->pagetrie, 
        span->addr, 
        span, 
        span->nbytes
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)span_destroy(
            arena_cfg, 
            error_ctx, 
            &(cache->spanpool), 
            span
        );
        *dest   = nullptr;

        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    records_push(&(cache->origins), span);
    *dest   = span;

    return TSALLOC_SUCCESS;
}


static inline tsalloc_err_t
_scache_put_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                     *span
){
    if (span->flags.is_slab)
    {
        slab_deinit(&cache->slabpool, span);
    }

    bin_t          *bin;
    span_t         *lspan;
    span_t         *rspan;
    span_t         *mspan;
    ts_szclass_t    szclass;
    tsalloc_err_t   ret;

    span->flags.is_alloc    = false;
    span_get_adj(cache->pagetrie, span, &lspan, &rspan);

    if (span_can_merge(arena_cfg, lspan, span))
    {
        bin = &cache->bins[lspan->flags.szclass];
        bin_remove_span(bin, lspan);
        if (bin_isempty(bin))
        {
            scache_set_bitmap(
                cache, 
                (ts_szclass_t)(lspan->flags.szclass), 
                false
            );
        }

        scache_merge_and_update(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            cache, 
            lspan, 
            span, 
            &mspan
        );

        span = mspan;
    }

    if (span_can_merge(arena_cfg, span, rspan))
    {
        bin = &cache->bins[rspan->flags.szclass];
        bin_remove_span(bin, rspan);
        if (bin_isempty(bin))
        {
            scache_set_bitmap(
                cache, 
                (ts_szclass_t)(rspan->flags.szclass), 
                false
            );
        }

        scache_merge_and_update(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            cache, 
            span, 
            rspan, 
            &mspan
        );
        
        span = mspan;
    }

    szclass = (ts_szclass_t)span->flags.szclass;
    ret     = bin_put_span(
        arena_cfg, 
        error_ctx, 
        &(cache->bins[szclass]), 
        span
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)pagetrie_remove(
            cache->pagetrie,
            span->addr,
            span->nbytes
        );
        (void)span_destroy(
            arena_cfg, 
            nullptr, 
            &(cache->spanpool), 
            span
        );
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    scache_set_bitmap(cache, szclass, true); 

    return TSALLOC_SUCCESS;
}


tsalloc_err_t
scache_init(
    const glob_alloc_state_t   *global_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    pagetrie_t                 *pagetrie,
    byte_t                     *auxil_mem,
    scache_t                   *cache,
    uint16_t                    arena_uid
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

    byte_t         *bitmap_addr;
    size_t          nbytes_bitmap;
    ts_szclass_t    nszclasses;

    nszclasses      = global_state->nszclasses_span;
    bitmap_addr     = (byte_t*)ALIGN_UP(
        (uintptr_t)(auxil_mem + (sizeof(bin_t) * nszclasses)),
        8
    );
    nbytes_bitmap   = ((nszclasses + 63) / 64) * 8;

    *cache  = (scache_t){
        .bins       = (bin_t*)auxil_mem,
        .bitmap     = bitmap_addr,
        .pagetrie   = pagetrie,
        .nszclasses = nszclasses,
        .arena_uid  = arena_uid
    };

    tsalloc_err_t   ret;

    ret = mutex_init(error_ctx, &(cache->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = objpool_init(
        error_ctx, 
        &(cache->spanpool), 
        0,                  
        (arena_cfg->unmap_on_termination)
            ? (sizeof(span_t) + sizeof(record_t))
            : sizeof(span_t),
        64                  
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)mutex_deinit(nullptr, &(cache->lock));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = objpool_init(
        error_ctx, 
        &(cache->slabpool), 
        0,                  
        sizeof(slab_t) + (global_state->nbytes_bitmap),
        64                  
    );
    if (ret != TSALLOC_SUCCESS)
    {
        objpool_deinit(&(cache->spanpool));
        (void)mutex_deinit(nullptr, &(cache->lock));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    for (int i = 0; i < nszclasses; i++)
    {
        bin_init(&(cache->bins[i]), i);
    }

    memset(cache->bitmap, 0, nbytes_bitmap);

    return TSALLOC_SUCCESS;
}


tsalloc_err_t
scache_deinit(
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
){
    objpool_deinit(&(cache->spanpool));
    objpool_deinit(&(cache->slabpool));

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
scache_destroy(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
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

    ret     = TSALLOC_SUCCESS;
    unmap   = arena_cfg->auxil_unmap;

    while (!records_isempty(&(cache->origins)))
    {
        span    = records_pop(&(cache->origins));
        nbytes  = span->record->nbytes;

        if (unmap(arena_cfg->extra, ((void*)span->addr), nbytes) != 0)
        {
            set_tsalloc_error(
                error_ctx,
                "scache_destroy::scache.c unmap failure",
                TSALLOC_AUXIL_UNMAP_ERR
            );
            ret = TSALLOC_AUXIL_UNMAP_ERR;
        }
    }

    objpool_deinit(&(cache->spanpool));
    objpool_deinit(&(cache->slabpool));

    return ret;
}


tsalloc_err_t
scache_put_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                     *span
){
    tsalloc_err_t ret;

    ret = TSALLOC_SUCCESS;

    mutex_lock(&(cache->lock));

    ret = _scache_put_span(
        glob_state, 
        arena_cfg, 
        error_ctx, 
        cache, 
        span
    );

    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
    }

    mutex_unlock(&(cache->lock));

    return ret;
}


tsalloc_err_t
scache_get_span(
    const tsalloc_slab_info_t  *slab_init_info,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                    **dest,
    ts_szclass_t                szclass
){
    span_t             *span;
    ts_szclass_t        bin_idx;
    tsalloc_err_t       ret;
    bool                isoverfit;

    mutex_lock(&(cache->lock));

    isoverfit = scache_find_nonempty_bin_idx(cache, &bin_idx, szclass);
    if (bin_idx < 0)
    {
        ret = scache_mint_span(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            cache, 
            &span, 
            szclass
        );
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

        bin     = &(cache->bins[bin_idx]);
        span    = bin_get_span(bin);

        if (bin_isempty(bin))
        {
            scache_set_bitmap(cache, bin_idx, false); 
        }
    }

    if (isoverfit)
    {
        span_t *cut;

        ret = span_split(
            glob_state, 
            error_ctx, 
            &(cache->spanpool), 
            &span, 
            &cut, 
            szclass
        );
        if (ret != TSALLOC_SUCCESS)
        {
            (void)_scache_put_span(
                glob_state,
                arena_cfg,
                nullptr,
                cache,
                span
            );
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        ret = pagetrie_insert(
            error_ctx,
            cache->pagetrie,
            cut->addr,
            cut,
            cut->nbytes
        );
        if (ret != TSALLOC_SUCCESS)
        {
            (void)pagetrie_remove(
                cache->pagetrie,
                span->addr,
                span->nbytes
            );
            (void)pagetrie_remove(
                cache->pagetrie,
                cut->addr,
                cut->nbytes
            );
            (void)span_destroy(
                arena_cfg, 
                nullptr, 
                &(cache->spanpool), 
                span
            );
            (void)span_destroy(
                arena_cfg, 
                nullptr, 
                &(cache->spanpool), 
                cut
            );
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        ret = _scache_put_span(
            glob_state,
            arena_cfg,
            error_ctx,
            cache,
            span
        ); 
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(error_ctx);
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
            slab_deinit(
                &(cache->slabpool),
                span
            );
            (void)pagetrie_remove(
                cache->pagetrie,
                span->addr,
                span->nbytes
            );
            (void)span_destroy(
                arena_cfg, 
                nullptr, 
                &(cache->spanpool), 
                span
            );
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    span->flags.is_alloc    = true;

    mutex_unlock(&(cache->lock));

    *dest   = span;

    return TSALLOC_SUCCESS;
}


tsalloc_err_t
scache_decay(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    scache_t           *cache
){
    tsalloc_err_t   ret1, ret2;
    ts_szclass_t    nszclasses;

    ret2        = TSALLOC_SUCCESS;
    nszclasses  = cache->nszclasses;

    mutex_lock(&(cache->lock));

    for (int i = 0; i < nszclasses; i++)
    {
        ret1    = bin_decay(arena_cfg, error_ctx, cache->bins + i);
        if (ret1 != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            ret2    = ret1;
        }
    }

    mutex_unlock(&(cache->lock));

    return ret2;
}