
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
 * @param   pagetrie    pointer to the pagetrie to initialize
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 * 
 * @return  status code representing success or failure
 */
ts_err_t
pagetrie_init(
    pagetrie_t   *pagetrie,
    int32_t       glob_uid
);

/*
 * @brief   destroys the pagetrie and frees all associated nodes
 *
 * @param   pagetrie    pointer to the pagetrie to deinitialize
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 *
 * @return  status code representing success or failure
 */
ts_err_t
pagetrie_deinit(
    pagetrie_t   *pagetrie,
    int32_t       glob_uid
);

/*
 * @brief   inserts data into the pagetrie for a specific virtual page
 * 
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page to map
 * @param   data        pointer to the metadata to associate with the page
 * @param   nbytes      size of key
 * @param   glob_uid    global uid of corresponding `glob_t` instance
 * 
 * @return  status code representing success or failure
 */
ts_err_t
pagetrie_insert(
    pagetrie_t   *pagetrie,
    const void   *key,
    const void   *data,
    size_t        nbytes,
    int32_t       glob_uid
);

/*
 * @brief   removes the metadata key associated with a virtual page
 * 
 * @param   pagetrie    pointer to the pagetrie instance
 * @param   key         virtual memory address in page associated with key to be removed from trie
 * @param   nbytes      size of key
 */
bool 
pagetrie_remove(
    pagetrie_t   *pagetrie,
    const byte_t *key,
    size_t        nbytes                                                
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