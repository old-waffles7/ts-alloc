
#pragma once
#ifndef REGISTRY_H
#define REGISTRY_H


#include    "common.h"

#include    "genlist.h"


typedef struct span span_t;

//  forward declaration suffices for structure definitions
gen_dll_struct(registry, span_t);
typedef dll(registry)       registry_t;
typedef dll_coord(registry) registry_coord_t;


static inline bool
registry_isempty(
    registry_t *registry
){
    return !registry->head;
}

void 
registry_push(
    registry_t *registry, 
    span_t     *span
);

span_t*
registry_pop(
    registry_t *registry
);

void
registry_remove(
    registry_t *registry, 
    span_t     *span
);


#endif  //REGISTRY_H