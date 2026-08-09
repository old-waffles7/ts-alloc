/**
 * @file    arena_config.h
 * @brief   configuration structures and default implementations for the arena backend
 * 
 * provides the interface for dependency injection of custom memory mapping and unmapping
 * functions. allows arenas to target different hardware backends seamlessly
 */

 
#pragma once
#ifndef ARENACONFIG_H
#define ARENACONFIG_H


#include    "common.h"

#include    "../config/tsalloc_config.h"

#include    "os.h"

#include    <sys/mman.h>
#include    <errno.h>


//  expose this later (to both arena header malloc header)
/*
 * @enum    TSALLOC_ADVISE_FLAG
 * @brief   status codes representing the outcome of allocator operations
 * 
 * @warning implementation of all flags in `auxil_madvise` is NOT required, the function can even
 *          be `NULL`
 */
enum TSALLOC_ADVISE_FLAG    : uint8_t
{
    /* todo
        TSALLOC_DONT_FORK,  ///< flags that memory should not be duplicated on `fork` invocations
        TSALLOC_DO_FORK,    ///< flags that memory must be duplicated on `fork` invocations
    */
    
    TSALLOC_ADVISE_RETAIN   ///< flags unused (unallocated) pages for immediate reclamation by mapper, automatically provides new zerod-out pages on next access
};
typedef enum TSALLOC_ADVISE_FLAG    tsalloc_advice_t;


typedef void* (*auxil_map_fn)(
    void   *extra,
    size_t  align,
    size_t  nbytes
);

typedef int (*auxil_unmap_fn)(
    void   *extra,
    void   *addr,
    size_t  nbytes
);

typedef int (*auxil_madvise_fn)(
    void               *extra,
    void               *addr,
    size_t              nbytes,
    tsalloc_advice_t    flag
);


/**
 * @brief   defines the hardware backend interface and configuration for an arena
 * 
 * holds function pointers for custom allocation and deallocation routines, allowing 
 * the arena to interact with arbitrary memory sources. includes an opaque pointer 
 * for backend-specific state
 */
struct arena_config
{
    /**
     * @brief   core allocation function pointer
     * 
     * @param   extra   opaque pointer to backend-specific state
     * @param   align   minimum alignment required for the allocation
     * @param   nbytes  exact nbytes of memory to allocate
     * 
     * @return  pointer to the allocated memory region
     * 
     * @warning must be thread-safe (e.g., `mmap`, 'cudaMalloc` are thread-safe)
     */
    auxil_map_fn        auxil_map;

    /**
     * @brief   core deallocation function pointer
     * 
     * @param   extra   opaque pointer to backend-specific state
     * @param   addr    pointer to the start of the mapped memory region
     * @param   nbytes  exact nbytes originally requested
     * 
     * @return  exactly 0 on success, otherwise failure
     * 
     * @warning must be thread-safe (e.g., 'munmap`, `cudaFree` are thread-safe )
     */
    auxil_unmap_fn      auxil_unmap;

    /**
     * @brief   pointer to core mutator of memory-state
     * 
     * @param   extra   opaque pointer to backend-specific state
     * @param   addr    pointer to the start of the mapped memory region
     * @param   nbytes  exact nbytes originally requested
     * @param   flag    dictates how memory-state will be mutated
     * 
     * @return  exactly 0 on success, otherwise failure
     * 
     * @warning implementation of this function is optional, can be `NULL`
     * @warning must be thread-safe (e.g., `mmap`, `posix_madvise` are thread-safe)
     */
    auxil_madvise_fn    auxil_madvise;

    void   *extra;  ///< pointer to state for use by user in auxiliary mapping, unmapping functions
    
    const tsalloc_cfg_t    *tsalloc_cfg;    ///< pointer to configuration profile for library
    
    tsalloc_szclass_t   default_new_span_szclass;
    size_t              auxil_align;    ///< default alignment of auxilliary allocator; e.g `def_auxil_map` invokes `mmap`, aligns to page-size

    bool    unmap_on_termination;       ///< true if all mapped memory must be explicitly unmapped via `auxil_unmap` on program termination
    bool    allow_cross_origin_merge;   ///< true if contiguous regions from different map calls can be coalesced (e.g., POSIX `mmap`)
};
typedef struct arena_config arena_cfg_t;

