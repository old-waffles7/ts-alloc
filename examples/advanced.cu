
/**
 *  welcome to the advanced example in using ltsalloc! in this document we will be exploring using 
 *  multiple allocator instances in addition to implementing an allocator using a custom 
 *  configuration: in this case, a VRAM backed allocator (NVIDIA gpu and -lcuda compilation flag 
 *  required for this example). good luck!
 *
 *  please remember to go through the README and configure libtsalloc prior to compiling the library
 *  by using the configuration script: tsalloc/src/config/config.py 
*/

//  have to wrap since .cu is based in ugly cpp
extern "C" 
{
    #include "../include/tsalloc.h"
}

#include    <stddef.h>
#include    <stdint.h>
#include    <stdio.h> 

#include    <cuda.h>


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#else
    #define nullptr NULL
#endif


static inline void
print_error(
    tsalloctr_t    *allocator
){
    ts_errstate_t   state;

    //  see "tsalloc.h" for all `ts_err_t` values and their corresponding descriptions
    ts_req_errstate(allocator, &state);
    printf(
        "TSALLOC_ERROR_CODE:    %d\n"
        "OS_ERROR_CODE:         %d\n"
        "TSALLOC_MESSAGE:       %s\n"
        "TRACE:                 %s\n",
        state.ts_error_code,
        state.os_error_code,
        state.message,
        state.trace
    );
}


//  encapsulation of context needed by CUDAs VMM api to map, unmap, and mutate memory
struct cuda_vmm_context 
{
    CUmemAllocationProp prop;
    CUmemAccessDesc     access_desc;
    CUcontext           cu_ctx;
};
typedef struct cuda_vmm_context cuda_vmm_ctx_t;


//  implementation of auxil_mmap for VRAM (NVIDIA) backed allocator
static void*
cuda_vmm_mmap(
    void   *extra,
    size_t  align,
    size_t  nbytes
){
    cuda_vmm_ctx_t *cuda_vmm_ctx;
    CUdeviceptr     addr;
    int             ret;

    cuda_vmm_ctx    = (cuda_vmm_ctx_t*)extra;
    ret             = cuCtxPushCurrent(cuda_vmm_ctx->cu_ctx);
    if (ret != CUDA_SUCCESS)
    {
        return nullptr;
    }
    
    //  reserve virtual address
    ret = cuMemAddressReserve(
        &addr, 
        nbytes, 
        align, 
        0,
        0
    );
    if (ret != CUDA_SUCCESS)
    {
        return nullptr;
    }

    CUmemGenericAllocationHandle    handle;
    
    //  reserve physical pages
    ret = cuMemCreate(
        &handle, 
        nbytes, 
        &(cuda_vmm_ctx->prop), 
        0
    );
    if (ret != CUDA_SUCCESS)
    {
        cuMemAddressFree(addr, nbytes);
        cuCtxPopCurrent(nullptr);
        return nullptr;
    }

    //  map virtual address to physical pages
    ret = cuMemMap(
        addr, 
        nbytes, 
        0, 
        handle, 
        0
    );
    if (ret != CUDA_SUCCESS)
    {
        cuMemRelease(handle);
        cuMemAddressFree(addr, nbytes);
        cuCtxPopCurrent(nullptr);
        return nullptr;
    }
    cuMemRelease(handle);

    //  set permission
    ret = cuMemSetAccess(
        addr, 
        nbytes, 
        &(cuda_vmm_ctx->access_desc), 
        1
    );
    if (ret != CUDA_SUCCESS)
    {
        cuMemUnmap(addr, nbytes);
        cuMemAddressFree(addr, nbytes);
        cuCtxPopCurrent(nullptr);
        return nullptr;
    }

    cuCtxPopCurrent(nullptr);
    return ((void*)addr);
}

