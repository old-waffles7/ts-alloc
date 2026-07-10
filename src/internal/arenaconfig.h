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

#include    <sys/mman.h>


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
    TSALLOC_MAYBE_NEED, ///< flags pages for potential reclamation by mapper under memory pressure without immediately destroying the data if memory is accessed beforehand
    TSALLOC_DONT_NEED,  ///< flags pages for immediate reclamation by mapper, automatically provides new zerod-out pages on next access
    TSALLOC_DONT_FORK,  ///< flags that memory should not be duplicated on `fork` invocations
    TSALLOC_DO_FORK     ///< flags that memory must be duplicated on `fork` invocations
};
typedef enum TSALLOC_ADVISE_FLAG    tsalloc_advice_t;


typedef void*   
(*auxil_map_fn)(
    void   *extra,
    size_t  align,
    size_t  nbytes
);

typedef int   
(*auxil_unmap_fn)(
    void   *extra,
    void   *addr,
    size_t  nbytes
);

typedef int  
(*auxil_madvise_fn)(
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

    void   *extra;          ///< pointer to state for use by user in auxiliary mapping, unmapping functions
    size_t  auxil_align;    ///< default alignment of auxilliary allocator; e.g `def_auxil_map` invokes `mmap`, aligns to page-size

    bool    default_to_core_dump;
    bool    unmap_on_termination;       ///< true if all mapped memory must be explicitly unmapped via `auxil_unmap` on program termination
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
    #define     MEM_DONTFORK(addr, nbytes)  madvise((addr), (nbytes), MADV_DONTFORK)
    #define     MEM_DOFORK(addr, nbytes)    madvise((addr), (nbytes), MADV_DOFORK)
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define     MEM_DONTFORK(addr, nbytes)  minherit((addr), (nbytes), INHERIT_NONE)
    #define     MEM_DOFORK(addr, nbytes)    minherit((addr), (nbytes), INHERIT_COPY)
#else
    #define     MEM_DONTFORK(addr, nbytes)  0
    #define     MEM_DOFORK(addr, nbytes)    0
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
    uintptr_t   ret;

    switch (flag)
    {
        case TSALLOC_MAYBE_NEED:
            ret = posix_madvise(addr, nbytes, POSIX_MADV_DONTNEED);
            if (ret)
            {
                errno   = ret;
            }
            return ret;
        
        case TSALLOC_DONT_NEED:
            ret = ((uintptr_t)mmap
            (
                addr, 
                nbytes, 
                (PROT_READ | PROT_WRITE), 
                (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED), 
                -1, 
                0
            ));
            return (((void*)ret) == MAP_FAILED)? -1 : 0;
        
        case TSALLOC_DONT_FORK:
            return MEM_DONTFORK(addr, nbytes);
        
        case TSALLOC_DO_FORK:
            return MEM_DOFORK(addr,nbytes);
        
        default:
            return -1;
    }
}


#endif  //ARENACONFIG_H