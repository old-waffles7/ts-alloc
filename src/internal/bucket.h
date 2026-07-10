
#pragma once
#ifndef BUCKET_H
#define BUCKET_H


#include    "common.h"

#include    "genheap.h"


struct span;
typedef struct span span_t;


typedef heap_coord(span_t)  bucket_coord;
typedef heap(span_t)        bucket_t;


inline span_t*
bucket_pop(
    bucket_t   *heap
);

inline void
bucket_insert(
    bucket_t   *heap,
    span_t     *node
);

inline void
bucket_remove(
    bucket_t   *heap,
    span_t     *node
);


#endif  //BUCKET_H