//  implementation of auxil_unmap for VRAM (NVIDIA) backed allocator
static int 
cuda_vmm_unmap(
    void   *extra, 
    void   *addr, 
    size_t  nbytes
){
    cuda_vmm_ctx_t *cuda_vmm_ctx;
    int             ret;

    cuda_vmm_ctx    = (cuda_vmm_ctx_t*)extra;
    ret             = cuCtxPushCurrent(cuda_vmm_ctx->cu_ctx);
    if (ret != CUDA_SUCCESS)
    {
        return -1;
    }
    
    CUdeviceptr _addr;

    _addr   = (CUdeviceptr)addr;

    //  release physical pages
    ret = cuMemUnmap(_addr, nbytes);
    if (ret != CUDA_SUCCESS)
    {
        cuCtxPopCurrent(nullptr);
        return -1;
    }

    //  release virtual address
    ret = cuMemAddressFree(_addr, nbytes);
    if (ret != CUDA_SUCCESS)
    {
        cuCtxPopCurrent(nullptr);
        return -1;
    }

    return 0;
}

//  implementation of auxil_madvise for VRAM (NVIDIA) backed allocator
static int 
cuda_vmm_madvise(
    void               *extra,
    void               *addr,
    size_t              nbytes,
    tsalloc_advice_t    advice
){
    cuda_vmm_ctx_t *cuda_vmm_ctx;
    int             ret;

    cuda_vmm_ctx    = (cuda_vmm_ctx_t*)extra;
    ret             = cuCtxPushCurrent(cuda_vmm_ctx->cu_ctx);
    if (ret != CUDA_SUCCESS)
    {
        return -1;
    }
    
    CUdeviceptr _addr;

    _addr           = (CUdeviceptr)addr;
    switch (advice) 
    {
        case TSALLOC_ADVISE_RETAIN:
            // release physical pages
            ret = cuMemUnmap(_addr, nbytes);
            if (ret != CUDA_SUCCESS)
            {
                cuCtxPopCurrent(nullptr);
                return -1;
            }
            break;

        case TSALLOC_ADVISE_UNRETAIN:
        {
            CUmemGenericAllocationHandle    handle;

            //  reserve physical pages
            ret = cuMemCreate(
                &handle, 
                nbytes, 
                &(cuda_vmm_ctx->prop), 
                0
            );
            if (ret != CUDA_SUCCESS)
            {
                cuCtxPopCurrent(nullptr);
                return -1;
            }

            //  map virtual address to new physical pages
            ret = cuMemMap(
                _addr, 
                nbytes, 
                0, 
                handle, 
                0
            );
            if (ret != CUDA_SUCCESS)
            {
                cuCtxPopCurrent(nullptr);
                cuMemRelease(handle);
                return -1;
            }
            cuMemRelease(handle);
            
            //  set permissions
            ret = cuMemSetAccess(
                _addr, 
                nbytes, 
                &(cuda_vmm_ctx->access_desc), 
                1
            );
            if (ret != CUDA_SUCCESS)
            {
                cuCtxPopCurrent(nullptr);
                cuMemUnmap(_addr, nbytes);
                return -1;
            }
        }
            break;
        
        default:
            cuCtxPopCurrent(nullptr);
            return -1;
    }

    cuCtxPopCurrent(nullptr);
    return 0;
}


//  populate context needed by CUDAs VMM api to map, unmap, and mutate memory
static cuda_vmm_ctx_t _cuda_vmm_ctx   = {
    .prop           = {
        .type                   = CU_MEM_ALLOCATION_TYPE_PINNED,
        // not shared across multiple processes
        .requestedHandleTypes   = (CUmemAllocationHandleType)0,    
        .location               = {
            //  get pages from gpu, not host
            .type   = CU_MEM_LOCATION_TYPE_DEVICE,  
            //  uid of target (NVIDIA) gpu, we pick the first one for this example
            .id     = 0
        }
        //  no special flags needed for this example
    },
    .access_desc    = {
        .location   = {
            //  get pages from gpu, not host
            .type   = CU_MEM_LOCATION_TYPE_DEVICE,  
            //  uid of target (NVIDIA) gpu, we pick the first one for this example
            .id     = 0
        },
        //  we want read/write permission to all allocated memory
        .flags      = CU_MEM_ACCESS_FLAGS_PROT_READWRITE
    }
};

//  practice kernels
__global__ void 
vector_add(
    const uint32_t *a,
    const uint32_t *b,
    uint32_t       *dest,
    size_t          nelements
){
    size_t  idx;

    idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < nelements)
    {
        dest[idx]   = a[idx] + b[idx];
    }
}

__device__ uint32_t
pseudo_randint(
    uint32_t    state
){
    state   = state * 747796405u + 2891336453u;
    state   = ((state >> ((state >> 28) + 4)) ^ state) * 277803737u;
    state   = (state >> 22) ^ state;
    return state;
}

