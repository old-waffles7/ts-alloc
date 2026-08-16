# tsalloc System Requirements

## 1. C Standard Requirements
*   **C23:**
    *   Typed Enums. *(Supported as an extension in GCC/Clang under C11)*
*   **C11:**
    *   Atomics (`<stdatomic.h>`, `_Atomic`).
    *   Thread-Local Storage (`_Thread_local`).
    *   Anonymous Structs and Unions.
*   **C99:**
    *   Designated Initializers.
    *   Inline Functions (`static inline`).
    *   Fixed-Width Types (`<stdint.h>`, `<stdbool.h>`).

## 2. Operating System Requirements
*   **POSIX Compliance:**
    *   Virtual memory mapping (`mmap`, `munmap`, `posix_madvise`, `madvise`).
    *   System configuration (`sysconf`).
    *   Threading and synchronization (`pthread_key_create`, `sem_init`, `sem_wait`).
*   **Supported Platforms:**
    *   **Primary:** Linux.
    *   **Secondary:** macOS, FreeBSD, OpenBSD, NetBSD.
    *   **Tertiary** Other Unix-like systems that expose POSIX APIs.
    *   **Unsupported:** Windows (Requires WSL, MSYS2, or Cygwin).

## 3. Hardware Requirements
*   **Page Sizes:**
    *   Minimum: 4 KiB.
    *   Maximum Allocation: 4.6 ExiB.
    *   Default Configurations: 4 KiB, 16 KiB, 32 KiB, 64 KiB, 2 MiB.
    *   Custom Configurations: Positive powers of 2.

## 4. Build Dependencies
*   **Python:** 3.x (Required for `config.py`).
*   **CMake:** 3.20 or newer.