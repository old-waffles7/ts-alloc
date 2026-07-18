
#pragma once
#ifndef BUCKET_H
#define BUCKET_H


#include    "common.h"

#include    "genheap.h"


typedef struct span span_t;

//  forward declaration suffices for structure definitions
gen_heap_struct(bucket, span_t)
typedef heap(bucket)        bucket_t;
typedef heap_coord(bucket)  bucket_coord_t;

span_t*
bucket_pop(
    bucket_t   *bucket
);

void
bucket_insert(
    bucket_t   *bucket,
    span_t     *span
);

void
bucket_remove(
    bucket_t   *bucket,
    span_t     *span
);


#endif  //BUCKET_H