__global__ void 
randint_vec(
    uint32_t   *dest,
    uint32_t    seed,
    size_t      nelements
){
    size_t      idx;
    uint32_t    state;

    idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx < nelements)
    {
        state       = seed ^ idx;
        state       = pseudo_randint(state);
        dest[idx]   = state;
    }
}

#define     N_ELEMENTS          4194304ULL
#define     THREADS_PER_BLOCK   256
#define     N_BLOCKS            ((uint64_t)(N_ELEMENTS) + (uint64_t)(THREADS_PER_BLOCK) - (uint64_t)(1)) / (THREADS_PER_BLOCK)


int main(void)
{
    CUdevice    cu_device;
    size_t      pagesize;
    int         ret;

    //  initialize CUDA driver API
    ret = cuInit(0);
    if (ret != CUDA_SUCCESS)
    {
        printf("failed to initialize CUDA driver.\n");
        return -1;
    }

    //  get and discard handle
    ret = cuDeviceGet(&cu_device, 0);
    if ((ret != CUDA_SUCCESS) || (cu_device != 0))
    {
        printf("failed to get GPU 0.\n");
        return -1;
    }

    //  initialize global CUDA ctx
    ret = cuDevicePrimaryCtxRetain(&(_cuda_vmm_ctx.cu_ctx), 0);
    if (ret != CUDA_SUCCESS)
    {
        printf("failed to create CUDA context.\n");
        return -1;
    }
    cuCtxPushCurrent(_cuda_vmm_ctx.cu_ctx);

    //  ask the driver for the minimum required alignment/pagesize (granularity) 
    ret = cuMemGetAllocationGranularity(
        &pagesize, 
        &(_cuda_vmm_ctx.prop), 
        CU_MEM_ALLOC_GRANULARITY_MINIMUM    //  we only require the minimum possible value
    );
    if (ret != CUDA_SUCCESS)
    {
        printf("failed to get allocation granularity.\n");
        return -1;
    }
    printf("the physical page granularity is: %zu bytes\n", pagesize);


    //  all done with that cuda jargon now we can initialize our allocators ^_^
    tsalloctr_t    *allocator_cu;   //  VRAM backed
    tsalloctr_t    *allocator_host; //  RAM backed
    ts_arena_cfg_t  arena_cfg_cu;
    cudaError_t     err;

    //  define arena configuration
    arena_cfg_cu    = (ts_arena_cfg_t){
        .auxil_map                  = cuda_vmm_mmap,
        .auxil_unmap                = cuda_vmm_unmap,
        .auxil_madvise              = cuda_vmm_madvise,
        .extra                      = (void*)&_cuda_vmm_ctx,
        .pagesize                   = pagesize,
        //  1 GiB
        .default_new_span_szclass   = tsszclass_of_nbytes(pagesize, (1024ULL * 1024ULL * 1024ULL)),
        .lcpu_arena_count           = 4,
        .unmap_on_termination       = true,
        //  cannot implicity coalesce seperate mappings in this implementation, but it is possible
        //  by augmenting the auxil functions we defined.
        .allow_cross_origin_merge   = false
    };

    //  create allocators
    ret = tsalloc_create(&arena_cfg_cu, &allocator_cu);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_cu);
        return -1;
    }
    ret = tsalloc_create(TSALLOC_DEFAULT_ARG, &allocator_host);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_host);
        (void)tsalloc_destroy(allocator_cu);
        (void)tsalloc_destroy(allocator_host);
        return -1;
    }

    static uint32_t    *a, *b, *c, *h_a, *h_b, *h_c;

    //  allocate memory for vectors
    {
        //  from gpu
        ret = tsalloc(allocator_cu, &a, sizeof(uint32_t)*N_ELEMENTS);
        if (ret != TSALLOC_SUCCESS)
        {
            print_error(allocator_cu);
            (void)tsalloc_destroy(allocator_cu);
            (void)tsalloc_destroy(allocator_host);
            return -1;
        }
        ret = tsalloc(allocator_cu, &b, sizeof(uint32_t)*N_ELEMENTS);
        if (ret != TSALLOC_SUCCESS)
        {
            print_error(allocator_cu);
            (void)tsalloc_destroy(allocator_cu);
            (void)tsalloc_destroy(allocator_host);
            return -1;
        }
        ret = tsalloc(allocator_cu, &c, sizeof(uint32_t)*N_ELEMENTS);
        if (ret != TSALLOC_SUCCESS)
        {
            print_error(allocator_cu);
            (void)tsalloc_destroy(allocator_cu);
            (void)tsalloc_destroy(allocator_host);
            return -1;
        }

        //  from host
        ret = tsalloc(allocator_host, &h_a, sizeof(uint32_t)*N_ELEMENTS);
        if (ret != TSALLOC_SUCCESS)
        {
            print_error(allocator_host);
            (void)tsalloc_destroy(allocator_cu);
            (void)tsalloc_destroy(allocator_host);
            return -1;
        }
        ret = tsalloc(allocator_host, &h_b, sizeof(uint32_t)*N_ELEMENTS);
        if (ret != TSALLOC_SUCCESS)
        {
            print_error(allocator_host);
            (void)tsalloc_destroy(allocator_cu);
            (void)tsalloc_destroy(allocator_host);
            return -1;
        }
        ret = tsalloc(allocator_host, &h_c, sizeof(uint32_t)*N_ELEMENTS);
        if (ret != TSALLOC_SUCCESS)
        {
            print_error(allocator_host);
            (void)tsalloc_destroy(allocator_cu);
            (void)tsalloc_destroy(allocator_host);
            return -1;
        }
    }

    //  invoke some kernels to fill `a` and `b` with some data
    randint_vec<<<N_BLOCKS, THREADS_PER_BLOCK>>>((uint32_t*)a, 1234, N_ELEMENTS);
    err = cudaGetLastError();
    if (err != cudaSuccess) 
    {
        printf("kernel Error: %s\n", cudaGetErrorString(err));
    }
    randint_vec<<<N_BLOCKS, THREADS_PER_BLOCK>>>((uint32_t*)b, 5678, N_ELEMENTS);
    err = cudaGetLastError();
    if (err != cudaSuccess) 
    {
        printf("kernel Error: %s\n", cudaGetErrorString(err));
    }
    cuCtxSynchronize();

    //  perform vector addition
    vector_add<<<N_BLOCKS, THREADS_PER_BLOCK>>>(a, b, c, N_ELEMENTS);
    err = cudaGetLastError();
    if (err != cudaSuccess) 
    {
        printf("kernel Error: %s\n", cudaGetErrorString(err));
    }
    cuCtxSynchronize();

    //  print results by copying vectors to RAM backed memory
    cuMemcpyDtoH(h_a, (CUdeviceptr)a, sizeof(uint32_t) * N_ELEMENTS);
    cuMemcpyDtoH(h_b, (CUdeviceptr)b, sizeof(uint32_t) * N_ELEMENTS);
    cuMemcpyDtoH(h_c, (CUdeviceptr)c, sizeof(uint32_t) * N_ELEMENTS);
    for (size_t i = 0; i < N_ELEMENTS; i++)
    {
        printf("a[%lu] + b[%lu] = %u + %u = %u\n", i, i, h_a[i], h_b[i], h_c[i]);
    }


    //  free RAM backed (host) allocations
    ret = tsfree(allocator_host, h_a);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_host);
        (void)tsalloc_destroy(allocator_cu);
        (void)tsalloc_destroy(allocator_host);
        return -1;
    }
    ret = tsfree(allocator_host, h_b);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_host);
        (void)tsalloc_destroy(allocator_cu);
        (void)tsalloc_destroy(allocator_host);
        return -1;
    }
    ret = tsfree(allocator_host, h_c);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_host);
        (void)tsalloc_destroy(allocator_cu);
        (void)tsalloc_destroy(allocator_host);
        return -1;
    }


    //  our arena configuration set `unmap_on_termination1 to true. so, unless we actually need to
    //  recycle memory during the lifetime of our program, we can skip frees and just destroy our 
    //  allocator
    ret = tsalloc_destroy(allocator_cu);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_cu);
        return -1;
    }
    ret = tsalloc_destroy(allocator_host);
    if (ret != TSALLOC_SUCCESS)
    {
        print_error(allocator_host);
        return -1;
    }

    return 0;
}