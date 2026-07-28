

#pragma once
#ifndef BCACHE_H
#define BCACHE_H


#include    "common.h"
#include    "error.h"

#include    "span.h"
#include    "pail.h"
#include    "arenaconfig.h"


struct block_cache
{
    pail_t     *pails;
    size_t      nclasses;
};
typedef struct block_cache  bcache_t;

static inline size_t
bcache_auxil_mem_size(
    arena_cfg_t        *arena_cfg
){
    size_t  nbytes;
    size_t  nclasses;

    nclasses    = arena_cfg->tsalloc_cfg->nszclasses_slab;
    nbytes      = sizeof(pail_t) * nclasses;

    return nbytes;
}


#endif  //BCACHE_H