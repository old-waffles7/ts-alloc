
## todo
*   add -fno-strict-aliasing to cmake bc of the casting
*   fix tcache_info in the config script.... rename to tcache_capacity or something
*   make the error logging thread safe. maybe declare the macro auxilliarty functions
    then define in error.c. in error.c declare the thread local. alternatively add
    parameter to init function on how many error contexts there are. then in glob_t
    add a struct that contains n-many error context structs and n mnay atomic booleans
    specifying if it has been written to already. add a function to get error state of 
    a given thread and once it does, clears that associated context. also function to get
    how many contexts are left and have context store pthread_t id.

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