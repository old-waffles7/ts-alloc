
/**
 * @file    span.h
 * @brief   definitions of functionalities for managing memory spans
 */


#pragma once
#ifndef SPAN_H
#define SPAN_H


#include    "common.h"
#include    "error.h"

#include    "../config/tsalloc_config.h"

#include    "slab.h"
#include    "bucket.h"
#include    "objpool.h"
#include    "records.h"
#include    "pagetrie.h"
#include    "registry.h"
#include    "arenaconfig.h"


#define     SPAN_NSTATES    3
#define     MAXN_ARENAS     4096


enum TSALLOC_SPAN_STATE : uint8_t
{
    TSALLOC_SPAN_CLEAN  = 0,
    TSALLOC_SPAN_DIRTY,
    TSALLOC_SPAN_RETAINED
};
typedef enum TSALLOC_SPAN_STATE tsalloc_span_state_t;


/**
 * @struct  span
 * @brief   represents a contiguous region of memory managed by the arena
 */
struct span
{
    union 
    {
        uint64_t    raw;
        struct
        {
            uint64_t    age         : 28;
            uint64_t    szclass     : 16;
            uint64_t    arena_uid   : 12;
            uint64_t    state       : 2;
            uint64_t    is_slab     : 1;
            uint64_t    is_alloc    : 1;
            uint64_t    reserved    : 4;
        };
    } flags;

    union
    {
        bucket_coord_t      bucket;     ///< coordinates for bucket placement
        registry_coord_t    registry;   ///< coordinates for registry placement
    } coord;

    record_t   *record;
    byte_t     *addr;
    slab_t     *slabmeta;
    size_t      nbytes;
};
typedef struct span span_t;

/**
 * @brief   creates and maps a new memory span
 *
 * @param   glob_state   pointer to the global allocator state
 * @param   arena_cfg    pointer to the arena configuration struct
 * @param   error_ctx    pointer to the error context struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   dest        double pointer to output the newly created span
 * @param   szclass     size class of the span being created
 * @param   epoch       pointer to the epoch used as the age of the newly minted span
 * @param   arena_uid   unique identifier of the arena owning the span
 * @param   _align      memory alignment requirement
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_create(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    tsalloc_errctx_t            *error_ctx,
    objpool_t                  *spanpool,
    span_t                    **dest,
    ts_szclass_t                szclass,
    uint32_t                   *epoch,
    uint16_t                    arena_uid,
    size_t                      _align
);

/**
 * @brief   destroys a memory span and unmaps its backing memory
 *
 * @param   arena_cfg    pointer to the arena configuration struct
 * @param   error_ctx   pointer to the error context struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   span        pointer to the span to destroy
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_destroy(
    const arena_cfg_t  *arena_cfg,
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *spanpool,
    span_t             *span
);

/**
 * @brief   splits a span into a smaller span and a remainder
 *
 * @param   glob_state   pointer to the global allocator state
 * @param   error_ctx   pointer to the error context struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   origin      double pointer to the original span to split (updated to remainder)
 * @param   dest        double pointer to output the newly split span
 * @param   szclass     target size class for the new split span
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_split(
    const glob_alloc_state_t   *glob_state,
    tsalloc_errctx_t           *error_ctx,
    objpool_t                  *spanpool,
    span_t                    **origin,
    span_t                    **dest,
    ts_szclass_t                szclass
);

/**
 * @brief   coalesces two physically adjacent free spans into a single span
 *
 * @param   glob_state   pointer to the global allocator state
 * @param   arena_cfg    pointer to the arena configuration struct
 * @param   error_ctx    pointer to the error context struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   records     pointer to the list that records all minted spans
 * @param   lspan       pointer to the left span (lower memory address)
 * @param   rspan       pointer to the right span (higher memory address)
 * @param   dest        double pointer to output the coalesced span
 */
void
span_coalesce(
    const glob_alloc_state_t   *glob_state,
    const arena_cfg_t          *arena_cfg,
    objpool_t                  *spanpool,
    records_t                  *records,
    span_t                     *lspan,
    span_t                     *rspan,
    span_t                    **dest
);

/**
 * @brief   mutates the state of a span
 *
 * @param   arena_cfg    pointer to the arena configuration struct
 * @param   error_ctx    pointer to the error context struct
 * @param   span        pointer to the span to be mutated
 * @param   state       flag corresponding to state to be implemented
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_set_state(
    const arena_cfg_t      *arena_cfg,
    tsalloc_errctx_t       *error_ctx,
    span_t                 *span,
    tsalloc_span_state_t    state
);

/**
 * @brief   retrieves the left and right adjacent spans from the pagetrie
 *
 * @param   pagetrie    pointer to the pagetrie
 * @param   span        pointer to the target span
 * @param   dest_lspan  pointer to store the left adjacent span
 * @param   dest_rspan  pointer to store the right adjacent span
 */
void
span_get_adj(
    const pagetrie_t   *pagetrie,
    const span_t       *span,
    span_t            **dest_lspan,
    span_t            **dest_rspan
);

/**
 * @brief   determines whether two adjacent spans can be merged
 *
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   lspan       pointer to the left span (lower memory address)
 * @param   rspan       pointer to the right span (higher memory address)
 *
 * @return  true if the spans can be merged, otherwise false
 */
static inline bool
span_can_merge(
    const arena_cfg_t  *arena_cfg,
    const span_t       *lspan,
    const span_t       *rspan
){
    if ((!lspan) || (!rspan))
    {
        return false;
    }
    if ((lspan->flags.is_alloc) || (rspan->flags.is_alloc))
    {
        return false;
    }
    if (lspan->flags.arena_uid != rspan->flags.arena_uid)
    {
        return false;
    }
    if ((lspan->addr + lspan->nbytes) != (rspan->addr))
    {
        return false;
    }
    if (TSALLOC_ALLOC_MAX - lspan->nbytes < rspan->nbytes)
    {
        return false;
    }

    return arena_cfg->allow_cross_origin_merge || (lspan->flags.age == rspan->flags.age);
}


#endif  //SPAN_H