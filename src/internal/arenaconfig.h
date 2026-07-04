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
     * @return  pointer to the allocated memory region
     */
    void*   (*auxil_map)
    (
        void   *extra,
        size_t  align,
        size_t  nbytes
    );

    /**
     * @brief   core deallocation function pointer
     * 
     * @param   extra   opaque pointer to backend-specific state
     * @param   ptr     pointer to the start of the mapped memory region
     * @param   nbytes  exact nbytes originally requested
     */
    void    (*auxil_unmap)
    (
        void   *extra,
        void   *ptr,
        size_t  nbytes
    );

    void   *extra;      /**< pointer to state for use by user in auxiliary mapping, unmapping functions */
    size_t  def_align;  /**< default hardware alignment requirement for this backend */
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
        return sys_alloc(nbytes);
    }

    return sys_aligned_alloc(nbytes, align);
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
static inline void
def_auxil_unmap(
    void   *extra,
    void   *ptr,
    size_t  nbytes
){
    sys_free(ptr, nbytes);
}


#endif  //ARENACONFIG_H