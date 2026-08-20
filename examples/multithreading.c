
/**
 *  welcome to the multithreading example in using ltsalloc! in this document we will be exploring 
 *  using a single allocator instance across multiple threads. good luck!
 *
 *  please remember to go through the README and configure libtsalloc prior to compiling the library
 *  by using the configuration script: tsalloc/src/config/config.py 
*/

#include    "../include/tsalloc.h"

#include    <pthread.h>
#include    <unistd.h>
#include    <stddef.h>
#include    <stdint.h>
#include    <stdio.h>


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#else
    #define nullptr NULL
#endif


#define     NTHREADS        4096
#define     ALLOC_SIZE      (1024ULL * 8)  // 8 KiB allocation


static inline void
print_error(
    tsalloctr_t    *allocator
){
    ts_errstate_t   state;

    //  see "tsalloc.h" for all `ts_err_t` values and their corresponding descriptions
    ts_req_errstate(allocator, &state);
    printf(
        "TSALLOC_ERROR_CODE:    %d\n"
        "OS_ERROR_CODE:         %d\n"
        "TSALLOC_MESSAGE:       %s\n"
        "TRACE:                 %s\n",
        state.ts_error_code,
        state.os_error_code,
        state.message,
        state.trace
    );
}


//  encapsulation of pthread arguments
struct thread_arg
{
    tsalloctr_t    *allocator;
    size_t          t_uid;
};
typedef struct thread_arg   targ_t;

//  working function for threads
static inline void*
thread_work(
    void   *arg
){
    tsalloctr_t    *allocator;
    targ_t         *args;
    int32_t        *a;
    int32_t        *b;
    int32_t        *A;
    int32_t        *B;
    size_t          t_uid;
    ts_err_t        ret;

    args        = (targ_t*)arg;
    allocator   = args->allocator;
    t_uid       = args->t_uid;
    
    //  these allocations are small enough to come from tcache since `allocator` was initialized  
    //  using `TSALLOC_DEFAULT_ARG` as the configuration
    ret = tsalloc(allocator, &a, sizeof(int32_t));
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf("thread: %lu allocated 4 bytes from tcache to address: %lx\n", t_uid, ((uintptr_t)a));
    
    ret = tsalloc(allocator, &b, sizeof(int32_t));
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf("thread: %lu allocated 4 bytes from tcache to address: %lx\n", t_uid, ((uintptr_t)b));

    //  this tcache allocation will be freed back to tcache
    ret = tsfree(allocator, a);
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf("thread: %lu released 4 bytes from tcache back to tcache\n", t_uid);


    //  disable tcache. this saves the memory that would otherwise be expended on the tcache
    //  structure, though this may decrease the speed of small allocations during heavy contestation
    ret = ts_turnoff_tcache(allocator);
    if (ret != TSALLOC_SUCCESS)goto failure;


    //  this tcache allocation will be freed directly to a global arena
    ret = tsfree(allocator, b);
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf("thread: %lu released 4 bytes from tcache to a global arena\n", t_uid);

    //  this allocation will come directly from a global arena since this threads tcache has been 
    //  disabled
    ret = tsalloc(allocator, &A, sizeof(int32_t));
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf(
        "thread: %lu allocated 4 bytes from global arena to address: %lx\n", 
        t_uid, ((uintptr_t)A)
    );

    ret = tsalloc(allocator, &B, sizeof(int32_t));
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf(
        "thread: %lu allocated 4 bytes from global arena to address: %lx\n", 
        t_uid, ((uintptr_t)B)
    );

    //  this global arena allocation will be freed back to a global arena
    ret = tsfree(allocator, A);
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf("thread: %lu released 4 bytes from a global arena back to the global arena\n", t_uid);


    //  re-enable tcache
    ts_turnon_tcache(allocator);


    //  this global arena allocation will be freed to the tcache
    ret = tsfree(allocator, ((void*)B));
    if (ret != TSALLOC_SUCCESS)goto failure;
    printf("thread: %lu released 4 bytes from a global arena to a tcache\n", t_uid);
    

    pthread_exit(nullptr);

    failure: 
        print_error(allocator);
        pthread_exit(nullptr);
}

int main(void)
{
    tsalloctr_t    *allocator;
    targ_t          t_args[NTHREADS];
    pthread_t       threads[NTHREADS];
    ts_err_t        ret;

    ret = tsalloc_create(TSALLOC_DEFAULT_ARG, &allocator);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator);
        return -1;
    }
    printf("allocator created\n");

    //  create threads
    for (size_t i = 0; i < NTHREADS; i++)
    {
        t_args[i]   = (targ_t){
            .allocator  = allocator,
            .t_uid      = i + 1
        };
        if (pthread_create(threads + i, nullptr, thread_work, (void*)(t_args + i)))
        {
            printf("failed to create thread #%ld\n", i);
            return -1;
        }
    }
    //  wait for threads to finish work
    for (size_t i = 0; i <NTHREADS; i++)
    {
        if (pthread_join(threads[i], nullptr))
        {
            printf("failed to create thread #%ld\n", i);
            return -1;
        }
    }

    ret = tsalloc_destroy(allocator);
    if (ret != TSALLOC_SUCCESS)
    {
        printf("failed to destroy allocator.\n");
        return -1;
    }

    return 0;
}