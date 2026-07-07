
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
    union node *child[NCHILD_NODES];
    void       *data[NCHILD_NODES];
};

typedef union node  node_t;


inline tsalloc_err_t
pagetrie_init(
    tsalloc_errctx_t   *error_ctx,
    pagetrie_t         *pagetrie
){
    tsalloc_err_t   ret;

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
        return ret;
    }

    node_t *root;

    ret     = objpool_alloc(error_ctx, ((objpool_t*)(&pagetrie->nodepool)), ((void*)&root));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
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
    
    if (!(root->child[idx1]))
    {
        node_t         *node;
        tsalloc_err_t   ret;

        ret     = objpool_alloc(error_ctx, (objpool_t*)(&pagetrie->nodepool), ((void*)&node));
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        memset(node, 0, sizeof(node_t));
        root->child[idx1]   = node;
    }
    if (!(root->child[idx1]->child[idx2]))
    {
        node_t         *node;
        tsalloc_err_t   ret;

        ret     = objpool_alloc(error_ctx, (objpool_t*)(&pagetrie->nodepool), ((void*)&node));
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }
        memset(node, 0, sizeof(node_t));
        root->child[idx1]->child[idx2]  = node;
    }

    root->child[idx1]->child[idx2]->data[idx3]  = data;

    return TSALLOC_SUCCESS;
}

inline void* 
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
    
    if (!root->child[idx1] || !(root->child[idx1]->child[idx2]))
    {
        return nullptr;
    }

    return root->child[idx1]->child[idx2]->data[idx3];
}

inline bool 
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
    
    if (!root->child[idx1] || !(root->child[idx1]->child[idx2]))
    {
        return false;
    }

    root->child[idx1]->child[idx2]->data[idx3]  = nullptr;

    return true;
}