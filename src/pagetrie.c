
#include    <string.h>

#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/pagetrie.h"

#include    "internal/objpool.h"


#define     MAX_NBITS_PAGE_ADRESS   36
#define     MIN_PAGE_SHIFT          12      // __builtin_ctzll(sys_page_size()) (>= 4096)
#define     NLEVELS_TRIE            3
#define     NCHILD_NODES            4096    // 2^(MAX_NBITS_PAGE_ADRESS/NLEVELS_TRIE)
#define     LEVEL_SHIFT             12      // (MAX_NBITS_PAGE_ADRESS - MIN_PAGE_SHIFT)/NLEVELS_TRIE
#define     LEVEL_MASK              0xFFF


union node
{
    _Atomic(union node*)    child[NCHILD_NODES];
    _Atomic(void*)          data[NCHILD_NODES];
};

typedef union node  node_t;


inline tsalloc_err_t
pagetrie_init(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie
){
    tsalloc_err_t   ret;

    ret = mutex_init(error_ctx, &(pagetrie->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    ret = objpool_init
    (
        error_ctx,
        ((objpool_t*)(&pagetrie->nodepool)),
        8,
        sizeof(node_t),
        256
    );
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        mutex_deinit(error_ctx, &(pagetrie->lock));
        return ret;
    }

    node_t *root;

    ret     = objpool_alloc(error_ctx, ((objpool_t*)(&pagetrie->nodepool)), ((void*)&root));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        objpool_deinit(&(pagetrie->nodepool));
        mutex_deinit(error_ctx, &(pagetrie->lock));
        return ret;
    }
    memset(root, 0, sizeof(node_t));

    pagetrie->root  = root;

    return TSALLOC_SUCCESS;
}

inline void
pagetrie_deinit(
    pagetrie_t *pagetrie
){
    objpool_deinit(((objpool_t*)(&pagetrie->nodepool)));
}

tsalloc_err_t
pagetrie_insert(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie,
    void               *key,
    void               *data
){
    node_t     *root;
    uintptr_t   page_addr;
    uintptr_t   idx1;
    uintptr_t   idx2;
    uintptr_t   idx3;

    page_addr   = ((uintptr_t)key) >> MIN_PAGE_SHIFT;
    idx1        = (page_addr >> (2 * LEVEL_SHIFT)) & LEVEL_MASK;
    idx2        = (page_addr >> LEVEL_SHIFT) & LEVEL_MASK;
    idx3        = page_addr & LEVEL_MASK;
    root        = ((node_t*)pagetrie->root);
    
    mutex_lock(&pagetrie->lock);

    node_t         *node;
    node_t         *node1;
    tsalloc_err_t   ret;

    node1   = atomic_load_explicit(&root->child[idx1], memory_order_relaxed);
    if (!node1)
    {
        ret = objpool_alloc(error_ctx, (objpool_t*)(&pagetrie->nodepool), ((void*)&node));
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&pagetrie->lock);
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        memset(node, 0, sizeof(node_t));
        
        atomic_store_explicit(&root->child[idx1], node, memory_order_release);
        node1   = node;          
    }
    
    node_t *node2;

    node2   = atomic_load_explicit(&node1->child[idx2], memory_order_relaxed); 
    if (!node2)
    {
        ret = objpool_alloc(error_ctx, (objpool_t*)(&pagetrie->nodepool), ((void*)&node));
        if (ret != TSALLOC_SUCCESS)
        {
            mutex_unlock(&pagetrie->lock); 
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        memset(node, 0, sizeof(node_t));
        
        atomic_store_explicit(&node1->child[idx2], node, memory_order_release);
        node2   = node;
    }
    atomic_store_explicit(&node2->data[idx3], data, memory_order_release); 

    mutex_unlock(&pagetrie->lock);

    return TSALLOC_SUCCESS;
}

void* 
pagetrie_lookup(
    pagetrie_t *pagetrie,
    void       *key
){
    node_t     *root;
    uintptr_t   page_addr;
    uintptr_t   idx1;
    uintptr_t   idx2;
    uintptr_t   idx3;

    page_addr   = ((uintptr_t)key) >> MIN_PAGE_SHIFT;
    idx1        = (page_addr >> (2 * LEVEL_SHIFT)) & LEVEL_MASK;
    idx2        = (page_addr >> LEVEL_SHIFT) & LEVEL_MASK;
    idx3        = page_addr & LEVEL_MASK;
    root        = ((node_t*)pagetrie->root);
    
    node_t *node1;
    node1   = atomic_load_explicit(&root->child[idx1], memory_order_acquire); 
    if (!node1)
    {
        return nullptr;
    }

    node_t *node2;
    node2   = atomic_load_explicit(&node1->child[idx2], memory_order_acquire);
    if (!node2)
    {
        return nullptr;
    }

    return atomic_load_explicit(&node2->data[idx3], memory_order_acquire);
}

bool 
pagetrie_remove(
    pagetrie_t *pagetrie,
    void       *key
){
    node_t     *root;
    uintptr_t   page_addr;
    uintptr_t   idx1;
    uintptr_t   idx2;
    uintptr_t   idx3;

    page_addr   = ((uintptr_t)key) >> MIN_PAGE_SHIFT;
    idx1        = (page_addr >> (2 * LEVEL_SHIFT)) & LEVEL_MASK;
    idx2        = (page_addr >> LEVEL_SHIFT) & LEVEL_MASK;
    idx3        = page_addr & LEVEL_MASK;
    root        = ((node_t*)pagetrie->root);
    
    node_t *node1;     
    node1   = atomic_load_explicit(&root->child[idx1], memory_order_acquire);
    if (!node1)
    {
        return false;
    }

    node_t *node2;          
    node2   = atomic_load_explicit(&node1->child[idx2], memory_order_acquire); 
    if (!node2)
    {
        return false;
    }

    atomic_store_explicit(&node2->data[idx3], nullptr, memory_order_release);

    return true;
}