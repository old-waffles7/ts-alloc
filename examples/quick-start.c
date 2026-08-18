
/**
 *  hello, this is a quick-start guide to using libtsalloc! in this document we will initialize
 *  a simple allocator backed by RAM pages.
 *
 *  please remember to go through the README and configure libtsalloc prior to compiling the library
 *  by using the configuration script: tsalloc/src/config/config.py 
*/

#include    "../include/tsalloc.h"

#include    <stdio.h>


static inline void
print_error(
    tsalloctr_t    *allocator
){
    tsalloc_errstate    state;

    //  see "tsalloc.h" for all `ts_err_t` values and their corresponding descriptions
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


int main()
{
    tsalloctr_t    *allocator;
    ts_err_t        ret;

    //  initialize a memory allocator with TSALLOC_DEFAULT_ARG. this is a macro provided by 
    //  libtsalloc that, when argued, initializes `tsalloctr_t` instances to be backed by RAM pages 
    //  and guarantees that allocations have 16 byte alignment.
    ret = tsalloc_create(TSALLOC_DEFAULT_ARG, &allocator);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("allocator created\n");

    
    void   *allocated_from_tcache;
    void   *allocated_from_global_arena;

    //  by the default configuration set via `TSALLOC_DEFAULT_ARG`, this 8 KiB allocation will be
    //  a rapid allocation made from a `tcache_t` instance. 
    ret = tsalloc(allocator, &allocated_from_tcache, (1024ULL * 8ULL));
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf(
        "address 1:     %lx\n"
        "first 8 bytes: %lx\n", 
        ((uintptr_t)allocated_from_tcache),
        *((uint64_t*)allocated_from_tcache)
    );

    //  by the default configuration set via `TSALLOC_DEFAULT_ARG`, this 64 KiB allocation will be
    //  a slower, bulk allocation made from a global `arena_t` instance. 
    ret = tsalloc(allocator, &allocated_from_global_arena, (1024ULL * 64ULL));
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf(
        "address 2:     %lx\n"
        "first 8 bytes: %lx\n", 
        ((uintptr_t)allocated_from_global_arena),
        *((uint64_t*)allocated_from_global_arena)
    );


    //  free `tcache_t` backed allocation
    ret = tsfree(allocator, allocated_from_tcache);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("free on `allocated_from_tcache`\n");

    //  free `arena_t` backed allocation
    ret = tsfree(allocator, allocated_from_global_arena);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("free on `allocated_from_global_arena`\n");


    //  remember to destroy the allocator
    ret = tsalloc_destroy(allocator);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }

    return 0;
}