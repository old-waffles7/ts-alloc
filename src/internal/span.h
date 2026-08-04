/*
 * @file    span.h
 * @brief   definitions of functionalities for managing memory spans
 */


#pragma once
#ifndef SPAN_H
#define SPAN_H


#include    "common.h"
#include    "error.h"

#include    "slab.h"
#include    "bucket.h"
#include    "objpool.h"
#include    "records.h"
#include    "pagetrie.h"
#include    "registry.h"
#include    "arenaconfig.h"


#define     SPAN_NSTATES    3


enum TSALLOC_SPAN_STATE : uint8_t
{
    TSALLOC_SPAN_CLEAN  = 0,
    TSALLOC_SPAN_DIRTY,
    TSALLOC_SPAN_RETAINED
};
typedef enum TSALLOC_SPAN_STATE tsalloc_span_state_t;


/*
 * @struct  span
 * @brief   represents a contiguous region of memory managed by the arena
 */
struct span
{
    struct 
    {
        uint64_t    age         : 28;   ///< age/origin-uid of the span, max 268435456
        uint64_t    szclass     : 16;   ///< size class index, max 65535
        uint64_t    arena       : 12;   ///< arena index, max 4096
        uint64_t    state       : 2;    ///< 0 clean -> 1 dirty -> 2 may not need -> 3 do not need
        uint64_t    is_slab     : 1;    ///< boolean flag indicating if span is a slab
        uint64_t    is_alloc    : 1;
        uint64_t    is_dumpable : 1;    // !maybe remove?add do/dont forked, pin flags?
        uint64_t    reserved    : 3;
    } flags;

    union
    {
        bucket_coord_t      bucket;     ///< coordinates for bucket placement
        registry_coord_t    registry;   ///< coordinates for registry placement
    } coord;
    
    record_t   *record;
    byte_t     *addr;               ///< pointer to the base address of the span memory
    slab_t     *slab_metadata;      ///< pointer to slab metadata (if is_slab is set)
    size_t      nbytes;             ///< total size of the span in bytes
};
typedef struct span span_t;

/*
 * @brief   creates and maps a new memory span
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   dest        double pointer to output the newly created span
 * @param   epoch       age/uid of newly minted span 
 * @param   szclass     size class of the span being created
 * @param   _align      memory alignment requirement
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_create(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    objpool_t          *spanpool,
    span_t            **dest,
    uint32_t           *epoch,
    tsalloc_szclass_t   szclass,
    size_t              _align,
    bool                init_record
);

/*
 * @brief   destroys a memory span and unmaps its backing memory
 *
 * @param   error_ctx       pointer to the error context struct
 * @param   arena_config    pointer to the arena configuration struct
 * @param   spanpool        pointer to the object pool for span metadata
 * @param   span            pointer to the span to destroy
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_destroy(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    objpool_t          *spanpool,
    span_t             *span
);

/*
 * @brief   splits a span into a smaller span and a remainder
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   origin      double pointer to the original span to split (updated to remainder)
 * @param   dest        double pointer to output the newly split span
 * @param   szclass     target size class for the new split span
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_split(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    objpool_t          *spanpool,
    span_t            **origin,
    span_t            **dest,
    tsalloc_szclass_t   szclass
);

/*
 * @brief   coalesces two physically adjacent free spans into a single span
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   spanpool    pointer to the object pool for span metadata
 * @param   records     pointer to the list that records all minted spans
 * @param   lspan       pointer to the left span (lower memory address)
 * @param   rspan       pointer to the right span (higher memory address)
 * @param   dest        double pointer to output the coalesced span
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_coalesce(
    tsalloc_errctx_t   *error_ctx,
    arena_cfg_t        *arena_cfg,
    objpool_t          *spanpool,
    records_t          *records,
    span_t             *lspan,
    span_t             *rspan,
    span_t            **dest
);

/*
 * @brief   mutates the state of a span
 *
 * @param   error_ctx   pointer to the error context struct
 * @param   arena_cfg   pointer to the arena configuration struct
 * @param   span        pointer to the span to be mutated
 * @param   state       flag corresponding to state to me implemented
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_set_state(
    tsalloc_errctx_t       *error_ctx,
    arena_cfg_t            *arena_cfg,
    span_t                 *span,
    tsalloc_span_state_t    state
);

/*
 * @brief   retrieves the left and right adjacent spans from the pagetrie
 *
 * @param   pagetrie    pointer to the pagetrie
 * @param   span        pointer to the target span
 * @param   dest_lspan  pointer to store the left adjacent span
 * @param   dest_rspan  pointer to store the right adjacent span
 */
void
span_get_adj(
    pagetrie_t *pagetrie,
    span_t     *span,
    span_t    **dest_lspan,
    span_t    **dest_rspan
);


#endif  //SPAN_H