
#include    "internal/common.h"
#include    "internal/bucket.h"

#include    "internal/span.h"
#include    "internal/genheap.h"


static inline bool 
span_cmp(
    span_t *span_1,
    span_t *span_2
){
    if ((span_1->flags.age) < (span_2->flags.age))
    {
        return true;
    }
    else if ((span_1->flags.age) > (span_2->flags.age))
    {
        return false;
    }

    return (((uintptr_t)(span_1->addr)) < ((uintptr_t)(span_2->addr)));
}

gen_heap_func
(
    inline, 
    bucket, 
    span_t, 
    coord.bucket, 
    span_cmp
);