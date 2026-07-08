
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/region.h"

#include    "internal/mutex.h"
#include    "internal/pagetrie.h"
#include    "internal/arenaconfig.h"


//          --- region_t implementation ---

//  use an object pool for descriptor
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



