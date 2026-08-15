/*
 * @file    mutex.h
 * @brief   posix-sephamore backed mutex with adaptive spin support
 */


#pragma once
#ifndef MUTEX_H
#define MUTEX_H


#include    "common.h"
#include    "error.h"

#include    <semaphore.h>
#include    <stdatomic.h>
#include    <pthread.h>


#if defined(__x86_64__) || defined(__i386__)
    #include <immintrin.h>
    #define CPU_RELAX() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
    #define CPU_RELAX() __asm__ volatile("yield" ::: "memory")
#else
    #define CPU_RELAX() ((void)0)
#endif

#define MAX_SPIN_COUNT 600


enum TSALLOC_MUTEX_STATE    : int
{
    FREE    = 0,
    LOCKED,
    LOCKED_WAITERS
};
typedef enum TSALLOC_MUTEX_STATE    tsalloc_mtx_state_t;


/*
 * @struct  mutex
 * @brief   wrapper for posix-sephamore primitive
 */
struct mutex 
{
    _Atomic(int)        state; 
    _Atomic(pthread_t)  locker_tid; 
    sem_t               waitq;
};
typedef struct mutex mutex_t;


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

    ret = sem_init(&(mutex->waitq), false, 0);
    if (ret != 0)
    {
        set_tsalloc_error(
            error_ctx,
            "mutex_init()::mutex.h os semaphore initialization error",
            TSALLOC_OS_ERR
        );
        return TSALLOC_OS_ERR;
    }
    atomic_store_explicit(&(mutex->state), FREE, memory_order_release); 
    atomic_store_explicit(&(mutex->locker_tid), (pthread_t){0}, memory_order_release);  

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

    ret = sem_destroy(&(mutex->waitq));
    if (ret != 0)
    {
        set_tsalloc_error(
            error_ctx,
            "mutex_deinit()::mutex.h os semaphore destruction error",
            TSALLOC_OS_ERR
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
    int ret;
    int exp;

    for (int i = 0; i < MAX_SPIN_COUNT; i++)
    {
        ret = atomic_load_explicit(&(mutex->state), memory_order_relaxed);
        if (ret == FREE)
        {
            exp = FREE;
            ret = atomic_compare_exchange_weak_explicit(
                &(mutex->state),
                &exp,
                LOCKED,
                memory_order_acquire,
                memory_order_relaxed
            );
            if (ret == true)
            {
                atomic_store_explicit(&(mutex->locker_tid), pthread_self(), memory_order_relaxed);
                return;
            }
        }
        CPU_RELAX();
    }
    
    ret = atomic_exchange_explicit(&(mutex->state), LOCKED_WAITERS, memory_order_acquire);
    while (ret != FREE)
    {
        sem_wait(&(mutex->waitq));
        ret = atomic_exchange_explicit(&(mutex->state), LOCKED_WAITERS, memory_order_acquire);
    }

    atomic_store_explicit(&(mutex->locker_tid), pthread_self(), memory_order_release); 
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
    pthread_t   locker_tid; 
    pthread_t   self_tid; 
    int         ret;

    locker_tid  = atomic_load_explicit(&(mutex->locker_tid), memory_order_acquire);
    self_tid    = pthread_self();
    if (!pthread_equal(self_tid, locker_tid))
    { 
        return;
    }

    atomic_store_explicit(&(mutex->locker_tid),(pthread_t){0},memory_order_relaxed);

    ret = atomic_exchange_explicit(&(mutex->state), FREE, memory_order_release);
    if (ret == LOCKED_WAITERS)
    {
        sem_post(&(mutex->waitq));
    }
}


#endif  //MUTEX_H