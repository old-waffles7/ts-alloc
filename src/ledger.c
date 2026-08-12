
#include    "internal/common.h"
#include    "internal/ledger.h"

#include    "internal/tcache.h"
#include    "internal/genlist.h"


gen_dll_func(
    , 
    ledger, 
    tcache_t, 
    coord
);

void 
ledger_push(
    ledger_t   *ledger, 
    tcache_t   *cache
){
    ledger_push_front(ledger, cache);
}

tcache_t*
ledger_pop(
    ledger_t   *ledger
){
    return ledger_pop_front(ledger);
}