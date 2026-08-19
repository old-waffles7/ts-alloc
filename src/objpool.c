
#include    "internal/common.h"
#include    "internal/error.h"
#include    "internal/objpool.h"

#include    "internal/os.h"


#define     DEFAULT_ALIGN           8
#define     DEFAULT_NOBJS_CHUNK     256


struct chunk
{
    struct chunk   *prev;
    uint64_t        nbytes_free;
};

typedef struct chunk    chunk_t;

static ts_err_t
chunk_create(
    chunk_t   **dest,
    size_t      align,
    size_t      nbytes,
    int32_t     glob_uid
){
    void   *raw;

    if (align <= sys_page_size())
    {
        raw = sys_map(nbytes);
    }
    else
    {
        raw = sys_aligned_map(align, nbytes);
    }

    if (!raw)
    {
        set_tsalloc_error
        (
            glob_uid,
            "chunk_create()::objpool.c os allocation error",
            TSALLOC_OS_ERR
        );
        return TSALLOC_OS_ERR;
    }

    chunk_t    *chunk;

    chunk   = ((chunk_t*)raw);
    *chunk  = (chunk_t)
    {
        nullptr,
        nbytes - (ALIGN_UP(sizeof(chunk_t), align))
    };

    *dest   = chunk;
    
    return TSALLOC_SUCCESS;
}

static void
chunk_destroy(
    chunk_t    *chunk,
    size_t      nbytes
){
    if (!chunk)
    {
        return;
    }
    
    sys_unmap(chunk, nbytes);
}


typedef struct iobjpool_block   slab_t;


ts_err_t
objpool_init(
    objpool_t   *objpool,
    size_t       align,
    size_t       nbytes_obj,
    size_t       nobjs_chunk,
    int32_t      glob_uid
){
    if (!objpool)
    {
        set_tsalloc_error
        (
            glob_uid,
            "objpool_init::objpool.c invalid argument: nullptr objpool",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

    align       = align? align : DEFAULT_ALIGN;
    nobjs_chunk = nobjs_chunk? nobjs_chunk : DEFAULT_NOBJS_CHUNK;
    
    if (!IS_POWER_OF_TWO(align))
    {
        set_tsalloc_error
        (
            glob_uid,
            "objpool_init::objpool.c invalid argument: align must be power of 2",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }
    
    size_t  nbytes_slab;
    size_t  nbytes_chunk;
    size_t  chunk_desc_offset;

    nbytes_slab         = ALIGN_UP(MAX(nbytes_obj, sizeof(slab_t)), align);
    chunk_desc_offset   = ALIGN_UP(sizeof(chunk_t), align);
    nbytes_chunk        = chunk_desc_offset + nbytes_slab * nobjs_chunk;

    if (nobjs_chunk > ((SIZE_MAX - chunk_desc_offset) / nbytes_slab))
    {
        set_tsalloc_error(
            glob_uid,
            "objpool_init::objpool.c chunk size overflow",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }


    if(align <= sys_page_size())
    {
        nbytes_chunk    = ALIGN_UP(nbytes_chunk, sys_page_size());
    }
    else 
    {
        nbytes_chunk    = ALIGN_UP(nbytes_chunk, align);
    }

    *objpool    = (objpool_t)
    {
        nullptr,
        nullptr,
        align,
        nbytes_slab,
        nbytes_chunk
    };

    return TSALLOC_SUCCESS;
}

void 
objpool_deinit(
    objpool_t  *objpool
){
    if (!objpool)
    {
        return;
    }

    chunk_t    *curr;
    chunk_t    *prev;

    curr    = objpool->chunk_stack;
    while (curr)
    {
        prev    = curr->prev;
        chunk_destroy(curr, objpool->nbytes_chunk);
        curr    = prev;
    }
}

ts_err_t
objpool_alloc(
    objpool_t   *objpool,
    void       **dest,
    int32_t      glob_uid
){
    if (objpool->slab_stack)
    {
        *dest               = objpool->slab_stack;
        objpool->slab_stack = ((slab_t*)objpool->slab_stack)->prev;

        return TSALLOC_SUCCESS;
    }

    chunk_t    *chunk;

    chunk   = ((chunk_t*)objpool->chunk_stack);
    if ((!chunk) || (chunk->nbytes_free < objpool->nbytes_slab))
    {
        ts_err_t   ret;
        ret = chunk_create
        (
            &chunk,
            objpool->align,
            objpool->nbytes_chunk,
            glob_uid
        );
        if (ret != TSALLOC_SUCCESS)
        {
            append_tsalloc_error_trace(glob_uid);
            return ret;
        }

        chunk->prev             = ((chunk_t*)objpool->chunk_stack);
        objpool->chunk_stack    = ((void*)chunk);
    }

    void   *mem;
    byte_t *raw;

    raw = ((byte_t*)chunk);
    mem = raw + (ALIGN_UP(sizeof(chunk_t), objpool->align)) + chunk->nbytes_free - objpool->nbytes_slab;
    
    chunk->nbytes_free -= objpool->nbytes_slab;

    *dest   = mem;

    return TSALLOC_SUCCESS;
}