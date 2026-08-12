
#pragma once
#ifndef LEDGER_H
#define LEDGER_H


#include    "common.h"

#include    "genlist.h"


typedef struct thread_loc_cache tcache_t;

//  forward declaration suffices for structure definitions
gen_sll_struct(ledger, tcache_t);
typedef sll(ledger)         ledger_t;
typedef sll_coord(ledger)   ledger_coord_t;


static inline bool
ledger_isempty(
    ledger_t   *ledger
){
    return !ledger->head;
}

void 
ledger_push(
    ledger_t   *ledger, 
    tcache_t   *tcache
);

tcache_t*
ledger_pop(
    ledger_t   *ledger
);


#endif  //LEDGER_H