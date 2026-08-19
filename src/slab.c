
#include    "internal/common.h"
#include    "internal/error.h"

#include    "internal/span.h"
#include    "internal/objpool.h"

#include    <string.h>


ts_err_t 
slab_init(
    const tsalloc_slab_info_t  *slabinfo,
    objpool_t                  *slabpool,
    span_t                     *span,
    int32_t                     glob_uid
){
    slab_t     *slabmeta;
    ts_err_t    ret;

    ret = objpool_alloc(slabpool, ((void*)(&slabmeta)), glob_uid);
    if (ret != TSALLOC_SUCCESS)
    {
        append_tsalloc_error_trace(glob_uid);
        return ret;
    }

    byte_t     *bitmap;
    uint64_t   *words;
    uint32_t    nwords;
    uint32_t    remainder;

    bitmap  = ((byte_t*)slabmeta) + sizeof(slab_t);
    memset(bitmap, 0, slabpool->nbytes_slab - sizeof(slab_t));

    words   = (uint64_t*)bitmap;
    nwords  = (slabinfo->nblocks + 63) / 64;
    for (uint32_t i = 0; i < nwords - 1; i++) 
    {
        words[i] = ~0ULL;
    }
    remainder   = slabinfo->nblocks % 64;
    if (remainder == 0) 
    {
        words[nwords - 1]   = ~0ULL;
    } 
    else 
    {
        words[nwords - 1]   = (1ULL << remainder) - 1;
    }

    *slabmeta = (slab_t){
        .bitmap         = bitmap,
        .szclass        = slabinfo->szclass,
        .nbytes_block   = slabinfo->block_size,
        .nblocks_free   = slabinfo->nblocks
    };
    span->flags.is_slab = true;
    span->slabmeta      = slabmeta;

    return TSALLOC_SUCCESS;
}

void 
slab_deinit(
    objpool_t  *slabpool,
    span_t     *span
){
    span->flags.is_slab = false;
    objpool_free(slabpool, ((void*)(span->slabmeta)));
}

byte_t* 
slab_get_block(
    span_t *span
){
    if (span->slabmeta->nblocks_free == 0)
    {
        return nullptr;
    }

    slab_t     *slabmeta;
    byte_t     *base_mem;
    uint64_t   *bitmap;
    uint64_t    word_idx;
    uint64_t    word;
    uint64_t    bit_idx;
    uint64_t    block_idx;

    slabmeta    = span->slabmeta;
    base_mem    = span->addr;
    bitmap      = (uint64_t*)(slabmeta->bitmap);
    word_idx    = 0;

    while ((word = bitmap[word_idx]) == 0ULL)
    {
        word_idx++;
    }

    bit_idx     = __builtin_ctzll(word);
    block_idx   = (word_idx * 64) + bit_idx;

    bitmap[word_idx] &= ~(1ULL << bit_idx);
    slabmeta->nblocks_free--;

    return (byte_t*)(base_mem + (block_idx * slabmeta->nbytes_block));
}

void 
slab_put_block(
    span_t *span,
    void   *block
){
    slab_t     *slab;
    uint64_t   *bitmap;
    uintptr_t   offset;
    uint64_t    block_idx;
    uint64_t    word_idx;
    uint64_t    bit_idx;

    slab        = span->slabmeta;
    bitmap      = (uint64_t*)(slab->bitmap);

    offset      = (uintptr_t)block - (uintptr_t)(span->addr);
    block_idx   = offset / slab->nbytes_block;
    word_idx    = block_idx / 64;
    bit_idx     = block_idx % 64;

    bitmap[word_idx] |= (1ULL << bit_idx);
    slab->nblocks_free++;
}