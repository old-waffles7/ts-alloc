
#pragma once
#ifndef SPAN_H
#define SPAN_H


#include    "common.h"

#include    "bucket.h"
#include    "objpool.h"
#include    "registry.h"


struct slab
{
    uint16_t    nbytes_block;
    uint16_t    nblocks_free;
    byte_t     *bitmap;
};
typedef struct slab slab_t;


struct span
{
    struct 
    {
        uint64_t    age         : 16;   // max 65535
        uint64_t    szclass     : 16;   // max 65535
        uint64_t    arena       : 12;   // max 4096
        uint64_t    state       : 2;    // 0 clean -> 1 dirty -> 2 may not need -> 3 do not need
        uint64_t    is_slab     : 1;    // bool
        uint64_t    is_dumpable : 1;    // bool
    } flags;

    struct
    {
        bucket_coord_t      bucket;
        registry_coord_t    registry;
    } coord;
    
    byte_t *addr;
    slab_t *slab_metadata;
    size_t  nbytes;
};
typedef struct span span_t;


#endif  //SPAN_H