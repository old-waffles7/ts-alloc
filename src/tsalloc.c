
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
        append_tsalloc_error_trace(&(tsalloctr->error_ctx));
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
        append_tsalloc_error_trace(&(tsalloctr->error_ctx));
    }

    return ret;
}


/*
TODO: rework free to work for blocks that came from tsalloc_aligned

ts_err_t
talloc_aligned(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes,
    size_t          align
){
    if (!IS_POWER_OF_TWO(align))
    {
        set_tsalloc_error(
            &(tsalloctr->error_ctx),
            "talloc_aligned::tsalloc.c align must be power of 2",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    if (align > tsalloctr->arena_cfg.def_alloc_align)
    {
        nbytes  = ALIGN_UP(nbytes, align);
    }

    byte_t     *_dest;
    ts_err_t    ret;
    
    ret = glob_alloc(tsalloctr, ((void**)&_dest), nbytes);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(&(tsalloctr->error_ctx));
    }

    *dest   = ((void*)(_dest + align));

    return ret;
}
*/

ts_err_t
tsfree(
    tsalloctr_t    *tsalloctr,
    void           *addr
){
    ts_err_t    ret;
    
    ret = glob_free(tsalloctr, addr);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(&(tsalloctr->error_ctx));
    }

    return ret;
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
    tsalloctr_t        *tsalloctr,
    tsalloc_errstate   *state
){
    #ifdef  OPT_TRACE_ERRORS
    {
        *state  = (tsalloc_errstate){
            .trace              = tsalloctr->error_ctx.trace,
            .message            = tsalloctr->error_ctx.message,
            .os_error_code      = tsalloctr->error_ctx.os_error_code,
            .tsalloc_error_code = tsalloctr->error_ctx.error_code
        };
    }
    #else 
        *state  = (tsalloc_errstate){
            .message            = tsalloctr->error_ctx.message,
            .os_error_code      = tsalloctr->error_ctx.os_error_code,
            .tsalloc_error_code = tsalloctr->error_ctx.error_code
        };
    #endif  //OPT_TRACE_ERRORS
}