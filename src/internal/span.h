
#pragma once
#ifndef SPAN_H
#define SPAN_H


#include    "common.h"

#include    "bucket.h"
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
        uint64_t    sclass      : 8;
        uint64_t    state       : 2;
        uint64_t    is_slab     : 1;
        uint64_t    is_dumpable : 1;
    } flags;

    union
    {
        bucket_coord_t      bucket;
        registry_coord_t    registry;
    } coord;
    
    void   *addr;
    slab_t  slab_mdata;
    size_t  nbytes;
};


#endif  //SPAN_H