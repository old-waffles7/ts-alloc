
/*  tsalloc.h   */

#pragma once
#ifndef TSALLOC_H
#define TSALLOC_H


#include    <stdbool.h>
#include    <stddef.h>
#include    <stdint.h>


#ifndef     TSALLOC_DEFAULT_ARG
    #define     TSALLOC_DEFAULT_ARG     0
#endif      //TSALLOC_DEFAULT_ARG


#ifndef     TSALLOC_ERROR_DEFINED
#define     TSALLOC_ERROR_DEFINED

    /*
    * @enum    TSALLOC_ERROR
    * @brief   status codes representing the outcome of allocator operations
    */
    enum TSALLOC_ERROR    : uint8_t
    {
        TSALLOC_SUCCESS = 0,        ///< operation completed successfully
        TSALLOC_UNTRACKED_FAILURE,  ///< arena failed to initialize/deinitialize
        TSALLOC_OS_ERR,             ///< os error, sets `os_error_code`
        TSALLOC_AUXIL_MAP_ERR,      ///< auxilliary allocator could not allocate memory
        TSALLOC_AUXIL_UNMAP_ERR,    ///< auxilliary allocator could not free memory
        TSALLOC_AUXIL_MADVISE_ERR,  ///< auxilliary madvise could not implement flag
        TSALLOC_INVALID_ARGS        ///< invalid arguments passed as parameters
    }; 
    typedef enum TSALLOC_ERROR  ts_err_t;
    
    
    struct ts_error_state
    {
        const char *trace;              ///< set only if TSALLOC is compiled with OPT_TRACE_ERRORS enabled
        const char *message;            ///< error message set by TSALLOC
        int         os_error_code;      ///< error code set by OS
        ts_err_t    ts_error_code;      ///< error code set by TSALLOC
    };
    typedef struct ts_error_state   ts_errstate_t;

#endif      //TSALLOC_ERROR_DEFINED


#ifndef     TSALLOC_ARENACONFIG_DEFINED
#define     TSALLOC_ARENACONFIG_DEFINED

    enum TSALLOC_ADVISE_FLAG    : uint8_t
    {
        TSALLOC_ADVISE_RETAIN,  ///< flags unused physical pages for reclaimation by mapper, maintains virtual address
        TSALLOC_ADVISE_UNRETAIN ///< remaps physical pages to a virutal address formerly put into retained state via `TSALLOC_ADVISE_RETAIN`
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

    typedef int32_t ts_szclass_t;


    /**
    * @brief   defines the hardware backend interface and configuration for an arena
    * 
    * holds function pointers for custom allocation and deallocation routines, allowing 
    * the arena to interact with arbitrary memory sources. includes an opaque pointer 
    * for backend-specific state
    */
    struct tsalloc_arena_config
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
            
        size_t          pagesize;                   ///< default alignment of auxilliary allocator; e.g `def_auxil_map` invokes `mmap`, aligns to page-size
        ts_szclass_t    default_new_span_szclass;
        uint16_t        lcpu_arena_count;

        bool    unmap_on_termination;       ///< true if all mapped memory must be explicitly unmapped via `auxil_unmap` on program termination
        bool    allow_cross_origin_merge;   ///< true if contiguous regions from different map calls can be coalesced (treated as a single mapping). e.g. 2 contiguous mappings from POSIX `mmap` can be treated as a single mapping an unmaped with a single `unmap` invocation
    };
    typedef struct tsalloc_arena_config ts_arena_cfg_t;

#endif      //TSALLOC_ARENACONFIG_DEFINED


/**
 * @struct  tsalloc_global_arena
 * @brief   thread-global tsalloc memory allocator; manages thread-local caches and thread safe 
            memory arenas
 */
typedef struct tsalloc_global_arena tsalloctr_t;


ts_err_t
_tsalloc(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes
);

ts_err_t
_tsalloc_aligned(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes,
    size_t          align
);


/**
 *  @brief  creates a new allocator, instance of `tsalloctr_t`
 *
 *  @param  arena_cfg   pointer to the an instance of the arena configuration struct, 
 *                      `ts_arena_cfg_t`, that determines implementation and behavior of allocator;
 *                      arguing `TSALLOC_DEFAULT_ARG` configures a conventional RAM-backed allocator
 *  @param  dest        pointer to destination for address of created allocator
 *  
 *  @return `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
*/
ts_err_t
tsalloc_create(
    const ts_arena_cfg_t  *arena_cfg,
    tsalloctr_t          **dest
);

/**
 *  @brief  destroys an allocator
 *
 *  @param  tsalloctr   pointer to allocator to destroy
 *
 *  @return `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
*/
ts_err_t
tsalloc_destroy(
    tsalloctr_t    *tsalloctr
);

/**
 *  @brief  allocates a block of memory
 *
 *  @param  tsalloctr   pointer to the allocator instance to use
 *  @param  dest        pointer to destination for the allocated memory address
 *  @param  nbytes      number of bytes to allocate
 *  
 *  @return `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
*/
#define tsalloc(            \
        tsalloctr,          \
        dest,               \
        nbytes              \
    )                       \
    _tsalloc(               \
        (tsalloctr),        \
        ((void**)dest),     \
        (nbytes)            \
    )

/**
 *  @brief  allocates a block of memory aligned to a specific boundary
 *
 *  @param  tsalloctr   pointer to the allocator instance to use
 *  @param  dest        pointer to destination for the allocated memory address
 *  @param  nbytes      number of bytes to allocate
 *  @param  align       alignment of the allocated memory (must be a power of two)
 *  
 *  @return `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
*/
#define tsalloc_aligned(    \
        tsalloctr,          \
        dest,               \
        nbytes,             \
        align               \
    )                       \
    _tsalloc_aligned(       \
        (tsalloctr),        \
        ((void**)dest),     \
        (nbytes),           \
        (align)             \
    )

/**
 *  @brief  frees a previously allocated block of memory
 *
 *  @param  tsalloctr   pointer to the allocator instance that owns the memory
 *  @param  addr        pointer to the memory block to free
 *  
 *  @return `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
*/
ts_err_t
tsfree(
    tsalloctr_t    *tsalloctr,
    void           *addr
);


/**
 *  @brief  calculates the size class for a given allocation request
 *
 *  @param  pagesize    the configured or system page size
 *  @param  nbytes      the requested allocation size in bytes
 *  
 *  @return the computed size class corresponding to the requested bytes or `-1` on failure
*/
ts_szclass_t
tsszclass_of_nbytes(
    size_t  pagesize,
    size_t  nbytes
);

/**
 *  @brief  enables the thread cache (tcache) for the specified allocator
 *
 *  @param  tsalloctr   pointer to the allocator instance
*/
void 
ts_turnon_tcache(
    tsalloctr_t    *tsalloctr
);

/**
 *  @brief  disables the thread cache (tcache) for the specified allocator
 *
 *  @param  tsalloctr   pointer to the allocator instance
 *  
 *  @return `TSALLOC_SUCCESS` on success, otherwise an appropriate error code
*/
ts_err_t 
ts_turnoff_tcache(
    tsalloctr_t    *tsalloctr
);

/**
 *  @brief  retrieves the current error state of the allocator
 *
 *  @param  tsalloctr   pointer to the allocator instance
 *  @param  state       pointer to destination struct to populate with the error state
*/
void 
ts_req_errstate(
    tsalloctr_t    *tsalloctr,
    ts_errstate_t  *state
);


#endif  //TSALLOC_H