
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
    ts_szclass_t     *dest,
    ts_szclass_t      szclass
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

static inline ts_err_t 
scache_bin_remove(
    const arena_cfg_t  *arena_cfg,
    scache_t           *cache,
    span_t             *span,
    int32_t             glob_uid
){
    bin_t      *bin;
    ts_err_t    ret;

    bin = cache->bins + span->flags.szclass;
    ret = bin_remove_span(
        arena_cfg, 
        bin,
        span,
        glob_uid
    );  
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    if (bin_isempty(bin))
    {
        scache_set_bitmap(
            cache, 
            (ts_szclass_t)(span->flags.szclass), 
            false
        );
    }

    return TSALLOC_SUCCESS;
}

static inline void
scache_bin_put(
    const arena_cfg_t  *arena_cfg,
    scache_t           *cache,
    span_t             *span
){
    bin_t  *bin;

    bin = cache->bins + ((ts_szclass_t)span->flags.szclass);
    //  cannot fail, only chance to fail if mutates span state to retained which is not the case
    (void)bin_put_span(arena_cfg, bin, span, TSALLOC_NO_ERROR_CONTEXT);
    scache_set_bitmap(
        cache, 
        (ts_szclass_t)(span->flags.szclass), 
        true
    );
}

static inline ts_err_t
scache_bin_pop(
    const arena_cfg_t  *arena_cfg,
    scache_t           *cache,
    span_t            **dest,
    ts_szclass_t        bin_idx,
    int32_t             glob_uid
){
    bin_t      *bin;
    span_t     *span;
    ts_err_t    ret;

    bin = cache->bins + bin_idx;
    ret = bin_get_span(
        arena_cfg, 
        &span, 
        bin,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    if (bin_isempty(bin))
    {
        scache_set_bitmap(
            cache, 
            bin_idx, 
            false
        );
    }
    //  if a span has made it to a bin, it must already be in pagetrie
    
    return TSALLOC_SUCCESS;
}

static inline ts_err_t
scache_mint_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                    **dest,
    bool                       *isoverfit,
    ts_szclass_t                req_szclass,
    int32_t                     glob_uid
){
    span_t         *span;
    ts_szclass_t    szclass;
    ts_err_t        ret;

    if (arena_cfg->default_new_span_szclass > req_szclass)
    {
        szclass     = arena_cfg->default_new_span_szclass;
        if (isoverfit != nullptr)
        {
            *isoverfit  = true;
        }
    }
    else 
    {
        szclass = req_szclass;
        if (isoverfit != nullptr)
        {
            *isoverfit  = false;
        }
    }

    ret = span_create(
        glob_state, 
        arena_cfg, 
        &(cache->spanpool), 
        &span, 
        szclass, 
        &(cache->epoch), 
        cache->arena_uid, 
        TSALLOC_DEFAULT_ARG,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        *dest   = nullptr;
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    ret = pagetrie_insert(
        cache->pagetrie, 
        span->addr, 
        span, 
        span->nbytes,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)span_destroy(
            arena_cfg, 
            &(cache->spanpool), 
            span,
            TSALLOC_NO_ERROR_CONTEXT
        );
        *dest   = nullptr;
        append_tsalloc_error_trace(glob_uid);
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
static inline ts_err_t
scache_merge_adj(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                     *span,
    span_t                    **dest,
    int32_t                     glob_uid
){
    span_t     *lspan;
    span_t     *rspan;
    span_t     *mspan;
    bool        lmerge, rmerge;
    ts_err_t    ret;

    span_get_adj(cache->pagetrie, span, &lspan, &rspan);
    lmerge  = span_can_merge(arena_cfg, lspan, span);
    rmerge  = span_can_merge(arena_cfg, span, rspan);

    //  remove spans to be merged from bins
    if (lmerge)
    {
        ret = scache_bin_remove(
            arena_cfg, 
            cache, 
            lspan,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }
    }
    if (rmerge)
    {
        ret = scache_bin_remove(
            arena_cfg, 
            cache, 
            rspan,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }
    }

    //  merge spans
    if (lmerge)
    {
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
    if (rmerge)
    {
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
        cache->pagetrie, 
        span->addr, 
        ((void*)span), 
        span->nbytes,
        glob_uid
    );

    *dest   = span;

    return TSALLOC_SUCCESS;
}

//  span must be in pagetrie and not in a bin, szclass must be valid
static inline ts_err_t
scache_cutout_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                     *origin,
    span_t                    **dest,
    ts_szclass_t                szclass,
    int32_t                     glob_uid
){
    span_t     *cut;
    ts_err_t    ret;

    //  fails only if objpool cant alloc new span descriptor, assume szclass is valid
    ret = span_split(
        glob_state, 
        &(cache->spanpool), 
        &origin, 
        &cut, 
        szclass,
        glob_uid
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    if (origin)
    {
        scache_bin_put(arena_cfg, cache, origin);
    }

    //  cannot fail, path in pagetrie already exists since uncut origin was there, no need for objpool to alloc
    (void)pagetrie_insert(
        cache->pagetrie, 
        (void*)cut->addr, 
        (void*)cut, 
        cut->nbytes,
        glob_uid
    );

    *dest   = cut;

    return TSALLOC_SUCCESS;
}


ts_err_t
scache_init(
    const glob_alloc_state_t   *global_state,
    const arena_cfg_t          *arena_cfg,
    pagetrie_t                 *pagetrie,
    byte_t                     *auxil_mem,
    scache_t                   *cache,
    uint16_t                    arena_uid,
    int32_t                     glob_uid
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            glob_uid,
            "scache_init::scache.c nullptr auxil_mem argument",
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

    ts_err_t    ret;

    ret = mutex_init(&(cache->lock), glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    ret = objpool_init(
        &(cache->spanpool), 
        0,                  
        (arena_cfg->unmap_on_termination)
            ? (sizeof(span_t) + sizeof(record_t))
            : sizeof(span_t),
        64,
        glob_uid                  
    );
    if (ret != TSALLOC_SUCCESS)
    {
        (void)mutex_deinit(&(cache->lock), glob_uid);
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    ret = objpool_init(
        &(cache->slabpool), 
        0,                  
        sizeof(slab_t) + (global_state->nbytes_bitmap),
        64,
        glob_uid                  
    );
    if (ret != TSALLOC_SUCCESS)
    {
        objpool_deinit(&(cache->spanpool));
        (void)mutex_deinit(&(cache->lock), glob_uid);
        append_tsalloc_error_trace(glob_uid);
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
ts_err_t
scache_deinit(
    scache_t   *cache,
    int32_t     glob_uid
){
    objpool_deinit(&(cache->spanpool));
    objpool_deinit(&(cache->slabpool));

    ts_err_t    ret;

    ret = mutex_deinit(&(cache->lock), glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

//  explicity unmaps all allocated memory (e.g vram arena) and deinits
ts_err_t
scache_destroy(
    const arena_cfg_t  *arena_cfg,
    scache_t           *cache,
    int32_t             glob_uid
){
    if (!arena_cfg->unmap_on_termination)
    {
        set_tsalloc_error(
            glob_uid,
            "scache_destroy::scache.c spans missing record instances",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    auxil_unmap_fn  unmap;
    span_t         *span;
    size_t          nbytes;
    ts_err_t        ret;

    ret     = TSALLOC_SUCCESS;
    unmap   = arena_cfg->auxil_unmap;

    while (!records_isempty(&(cache->origins)))
    {
        span    = records_pop(&(cache->origins));
        nbytes  = span->record->nbytes;

        if (unmap(arena_cfg->extra, ((void*)span->addr), nbytes) != 0)
        {
            set_tsalloc_error(
                glob_uid,
                "scache_destroy::scache.c unmap failure",
                TSALLOC_AUXIL_UNMAP_ERR
            );
            ret = TSALLOC_AUXIL_UNMAP_ERR;
        }
    }

    objpool_deinit(&(cache->spanpool));
    objpool_deinit(&(cache->slabpool));

    ret = mutex_deinit(&(cache->lock), glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    return ret;
}

//  warning span must already be in pagetrie
ts_err_t
scache_put_span(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                     *span,
    int32_t                     glob_uid
){
    span_t     *_span;
    ts_err_t    ret;
    
    if (span->flags.is_slab)
    {
        slab_deinit(&(cache->slabpool), span);
    }

    mutex_lock(&(cache->lock));

        span->flags.is_alloc    = false;
        ret = scache_merge_adj(
            glob_state, 
            arena_cfg, 
            cache, 
            span, 
            &_span,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }
        _span->flags.is_alloc   = false;
        scache_bin_put(arena_cfg, cache, _span);

    mutex_unlock(&(cache->lock));

    return TSALLOC_SUCCESS;
}

ts_err_t
scache_get_span(
    const tsalloc_slab_info_t  *slab_init_info,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                    **dest,
    ts_szclass_t                szclass,
    int32_t                     glob_uid
){
    if ((szclass < 0) || (szclass >= cache->nszclasses))
    {
        set_tsalloc_error(
            glob_uid,
            "scache_get_span::scache.c invalid size-class",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    span_t         *span;
    bool            isoverfit;
    ts_szclass_t    bin_idx;
    ts_err_t        ret;

    mutex_lock(&(cache->lock));

        isoverfit   = scache_find_nonempty_bin(cache, &bin_idx, szclass);
        if (bin_idx < 0)
        {
            ret = scache_mint_span(
                glob_state, 
                arena_cfg, 
                cache, 
                &span, 
                &isoverfit, 
                szclass,
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS)
            {
                mutex_unlock(&(cache->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }
        }
        else 
        {
            ret = scache_bin_pop(
                arena_cfg,
                cache,
                &span,
                bin_idx,
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS)
            {
                mutex_unlock(&(cache->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }
        }

        if (isoverfit)
        {
            span_t *cut;

            ret = scache_cutout_span(
                glob_state, 
                arena_cfg, 
                cache, 
                span, 
                &cut, 
                szclass,
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS)
            {
                //  only possible failure is that spanpool is out of memory; span, pagetrie are unmutated
                scache_bin_put(arena_cfg, cache, span);
                mutex_unlock(&(cache->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }

            span    = cut;
        }

        if (slab_init_info)
        {
            ret = slab_init(
                slab_init_info, 
                &(cache->slabpool), 
                span,
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS)
            {
                //  only possible failure is that slabpool is out of memory; span, pagetrie are unmutated
                scache_bin_put(arena_cfg, cache, span);
                mutex_unlock(&(cache->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }
        }
        span->flags.is_alloc    = true;

    mutex_unlock(&(cache->lock));

    *dest   = span;

    return TSALLOC_SUCCESS;
}

//  align must be > pgsize 
ts_err_t
scache_get_span_aligned(
    const tsalloc_slab_info_t  *slab_init_info,
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    scache_t                   *cache,
    span_t                    **dest,
    size_t                      align,
    ts_szclass_t                szclass,
    int32_t                     glob_uid
){
    size_t          req_nbytes;
    ts_szclass_t    req_szclass;

    req_nbytes  = align + tsconfig_get_nbytes_szclass(glob_state, szclass, false);
    req_szclass = tsconfig_get_szclass(glob_state, req_nbytes).szclass;
    if ((req_szclass < 0) || (req_szclass >= cache->nszclasses))
    {
        set_tsalloc_error(
            glob_uid,
            "scache_get_span::scache.c invalid size-class",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    span_t         *origin;
    ts_szclass_t    bin_idx;
    ts_err_t        ret;

    mutex_lock(&(cache->lock));

        //  ignore overfit flag as split will be performed regardless
        (void)scache_find_nonempty_bin(cache, &bin_idx, req_szclass);
        if (bin_idx < 0)
        {
            ret = scache_mint_span(
                glob_state, 
                arena_cfg, 
                cache, 
                &origin, 
                nullptr, 
                req_szclass,
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS)
            {
                mutex_unlock(&(cache->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }
        }
        else 
        {
            ret = scache_bin_pop(
                arena_cfg,
                cache,
                &origin,
                bin_idx,
                glob_uid
            );
            if (ret != TSALLOC_SUCCESS)
            {
                mutex_unlock(&(cache->lock));
                append_tsalloc_error_trace(glob_uid);
                return ret;
            }
        }

        span_t *lcut;
        span_t *rcut;
        span_t *span;

        ret = span_split_aligned(
            glob_state, 
            &(cache->spanpool), 
            origin, 
            &span, 
            &lcut, 
            &rcut, 
            align, 
            szclass,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&(cache->lock));
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }

        //  cannot fail as paths in pagetrie already exist, objpool will not need to alloc
        (void)pagetrie_insert(
            cache->pagetrie, 
            ((void*)span->addr), 
            span, 
            span->nbytes,
            glob_uid
        );
        (void)pagetrie_insert(
            cache->pagetrie, 
            ((void*)rcut->addr), 
            rcut, 
            rcut->nbytes,
            glob_uid
        );
        scache_bin_put(arena_cfg, cache, rcut);
        if (lcut)
        {
            (void)pagetrie_insert(
                cache->pagetrie, 
                ((void*)lcut->addr), 
                lcut, 
                lcut->nbytes,
                glob_uid
            );
            scache_bin_put(arena_cfg, cache, lcut);
        }

        span->flags.is_alloc    = true;

    mutex_unlock(&(cache->lock));

    *dest   = span;

    return TSALLOC_SUCCESS;
}

ts_err_t
scache_decay(
    const arena_cfg_t  *arena_cfg,
    scache_t           *cache,
    int32_t             glob_uid
){
    ts_err_t        ret1;
    ts_err_t        ret2;
    ts_szclass_t    nszclasses;

    ret2       = TSALLOC_SUCCESS;
    nszclasses = cache->nszclasses;

    mutex_lock(&(cache->lock));

        for (ts_szclass_t i = 0; i < nszclasses; i++)
        {
            ret1 = bin_decay(
                arena_cfg,
                &(cache->bins[i]),
                glob_uid
            );

            if (ret1 != TSALLOC_SUCCESS)
            {
                append_tsalloc_error_trace(glob_uid);
                ret2 = ret1;
            }
        }

    mutex_unlock(&(cache->lock));

    return ret2;
}