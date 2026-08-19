/*
 * @file    tsalloc_config.h
 * @brief   definitions and inline routines for parsing and accessing allocator configurations
 */

#pragma once
#ifndef TSALLOC_CONFIG_H
#define TSALLOC_CONFIG_H


#include    "../internal/common.h"
#include    "_tsalloc_config.h"


#define     TSALLOC_ALLOC_MAX   ((1ULL << 62))  // 4.6 ExiB


typedef int32_t         ts_szclass_t;
typedef tsalloc_cfg_t   glob_alloc_state_t;


/*
 * @struct  tsalloc_szclass_request
 * @brief   encapsulates the parsed size class index and its allocation type (slab vs span)
 */
struct tsalloc_szclass_request
{
    ts_szclass_t    szclass;    ///< the computed size class index
    int8_t          isslab;     ///< flag indicating if the size class belongs to a slab (1), span (0), or error (-1)
};
typedef struct tsalloc_szclass_request  tsalloc_szreq_t;


/*
 * @brief   determines if a requested allocation size falls within the slab limit
 *
 * @param   cfg     pointer to the allocator configuration structure
 * @param   nbytes  the requested allocation size in bytes
 *
 * @return  true if the size is managed by slabs, false otherwise
 */
static inline bool
tsconfig_isslab_alloc(
    const tsalloc_cfg_t    *cfg,
    size_t  nbytes
){
    return nbytes <= cfg->slab_alloc_max;
}

/*
 * @brief   calculates the size class index and type for a given byte size
 *
 * @param   cfg     pointer to the allocator configuration structure
 * @param   nbytes  the requested allocation size in bytes
 *
 * @return  a request struct containing the size class index and allocation type
 */
static inline tsalloc_szreq_t 
tsconfig_get_szclass(
    const tsalloc_cfg_t    *cfg,
    size_t                  nbytes
){
    tsalloc_szreq_t req;

    req = (tsalloc_szreq_t){
        .isslab = (-1)
    };

    if (nbytes == 0 || (nbytes > TSALLOC_ALLOC_MAX))
    {
        return req;
    }

    if (tsconfig_isslab_alloc(cfg, nbytes))
    {
        uint64_t    idx;
        idx         = (nbytes + cfg->min_align - 1) >> (cfg->min_align_shift);
        req.szclass = (cfg->szclass_of_nbytes_slab[(idx == 0)? 0 : (idx - 1)]);
        req.isslab  = true;
        return req;
    }

    size_t  req_nbytes;
    size_t  page_shift;
    size_t  grp;

    req_nbytes  = nbytes - 1;
    page_shift  = (size_t)__builtin_ctzll(cfg->page_size);
    grp         = req_nbytes >> page_shift;

    if (grp < cfg->steps_per_pow2)
    {
        req.szclass = grp;
    }
    else
    {
        size_t b                = 63 - __builtin_clzll(grp);
        size_t epoch            = b - cfg->steps_per_pow2_shift + 1;
        size_t epoch_base_pages = 1ULL << (epoch + cfg->steps_per_pow2_shift - 1);
        size_t step             = 1ULL << (epoch - 1);
        size_t offset           = (grp - epoch_base_pages) / step;
        
        req.szclass = (epoch << cfg->steps_per_pow2_shift) + offset;
    }
    req.isslab  = false;
    
    return req;
}

static inline ts_szclass_t 
tsconfig_get_span_szclass(
    const glob_alloc_state_t   *glob_state,
    size_t                      nbytes
){
    if ((nbytes == 0 )|| (nbytes > TSALLOC_ALLOC_MAX))
    {
        return (ts_szclass_t)(-1);
    }

    size_t  req_nbytes;
    size_t  page_shift;
    size_t  grp;

    req_nbytes  = nbytes - 1;
    page_shift  = (size_t)__builtin_ctzll(glob_state->page_size);
    grp         = req_nbytes >> page_shift;

    if (grp < glob_state->steps_per_pow2)
    {
        return (ts_szclass_t)grp;
    }

    size_t b                = 63 - __builtin_clzll(grp);
    size_t epoch            = b - glob_state->steps_per_pow2_shift + 1;
    size_t epoch_base_pages = 1ULL << (epoch + glob_state->steps_per_pow2_shift - 1);
    size_t step             = 1ULL << (epoch - 1);
    size_t offset           = (grp - epoch_base_pages) / step;

    return (ts_szclass_t)((epoch << glob_state->steps_per_pow2_shift) + offset);
}

/*
 * @brief   retrieves the slab metadata associated with a specific slab size class
 *
 * @param   cfg      pointer to the allocator configuration structure
 * @param   szclass  the size class index to query
 *
 * @return  pointer to the slab info structure, or nullptr if out of bounds
 */
static inline const tsalloc_slab_info_t*
tsconfig_get_slabinfo(
    const tsalloc_cfg_t    *cfg,
    ts_szclass_t            szclass
){
    if (szclass >= (cfg->nszclasses_slab))
    {
        return nullptr;
    }

    return &(cfg->slab_infos[szclass]);
}

/*
 * @brief   retrieves the maximum byte size bounded by a given size class
 *
 * @param   cfg      pointer to the allocator configuration structure
 * @param   szclass  the size class index to query
 * @param   isslab   boolean flag indicating if the class is a slab (true) or span (false)
 *
 * @return  the maximum bytes for the class, or -1 on error
 */
static inline ssize_t
tsconfig_get_nbytes_szclass(
    const tsalloc_cfg_t    *cfg,
    ts_szclass_t            szclass,
    bool                    isslab
){
    if (isslab)
    {
        if (szclass < 0 || (szclass >= cfg->nszclasses_slab))
        {
            return -1;
        }

        return cfg->szclass_max_nbytes_slab[szclass];
    }

    if (szclass >= cfg->nszclasses_span)
    {
        return (-1);
    }

    return cfg->szclass_max_nbytes_span[szclass];
}

static inline ssize_t
tsconfig_get_nbytes_span_szclass(
    const tsalloc_cfg_t    *cfg,
    ts_szclass_t            szclass,
    bool                    isslab
){
    if (isslab)
    {
        if (szclass >= cfg->nszclasses_slab)
        {
            return (-1);
        }

        return cfg->slab_infos[szclass].slab_size;
    }

    if (szclass < 0 || (szclass >= cfg->nszclasses_span)) {
        return -1;
    }

    return cfg->szclass_max_nbytes_span[szclass];
}

static inline const tsalloc_cfg_t*
tsconfig_get_cfg(
    size_t  pagesize
);


#endif  //TSALLOC_CONFIG_H