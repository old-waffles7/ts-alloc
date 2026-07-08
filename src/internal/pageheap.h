
#pragma once
#ifndef PAGEHEAP_H
#define PAGEHEAP_H


#include    "common.h"
#include    "error.h"

#include    "mutex.h"


struct pageheap_node
{
    struct pageheap_node   *next;
    struct pageheap_node   *prev;
    struct {
        struct pageheap_node    *head;
    } child_list;
};
typedef struct pageheap_node    pageheap_node_t;


struct intrusive_pageheap_node
{
    pageheap_node_t coord;
    void           *page;     
};
typedef struct intrusive_pageheap_node  ipageheap_node_t;

static inline bool
ipageheap_node_cmp(
    ipageheap_node_t   *node_1,
    ipageheap_node_t   *node_2
){
    return ((uintptr_t)(node_1->page)) > ((uintptr_t)(node_2->page));
}

static inline void*
ipageheap_node_get_page(
    ipageheap_node_t   *node
){
    return node->page;
}

static inline pageheap_t*
ipageheap_node_get_coord(
    ipageheap_node_t   *node
){
    return &(node->coord);
}

static inline ipageheap_node_t*
coord_get_intrusive(
    pageheap_node_t    *coord
){
    return ((ipageheap_node_t*)coord);
}


struct pageheap
{
    pageheap_node_t    *root;
    mutex_t mutex;
};
typedef struct pageheap pageheap_t;

inline tsalloc_err_t
pageheap_init(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap
);

inline tsalloc_err_t
pageheap_deinit(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap
);

ipageheap_node_t*
pageheap_pop(
    pageheap_t *pageheap
);

inline void
pageheap_insert(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap,
    ipageheap_node_t   *node
);

inline void
pageheap_remove(
    pageheap_t         *pageheap,
    ipageheap_node_t   *node
);


#endif  //PAGEHEAP_H