/**
 * @file    os.h
 * @brief   abstractions of os functionalities
 * 
 * provides the interface accessing specifc os-functionalities via simplified wrapper-functions
 */

 
#pragma once
#ifndef OS_H
#define OS_H


#include    "common.h"

#include    <stdatomic.h>
#include    <sys/mman.h>
#include    <unistd.h>
#include    <errno.h>


/**
 * @brief   retrieves the hardware memory page size from the os
 * 
 * utilizes `sysconf(_SC_PAGESIZE)` to determine the base virtual memory page size of the active 
 * system. the result is cached internally via a local static variable, guaranteeing that all 
 * subsequent calls execute instantly with zero system call overhead
 * 
 * @return  the hardware page size in bytes (e.g., 4096, 16384)
 */
static inline uint64_t
sys_page_size(void)
{
    static _Atomic uint64_t cached_page_size;

    atomic_init(&cached_page_size, 0);
    if (cached_page_size == 0)
    {
        atomic_store(&cached_page_size, sysconf(_SC_PAGESIZE));
    }

    return atomic_load(&cached_page_size);
}

/*
 * @brief   retrives system error-code implemented by os
 *
 * @return  exact os error-code
*/
static inline int
sys_error_code(void){
    return errno;
}

/**
 * @brief   allocates raw, page-aligned virtual memory directly from the os 
 * 
 * bypasses c standard library heap to request an anonymous, private memory mapping. the underlying 
 * allocation is guaranteed to be zero-initialized by the kernel
 * 
 * @param   nbytes  exact nbytes of memory to allocate
 * @return  pointer to the allocated memory region, or `nullptr` if mapping fails
 * 
 * @warning can set errno
 */
static inline void*
sys_map(
    size_t  nbytes
){
    void   *mem = mmap
    (
        nullptr, 
        nbytes, 
        PROT_READ | PROT_WRITE,      //  can read & write 
        MAP_PRIVATE | MAP_ANONYMOUS, //  private to current process, not file-backed
        -1,                          //  MAP_ANONYMOUS => -1 to fd, 0 to offs
        0
    );
                      
    return (mem == MAP_FAILED)? nullptr : mem;
}

/**
 * @brief   allocates raw, custom-aligned virtual memory directly from the os 
 * 
 * bypasses c standard library heap to request an anonymous, private memory mapping. the underlying 
 * allocation is guaranteed to be zero-initialized by the kernel
 * 
 * @param   nbytes  exact nbytes of memory to allocate
 * @param   align   integer to which start of memory segment will be aligned
 * @return  pointer to the allocated memory region, or `nullptr` if mapping fails
 * 
 * @warning can set errno
 */
static inline void*
sys_aligned_map(
    size_t  align,
    size_t  nbytes
){
    if ((!IS_POWER_OF_TWO(align)) || align < sys_page_size())
    {
        return nullptr;
    }

    // overflow
    if (nbytes > SIZE_MAX - (align - sys_page_size()))
    {
        return nullptr;
    }
    
    uint8_t    *raw_mem;
    size_t      nbytes_req;
    
    nbytes      = ALIGN_UP(nbytes, sys_page_size());
    nbytes_req  = nbytes + align - sys_page_size();
    raw_mem     = (uint8_t*)mmap
    (
        nullptr, 
        nbytes_req, 
        PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, 
        -1, 
        0
    );

    if (raw_mem == MAP_FAILED)
    {
        return nullptr;
    }

    uintptr_t   raw_addr;
    uintptr_t   aligned_addr;
    size_t      prefix_size;
    size_t      suffix_size;

    raw_addr        = (uintptr_t)raw_mem;
    aligned_addr    = ALIGN_UP(raw_addr, align);

    prefix_size     = aligned_addr - raw_addr;
    suffix_size     = nbytes_req - nbytes - prefix_size;

    if (prefix_size > 0)
    {
        munmap((void*)raw_addr, prefix_size);
    }
    
    if (suffix_size > 0)
    {
        munmap((void*)(aligned_addr + nbytes), suffix_size);
    }

    return (void*)aligned_addr;
}

/**
 * @brief   releases raw virtual memory back to the os 
 * 
 * unmaps a previously mapped memory region. capacity passed must match the original requested 
 * capacity used during allocation via `sys_alloc`
 *          
 * @param   ptr     pointer to the start of the mapped memory region
 * @param   nbytes  exact nbytes originally requested via `sys_alloc` or `sys_aligned_alloc`
 * 
 * @warning can set errno
 */
static inline int
sys_unmap(
    void   *ptr, 
    size_t  nbytes
){
    if (!ptr)
    {
        return -1;
    }
    return munmap(ptr, nbytes);
}


#endif  //OS_H