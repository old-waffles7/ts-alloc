
#pragma once
#ifndef SPAN_H
#define SPAN_H


#include    "common.h"

#include    "bucket.h"
#include    "objpool.h"
#include    "registry.h"


struct slab
{
    uint64_t    bitmap;
};
typedef struct slab slab_t;


struct span
{
    struct
    {
        uint64_t    age         : 32;
        uint64_t    arena_id    : 12;   ///< maximum 4096 arenas
        uint64_t    sclass      : 8;    // init by span_create
        uint64_t    state       : 2;    // initially clean
        uint64_t    is_slab     : 1;    // initially by slab_create
        uint64_t    is_dumpable : 1;    // add to auxil_madvise, initially by config
    } flag;

    struct
    {
        bucket_coord_t      bucket;
        registry_coord_t    registry;
    } coord;
    
    void   *addr;
    slab_t  slab_mdata;
    size_t  nbytes;
};
typedef struct span span_t;


#endif  //SPAN_H