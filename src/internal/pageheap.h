
#pragma once
#ifndef PAGEHEAP_H
#define PAGEHEAP_H


#include    "common.h"
#include    "error.h"

#include    "mutex.h"


struct pageheap
{
    void   *root;
    mutex_t mutex;
};

typedef struct pageheap pageheap_t;


inline tsalloc_err_t
pageheap_init(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap
);

inline tsalloc_err_t
pageheap_deinit(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap
);

tsalloc_err_t
pageheap_insert(
    tsalloc_errctx_t   *error_ctx,
    pageheap_t         *pageheap,
    void               *key,
    void               *data
);

void*
pageheap_pop(
    pageheap_t *pageheap
);

inline bool
pageheap_remove(
    pageheap_t *pageheap,
    void        
)


#endif  //PAGEHEAP_H