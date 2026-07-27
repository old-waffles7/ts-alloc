
#pragma once
#ifndef PAIL_H
#define PAIL_H


#include    "common.h"
#include    "error.h"

#include    "span.h"
#include    "mutex.h"
#include    "scache.h"
#include    "objpool.h"
#include    "registry.h"
#include    "arenaconfig.h"


struct pail_stats
{
    size_t  nslabs_nempty;
    size_t  nslabs_empty;
};
typedef struct pail_stats   pail_stats_t;


struct pail
{
    #ifdef  OPT_TRACK_STATS
        pail_stats_t    stats;
    #endif  //OPT_TRACK_STATS

    const tsalloc_slab_info_t  *init_info;
    registry_t          slabs;
    objpool_t           slabpool;
    mutex_t             lock;
    tsalloc_szclass_t   szclass;
};
typedef struct pail pail_t;

// update functionalities for new abstractions like the spanpool being moved, no more direct
// interface with pagetrie
static inline tsalloc_err_t
_pail_get_block(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    scache_t           *scache,
    pail_t             *pail,
    byte_t            **dest
){
    span_t *slab;

    if (!pail->slabs.head)
    {
        tsalloc_err_t   ret;

        ret = scache_get_span(
            error_ctx, 
            arena_cfg, 
            scache, 
            &slab, 
            pail->szclass
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        ret = slab_init(
            arena_cfg->tsalloc_cfg, 
            pail->init_info, 
            error_ctx, 
            slabpool, 
            slab
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(error_ctx);
            return ret;
        }

        registry_push(&(pail->slabs), slab);
        // insert into pagetrie
    }
    else 
    {
        slab    = pail->slabs.head;
    }

    byte_t *block;

    block   = slab_get_block(slab);
    if (slab->slab_metadata->nblocks_free == 0)
    {
        registry_pop(&(pail->slabs));
    }

    *dest   = block;

    return TSALLOC_SUCCESS;
}


static inline tsalloc_err_t
pail_init(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    pail_t             *pail,
    tsalloc_szclass_t   szclass
){
    *pail   = (pail_t){0};

    if (szclass >= arena_cfg->tsalloc_cfg->nszclasses_slab)
    {
        set_tsalloc_error(
            error_ctx,
            "pail_init::pail.h invalide szclass argument",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    tsalloc_err_t   ret;

    ret = mutex_init(error_ctx, &(pail->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    pail->init_info = tsalloc_get_slabinfo(arena_cfg->tsalloc_cfg, szclass);
    pail->szclass   = szclass;

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
pail_deinit(
    tsalloc_errctx_t   *error_ctx,
    pail_t             *pail
){
    tsalloc_err_t   ret;

    ret = mutex_deinit(error_ctx, &(pail->lock));
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(error_ctx);
        return ret;
    }

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
pail_get_batch(
    tsalloc_errctx_t   *error_ctx,
    arena_conf_t       *arena_cfg,
    objpool_t          *spanpool,
    objpool_t          *slabpool,
    scache_t           *scache,
    pail_t             *pail,
    byte_t**            dest,
    size_t              nblocks
){

}


#endif  //PAIL_H