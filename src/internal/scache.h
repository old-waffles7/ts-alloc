
#pragma once
#ifndef SCACHE_H
#define SCACHE_H


#include    "common.h"
#include    "error.h"

#include    "bin.h"
#include    "span.h"
#include    "mutex.h"
#include    "pagetrie.h"


struct span_cache
{
    bin_t      *bins;
    byte_t     *bitmap;
    size_t      nclasses;
    mutex_t     lock;
};
typedef struct span_cache   scache_t;

static inline size_t
scache_auxil_mem_size(
    arena_conf_t   *arena_cfg
){
    size_t  nbytes;
    size_t  nclasses;

    nclasses    = (arena_cfg->tsalloc_cfg->nszclasses);
    nbytes      = sizeof(bin_t) * nclasses;
    nbytes     += ((nclasses + 63) / 64) * 8;

    return nbytes;
}


#endif  //SCACHE_H