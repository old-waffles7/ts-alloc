
#include    "../include/tsalloc.h"

#include    <stdio.h>


static inline void
print_error(
    tsalloctr_t    *allocator
){
    tsalloc_errstate    state;

    ts_req_errstate(allocator, &state);
    printf(
        "TSALLOC_ERROR_CODE:    %d\n"
        "OS_ERROR_CODE:         %d\n"
        "TSALLOC_MESSAGE:       %s\n"
        "TRACE:                 %s\n",
        state.tsalloc_error_code,
        state.os_error_code,
        state.message,
        state.trace
    );
}