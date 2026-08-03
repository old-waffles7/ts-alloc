
#include "internal/common.h"
#include "internal/registry.h"

#include "internal/span.h"
#include "internal/genlist.h"


gen_dll_func
(
    , 
    registry,
    span_t, 
    coord.registry
);


void 
registry_push(
    registry_t *registry, 
    span_t     *span
){
    registry_push_front(registry, span);
}

span_t*
registry_pop(
    registry_t *registry
){
    return registry_pop_front(registry);
}