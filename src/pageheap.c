
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/pageheap.h"

#include    "internal/mutex.h"


static inline ipageheap_node_t*
ipageheap_node_merge(
    ipageheap_node_t   *node_1,
    ipageheap_node_t   *node_2
){
    if (!node_1)
    {
        return node_2;
    }
    if (!node_2)
    {
        return node_1;
    }

    pageheap_node_t    *root;
    pageheap_node_t    *child;

    if (ipageheap_node_cmp(node_1, node_2))
    {
        root    = &(node_1->coord);
        child   = &(node_2->coord);
    }
    else
    {
        root    = &(node_2->coord);
        child   = &(node_1->coord);
    }

    child->prev = root;
    child->next = root->child_list.head;
    if (root->child_list.head)
    {
        root->child_list.head->prev = child;
    }

    root->child_list.head   = child;
    root->next              = nullptr;
    root->prev              = nullptr;

    return coord_get_intrusive(root);
}

inline tsalloc_err_t
pageheap_init(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap
){
    tsalloc_err_t   ret;
    
    ret = mutex_init(error_ctx, &(pageheap->mutex));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

inline tsalloc_err_t
pageheap_deinit(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap
){
    tsalloc_err_t   ret;

    ret = mutex_deinit(error_ctx, &(pageheap->mutex));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

ipageheap_node_t*
pageheap_pop(
    pageheap_t *pageheap
){
    pageheap_node_t    *root;
    pageheap_node_t    *tail;
    pageheap_node_t    *curr;
    
    root    = pageheap->root;
    if (!root)
    {
        return nullptr;
    }
    curr    = root->child_list.head;
    if (!curr)
    {
        pageheap->root  = nullptr;
        return coord_get_intrusive(root);
    }

    tail    = nullptr;
    // initial pass
    while (curr && curr->next)
    {
        pageheap_node_t    *merge;
        pageheap_node_t    *pair_l;
        pageheap_node_t    *pair_r;
        pageheap_node_t    *next_pair;

        pair_l      = curr;
        pair_r      = pair_l->next;
        next_pair   = pair_r->next;

        merge       = &(ipageheap_node_merge
        (
            coord_get_intrusive(pair_l), 
            coord_get_intrusive(pair_r)
        )->coord);
        merge->prev = tail;
        if (tail)
        {
            tail->next  = merge;
        }
        
        tail    = merge;
        curr    = next_pair;
    }

    // root had odd number of children
    if (curr)
    {
        if (tail)
        {
            tail->next  = curr;
        }
        curr->prev  = tail;
        tail        = curr;
    }

    //  second pass
    while (curr && curr->prev)
    {
        pageheap_node_t    *prev;

        prev        = tail->prev;
        tail->prev  = nullptr;
        prev->next  = nullptr;
        tail        = &(ipageheap_node_merge
        (
            coord_get_intrusive(prev), 
            coord_get_intrusive(tail)
        )->coord);
    }

    pageheap->root  = tail;

    *root   = (pageheap_node_t){0};

    return coord_get_intrusive(root);
}

inline void
pageheap_insert(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap,
    ipageheap_node_t   *node
){
    if (!node)
    {
        return;
    }

    node->coord = (pageheap_node_t){0};

    pageheap->root = &(ipageheap_node_merge
    (
        coord_get_intrusive(pageheap->root), 
        node
    )->coord);
}

inline void
pageheap_remove(
    pageheap_t         *pageheap,
    ipageheap_node_t   *node
){
    if (!node)
    {
        return;
    }

    pageheap_node_t    *coord;

    coord   = &(node->coord);
    if (pageheap->root == coord)
    {
        pageheap_pop(pageheap);
        return;
    }

    if (coord->prev->child_list.head == coord)
    {
        coord->prev->child_list.head    = coord->next;
    }
    else
    {
        coord->prev->next   = coord->next;
    }
    if (coord->next)
    {
        coord->next->prev   = coord->prev;
    }

    pageheap_t  temp_heap;

    temp_heap.root  = coord;
    pageheap_pop(&temp_heap);
    pageheap->root = &(ipageheap_node_merge
    (
        coord_get_intrusive(pageheap->root), 
        coord_get_intrusive(temp_heap.root)
    )->coord);
    
    *coord  = (pageheap_node_t){0};
}


