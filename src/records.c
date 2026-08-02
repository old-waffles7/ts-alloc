
#include "internal/common.h"
#include "internal/registry.h"

#include "internal/span.h"
#include "internal/genlist.h"


gen_dll_func
(
    , 
    records,
    span_t, 
    record->coord
);


void 
records_push(
    records_t  *records, 
    span_t     *span
){
    records_push_front(records, span);
}

span_t*
records_pop(
    records_t  *records
){
    return records_pop_front(records);
}