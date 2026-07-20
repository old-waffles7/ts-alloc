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
    void       *root;       ///< pointer to the level 1 root node
    mutex_t     lock;       ///< mutex for locking trie on interior node creation
    objpool_t   nodepool;   ///< object pool for allocating interior and leaf nodes
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
inline tsalloc_err_t
pagetrie_init(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie
);

/*
 * @brief   destroys the pagetrie and frees all associated nodes
 * 
 * @param   pagetrie    pointer to the pagetrie to deinitialize
 */
inline void
pagetrie_deinit(
    pagetrie_t *pagetrie
);

/*
 * @brief   inserts data into the pagetrie for a specific virtual page
 * 
 * @param   error_ctx   pointer to the error context struct
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page to map
 * @param   data        pointer to the metadata to associate with the page
 * 
 * @return  status code representing success or failure
 */
tsalloc_err_t
pagetrie_insert(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie,
    void               *key,
    void               *data
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
    pagetrie_t *pagetrie,
    void       *key
);

/*
 * @brief   removes the metadata key associated with a virtual page
 * 
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page associated with key to be removed from trie
 */
bool 
pagetrie_remove(
    pagetrie_t *pagetrie,
    void       *key
);


#endif  // PAGETRIE_H