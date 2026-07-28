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



