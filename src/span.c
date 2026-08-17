
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/span.h"

#include    "config/tsalloc_config.h"

#include    "internal/records.h"
#include    "internal/objpool.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"

#include    <string.h>


tsalloc_err_t
span_create(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t           *error_ctx,
    objpool_t                  *spanpool,
    span_t                    **dest,
    ts_szclass_t                szclass,
    uint32_t                   *epoch,
    uint16_t                    arena_uid,
    size_t                      _align
){
    span_t         *span;
    tsalloc_err_t   ret;

    ret = objpool_alloc(error_ctx, spanpool, ((void*)(&span)));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    void   *mem;
    size_t  nbytes;

    nbytes  = tsconfig_get_nbytes_szclass(glob_state, szclass, false);
    mem     = arena_cfg->auxil_map(
        arena_cfg->extra, 
        (_align != TSALLOC_DEFAULT_ARG)? _align : arena_cfg->pagesize, 
        nbytes
    );
    if (!mem)
    {
        set_tsalloc_error
        (
            error_ctx,
            "span_create::span.c auxilliary mapper could not allocate memory",
            TSALLOC_AUXIL_MAP_ERR
        );
        objpool_free(spanpool, ((void*)span));
        return TSALLOC_AUXIL_MAP_ERR;
    }

    record_t   *record;

    if (arena_cfg->unmap_on_termination)
    {
        record  = (record_t*)(((byte_t*)span) + sizeof(span_t));
        record_init(record, nbytes);
    }
    else 
    {
        record  = nullptr;
    }

    *span   = (span_t){
        .flags.age          = *epoch,
        .flags.szclass      = szclass,
        .flags.arena_uid    = arena_uid,
        .addr               = mem,
        .nbytes             = nbytes,
        .record             = record
    };
    *epoch += 1;

    *dest   = span;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
span_destroy(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *spanpool,
    span_t             *span
){
    if (span == nullptr)
    {
        return TSALLOC_SUCCESS;
    }

    int ret;

    ret = arena_cfg->auxil_unmap(
        arena_cfg->extra,
        ((void*)(span->addr)),
        span->nbytes
    );
    if (ret)
    {
        set_tsalloc_error(
            error_ctx,
            "span_destroy::span.c auxilliary unmapper could not free memory",
            TSALLOC_AUXIL_UNMAP_ERR
        );
        return TSALLOC_AUXIL_UNMAP_ERR;
    }

    if (spanpool)
    {
        objpool_free(spanpool, ((void*)span));
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t 
span_split(
    const glob_alloc_state_t   *glob_state,
    tsalloc_errctx_t           *error_ctx,
    objpool_t                  *spanpool,
    span_t                    **origin,
    span_t                    **dest,
    ts_szclass_t                szclass
){
    size_t  split_nbytes;
    
    split_nbytes    = tsconfig_get_nbytes_szclass(glob_state, szclass, false);

    if (((*origin)->nbytes) < split_nbytes)
    {
        set_tsalloc_error(
            error_ctx,
            "span_split::span.c origin span too small to split",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS; 
    }

    span_t *split;
    size_t  origin_nbytes;

    origin_nbytes   = ((*origin)->nbytes) - split_nbytes;
    if (origin_nbytes == 0) 
    {
        split   = *origin;
        *origin = nullptr;
        *dest   = split;
        return TSALLOC_SUCCESS;
    }
    
    tsalloc_err_t   ret;

    ret = objpool_alloc(error_ctx, spanpool, ((void*)(&split)));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    union 
    {
        uint64_t    raw;
        struct 
        { 
            uint64_t age       : 28;
            uint64_t szclass   : 16;
            uint64_t arena_uid : 12;
            uint64_t state     : 2;
            uint64_t is_slab   : 1;
            uint64_t is_alloc  : 1;
            uint64_t reserved  : 4; 
        } flags;
    } split_flags;

    split_flags.raw             = (*origin)->flags.raw;
    split_flags.flags.szclass   = szclass;
    *split                      = (span_t){
        .flags.raw  = split_flags.raw,
        .addr       = ((*origin)->addr) + origin_nbytes,
        .nbytes     = split_nbytes
    };

    (*origin)->nbytes           = origin_nbytes;
    (*origin)->flags.szclass    = tsconfig_get_span_szclass(glob_state, origin_nbytes);
    
    *dest   = split;

    return TSALLOC_SUCCESS;
}

//  assume split_align >= pagesize and pow of 2 (minimum span size)
tsalloc_err_t 
span_split_aligned(
    const glob_alloc_state_t   *glob_state,
    tsalloc_errctx_t           *error_ctx,
    objpool_t                  *spanpool,
    span_t                     *origin,
    span_t                    **dest_split,
    span_t                    **dest_lcut,
    span_t                    **dest_rcut,
    size_t                      split_align,
    ts_szclass_t                split_szclass
){
    size_t  split_nbytes;

    split_nbytes    = tsconfig_get_nbytes_szclass(
        glob_state, 
        split_szclass, 
        false
    );

    if ((origin->nbytes) < (split_nbytes + split_align))
    {
        set_tsalloc_error(
            error_ctx,
            "span_split_aligned::span.c origin span too small to split",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS; 
    }

    byte_t     *lcut_addr;
    byte_t     *rcut_addr;
    byte_t     *split_addr;
    size_t      lcut_nbytes;
    size_t      rcut_nbytes;
    uintptr_t   split_addr_offset;

    split_addr_offset   = ALIGN_UP(origin->addr, split_align)
                          - ((uintptr_t)(origin->addr));
    if (split_addr_offset == 0)
    {
        //  origin addr already aligned so lcut does not exist
        lcut_addr   = nullptr;
        rcut_addr   = origin->addr + split_nbytes;
        split_addr  = origin->addr;
        rcut_nbytes = origin->nbytes - split_nbytes;
    }
    else
    {
        //  offset is positive so lcut exists
        lcut_addr   = origin->addr;
        //  (offset + split_nbytes) less than origin nbytes so rcut exists
        rcut_addr   = origin->addr + split_addr_offset + split_nbytes;
        split_addr  = origin->addr + split_addr_offset;
        lcut_nbytes = split_addr_offset;
        rcut_nbytes = origin->nbytes - split_addr_offset - split_nbytes;
    }

    span_t         *split;
    span_t         *lcut;
    span_t         *rcut;
    tsalloc_err_t   ret;
    
    union 
    {
        uint64_t    raw;
        struct 
        { 
            uint64_t age       : 28;
            uint64_t szclass   : 16;
            uint64_t arena_uid : 12;
            uint64_t state     : 2;
            uint64_t is_slab   : 1;
            uint64_t is_alloc  : 1;
            uint64_t reserved  : 4;
        } flags;
    } split_flags, flags;

    //  cache split spant descriptor flags
    split_flags.raw             = origin->flags.raw;
    split_flags.flags.szclass   = split_szclass;

    //  allocate and populate new span descriptor for rcut
    ret = objpool_alloc(error_ctx, spanpool, ((void*)&rcut));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }
    flags.raw           = origin->flags.raw;
    flags.flags.szclass = tsconfig_get_span_szclass(glob_state, rcut_nbytes);
    *rcut               = (span_t){
        .flags.raw  = flags.raw,
        .addr       = rcut_addr,
        .nbytes     = rcut_nbytes
    };

    if (lcut_addr != nullptr)
    {
        //  allocate new span descriptor for split
        ret = objpool_alloc(error_ctx, spanpool, ((void*)&split));
        if (ret != TSALLOC_SUCCESS)
        {
            objpool_free(spanpool, ((void*)rcut));
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        //  recycle origin span descriptor for lcut and populate fields
        lcut                = origin;
        flags.raw           = origin->flags.raw;
        flags.flags.szclass = tsconfig_get_span_szclass(glob_state, lcut_nbytes);
        *lcut   = (span_t){
            .flags.raw  = flags.raw,
            .addr       = lcut_addr,
            .nbytes     = lcut_nbytes
        };
    }
    else 
    {
        //  recycle origin span descriptor for split
        lcut    = nullptr;
        split   = origin;
    }
    
    *split  = (span_t){
        .flags.raw  = split_flags.raw,
        .addr       = split_addr,
        .nbytes     = split_nbytes
    };

    *dest_lcut  = lcut;
    *dest_rcut  = rcut;
    *dest_split = split;

    return TSALLOC_SUCCESS;
}

void 
span_coalesce(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    objpool_t                  *spanpool,
    records_t                  *records,
    span_t                     *lspan,
    span_t                     *rspan,
    span_t                    **dest
){
    if (arena_cfg->unmap_on_termination) 
    {
        if (rspan->record)
        {
            if (lspan->record)
            {
                lspan->record->nbytes += rspan->record->nbytes;
            }
            else
            {
                lspan->record   = (record_t*)(((byte_t*)lspan) + sizeof(span_t));
                record_init(lspan->record, rspan->record->nbytes);
                records_push(records, lspan);
            }
            
            records_remove(records, rspan);
            rspan->record   = nullptr;
        }
    }

    size_t          nbytes;
    ts_szclass_t    szclass; 

    nbytes  = (lspan->nbytes) + (rspan->nbytes);
    szclass = tsconfig_get_szclass(glob_state, nbytes).szclass;

    lspan->flags.age        = MIN((lspan->flags.age), (rspan->flags.age));
    lspan->flags.szclass    = szclass;
    lspan->flags.state      = MAX((lspan->flags.state), (rspan->flags.state));
    lspan->nbytes           = nbytes;
    
    objpool_free(spanpool, ((void*)rspan));

    *dest = lspan;
}

tsalloc_err_t
span_set_state(
    const arena_cfg_t      *arena_cfg,
    tsalloc_errctx_t       *error_ctx,
    span_t                 *span,
    tsalloc_span_state_t    state
){
    size_t  nbytes;

    nbytes  = span->nbytes;
    switch (state) 
    {
        case TSALLOC_SPAN_CLEAN:
            memset((span->addr), 0, nbytes);
            span->flags.state   = TSALLOC_SPAN_CLEAN;
            break;

        case TSALLOC_SPAN_DIRTY:
            span->flags.state   = TSALLOC_SPAN_DIRTY;
            break;
        
        case TSALLOC_SPAN_RETAINED:
        {
            if (arena_cfg->auxil_madvise) 
            {
                int ret;
                ret = arena_cfg->auxil_madvise(
                    (arena_cfg->extra),
                    ((void*)(span->addr)),
                    nbytes,
                    TSALLOC_ADVISE_RETAIN
                );
                if (ret != 0)
                {
                    set_tsalloc_error(
                        error_ctx,
                        "span_set_state::span.h auxilliary madvise error",
                        TSALLOC_AUXIL_MADVISE_ERR
                    );
                    return TSALLOC_AUXIL_MADVISE_ERR;
                }
            }
            span->flags.state   = TSALLOC_SPAN_RETAINED;
            break;
        }

        case TSALLOC_SPAN_UNRETAINED:
        {
            if (arena_cfg->auxil_madvise) 
            {
                int ret;
                ret = arena_cfg->auxil_madvise(
                    (arena_cfg->extra),
                    ((void*)(span->addr)),
                    nbytes,
                    TSALLOC_ADVISE_UNRETAIN
                );
                if (ret != 0)
                {
                    set_tsalloc_error(
                        error_ctx,
                        "span_set_state::span.h auxilliary madvise error",
                        TSALLOC_AUXIL_MADVISE_ERR
                    );
                    return TSALLOC_AUXIL_MADVISE_ERR;
                }
            }
            span->flags.state   = TSALLOC_SPAN_DIRTY;
            break;
        }

        default:
            set_tsalloc_error(
                    error_ctx,
                    "span_set_state::span.h invalid flag argued",
                    TSALLOC_INVALID_ARGS
                );
                return TSALLOC_INVALID_ARGS;
    }

    return TSALLOC_SUCCESS;
}

void
span_get_adj(
    const pagetrie_t   *pagetrie,
    const span_t       *span,
    span_t            **dest_lspan,
    span_t            **dest_rspan
){
    void *l_addr;
    void *r_addr;

    l_addr = (void*)(((uintptr_t)span->addr) - 1);
    r_addr = (void*)(((uintptr_t)span->addr) + span->nbytes);

    *dest_lspan = (span_t*)pagetrie_lookup(pagetrie, l_addr);
    *dest_rspan = (span_t*)pagetrie_lookup(pagetrie, r_addr);
}