cmake_minimum_required(VERSION 3.20)
project(tsalloc VERSION 0.7 LANGUAGES C)
set(CMAKE_C_STANDARD 23)    # 11 without specified-width enums, true float16


#   option(s)
option(VERBOSE_TRACE "Enable error stack breadcrumbs" ON)
option(VERBOSE_STATS "Enable tracking of stats regarding memory-management" ON)
option(ERROR_LOG "Enable error logging, saves state of error-context to specified file on error" OFF)

#   conditionally enable more verbose error-reporting (stack breadcrumbs), slower on error
if (VERBOSE_TRACE)
    target_compile_definitions(tsalloc PRIVATE OPT_TRACE_ERRORS)
endif()
#   conditionally enable tracking of memory-management stats
if (VERBOSE_STATS)
    target_compile_definitions(tsalloc PRIVATE OPT_TRACK_STATS)