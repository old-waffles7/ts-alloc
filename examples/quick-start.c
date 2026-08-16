
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

int main()
{
    tsalloctr_t    *allocator;
    ts_err_t        ret;

    /*
        initialize a memory allocator

        TSALLOC_DEFAULT_ARG is a macro provided by libtsalloc that, when argued, initializes
        `tsalloctr_t` instances to be backed by RAM pages with 16 byte allocation alignment
    */
    ret = tsalloc_create(TSALLOC_DEFAULT_ARG, &allocator);
    /*
        see "tsalloc.h" for all `ts_err_t` values and their corresponding descriptions
    */
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("allocator created\n");

    
    void   *allocated_from_tcache;
    void   *allocated_from_global_arena;

    /*
        By the default configuration set via `TSALLOC_DEFAULT_ARG`, this 8 KiB allocation will be
        a rapid allocation made from a `tcache_t` instance. 
    */
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

    /*
        By the default configuration set via `TSALLOC_DEFAULT_ARG`, this 64 KiB allocation will be
        a slower, bulk allocation, made from an `arena_t` instance.
    */
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


    /*
        free memory
    */
    ret = tsfree(allocator, allocated_from_tcache);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("free on `allocated_from_tcache`\n");

    ret = tsfree(allocator, allocated_from_global_arena);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("free on `allocated_from_global_arena`\n");


    /*
        destroy allocator
    */
    ret = tsalloc_destroy(allocator);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }

    return 0;
}