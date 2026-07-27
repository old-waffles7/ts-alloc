

#pragma once
#ifndef BCACHE_H
#define BCACHE_H


#include    "common.h"
#include    "error.h"

#include    "bin.h"
#include    "span.h"
#include    "mutex.h"
#include    "registry.h"
#include    "arenaconfig.h"


struct block_cache
{
    registry_t *bins;
    mutex_t    *locks;
    size_t      nclasses;
};
typedef struct block_cache  bcache_t;

static inline size_t
bcache_auxil_mem_size(
    arena_conf_t   *arena_cfg
){
    size_t  nbytes;
    size_t  nclasses;

    nclasses    = arena_cfg->tsalloc_cfg->nszclasses_slab;
    nbytes      = sizeof(registry_t) * nclasses;
    nbytes     += sizeof(mutex_t) * nclasses;

    return nbytes;
}


#endif  //BCACHE_H