/*
 * @file    pagetrie.h
 * @brief   3-level radix tree, implements mapping of virtual pages to keys
 */


#pragma once
#ifndef PAGETRIE_H
#define PAGETRIE_H


#include    "mutex.h"
#include    "objpool.h"
#include    <stdatomic.h>


/*
 * @struct  pagetrie
 * @brief   root state for the radix tree
 */
struct pagetrie
{
    void       *root;    
    mutex_t     lock;
    objpool_t   nodepool; 
};

typedef struct pagetrie pagetrie_t;


/*
 * @brief   initializes the pagetrie and its internal object pool
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   pagetrie    pointer to the pagetrie to initialize
 * 
 * @return  status code representing success or failure
 */
tsalloc_err_t
pagetrie_init(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie
);

/*
 * @brief   destroys the pagetrie and frees all associated nodes
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   pagetrie    pointer to the pagetrie to deinitialize
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
pagetrie_deinit(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie
);

/*
 * @brief   inserts data into the pagetrie for a specific virtual page
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page to map
 * @param   data        pointer to the metadata to associate with the page
 * @param   nbytes      size of key
 * 
 * @return  status code representing success or failure
 */
tsalloc_err_t
pagetrie_insert(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie,
    const void         *key,
    const void         *data,
    size_t              nbytes
);

/*
 * @brief   removes the metadata key associated with a virtual page
 * 
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page associated with key to be removed from trie
 */
bool 
pagetrie_remove(
    pagetrie_t     *pagetrie,
    const byte_t   *key,
    size_t          nbytes                                                
);

/*
 * @brief   retrieves the metadata key associated with a virtual page
 * 
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page to look up
 * 
 * @return  pointer to the metadata key, or nullptr if not found
 */
void* 
pagetrie_lookup(
    const pagetrie_t   *pagetrie,
    const byte_t       *key
);


#endif  // PAGETRIE_H