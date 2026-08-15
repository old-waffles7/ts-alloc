cmake_minimum_required(VERSION 3.20)
project(tsalloc VERSION 0.7 LANGUAGES C)
set(CMAKE_C_STANDARD 23)    # 11 without specified-width enums

#   option(s)
option(VERBOSE_TRACE "Enable error stack breadcrumbs" ON)
#option(VERBOSE_STATS "Enable tracking of stats regarding memory-management" ON)

# Define the target library and compile sources
add_library(tsalloc SHARED
    src/arena.c
    src/bucket.c
    src/glob.c
    src/ledger.c
    src/objpool.c
    src/pagetrie.c
    src/records.c
    src/registry.c
    src/scache.c
    src/slab.c
    src/span.c
    src/tsalloc.c
)

# Link required POSIX thread and semaphore dependencies
find_package(Threads REQUIRED)
target_link_libraries(tsalloc PRIVATE Threads::Threads)

#   conditionally enable more verbose error-reporting (stack breadcrumbs), slower on error
if (VERBOSE_TRACE)
    target_compile_definitions(tsalloc PRIVATE OPT_TRACE_ERRORS)
endif()

#   conditionally enable tracking of memory-management stats
#if (VERBOSE_STATS)
#    target_compile_definitions(tsalloc PRIVATE OPT_TRACK_STATS)
#endif()