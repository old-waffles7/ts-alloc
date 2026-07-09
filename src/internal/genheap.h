/*
 * @file    pageheap.h
 * @brief   pairing heap implementation for memory pages, priotizes enqueuing of lowest virtual 
 *          address
 */

 
#pragma once
#ifndef PAGEHEAP_H
#define PAGEHEAP_H


#include    "common.h"
#include    "error.h"
#include    "mutex.h"


/*
 * @struct  pageheap_node
 * @brief   internal structural node for the pairing heap
 */
struct pageheap_node
{
    struct pageheap_node   *next;       ///< pointer to the next sibling node
    struct pageheap_node   *prev;       ///< pointer to the previous sibling node
    struct {
        struct pageheap_node    *head;  ///< pointer to the first child node
    } child_list;
};
typedef struct pageheap_node    pageheap_node_t;

/*
 * @struct  intrusive_pageheap_node
 * @brief   wrapper node embedding heap coordinates and page memory
 */
struct intrusive_pageheap_node
{
    pageheap_node_t coord;  ///< embedded heap coordinate node
    void           *page;   ///< pointer to the managed memory page
};
typedef struct intrusive_pageheap_node  ipageheap_node_t;


/*
 * @brief   compares two intrusive pageheap nodes based on page address
 *
 * @param   node_1  pointer to the first node
 * @param   node_2  pointer to the second node
 *
 * @return  `true` if node_1 page address is greater than node_2, `false` otherwise
 */
static inline bool
ipageheap_node_cmp(
    ipageheap_node_t   *node_1,
    ipageheap_node_t   *node_2
){
    return ((uintptr_t)(node_1->page)) > ((uintptr_t)(node_2->page));
}

/*
 * @brief   retrieves the memory page pointer from an intrusive node
 *
 * @param   nod pointer to the intrusive node
 *
 * @return  pointer to the memory page
 */
static inline void*
ipageheap_node_get_page(
    ipageheap_node_t   *node
){
    return node->page;
}

/*
 * @brief   retrieves the coordinate node from an intrusive node
 *
 * @param   node    pointer to the intrusive node
 *
 * @return  pointer to the embedded coordinate node
 */
static inline pageheap_t*
ipageheap_node_get_coord(
    ipageheap_node_t   *node
){
    return &(node->coord);
}

/*
 * @brief   retrieves the parent intrusive node from a coordinate node
 *
 * @param   coord   pointer to the coordinate node
 *
 * @return  pointer to the parent intrusive node
 */
static inline ipageheap_node_t*
coord_get_intrusive(
    pageheap_node_t    *coord
){
    return ((ipageheap_node_t*)coord);
}


/*
 * @struct  pageheap
 * @brief   root state for the pairing heap
 */
struct pageheap
{
    pageheap_node_t    *root;   ///< pointer to the root node of the heap
};
typedef struct pageheap pageheap_t;

/*
 * @brief   removes and returns the minimum node from the heap
 *
 * @param   pageheap    pointer to the pageheap
 *
 * @return  pointer to the popped intrusive node, or nullptr if empty
 */
ipageheap_node_t*
pageheap_pop(
    pageheap_t *pageheap
);

/*
 * @brief   inserts a new node into the heap
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   pageheap    pointer to the pageheap
 * @param   node        pointer to the intrusive node to insert
 */
inline void
pageheap_insert(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap,
    ipageheap_node_t   *node
);

/*
 * @brief   removes a specific node from the heap
 *
 * @param   pageheap    pointer to the pageheap
 * @param   node        pointer to the intrusive node to remove
 */
inline void
pageheap_remove(
    pageheap_t         *pageheap,
    ipageheap_node_t   *node
);


#endif  //PAGEHEAP_H