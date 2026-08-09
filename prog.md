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

- add functionality to disable (memcaching) thread local cache in a thread

- in tcaches store blocks using arrays (columns)

- in config.py make it so if > uint16 num of classes failure

- can get from memory to span struct using pagetrie. e.g using madvise on a bit of memory to make it undumpable

- rememebr add -fno-strict-aliasing to cmake bc of the casting

- remove size generating epoch from config args, add argument for
    reset epoch

- column, col_t

- remove opt for logging

- make config script generate function that maps pagesize -> tsalloc_cfg_t

- make tcache have pointer to errorctx