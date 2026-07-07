
#pragma once
#ifndef REGION_H
#define REGION_H


#include    "common.h"
#include    "error.h"

#include    "mutex.h"
#include    "pagetrie.h"
#include    "arenaconfig.h"


struct region_list_node
{
    struct region_list_node    *prev;
};
typedef struct region_list_node reg_list_node_t;

static inline region_t*
rlist_node_get_region(
    reg_list_node_t    *node
){
    return ((region_t*)node);
}


struct region_heap_node 
{
    struct region_heap_node    *next;
    struct region_heap_node    *prev;
    struct region_heap_node    *lchild;
};
typedef struct region_heap_node reg_heap_node_t;

static inline uintptr_t
reg_heap_node_get_key(
    reg_heap_node_t    *node
){
    return ((uintptr_t)node);
}

static inline void*
reg_heap_node_get_region(
    reg_heap_node_t    *node
){
    return ((region_t*)(((byte_t*)node) - sizeof(reg_list_node_t)));
}



struct region_heap
{
    reg_heap_node_t    *root;
    mutex_t             mutex;
};
typedef struct region_heap  region_heap_t;

inline tsalloc_err_t
region_heap_init(
    tsalloc_errctx_t   *error_ctx,
    region_heap_t      *reg_heap
);

inline tsalloc_err_t
region_heap_deinit(
    tsalloc_errctx_t   *error_ctx,
    region_heap_t      *reg_heap
);

reg_heap_node_t*
region_heap_pop(
    region_heap_t  *region_heap
);

inline void
region_heap_push(
    region_heap_t      *region_heap,
    reg_heap_node_t    *node
);

inline void
region_heap_remove(
    region_heap_t      *region_heap,
    reg_heap_node_t    *node
);


struct region
{
    reg_list_node_t list_node;
    reg_heap_node_t heap_node;
    size_t          nbytes;
    size_t          mem_offset;
    uintptr_t       origin;
    bool            is_slab;
    bool            is_alloc;
};
typedef struct region   region_t;

tsalloc_err_t
region_create(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_config,
    region_t          **dest,
    size_t              align,
    size_t              nbytes
);

inline tsalloc_err_t
region_destroy(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_config,
    region_t           *region
);

//  for huge allocation that go directly to mmap, add functionality to prevernt them from being 
//  part of a coalesce operation. a flag in region and check in this function
inline void
region_get_adj(
    pagetrie_t *region_ptrie,
    region_t   *target,
    region_t  **dest_ladj,
    region_t  **dest_radj
);

inline region_t* 
region_coalesce(
    region_t       *ladj,
    region_t       *radj,
    size_t          align
);

// cut our parts of regions


#endif  //REGION_H