/*
struct arena
{
    region_t   *region_list;
    pagetrie_t  region_trie;
    pageheap_t  region_buckets[NCLASS_REGION];
    pageheap_t  slab_buckets[NCLASS_SLAB];
    objpool_t   region_dpool;
    objpool_t   slab_dpool;
    mutex_t     mutex;
    atomic_int  nthreads;
    size_t      nbytes_alloc;
    size_t      nbytes_cached;
}

struct arena 
{
    -   arenaconfig
    
    -   slab cache
        array of slabs for eaach slab block szclass
        each as a linked list? idk the need for the linked list
    
    -   span cache, scache instace

    -   span registry for destruction

    -   pagetrie

    -   spanpool 
    
    -   slabpool

    -   nthreads

    -   stats nbytes_alloc i think is enough then api to fetch it
}
*/

/*
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/bcache.h"

#include    "internal/bin.h"
#include    "internal/span.h"
#include    "internal/mutex.h"
#include    "internal/scache.h"
#include    "internal/objpool.h"
#include    "internal/arenaconfig.h"


tsalloc_err_t
bcache_init(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t             *arena_cfg,
    byte_t             *auxil_mem,
    bcache_t           *cache
){
    if (!auxil_mem)
    {
        set_tsalloc_error(
            error_ctx,
            "bcache_init::bcache.c nullptr axuil_mem argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    size_t  nclasses;

    nclasses        = arena_cfg->tsalloc_cfg->nszclasses_slab;
    cache->nclasses = nclasses;

    tsalloc_err_t   ret;

    cache->locks    = (mutex_t*)auxil_mem;
    for (int i = 0; i < nclasses; i++)
    {
        ret = mutex_init(error_ctx, (cache->locks) + i);
        if (ret != TSALLOC_SUCCESS)
        {
            for (int j = 0; j < i; j++)
            {
                mutex_deinit(error_ctx, (cache->locks) + j);
            }
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    byte_t *bins_addr;
    bins_addr   = auxil_mem + (sizeof(mutex_t) * nclasses);
    cache->bins = (registry_t*)bins_addr;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
bcache_deinit(
    tsalloc_errctx_t   *error_ctx,
    bcache_t           *cache
){
    tsalloc_err_t   ret;

    for (int i = 0; i < cache->nclasses; i++)
    {
        mutex_deinit(error_ctx, (cache->locks) + i);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
bcache_
*/



tsalloc_err_t
scache_get_span(
    const tsalloc_slab_info_t  *slab_init_info,
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    scache_t           *cache,
    span_t            **dest,
    tsalloc_szclass_t   szclass
){
    span_t             *span;
    tsalloc_szclass_t   bin_szclass;
    tsalloc_err_t       ret;
    bool                isoverfit;

    isoverfit = scache_find_nonempty_bin_idx(cache, &bin_szclass, szclass);
    if (bin_szclass >= cache->nclasses)
    {
        ret = scache_mint_span(error_ctx, arena_cfg, cache, &span, szclass);
        if (ret != TSALLOC_SUCCESS)
        {
            return ret;
        }
        isoverfit = span->flags.szclass > szclass;
    }
    else 
    {
        bin_t  *bin;

        bin     = &(cache->bins[bin_szclass]);
        span    = bin_get_span(bin);
        if (bin_isempty(bin)) 
        {
            scache_set_bitmap(cache, bin_szclass, false); 
        }
    }

    if (isoverfit)
    {
        span_t *cut;

        ret = span_split(error_ctx, arena_cfg, &(cache->spanpool), &span, &cut, szclass);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        ret = scache_put_span(error_ctx, arena_cfg, cache, span);
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        span = cut;
    }

    span->flags.is_alloc    = true;
    
    *dest = span;

    return TSALLOC_SUCCESS;
}




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

