
/**
 * @file    tcache.h
 * @brief   thread-local cache definitions and interface
 */


#pragma once
#ifndef TCACHE_H
#define TCACHE_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "os.h"
#include    "glob.h"
#include    "arena.h"
#include    "column.h"
#include    "ledger.h"
#include    "arenaconfig.h"


/**
 * @struct  thread_loc_cache
 * @brief   represents a thread-local cache of memory blocks
 */
struct thread_local_bcache
{
    col_t          *columns;
    glob_t         *macro;
    ledger_coord_t  coord;
    ts_szclass_t    nszclasses; 
};
typedef struct thread_local_bcache  tcache_t;


/**
 * @brief   calculates the memory required for a thread-local cache
 *
 * @param   glob_state  pointer to global allocation state
 *
 * @return  required memory size in bytes
 */
static inline size_t
_tcache_mem_size(
    const glob_alloc_state_t   *glob_state
){
    static size_t   nbytes;

    if (nbytes)
    {
        return nbytes;
    }

    ts_szclass_t    nszclasses;

    nbytes     += sizeof(tcache_t);
    nszclasses  = glob_state->nszclasses_span;
    for (ts_szclass_t i = 0; i < nszclasses; i++)
    {
        nbytes += col_auxil_mem_size(glob_state->tcache_info[i]) + sizeof(col_t);
    }

    return nbytes;
}


/**
 * @brief   flushes all columns in a thread-local cache
 *
 * @param   cache   pointer to thread-local cache being flushed
 */
static inline void
_tcache_flush(
    tcache_t   *cache
){
    ts_szclass_t    nszclasses;
    
    nszclasses  = cache->nszclasses;
    for (ts_szclass_t i = 0; i < nszclasses; i++)
    {
        col_flush(cache->macro, cache->columns + i);
    }
}


/**
 * @brief   creates a thread-local cache
 *
 * @param   glob    pointer to global allocator
 * @param   dest    pointer destination of pointer to created thread-local cache
 *
 * @return  status code representing success or failure
 */
static tsalloc_err_t
tcache_create(
    glob_t     *glob,
    tcache_t  **dest
){
    byte_t *raw;
    size_t  nbytes_req;

    nbytes_req  =_tcache_mem_size(glob->glob_state);
    raw         = (byte_t*)sys_map(nbytes_req);
    if (raw == nullptr)
    {
        set_tsalloc_error(
            &(glob->error_ctx),
            "tcache_create::tcache.h os failure to map memory for tcache",
            TSALLOC_OS_ERR
        );
        return TSALLOC_OS_ERR;
    }

    col_t          *columns_addr;
    byte_t         *auxil_mems_addr;
    tcache_t       *cache;
    ts_szclass_t    nszclasses;

    nszclasses      = glob->glob_state->nszclasses_span;
    columns_addr    = (col_t*)(raw + sizeof(tcache_t));
    auxil_mems_addr = ((byte_t*)columns_addr) + nszclasses * sizeof(col_t);
    cache           = (tcache_t*)raw;
    *cache          = (tcache_t){
        .columns    = columns_addr,
        .macro      = glob,
        .nszclasses = nszclasses
    };

    tsalloc_err_t   ret;

    for (ts_szclass_t i = 0; i < nszclasses; i++)
    {
        ret = col_init(
            glob->glob_state, 
            &(glob->error_ctx), 
            auxil_mems_addr, 
            columns_addr + i, 
            i
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(&(glob->error_ctx));
            return ret;
        }


        auxil_mems_addr    += col_auxil_mem_size(glob->glob_state->tcache_info[i]);
    }

    *dest   = cache;

    return TSALLOC_SUCCESS;
}


/**
 * @brief   destroys a thread-local cache
 *
 * @param   cache   pointer to thread-local cache being destroyed
 *
 * @return  status code representing success or failure
 */
static inline tsalloc_err_t
tcache_destroy(
    tcache_t   *cache
){
    if (!cache)
    {
        return TSALLOC_SUCCESS;
    }
    
    int ret;

    _tcache_flush(cache);
    ret = sys_unmap(((void*)cache), _tcache_mem_size(cache->macro->glob_state));

    if (ret != 0)
    {
        set_tsalloc_error(
            &(cache->macro->error_ctx),
            "tcache_destroy::tcache.h os failure to unmap memory for tcache",
            TSALLOC_OS_ERR
        );
        return TSALLOC_OS_ERR;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   retrieves a block from a thread-local cache
 *
 * @param   cache   pointer to thread-local cache supplying the block
 * @param   dest    pointer to destination of pointer to block 
 * @param   szclass size class of the requested block
 *
 * @return  status code representing success or failure
 *
 * @warning caller must check @p szclass is valid
 */
static inline tsalloc_err_t
tcache_get_block(
    tcache_t       *cache,
    byte_t        **dest,
    ts_szclass_t    szclass
){
    tsalloc_err_t   ret;

    ret = col_get_block(
        cache->macro, 
        cache->columns + szclass, 
        dest
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(&(cache->macro->error_ctx));
        return ret;
    }

    return TSALLOC_SUCCESS;
}


/**
 * @brief   returns a block to a thread-local cache
 *
 * @param   cache   pointer to thread-local cache receiving the block
 * @param   block   pointer to block being returned
 * @param   szclass size class of the block
 */
static inline void
tcache_put_block(
    tcache_t       *cache,
    byte_t         *block,
    ts_szclass_t    szclass
){
    col_put_block(
        cache->macro, 
        cache->columns + szclass, 
        block
    );
}


/**
 * @brief   decays all columns in a thread-local cache
 *
 * @param   cache   pointer to thread-local cache being decayed
 */
static inline void
tcache_decay(
    tcache_t   *cache
){
    ts_szclass_t    nszclasses;

    nszclasses  = cache->nszclasses;
    for (ts_szclass_t i = 0; i < nszclasses; i++)
    {
        col_decay(cache->macro, cache->columns + i);
    }
}


#endif  //TCACHE_H