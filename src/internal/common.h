/**
 * @file    common.h
 * @brief   definitions of commonly used functionalities
 * 
 * provides the interface accessing specifc os-functionalities via simplified wrapper-functions
 */


#pragma once
#ifndef COMMON_H
#define COMMON_H


#include    <assert.h>

//  remove
#include    "opt.h"


#define     ALIGN_UP(x, align)      (((x) + ((align) - 1)) & ~((align) - 1))
#define     ALIGN_DOWN(x, align)    ((x) & ~((align) - 1))
#define     IS_POWER_OF_TWO(x)      (((x) > 0) && (((x) & ((x) - 1)) == 0))
#define     MIN(x,y)                (((x) > (y))? (y):(x))
#define     MAX(x,y)                (((x) > (y))? (x):(y))


#include    <stdbool.h>
#include    <stddef.h>
#include    <stdint.h>
typedef int8_t  byte_t;

#include    <float.h>
typedef float   float32_t;
typedef double  float32_t;
#if defined(FLT16_MAX) || defined(__FLT16_MAX__)
    typedef _Float16    float16_t;
#else
    typedef float       float16_t
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#else
    #define nullptr NULL
#endif


#endif  //COMMON_H