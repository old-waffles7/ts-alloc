
#pragma once
#ifndef GLOB_H
#define GLOB_H


#include    "common.h"
#include    "error.h"

#include    "os.h"
#include    "arena.h"
#include    "arenaconfig.h"


/*
struct global_stats
{
    size_t *ncached_spans;          // need to add nelements to genheap?
    size_t *ncached_blocks;         // add nblocks to stats struct for pail_t
    size_t *nallocs_spans;          // lifetime
    size_t *nallocs_blocks;         // lifetime
    size_t  nbytes_alloc_lftime;
    size_t  nbytes_freed_lftime;
    size_t  nbytes_alloc_c_epoch;
    size_t  nbytes_freed_c_epoch;
    size_t  nbytes_infrastructure;
};
typedef struct global_stats global_stats_t
*/

struct global_arena
{
    arena_t    *loc_arenas;

    struct
    {
        size_t  free;
        size_t  alloc;
    } epoch;

    tsalloc_errctx_t    error_ctx;
    arena_cfg_t         cfg;
    uint16_t            narenas;
    uint16_t            arena_idx;
};
typedef struct global_arena glob_arena_t;

static tsalloc_err_t
glob_create(
    glob_arena_t  **dest,
    arena_cfg_t     cfg,
    uint16_t        narenas
){
    if (cfg.auxil_map == TSALLOC_DEFAULT_ARG)
    {
        const tsalloc_cfg_t    *tsalloc_cfg;

        tsalloc_cfg = tsconfig_get_cfg(sys_page_size());
        if (!tsalloc_cfg)
        {
            return TSALLOC_INITIALIZE_FAILURE;
        }

        tsalloc_szreq_t req;

        req = tsconfig_get_szclass(tsalloc_cfg, (1024 * 1024 * 2)); // 2 MiB
        if (req.isslab < 0)
        {
            return TSALLOC_INITIALIZE_FAILURE;
        }

        cfg = (arena_cfg_t){
            .auxil_map                  = &def_auxil_map,
            .auxil_unmap                = &def_auxil_unmap,
            .auxil_madvise              = &def_auxil_madvise,
            .tsalloc_cfg                = tsalloc_cfg,
            .default_new_span_szclass   = req.szclass,
            .auxil_align                = 16,
            .unmap_on_termination       = false,
            .allow_cross_origin_merge   = true
        };
    }

    byte_t *raw_mem;
    size_t  nbytes_req;
    size_t  nbytes_loc_arena_auxil_mem;

    nbytes_loc_arena_auxil_mem  = arena_auxil_mem_size(&cfg);
    nbytes_req                  = sizeof(glob_arena_t) + narenas * (sizeof(arena_t) + nbytes_loc_arena_auxil_mem);
    raw_mem                     = (byte_t*)sys_map(nbytes_req);
    if (raw_mem == nullptr)
    {
        return TSALLOC_INITIALIZE_FAILURE;
    }

    glob_arena_t   *arena;
    arena_t        *loc_arena;
    byte_t         *auxil_mem;
    byte_t         *arenas_addr;
    byte_t         *auxil_mems_addr;
    tsalloc_err_t   ret;

    arena           = (glob_arena_t*)raw_mem;
    arenas_addr     = raw_mem + sizeof(glob_arena_t);
    auxil_mems_addr = arenas_addr + narenas * sizeof(arena_t);
    for (int i = 0; i < narenas; i++)
    {
        loc_arena   = ((arena_t*)arenas_addr) + i;
        auxil_mem   = auxil_mems_addr + nbytes_loc_arena_auxil_mem;
        
        ret = arena_init(
            nullptr, 
            &cfg, 
            auxil_mem, 
            loc_arena
        );
        if (ret != TSALLOC_SUCCESS)
        {
            for (int j = 0; j < i; j++)
            {
                (void)arena_deinit(nullptr, ((arena_t*)arenas_addr) + j);
            }
            return TSALLOC_INITIALIZE_FAILURE;
        }
    }

    *arena  = (glob_arena_t){
        .loc_arenas = (arena_t*)arenas_addr,
        .cfg        = cfg,
        .narenas    = narenas
    };
    *dest   = arena;

    return TSALLOC_SUCCESS;
}

static inline tsalloc_err_t
glob_destroy(
    glob_arena_t   *arena
){
    arena_t        *loc_arena;
    tsalloc_err_t   ret1, ret2;
    size_t          narenas;

    ret2    = TSALLOC_SUCCESS;
    narenas = arena->narenas;
    for (int i = 0; i < narenas; i++)
    {
        ret1    = arena_deinit(nullptr, &(arena->loc_arenas[i]));
        if (ret1 != TSALLOC_SUCCESS)
        {
            ret2    = ret1;
        }
    }

    size_t  nbytes_req;

    nbytes_req  = sizeof(glob_arena_t) + narenas * (sizeof(arena_t) + arena_auxil_mem_size(&(arena->cfg)));
    sys_unmap(((void*)arena), nbytes_req);

    return ret2;
}

static inline arena_t*
glob_claim(
    glob_arena_t   *arena
){
    arena_t    *loc_arena;

    arena->arena_idx++;
    if (arena->arena_idx >= arena->narenas)
    {
        arena->arena_idx    = 0;
    }
    loc_arena   = arena->loc_arenas + arena->arena_idx;
    arena_claim(loc_arena);

    return loc_arena;
}


#endif  //GLOB_H