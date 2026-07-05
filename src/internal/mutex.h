/*
 * @file    mutex.h
 * @brief   posix thread mutex wrapper with adaptive spin support
 */


#pragma once
#ifndef MUTEX_H
#define MUTEX_H


#include    <pthread.h>
#include    "common.h"
#include    "error.h"


/* architecture-specific CPU pause for adaptive spinning */
#if defined(__x86_64__) || defined(__i386__)
    #include    <immintrin.h>
    #define     CPU_RELAX()     _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
    #define     CPU_RELAX()     __asm__ volatile("yield" ::: "memory")
#else
    #define     CPU_RELAX()
#endif

#define     MAX_SPIN_COUNT  600


/*
 * @struct  mutex
 * @brief   wrapper for pthread mutex primitive
 */
struct mutex
{
    pthread_mutex_t portable;
};

typedef struct mutex    mutex_t;


/*
 * @brief   initializes the mutex
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   mutex       pointer to the mutex to initialize
 * 
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
mutex_init(
    tsalloc_errctx_t   *error_ctx,
    mutex_t            *mutex
){
    int ret;

    ret = pthread_mutex_init(&(mutex->portable), nullptr);
    if (ret != 0)
    {
        set_tsalloc_error
        (
            error_ctx,
            "mutex_init()::mutex.h os mutex initialization error",
            TSALLOC_OS_ERR,
            ret
        );

        return TSALLOC_OS_ERR;
    }

    return TSALLOC_SUCCESS;
}

/*
 * @brief   destroys the mutex
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   mutex       pointer to the mutex to deinitialize
 * 
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
mutex_deinit(
    tsalloc_errctx_t   *error_ctx,
    mutex_t            *mutex
){
    int ret;

    ret = pthread_mutex_destroy(&(mutex->portable));
    if (ret != 0)
    {
        set_tsalloc_error
        (
            error_ctx,
            "mutex_deinit()::mutex.h os mutex destruction error",
            TSALLOC_OS_ERR,
            ret
        );

        return TSALLOC_OS_ERR;
    }
    
    return TSALLOC_SUCCESS;
}

/*
 * @brief   acquires the mutex lock using adaptive spinning
 * 
 * @param   mutex   pointer to the mutex to lock
 */
static inline void
mutex_lock(
    mutex_t    *mutex
){
    for (int i = 0; i < MAX_SPIN_COUNT; i++)
    {
        int ret;

        ret = pthread_mutex_trylock(&(mutex->portable));
        if (ret == 0)
        {
            return;
        }
        CPU_RELAX();
    }

    pthread_mutex_lock(&(mutex->portable));
}

/*
 * @brief   releases the mutex lock
 * 
 * @param   mutex   pointer to the mutex to unlock
 */
static inline void
mutex_unlock(
    mutex_t    *mutex
){
    pthread_mutex_unlock(&(mutex->portable));
}


#endif  //MUTEX_H