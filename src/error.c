
#include    "internal/common.h"
#include    "internal/error.h"

#include    "config/tsalloc_config.h"

#include    "internal/glob.h"


#define     SIZE_TRACE_BUFFER   512


struct tsalloc_error_ctx
{
    #ifdef  OPT_TRACE_ERRORS
        char    trace[512]; 
    #endif

    const char *orig_filename;
    const char *message;
    int         os_error_code;
    ts_err_t    ts_error_code;
    uint16_t    orig_line;
};
typedef struct tsalloc_error_ctx    ts_errctx_t;


_Thread_local static ts_errctx_t    _err_ctxs[TSALLOC_MAXN_GLOBS];


void 
_ts_req_errstate(
    ts_errstate_t  *state,
    int32_t         glob_uid
){
    ts_errctx_t    *err_ctx;

    err_ctx = _err_ctxs + glob_uid;
    #ifdef  OPT_TRACE_ERRORS
    {
        *state  = (ts_errstate_t){
            .trace          = err_ctx->trace,
            .message        = err_ctx->message,
            .os_error_code  = err_ctx->os_error_code,
            .ts_error_code  = err_ctx->ts_error_code
        };
    }
    #else 
    {
        *state  = (ts_errstate_t){
            .message        = err_ctx.message,
            .os_error_code  = err_ctx.os_error_code,
            .ts_error_code  = err_ctx.ts_error_code
        };
    }
    #endif  //OPT_TRACE_ERRORS
}

void 
_set_tsalloc_error(
    const char *orig_filename,
    const char *message,
    int32_t     glob_uid,
    int         os_error_code,
    ts_err_t    ts_error_code,
    uint16_t    orig_line
){
    if (glob_uid == TSALLOC_NO_ERROR_CONTEXT)
    {
        return;
    }

    ts_errctx_t    *err_ctx;

    err_ctx     = _err_ctxs + glob_uid;
    *err_ctx    = (ts_errctx_t){

        #ifdef  OPT_TRACE_ERRORS
            .trace  = "",
        #endif

        .orig_filename  = orig_filename,
        .message        = message,
        .os_error_code  = os_error_code,
        .ts_error_code  = ts_error_code,
        .orig_line      = orig_line
    };
}


#ifdef      OPT_TRACE_ERRORS

    #include    <string.h>
    #include    <stdio.h>


    void
    _append_tsalloc_error_trace(
        const char *orig_filename,
        const char *orig_function,
        int32_t     glob_uid
    ){
        if (glob_uid == TSALLOC_NO_ERROR_CONTEXT)
        {
            return;
        }

        ts_errctx_t    *err_ctx;

        err_ctx     = _err_ctxs + glob_uid;
        size_t  nbytes_used;
        size_t  nbytes_free;

        nbytes_used = strnlen(err_ctx->trace, SIZE_TRACE_BUFFER);
        nbytes_free = SIZE_TRACE_BUFFER - nbytes_used;
        if (nbytes_free > 5)
        {
            snprintf
            (
                err_ctx->trace + nbytes_used,
                nbytes_free,
                "<--%s()::%s ",
                orig_function,
                orig_filename
            );
        }
    }

#endif      //OPT_TRACE_ERRORS