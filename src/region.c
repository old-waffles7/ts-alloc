
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/region.h"

#include    "internal/mutex.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"


//          --- region_heap_t implementation ---

static inline reg_heap_node_t*
region_heap_merge(
    reg_heap_node_t    *lnode,
    reg_heap_node_t    *rnode
){
    if (!lnode)
    {
        return rnode;
    }
    if (!rnode)
    {
        return lnode;
    }

    reg_heap_node_t    *root;
    reg_heap_node_t    *child;

    if (reg_heap_node_get_key(lnode) < reg_heap_node_get_key(rnode))
    {
        root    = lnode;
        child   = rnode;
    }
    else
    {
        root    = rnode;
        child   = lnode;
    }

    child->prev = root;
    child->next = root->lchild;
    if (root->lchild)
    {
        root->lchild->prev  = child;
    }

    root->lchild    = child;
    root->next      = nullptr;
    root->prev      = nullptr;
    
    return root;
}

inline tsalloc_err_t
region_heap_init(
    tsalloc_errctx_t   *error_ctx,
    region_heap_t      *region_heap
){
    tsalloc_err_t   ret;
    
    ret = mutex_init(error_ctx, &(region_heap->mutex));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

inline tsalloc_err_t
region_heap_deinit(
    tsalloc_errctx_t   *error_ctx,
    region_heap_t      *region_heap
){
    tsalloc_err_t   ret;

    ret = mutex_deinit(error_ctx, &(region_heap->mutex));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

reg_heap_node_t*
region_heap_pop(
    region_heap_t  *region_heap
){
    reg_heap_node_t    *root;
    reg_heap_node_t    *tail;
    reg_heap_node_t    *curr;

    root    = region_heap->root;
    if (!root)
    {
        return nullptr;
    }
    curr    = root->lchild;
    if (!curr)
    {
        region_heap->root   = nullptr;
        return root;
    }

    tail    = nullptr;
    // initial pass
    while (curr && curr->next)
    {
        reg_heap_node_t    *merge;
        reg_heap_node_t    *pair_l;
        reg_heap_node_t    *pair_r;
        reg_heap_node_t    *next_pair;

        pair_l      = curr;
        pair_r      = pair_l->next;
        next_pair   = pair_r->next;

        merge       = region_heap_merge(pair_l, pair_r);
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
        reg_heap_node_t    *prev;

        prev        = tail->prev;
        tail->prev  = nullptr;
        prev->next  = nullptr;
        tail        = region_heap_merge(prev, tail);
    }

    region_heap->root   = tail;

    *root   = (reg_heap_node_t){0};

    return root;
}

inline void
region_heap_push(
    region_heap_t      *region_heap,
    reg_heap_node_t    *node
){
    if (!node)
    {
        return;
    }

    *node   = (reg_heap_node_t){0};

    region_heap->root = region_heap_merge(region_heap->root, node);
}

inline void
region_heap_remove(
    region_heap_t      *region_heap,
    reg_heap_node_t    *node
){
    if (!node)
    {
        return;
    }

    if (region_heap->root == node)
    {
        region_heap_pop(region_heap);
        return;
    }

    if (node->prev->lchild == node)
    {
        node->prev->lchild  = node->next;
    }
    else
    {
        node->prev->next    = node->next;
    }
    if (node->next)
    {
        node->next->prev    = node->prev;
    }

    region_heap_t   temp_heap;

    temp_heap.root      = node;
    region_heap_pop(&temp_heap);
    region_heap->root   = region_heap_merge(region_heap->root, temp_heap.root);
    
    *node   = (reg_heap_node_t){0};
}


//          --- region_t implementation ---

tsalloc_err_t
region_create(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_config,
    region_t          **dest,
    size_t              align,
    size_t              nbytes
){
    byte_t     *raw;
    size_t      mem_offset;
    size_t      req_nbytes;

    mem_offset  = ALIGN_UP(sizeof(region_t), align);
    req_nbytes  = ALIGN_UP((nbytes + mem_offset), arena_config->auxil_align);

    raw = arena_config->auxil_map(arena_config->extra, align, req_nbytes);
    if (!raw)
    {
        set_tsalloc_error
        (
            error_ctx,
            "region_create()::region.c auxilliary map failure",
            TSALLOC_AUXIL_MAP_ERR
        );
        return TSALLOC_AUXIL_MAP_ERR;
    }

    *dest   = ((region_t*)raw);
    **dest  = (region_t) {
        .nbytes     = req_nbytes - mem_offset,
        .mem_offset = mem_offset,
        .origin     = (arena_config->allow_cross_origin_merge)? 0 : ((uintptr_t)raw)
    };

    return TSALLOC_SUCCESS;
}

inline tsalloc_err_t
region_destroy(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_config,
    region_t           *region
){
    int ret;

    ret = arena_config->auxil_unmap
    (
        arena_config->extra, 
        ((void*)region),
        (region->nbytes + region->mem_offset)
    );
    if (!ret)
    {
        set_tsalloc_error
        (
            error_ctx,
            "region_destroy()::region.c auxilliary unmap failure",
            TSALLOC_AUXIL_MAP_ERR
        );
        return TSALLOC_AUXIL_UNMAP_ERR;
    }

    return TSALLOC_SUCCESS;
}

inline void
region_get_adj(
    pagetrie_t *region_ptrie,
    region_t   *target,
    region_t  **dest_ladj,
    region_t  **dest_radj
){
    region_t   *ladj;
    region_t   *radj;
    void       *l_addr;
    void       *r_addr;

    l_addr  = (void*)(((byte_t*)target) - 1);
    r_addr  = (void*)(((byte_t*)target) + target->mem_offset + target->nbytes);

    ladj    = pagetrie_lookup(region_ptrie, l_addr);
    radj    = pagetrie_lookup(region_ptrie, r_addr);

    *dest_ladj  = (ladj && ladj->origin == target->origin)? ladj : nullptr;
    *dest_radj  = (radj && radj->origin == target->origin)? radj : nullptr;
}

inline region_t* 
region_coalesce(
    region_t       *ladj,
    region_t       *radj,
    size_t          align
){
    size_t  nbytes;
    size_t  mem_offset;

    mem_offset  = ALIGN_UP(sizeof(region_t), align);
    nbytes      = ladj->nbytes + ladj->mem_offset + radj->nbytes + radj->mem_offset - mem_offset;

    ladj->nbytes        = nbytes;
    ladj->mem_offset    = mem_offset;

    return ladj;
}



