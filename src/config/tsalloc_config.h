
#pragma once
#ifndef TSALLOC_CONFIG_H
#define TSALLOC_CONFIG_H


#include    "../internal/common.h"
#include    "_tsalloc_config.h"


typedef uint16_t    tsalloc_szclass_t;


/*
    check if allocation is slab or span
*/
static inline bool
tsalloc_is_slab_alloc(
    const tsalloc_config_t *cfg,
    size_t  nbytes
){
    return nbytes <= (cfg->slab_alloc_max);
}

/*
    get size-class of nbytes
*/
static inline tsalloc_szclass_t
tsalloc_get_szclass(
    const tsalloc_config_t *cfg,
    size_t  nbytes
){
    //  slab
    if (nbytes <= (cfg->slab_alloc_max))
    {
        uint64_t    idx = (nbytes + cfg->min_align - 1) >> (cfg->min_align_shift);
        return (cfg->sz_class_of_nbytes[(idx == 0)? 0 : (idx - 1)]);
    }

    //span
    size_t  req_nbytes;
    size_t  base_shift;
    size_t  epoch;
    size_t  epoch_base_nbytes;
    size_t  offset;

    req_nbytes          = nbytes - 1;
    base_shift          = (cfg->min_align_shift) + (cfg->epoch_shift);
    epoch               = 63 - __builtin_clzll((req_nbytes >> base_shift) + 1);
    epoch_base_nbytes   = (1ULL << base_shift) * ((1ULL << epoch) - 1);
    offset              = (req_nbytes - epoch_base_nbytes) >> ((cfg->min_align_shift) + epoch);

    return (epoch << (cfg->epoch_shift)) + offset;
}

/*
    get slab_info struct for a slab size-class. content: {block-size, slab-size, nblocks}
*/
static inline const tsalloc_slab_info_t*
tsalloc_get_slabinfo(
    const tsalloc_config_t *cfg,
    tsalloc_szclass_t   szclass
){
    if (szclass >= (cfg->nszclasses_slab))
    {
        return nullptr;
    }

    return &(cfg->slab_infos[szclass]);
}

/*
    get size of span/slab
*/
static inline size_t
tsalloc_szclass_span_size(
    const tsalloc_config_t *cfg,
    tsalloc_szclass_t   szclass
){
    if (szclass < (cfg->nszclasses_slab))
    {
        return (cfg->slab_infos[szclass].slab_size);
    }

    return (cfg->sz_class_max_nbytes[szclass]);
}


#endif  //TSALLOC_CONFIG_H