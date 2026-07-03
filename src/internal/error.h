/*
 * @file    error.h
 * @brief   error-reporting subsystem for tracking and bubbling failures
 */


#pragma once
#ifndef ERROR_H
#define ERROR_H


#include    "common.h"


#define SIZE_TRACE_BUFFER   512


//  expose this later (to arena header instead of malloc header)
/*
 * @enum    TSALLOC_ERROR
 * @brief   status codes representing the outcome of allocator operations
 */
enum TSALLOC_ERROR    : uint8_t
{
    TSALLOC_SUCCESS,            ///< operation completed successfully
    TSALLOC_AUXIL_ALLOC_ERR,    ///< auxilliary allocator could not allocate memory
    TSALLOC_AUXIL_FREE_ERR,     ///< auxilliary allocator could not free memory
    TSALLOC_INVALID_ARGS        ///< invalid arguments passed as parameters
};

typedef enum TSALLOC_ERROR  tsalloc_err_t;


struct tsalloc_err_ctx
{
    const char     *orig_filename;
    const char     *message;
    uint16_t        orig_line;
    tsalloc_err_t   error_code;
    #ifdef  OPT_TRACE_ERRORS    // CMake option
        char    trace[512]; 
    #endif
};

typedef struct tsalloc_err_ctx  tsalloc_errctx_t;


static inline void 
macro_set_tsalloc_error(
    tsalloc_errctx_t   *ctx,
    const char         *orig_filename,
    const char         *message,
    uint16_t            orig_line,
    tsalloc_err_t       error_code
){
    if (!ctx)
    {
        return;
    }

    *ctx    = (tsalloc_errctx_t)  
    {
        orig_filename,
        message,
        orig_line,
        error_code
    };
}

/**
 * @brief   instantly captures the file, line, and reason for a failure
 * 
 * evaluates to a safe `do-while(false)` block. mutates the context's pinned error struct in-place
 * 
 * @param   error_ctx_ptr   pointer to `tsalloc_errctx_t` instance of interest
 * @param   error_code      `tsalloc_err_t` value representing reason of failure
 * @param   message         static string literal detailing what broke
 * 
 * @warning must be called precisely when the initial error occurs, not during bubbling
 */
#define set_tsalloc_error(              \
    error_ctx_ptr,                      \
    message,                            \
    error_code                          \
) do                                    \
{                                       \
    macro_set_tsalloc_error             \
    (                                   \
        (error_ctx_ptr),                \
        __FILE__,                       \
        (message),                      \
        __LINE__,                       \
        (error_code)                    \
    );                                  \
} while (false)


//  conditionally enable robust stack-tracing
#ifdef  OPT_TRACE_ERRORS

    #include    <string.h>
    #include    <stdio.h>

    static inline void 
    macro_append_tsalloc_error_trace(
        tsalloc_errctx_t   *ctx,
        const char         *orig_filename,
        const char         *orig_function
    ){
        if (!ctx)
        {
            return;
        }

        size_t  nbytes_used;
        size_t  nbytes_free;

        nbytes_used = strnlen(ctx->trace, SIZE_TRACE_BUFFER);
        nbytes_free = SIZE_TRACE_BUFFER - nbytes_used;
        if (nbytes_free > 5)
        {
            snprintf
            (
                ctx->trace + nbytes_used,
                nbytes_free,
                "<--%s()::%s ",
                orig_function,
                orig_filename
            );
        }
    }

    /**
     * @brief   appends the current function to the error breadcrumb trail
     * 
     * used exclusively when bubbling an existing error upward. safely writes to the internal trace 
     * buffer without dynamic allocation
     * 
     * @param   error_ctx_ptr pointer to `tsalloc_errctx_t` instance of interest
     */
    #define append_tsalloc_error_trace(     \
        error_ctx_ptr                       \
    ) do                                    \
    {                                       \
        macro_append_tsalloc_error_trace    \
        (                                   \
            (error_ctx_ptr),                \
            __FILE__,                       \
            __func__                        \
        );                                  \
    } while (false)
#else
    #define append_tsalloc_error_trace(     \
        error_ctx_ptr                       \
    ) ((void)0)
#endif  // TRACE_ERRORS


#endif  // INTERNALERR_H