

#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/pagetrie.h"

#include    "internal/mutex.h"
#include    "internal/objpool.h"

#include    <stdatomic.h>
#include    <string.h>


#define     MAX_NBITS_PAGE_ADRESS   36
#define     MIN_PAGE_SHIFT          12      // __builtin_ctzll(sys_page_size()) (>= 4096)
#define     NLEVELS_TRIE            3
#define     NCHILD_NODES            4096    // 2^(MAX_NBITS_ADRESS/NLEVELS_TRIE)
#define     LEVEL_SHIFT             12      // (MAX_NBITS_PAGE_ADRESS - MIN_PAGE_SHIFT)/NLEVELS_TRIE
#define     LEVEL_MASK              0xFFF


union node
{
    _Atomic(union node*)    child[NCHILD_NODES];
    _Atomic(void*)          data[NCHILD_NODES];
};

typedef union node  node_t;


static void
pagetrie_rollback(
    pagetrie_t   *pagetrie,
    node_t       *root,
    uintptr_t     start,
    uintptr_t     end
){
    for (uintptr_t page_addr = start; page_addr < end; page_addr++)
    {
        uintptr_t   idx1;
        uintptr_t   idx2;
        uintptr_t   idx3;
        node_t     *node1;
        node_t     *node2;

        idx1    = (page_addr >> (2 * LEVEL_SHIFT)) & LEVEL_MASK;
        idx2    = (page_addr >> LEVEL_SHIFT) & LEVEL_MASK;
        idx3    = page_addr & LEVEL_MASK;

        node1   = atomic_load_explicit(
            &root->child[idx1],
            memory_order_relaxed
        );

        node2   = atomic_load_explicit(
            &node1->child[idx2],
            memory_order_relaxed
        );

        atomic_store_explicit(
            &node2->data[idx3],
            nullptr,
            memory_order_release
        );
    }
}


tsalloc_err_t
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
        (void)mutex_deinit(error_ctx, &(pagetrie->lock));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    node_t *root;

    ret     = objpool_alloc(error_ctx, ((objpool_t*)(&pagetrie->nodepool)), ((void*)&root));
    if (ret != TSALLOC_SUCCESS)
    {
        (void)mutex_deinit(error_ctx, &(pagetrie->lock));
        objpool_deinit(&(pagetrie->nodepool));
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }
    memset(root, 0, sizeof(node_t));

    pagetrie->root  = root;

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
pagetrie_deinit(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie
){
    tsalloc_err_t   ret;

    objpool_deinit(((objpool_t*)(&pagetrie->nodepool)));
    ret = mutex_deinit(error_ctx, &(pagetrie->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

tsalloc_err_t
pagetrie_insert(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie,
    const void         *key,
    const void         *data,
    size_t              nbytes
){
    if (nbytes == 0)                                                 
    {
        return TSALLOC_SUCCESS;
    }
    if (nbytes - 1 > UINTPTR_MAX - (uintptr_t)key)
    {
        return TSALLOC_INVALID_ARGS;
    }

    node_t         *root;
    uintptr_t       start_page;                                       
    uintptr_t       last_page;                                        
    tsalloc_err_t   ret;

    start_page  = ((uintptr_t)key) >> MIN_PAGE_SHIFT;                  
    last_page   = (((uintptr_t)key) + nbytes - 1) >> MIN_PAGE_SHIFT;   
    root        = ((node_t*)pagetrie->root);
    
    mutex_lock(&(pagetrie->lock));

    for (uintptr_t page_addr = start_page; page_addr <= last_page; page_addr++)
    {
        node_t     *node;
        node_t     *node1;
        node_t     *node2;
        uintptr_t   idx1;
        uintptr_t   idx2;
        uintptr_t   idx3;
        
        idx1    = (page_addr >> (2 * LEVEL_SHIFT)) & LEVEL_MASK;
        idx2    = (page_addr >> LEVEL_SHIFT) & LEVEL_MASK;
        idx3    = page_addr & LEVEL_MASK;

        node1   = atomic_load_explicit(&root->child[idx1], memory_order_relaxed);
        if (!node1)
        {
            ret = objpool_alloc(error_ctx, (objpool_t*)(&pagetrie->nodepool), ((void*)&node));
            if (ret != TSALLOC_SUCCESS)
            {   
                pagetrie_rollback(
                    pagetrie,
                    root,
                    start_page,
                    page_addr
                );
                mutex_unlock(&(pagetrie->lock));
                append_tsalloc_error_trace(error_ctx);
                return ret;
            }
            memset(node, 0, sizeof(node_t));
            
            atomic_store_explicit(&root->child[idx1], node, memory_order_release);
            node1   = node;          
        }
        
        node2   = atomic_load_explicit(&node1->child[idx2], memory_order_relaxed); 
        if (!node2)
        {
            ret = objpool_alloc(error_ctx, (objpool_t*)(&pagetrie->nodepool), ((void*)&node));
            if (ret != TSALLOC_SUCCESS)
            {
                pagetrie_rollback(
                    pagetrie,
                    root,
                    start_page,
                    page_addr
                );
                mutex_unlock(&(pagetrie->lock));
                append_tsalloc_error_trace(error_ctx);
                return ret;
            }
            memset(node, 0, sizeof(node_t));
            
            atomic_store_explicit(&node1->child[idx2], node, memory_order_release);
            node2   = node;
        }
        
        atomic_store_explicit(&node2->data[idx3], ((void*)data), memory_order_release); 
    }

    mutex_unlock(&(pagetrie->lock));

    return TSALLOC_SUCCESS;
}

bool 
pagetrie_remove(
    pagetrie_t     *pagetrie,
    const byte_t   *key,
    size_t          nbytes                                                
){
    if (nbytes == 0)                                                    
    {
        return true;
    }
    if (nbytes - 1 > UINTPTR_MAX - (uintptr_t)key)
    {
        return TSALLOC_INVALID_ARGS;
    }
    
    node_t     *root;
    uintptr_t   start_page;                                            
    uintptr_t   last_page;                                            

    start_page  = ((uintptr_t)key) >> MIN_PAGE_SHIFT;                 
    last_page   = (((uintptr_t)key) + nbytes - 1) >> MIN_PAGE_SHIFT;   
    root        = ((node_t*)pagetrie->root);
    
    mutex_lock(&(pagetrie->lock));

    for (uintptr_t page_addr = start_page; page_addr <= last_page; page_addr++)
    {
        uintptr_t   idx1;
        uintptr_t   idx2;
        uintptr_t   idx3;
        node_t     *node1;     
        node_t     *node2;          

        idx1    = (page_addr >> (2 * LEVEL_SHIFT)) & LEVEL_MASK;
        idx2    = (page_addr >> LEVEL_SHIFT) & LEVEL_MASK;
        idx3    = page_addr & LEVEL_MASK;

        node1   = atomic_load_explicit(&root->child[idx1], memory_order_relaxed);
        if (!node1)
        {
            mutex_unlock(&(pagetrie->lock));
            return false;
        }

        node2   = atomic_load_explicit(&node1->child[idx2], memory_order_relaxed); 
        if (!node2)
        {
            mutex_unlock(&(pagetrie->lock));
            return false;
        }

        atomic_store_explicit(&node2->data[idx3], nullptr, memory_order_release);
    }

    mutex_unlock(&(pagetrie->lock));

    return true;
}

void* 
pagetrie_lookup(
    const pagetrie_t   *pagetrie,
    const byte_t       *key
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