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

#include    "os.h"


typedef void*   
(*auxil_map_fn)(
    void   *extra,
    size_t  align,
    size_t  nbytes
);

typedef int   
(*auxil_unmap_fn)(
    void   *extra,
    void   *ptr,
    size_t  nbytes
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
    auxil_map_fn    auxil_map;

    /**
     * @brief   core deallocation function pointer
     * 
     * @param   extra   opaque pointer to backend-specific state
     * @param   ptr     pointer to the start of the mapped memory region
     * @param   nbytes  exact nbytes originally requested
     * 
     * @return  exactly 0 on success, otherwise failure
     * 
     * @warning must be thread-safe (e.g., 'munmap`, `cudaFree` are thread-safe )
     */
    auxil_unmap_fn  auxil_unmap;

    void   *extra;          ///< pointer to state for use by user in auxiliary mapping, unmapping functions
    size_t  auxil_align;    ///< default alignment of auxilliary allocator; e.g `def_auxil_map` invokes `mmap`, aligns to page-size

    bool    allow_cross_origin_merge;   ///< true if contiguous regions from different map calls can be coalesced (e.g., POSIX `mmap`)
};

typedef struct arena_config arena_conf_t;

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

    return sys_aligned_map(nbytes, align);
}

/**
 * @brief   default auxiliary unmapping function for cpu memory
 * 
 * routes memory release requests to the standard os wrapper. the capacity passed 
 * must exactly match the originally mapped capacity
 * 
 * @param   extra   opaque pointer to backend-specific state (unused in default)
 * @param   ptr     pointer to the start of the mapped memory region
 * @param   nbytes  exact nbytes originally requested during mapping
 */
static inline int
def_auxil_unmap(
    void   *extra,
    void   *ptr,
    size_t  nbytes
){
    return sys_unmap(ptr, nbytes);
}


#endif  //ARENACONFIG_H