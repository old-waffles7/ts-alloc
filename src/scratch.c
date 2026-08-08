/*
struct arena
{
    region_t   *region_list;
    pagetrie_t  region_trie;
    pageheap_t  region_buckets[NCLASS_REGION];
    pageheap_t  slab_buckets[NCLASS_SLAB];
    objpool_t   region_dpool;
    objpool_t   slab_dpool;
    mutex_t     mutex;
    atomic_int  nthreads;
    size_t      nbytes_alloc;
    size_t      nbytes_cached;
}

struct arena 
{
    -   arenaconfig
    
    -   slab cache
        array of slabs for eaach slab block szclass
        each as a linked list? idk the need for the linked list
    
    -   span cache, scache instace

    -   span registry for destruction

    -   pagetrie

    -   spanpool 
    
    -   slabpool

    -   nthreads

    -   stats nbytes_alloc i think is enough then api to fetch it
}
*/