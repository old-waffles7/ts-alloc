
/**
 * @file    common.h
 * @brief   definitions of commonly used functionalities
 * 
 * provides the interface accessing specifc os-functionalities via simplified wrapper-functions
 */


#pragma once
#ifndef COMMON_H
#define COMMON_H


#define     _POSIX_C_SOURCE 200112L
#if defined(__linux__)
    #ifndef _GNU_SOURCE
        #define     _GNU_SOURCE
    #endif  //_GNU_SOURCE
#elif defined(__APPLE__)
    #ifndef _DARWIN_C_SOURCE
        #define     _DARWIN_C_SOURCE
    #endif  //_DARWIN_C_SOURCE
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #ifndef _BSD_SOURCE
        #define     _BSD_SOURCE
    #endif  //_BSD_SOURCE
#endif  //OS


#define     ALIGN_UP(x, align)      ((((uintptr_t)(x)) + ((uintptr_t)(align) - 1)) & ~((uintptr_t)(align) - 1))
#define     ALIGN_DOWN(x, align)    (((uintptr_t)(x)) & ~((uintptr_t)(align) - 1))     
#define     IS_POWER_OF_TWO(x)      (((x) != 0) && (((x) & ((x) - 1)) == 0))
#define     MIN(x,y)                (((x) > (y))? (y):(x))
#define     MAX(x,y)                (((x) > (y))? (x):(y))


#include    <stdbool.h>
#include    <stddef.h>
#include    <stdint.h>
#include    <unistd.h>
typedef uint8_t byte_t;

#include    <float.h>
typedef float   float32_t;
typedef double  float64_t;
#if defined(FLT16_MAX) || defined(__FLT16_MAX__)
    typedef _Float16    float16_t;
#else
    typedef float       float16_t;
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#else
    #define nullptr NULL
#endif


#endif  //COMMON_H