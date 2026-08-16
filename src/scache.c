
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

#include    <string.h>


static inline void
scache_set_bitmap(
    scache_t       *cache,
    ts_szclass_t    szclass,
    bool            val
){
    uint64_t    *bitmap;
    uint64_t    *mapword;
    size_t       word_idx;
    uint32_t     bit_idx;

    bitmap      = (uint64_t*)(cache->bitmap);
    word_idx    = ((size_t)szclass) / 64;
    bit_idx     = ((uint32_t)szclass) % 64;
    mapword     = &(bitmap[word_idx]);

    if (val)
    {
        *mapword |= (1ULL << bit_idx);
    }
    else
    {
        *mapword &= ~(1ULL << bit_idx);
    }
}

static inline bool
scache_find_nonempty_bin(
    const scache_t   *cache,
    ts_szclass_t      *dest,
    ts_szclass_t       szclass
){
    uint64_t        *bitmap;
    uint64_t         word;
    uint64_t         word_idx;
    uint64_t         bit_idx;
    uint64_t         max_word_idx;
    ts_szclass_t     idx;

    bitmap          = (uint64_t*)(cache->bitmap);
    word_idx        = szclass / 64;
    bit_idx         = szclass % 64;
    max_word_idx    = (cache->nszclasses + 63) / 64;

    word = bitmap[word_idx] & (~0ULL << bit_idx);

    if (word != 0)
    {
        idx         = (ts_szclass_t)((word_idx * 64) + __builtin_ctzll(word));
        *dest       = idx;
        return idx > szclass;
    }

    for (uint64_t i = word_idx + 1; i < max_word_idx; i++)
    {
        word = bitmap[i];

        if (word != 0)
        {
            idx         = (ts_szclass_t)((i * 64) + __builtin_ctzll(word));
            *dest       = idx;
            return true;
        }
    }

    *dest = (ts_szclass_t)-1;

    return false;
}

static inline void 
scache_bin_remove(
    scache_t   *cache,
    span_t     *span
){
    bin_t  *bin;

    bin = cache->bins + span->flags.szclass;
    bin_remove_span(bin, span);
    if (bin_isempty(bin))
    {
        scache_set_bitmap(
            cache, 
            (ts_szclass_t)(span->flags.szclass), 
            false
        );
    }
}

static inline void
scache_bin_put(
    const arena_cfg_t  *arena_cfg,
    scache_t           *cache,
    span_t             *span
){
    bin_t  *bin;

    bin = cache->bins + ((ts_szclass_t)span->flags.szclass);
    //  cannot fail, only chance to file if mutates span state to retained which is not the case
    (void)bin_put_span(arena_cfg, nullptr, bin, span);
    scache_set_bitmap(
        cache, 
        (ts_szclass_t)(span->flags.szclass), 
        true
    );
}

static inline span_t*
scache_bin_pop(
    scache_t       *cache,
    ts_szclass_t    bin_idx
){
    bin_t  *bin;
    span_t *span;

    bin     = cache->bins + bin_idx;
    span    = bin_get_span(bin);
    if (bin_isempty(bin))
    {
        scache_set_bitmap(
            cache, 
            bin_idx, 
            false
        );
    }
    //  if a span has made it to a bin, it must already be in pagetrie
    
    return span;
}

static inline tsalloc_err_t
scache_mint_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                    **dest,
    bool                       *isoverfit,
    ts_szclass_t                req_szclass
){
    span_t         *span;
    ts_szclass_t    szclass;
    tsalloc_err_t   ret;

    if (arena_cfg->default_new_span_szclass > req_szclass)
    {
        szclass     = arena_cfg->default_new_span_szclass;
        *isoverfit  = true;
    }
    else 
    {
        szclass = req_szclass;
        *isoverfit  = false;
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
        *dest   = nullptr;
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
            nullptr, 
            &(cache->spanpool), 
            span
        );
        *dest   = nullptr;
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    if (arena_cfg->unmap_on_termination)
    {
        records_push(&(cache->origins), span);
    }
    *dest   = span;

    return TSALLOC_SUCCESS;
}

