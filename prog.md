implemnt my own mallctl?

/*struct arena
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
}*/

tsalloc_szclass_t   szclass;

    szclass = ((tsalloc_szclass_t)tsalloc_get_szclass(arena_cfg->tsalloc_cfg, nbytes));
    if (((int32_t)szclass) == -1)
    {
        set_tsalloc_error(
            error_ctx,
            "scache_get_span::scach.c invalid argument nbytes",
            TSALLOC_INVALID_ARGS
        );
        return TSALLOC_INVALID_ARGS;
    }

- make ram pinnable via a pin_alloc or something like that (in malloc.h not arena.h)
  maybe use mlock, posix portable

- add functionality to disable thread local cache in a thread

- in tcaches intrusively store blocks ages, use LL as a queue. only update age of 
  oldest block pass its age/2 or something onto next block.

- in config.py make it so if > uint16 num of classes failure

- implement portable alternative to __builtin_clzll

- remove max alloc argument in config.py, replace it with largest szclass

- can get from memory to span struct using pagetrie. e.g using madvise on a bit of memory
  to make it undumpable

- use the mutex lock when getting and puttins spans in scache, add boolean isslab
  to api for getting/putting it will initialie the slab meta there.

- inline record in span objpool conditionally

- rememebr add -fno-strict-aliasing to cmake bc of the casting
