
#pragma once
#ifndef RECORDS_H
#define RECORDS_H


#include    "common.h"
#include    "error.h"

#include    "genlist.h"
#include    "objpool.h"


typedef struct span span_t;

//  forward declaration suffices for structure definitions
gen_dll_struct(records, span_t);
typedef dll(records)        records_t;
typedef dll_coord(records)  record_coord_t;


struct record
{
    record_coord_t  coord;
    size_t          nbytes;
};
typedef struct record   record_t;

static inline void
record_init(
    record_t   *record,
    size_t      nbytes
){
    *record         = (record_t){0};
    record->nbytes  = nbytes;
}

static inline bool
records_isempty(
    records_t *records
){
    return !records->head;
}

void 
records_push(
    records_t  *records, 
    span_t     *span
);

span_t*
records_pop(
    records_t  *records
);

void
records_remove(
    records_t  *records,
    span_t     *span
);


#endif  //RECORDS_H