//  span must be in pagetrie
static inline void
scache_merge_adj(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                     *span,
    span_t                    **dest
){
    span_t *lspan;
    span_t *rspan;
    span_t *mspan;

    span_get_adj(cache->pagetrie, span, &lspan, &rspan);

    if (span_can_merge(arena_cfg, lspan, span))
    {
        scache_bin_remove(cache, lspan);
        span_coalesce(
            glob_state, 
            arena_cfg, 
            &(cache->spanpool), 
            &(cache->origins), 
            lspan, 
            span, 
            &mspan
        );
        span    = mspan;
    }
    if (span_can_merge(arena_cfg, span, rspan))
    {
        scache_bin_remove(cache, rspan);
        span_coalesce(
            glob_state, 
            arena_cfg, 
            &(cache->spanpool), 
            &(cache->origins), 
            span, 
            rspan, 
            &mspan
        );
        span    = mspan;
    }

    //  cannot fail as paths in pagetrie already exist, objpool will not need to alloc
    (void)pagetrie_insert(
        nullptr, 
        cache->pagetrie, 
        span->addr, 
        ((void*)span), 
        span->nbytes
    );

    *dest   = span;
}

//  span must be in pagetrie and not in a bin, szclass must be valid
static inline tsalloc_err_t
scache_cutout_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    scache_t                   *cache,
    span_t                     *origin,
    span_t                    **dest,
    ts_szclass_t                szclass
){
    span_t         *cut;
    tsalloc_err_t   ret;

    //  fails only if objpool cant alloc new span descriptor, assume szclass is valid
    ret = span_split(
        glob_state, 
        error_ctx, 
        &(cache->spanpool), 
        &origin, 
        &cut, 
        szclass
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    if (origin)
    {
        scache_bin_put(arena_cfg, cache, origin);
    }

    //  cannot fail, path in pagetrie already exists since uncut origin was there, no need for objpool to alloc
    (void)pagetrie_insert(
        nullptr, 
        cache->pagetrie, 
        (void*)cut->addr, 
        (void*)cut, 
        cut->nbytes
    );

    *dest   = cut;

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

    (void)memset(cache->bitmap, 0, nbytes_bitmap);

    return TSALLOC_SUCCESS;
}

//  no explicit unmap leave cleanup to os (e.g just ram arena) and deinits
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

//  explicity unmaps all allocated memory (e.g vram arena) and deinits
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

    auxil_unmap_fn  unmap;
    span_t         *span;
    size_t          nbytes;
    tsalloc_err_t   ret;

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

    ret = mutex_deinit(error_ctx, &(cache->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return ret;
}

//  warning span must already be in pagetrie
void
scache_put_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                     *span
){
    span_t         *_span;
    
    if(span->flags.is_slab)
    {
        slab_deinit(&(cache->slabpool), span);
    }

    mutex_lock(&(cache->lock));

        scache_merge_adj(
            glob_state,
            arena_cfg,
            cache,
            span,
            &_span
        );
        //  cannot fail as function will only try to set span state to dirty, no error path
        (void)bin_put_span(
            arena_cfg, 
            nullptr, 
            cache->bins + ((ts_szclass_t)span->flags.szclass), 
            _span
        );
        _span->flags.is_alloc   = false;

    mutex_unlock(&(cache->lock));
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
    if ((szclass < 0) || (szclass >= cache->nszclasses))
    {
        set_tsalloc_error(
            error_ctx,
            "scache_get_span::scache.c invalid size-class",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    span_t         *span;
    bool            isoverfit;
    ts_szclass_t    bin_idx;
    tsalloc_err_t   ret;

    mutex_lock(&(cache->lock));

    isoverfit   = scache_find_nonempty_bin(cache, &bin_idx, szclass);
    if (bin_idx < 0)
    {
        ret = scache_mint_span(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            cache, 
            &span, 
            &isoverfit, 
            szclass
        );
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }
    else 
    {
        span    = scache_bin_pop(cache, bin_idx);
    }

    if (isoverfit)
    {
        span_t *cut;

        ret = scache_cutout_span(
            glob_state, 
            arena_cfg, 
            error_ctx, 
            cache, 
            span, 
            &cut, 
            szclass
        );
        if (ret != TSALLOC_SUCCESS)
        {
            //  only possible failure is that spanpool is out of memory; span, pagetrie are unmutated
            scache_bin_put(arena_cfg, cache, span);
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        span    = cut;
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
            //  only possible failure is that slabpool is out of memory; span, pagetrie are unmutated
            scache_bin_put(arena_cfg, cache, span);
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
    tsalloc_err_t   ret1;
    tsalloc_err_t   ret2;
    ts_szclass_t    nszclasses;

    ret2       = TSALLOC_SUCCESS;
    nszclasses = cache->nszclasses;

    mutex_lock(&(cache->lock));

        for (ts_szclass_t i = 0; i < nszclasses; i++)
        {
            ret1 = bin_decay(
                arena_cfg,
                error_ctx,
                &(cache->bins[i])
            );

            if (ret1 != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(error_ctx);
                ret2 = ret1;
            }
        }

    mutex_unlock(&(cache->lock));

    return ret2;
}