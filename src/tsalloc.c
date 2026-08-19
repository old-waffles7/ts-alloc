
#include    "internal/common.h"
#include    "internal/error.h"

#include    "internal/glob.h"
#include    "internal/arenaconfig.h"

#include    "../include/tsalloc.h"


ts_err_t
tsalloc_create(
    const tsarena_cfg_t    *arena_cfg,
    tsalloctr_t           **dest
){
    return glob_create(arena_cfg, dest);
}

ts_err_t
tsalloc_destroy(
    tsalloctr_t    *tsalloctr
){
    ts_err_t    ret;
    
    ret = glob_destroy(tsalloctr);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(tsalloctr->glob_uid);
        return ret;
    }

    return ret;
}

ts_err_t
tsalloc(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes
){
    ts_err_t    ret;
    
    ret = glob_alloc(tsalloctr, dest, nbytes);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(tsalloctr->glob_uid);
        return ret;
    }

    return ret;
}

ts_err_t
tsalloc_aligned(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes,
    size_t          align
){
    if (!IS_POWER_OF_TWO(align))
    {
        set_tsalloc_error(
            tsalloctr->glob_uid,
            "tsalloc_aligned::tsalloc.c align must be power of 2",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    ts_err_t    ret;

    if (align <= tsalloctr->glob_state->page_size)
    {
        nbytes  = ALIGN_UP(nbytes, align);
        ret     = glob_alloc(tsalloctr, dest, nbytes);
    }
    else
    {
        ret = glob_alloc_spc_aligned_bulk(tsalloctr, dest, align, nbytes);
    }
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(tsalloctr->glob_uid);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

ts_err_t
tsfree(
    tsalloctr_t    *tsalloctr,
    void           *addr
){
    if (addr == nullptr)
    {
        return TSALLOC_SUCCESS;
    }
    
    ts_err_t    ret;
    
    ret = glob_free(tsalloctr, addr);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(tsalloctr->glob_uid);
        return ret;
    }

    return ret;
}

ts_szclass_t
tsszclass_of_nbytes(
    size_t  pagesize,
    size_t  nbytes
){
    const glob_alloc_state_t   *glob_state;

    glob_state  = tsconfig_get_cfg(pagesize);
    if (glob_state == nullptr)
    {
        return (ts_szclass_t)(-1);
    }

    return tsconfig_get_span_szclass(glob_state, nbytes);
}

void 
ts_turnon_tcache(
    tsalloctr_t    *tsalloctr
){
    glob_turnon_tcache(tsalloctr);
}

void 
ts_turnoff_tcache(
    tsalloctr_t    *tsalloctr
){
    glob_turnoff_tcache(tsalloctr);
}

void 
ts_req_errstate(
    tsalloctr_t    *tsalloctr,
    ts_errstate_t  *state
){
    _ts_req_errstate(state, tsalloctr->glob_uid);
}