static inline bool
arena_cfg_isvalid(
    arena_cfg_t cfg
){
    if ((!cfg.auxil_map) || (!cfg.auxil_unmap))
    {
        return false;
    }
    if (!cfg.tsalloc_cfg)
    {
        return false;
    }
    if (!IS_POWER_OF_TWO(cfg.auxil_align))
    {
        return false;
    }
    if (cfg.default_new_span_szclass >= cfg.tsalloc_cfg->nszclasses_span)
    {
        return false;
    }

    return true;
}

/**
 * @brief   default auxiliary mapping function for cpu memory
 * 
 * routes memory requests to the standard os wrappers. automatically handles custom 
 * alignments by conditionally invoking the over-allocating aligned allocator only 
 * when the requested alignment exceeds the base hardware page size
 * 
 * @param   extra   opaque pointer to backend-specific state (unused in default)
 * @param   align   integer to which start of memory segment will be aligned
 * @param   nbytes  exact nbytes of memory to allocate
 * 
 * @return  pointer to the allocated memory region, or `nullptr` if mapping fails
 */
static inline void*   
def_auxil_map(
    void   *extra,
    size_t  align,
    size_t  nbytes
){
    if (align <= sys_page_size())
    {
        return sys_map(nbytes);
    }

    return sys_aligned_map(align, nbytes);
}

/**
 * @brief   default auxiliary unmapping function for cpu memory
 * 
 * routes memory release requests to the standard os wrapper. the capacity passed 
 * must exactly match the originally mapped capacity
 * 
 * @param   extra   opaque pointer to backend-specific state (unused in default)
 * @param   addr    pointer to the start of the mapped memory region
 * @param   nbytes  exact nbytes originally requested during mapping
 * 
 * @return  exactly `0` on success, otherwise failure
 */
static inline int
def_auxil_unmap(
    void   *extra,
    void   *addr,
    size_t  nbytes
){
    return sys_unmap(addr, nbytes);
}

#if defined(__linux__)
    #define     mem_dontfork(addr, nbytes)      \
        madvise((addr), (nbytes), MADV_DONTFORK)
    #define     mem_dofork(addr, nbytes)        \
        madvise((addr), (nbytes), MADV_DOFORK)
    #define     madvise_dont_need(addr, nbytes) \
        madvise((addr), (nbytes), MADV_DONTNEED)    
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define     mem_dontfork(addr, nbytes)      \
        minherit((addr), (nbytes), INHERIT_NONE)
    #define     mem_dofork(addr, nbytes)        \
        minherit((addr), (nbytes), INHERIT_COPY)
    #define     madvise_dont_need(addr, nbytes) \
        madvise((addr), (nbytes), MADV_FREE)    
#else
    #define     mem_dontfork(addr, nbytes)      ((void)0)
    #define     mem_dofork(addr, nbytes)        ((void)0)
    #define     madvise_dont_need(addr, nbytes) \
        posix_madvise(addr, nbytes, POSIX_MADV_DONTNEED)
#endif

/**
 * @brief   default auxiliary mutator of memory-state
 * 
 * @param   extra   opaque pointer to backend-specific state
 * @param   addr    pointer to the start of the mapped memory region
 * @param   nbytes  exact nbytes originally requested
 * @param   flag    dictates how memory-state will be mutated
 * 
 * @return  exactly `0` on success, otherwise failure
 */
static inline int
def_auxil_madvise(
    void               *extra,
    void               *addr,
    size_t              nbytes,
    tsalloc_advice_t    flag
){
    int ret;
    
    if (flag & TSALLOC_ADVISE_RETAIN)
    {
        ret = madvise_dont_need(addr, nbytes);
        if (ret)
        {
            return -1;
        }
    }

    /*
        if (flag & TSALLOC_DONT_FORK)
        {
            ret = MEM_DONTFORK(addr, nbytes);
            if (ret)
            {
                return -1;
            }
        }
        if (flag & TSALLOC_DO_FORK)
        {
            ret = MEM_DOFORK(addr, nbytes);
            if (ret)
            {
                return -1;
            }
        }
    */

    return 0;
}


#endif  //ARENACONFIG_H