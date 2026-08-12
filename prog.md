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

todo:
    -   add -fno-strict-aliasing to cmake bc of the casting

would add later:
    -   functionality to disable/re-enable tcache
    -   move pagetrie to global arena so a thread can alloc/free across
        multiple arenas, would also allow for bootstrapping some of 
        the entire arena's memory footprint
    -   change objpool into being a multi-class object pool. would then
        only need 1 instance (fewer wasted pages) since each objpool instance
        calls sys_alloc directly
    -   perhaps manage memory using tsalloc-virtual memory page sizes