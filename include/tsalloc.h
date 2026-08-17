
#pragma once
#ifndef TSALLOC_H
#define TSALLOC_H


#include    "../src/internal/common.h"


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

#endif      //TSALLOC_ERROR_DEFINED

typedef enum TSALLOC_ERROR  tsalloc_err_t;
typedef enum TSALLOC_ERROR  ts_err_t;


struct tsalloc_error_state
{
    const char *trace;              ///< set only if TSALLOC is compiled with OPT_TRACE_ERRORS enabled
    const char *message;            ///< error message set by TSALLOC
    int         os_error_code;      ///< error code set by OS
    ts_err_t    tsalloc_error_code; ///< error code set by TSALLOC
};
typedef struct tsalloc_error_state  tsalloc_errstate;


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

    typedef int32_t         ts_szclass_t;


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
        bool    allow_cross_origin_merge;   ///< true if contiguous regions from different map calls can be coalesced (e.g., POSIX `mmap`)
    };
    typedef struct tsalloc_arena_config tsarena_cfg_t;

#endif      //TSALLOC_ARENACONFIG_DEFINED


/*
 * @struct  tsalloc_global_arena
 * @brief   thread-global tsalloc memory allocator; manages thread-local caches and thread safe 
            memory arenas
 */
typedef struct tsalloc_global_arena tsalloctr_t;

//  passing TSALLOC_DEFAULT_ARG as parameter for @p arena_cfg configures conventional memory allocator ram.
ts_err_t
tsalloc_create(
    const tsarena_cfg_t    *arena_cfg,
    tsalloctr_t           **dest
);

ts_err_t
tsalloc_destroy(
    tsalloctr_t    *tsalloctr
);

ts_err_t
tsalloc(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes
);

ts_err_t
talloc_aligned(
    tsalloctr_t    *tsalloctr,
    void          **dest,
    size_t          nbytes,
    size_t          align
);

ts_err_t
tsfree(
    tsalloctr_t    *tsalloctr,
    void           *addr
);

void 
ts_turnon_tcache(
    tsalloctr_t    *tsalloctr
);

void 
ts_turnoff_tcache(
    tsalloctr_t    *tsalloctr
);

void 
ts_req_errstate(
    tsalloctr_t        *tsalloctr,
    tsalloc_errstate   *state
);


#endif  //TSALLOC_H