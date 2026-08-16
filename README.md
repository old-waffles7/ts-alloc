# ts-alloc (tsalloc)

`tsalloc` is a thread safe memory allocator written in C23 (portable to C11 with certain compilers, such as GCC or Clang, or by removing explicit enum widths). `tsalloc` is implemented using 2 tiers of memory caching. The first, and quickest, are the thread-local memory caches. These are used for small allocations (e.g up to 16 KiB allocations using the default profile for 4 KiB page systems). The second layer is comprised of a collection of much larger caches, i.e thread-safe memory arenas. These are used for bulk memory management.

## Features
*   **Hierarchical Allocation:** Thread-local memory caches (`tcache_t`) for block allocations and global arenas (`arena_t`) for large contiguous spans.
*   **Lock-Free Thread Caching:** Rapid allocations bypass global locks completely.
*   **Pagetrie Tracking:** Allocation metadata is not placed directly before a requested memory block. Instead, a thread-safe, 3-level radix tree provides O(1) metadata lookups for virtual pages.
*   **Adaptive Mutexes:** Custom POSIX-semaphore backed mutexes with adaptive spinning.
*   **Pluggable Backend:** Inject custom `mmap`, `munmap`, and `madvise` implementations via `tsarena_cfg_t`. For example, a `tsalloctr_t` instance may be configured to be a thread-safe VRAM memory allocator.
*   **Dynamic Configuration:** Python-based configuration script to add support for additional pagesizes.

## Requirements
See [REQUIREMENTS.md](REQUIREMENTS.md) for full details on C standard usage, POSIX dependencies, and supported platforms.

## Build Instructions
The project uses CMake and requires a C compiler supporting C11/C23 extensions.

```bash
    git clone https://github.com/old-waffles7/ts-alloc
    cd tsalloc
    python3 src/config/config.py -h
    python3 src/config/config.py --default
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DVERBOSE_TRACE=[ON/OFF]
    cmake --build .
```

##  License
See [LICENSE](LICENSE).