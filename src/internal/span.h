/*
 * @file    span.h
 * @brief   definitions of functionalities for managing memory spans and slabs
 */


#pragma once
#ifndef SPAN_H
#define SPAN_H


#include    "common.h"

#include    "bucket.h"
#include    "objpool.h"
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
 * @struct  slab
 * @brief   metadata for a memory slab
 */
struct slab
{
    uint16_t    nbytes_block;   ///< size of an individual block within the slab
    uint16_t    nblocks_free;   ///< number of currently free blocks in the slab
    byte_t     *bitmap;         ///< pointer to the bitmap tracking block allocation
};
typedef struct slab slab_t;

/*
 * @brief   initializes a new memory slab
 *
 * @param   cfg       pointer to the allocator configuration
 * @param   slabinfo  pointer to the slab information/layout configuration
 * @param   error_ctx pointer to the error context struct
 * @param   slabpool  pointer to the object pool used for slab metadata allocation
 * @param   span      pointer to the span being formatted as a slab
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
slab_init(
    const tsalloc_config_t     *cfg,
    const tsalloc_slab_info_t  *slabinfo,
    tsalloc_errctx_t   *error_ctx,
    objpool_t          *slabpool,
    span_t             *span
);

/*
 * @brief   deinitializes a memory slab and returns its metadata to the pool
 *
 * @param   slabpool  pointer to the object pool used for slab metadata
 * @param   span      pointer to the span formatted as a slab
 */
inline void
slab_deinit(
    objpool_t  *slabpool,
    span_t     *span
);


/*
 * @struct  span
 * @brief   represents a contiguous region of memory managed by the arena
 */
struct span
{
    struct 
    {
        uint64_t    age         : 16;   ///< age of the span, max 65535
        uint64_t    szclass     : 16;   ///< size class index, max 65535
        uint64_t    arena       : 12;   ///< arena index, max 4096
        uint64_t    state       : 2;    ///< 0 clean -> 1 dirty -> 2 may not need -> 3 do not need
        uint64_t    is_slab     : 1;    ///< boolean flag indicating if span is a slab
        uint64_t    is_dumpable : 1;    ///< boolean flag indicating if span can be dumped
    } flags;

    struct
    {
        bucket_coord_t      bucket;     ///< coordinates for bucket placement
        registry_coord_t    registry;   ///< coordinates for registry placement
    } coord;
    
    byte_t *addr;                       ///< pointer to the base address of the span memory
    slab_t *slab_metadata;              ///< pointer to slab metadata (if is_slab is set)
    size_t  nbytes;                     ///< total size of the span in bytes
};
typedef struct span span_t;

/*
 * @brief   creates and maps a new memory span
 *
 * @param   error_ctx pointer to the error context struct
 * @param   arena_cfg pointer to the arena configuration struct
 * @param   spanpool  pointer to the object pool for span metadata
 * @param   dest      double pointer to output the newly created span
 * @param   szclass   size class of the span being created
 * @param   _align    memory alignment requirement
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_create(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    span_t            **dest,
    tsalloc_szclass_t   szclass,
    size_t              _align
);

/*
 * @brief   destroys a memory span and unmaps its backing memory
 *
 * @param   error_ctx    pointer to the error context struct
 * @param   arena_config pointer to the arena configuration struct
 * @param   spanpool     pointer to the object pool for span metadata
 * @param   span         pointer to the span to destroy
 *
 * @return  status code representing success or failure
 */
inline tsalloc_err_t
span_destroy(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_config,
    objpool_t          *spanpool,
    span_t             *span
);

/*
 * @brief   splits a span into a smaller span and a remainder
 *
 * @param   error_ctx pointer to the error context struct
 * @param   arena_cfg pointer to the arena configuration struct
 * @param   spanpool  pointer to the object pool for span metadata
 * @param   origin    double pointer to the original span to split (updated to remainder)
 * @param   dest      double pointer to output the newly split span
 * @param   szclass   target size class for the new split span
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_split(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    span_t            **origin,
    span_t            **dest,
    tsalloc_szclass_t   szclass
);

/*
 * @brief   coalesces two physically adjacent free spans into a single span
 *
 * @param   error_ctx pointer to the error context struct
 * @param   arena_cfg pointer to the arena configuration struct
 * @param   spanpool  pointer to the object pool for span metadata
 * @param   lspan     pointer to the left span (lower memory address)
 * @param   rspan     pointer to the right span (higher memory address)
 * @param   dest      double pointer to output the coalesced span
 *
 * @return  status code representing success or failure
 */
tsalloc_err_t
span_coalesce(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    span_t             *lspan,
    span_t             *rspan,
    span_t            **dest
);

#endif  //SPAN_H