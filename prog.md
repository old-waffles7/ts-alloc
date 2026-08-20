
## todo
*   add -fno-strict-aliasing to cmake bc of the casting
*   fix tcache_info in the config script.... rename to tcache_capacity or something
*   remove default_span_size field for glob_alloc_state_t in config
*   replace tsalloc(), tsfee() with macros that call _tsalloc, _tsfree to avoid casting
    errors

## would add later
*   functionality to disable/re-enable tcache
*   move pagetrie to global arena so a thread can alloc/free across
    multiple arenas, would also allow for bootstrapping some of 
    the entire arena's memory footprint
*   change objpool into being a multi-class object pool. would then
    only need 1 instance (fewer wasted pages) since each objpool instance
    calls sys_alloc directly
*   perhaps manage memory using tsalloc-virtual memory page sizes
*   implemnt my own mallctl? would require implementing a parser
*   perhaps multiple pagetries if contestion for put/remove operations
    becomes too much? i do not know how i would implement the hash for 
    addr -> pagetrie_idx
*   document the specific cases in which an error is proced and what type the
    error is