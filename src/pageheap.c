
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/pageheap.h"

#include    "internal/mutex.h"


static inline pageheap_node_t*
pageheap_node_merge(
    pageheap_node_t   *node_1,
    pageheap_node_t   *node_2
){
    if (!node_1) return node_2;
    if (!node_2) return node_1;

    pageheap_node_t *root;
    pageheap_node_t *child;

    if (ipageheap_node_cmp(coord_get_intrusive(node_1), coord_get_intrusive(node_2)))
    {
        root  = node_1;
        child = node_2;
    }
    else
    {
        root  = node_2;
        child = node_1;
    }

    child->prev = root;
    child->next = root->child_list.head;
    if (root->child_list.head)
    {
        root->child_list.head->prev = child;
    }

    root->child_list.head = child;
    root->prev            = nullptr; 
    root->next            = nullptr;

    return root;
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

        merge       = pageheap_node_merge(pair_l, pair_r);
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
    while (tail && tail->prev)
    {
        pageheap_node_t    *prev;

        prev        = tail->prev;
        tail->prev  = nullptr;
        prev->next  = nullptr;
        tail        = pageheap_node_merge(prev, tail);
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

    pageheap->root = pageheap_node_merge(pageheap->root, &(node->coord));
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
    
    pageheap->root = pageheap_node_merge(pageheap->root, temp_heap.root);
    
    *coord  = (pageheap_node_t){0};
}