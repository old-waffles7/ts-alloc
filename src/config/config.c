
#include    "../internal/common.h"


/*
user input:
    - min alignment (pow of 2)
    - size quantum (pow of 2)
    - supported page sizes (pow of 2)

script define:
    - macro max size for slab (default 16kb)
    - array (#1) maps max-nbytes to size-class number
    - array maps size-class to max-nbytes for that class 
    - slab-info arrays for each supported page size

condsider:
    - fixed number of size classes per doubling of quantum #1
    - generate a tcache_bin_info array. This array defines the maximum number of items a thread is
      allowed to cache for each size class index


User Inputs & Configuration Defaults

Min Alignment: 16 bytes. Required for standard 64-bit systems and basic SIMD compliance.

Size Quantum: 16 bytes.

Supported Page Sizes: 4096 (x86/x64), 16384 (macOS ARM64), 65536 (Linux ARM64).

Max Small Allocation Size: 16384 bytes (16 KB). Allocations above this threshold bypass slabs and are served as direct extents.

Size Classes per Group: 4. This is the optimal number of subdivisions per size doubling to cap internal memory fragmentation at exactly 20%.

Step 1: Size Class Generation (Logarithmic Spacing)
The script iterates to generate size classes based on the quantum and group size until hitting the max small allocation size.

Group 0 (Step 16): 16, 32, 48, 64.

Group 1 (Step 32): 96, 128, 160, 192.

Group 2 (Step 64): 256, 320, 384, 448.

The script counts the total number of classes generated. This integer becomes your BIN_COUNT macro.

Step 2: Generated Output Arrays

sz_index2size_tab [Size: BIN_COUNT]

Mapping: Size class index -> Max bytes for that class.

Data: Directly contains the values calculated in Step 1 (16, 32, 48, 64, 96...).

sz_size2index_tab [Size: Max Small Allocation / Quantum]

Mapping: Requested Bytes / Quantum -> Size class index.

Data: For a 16 KB max size and 16-byte quantum, this is a 1,024-element array. Elements 1-4 point to index 0, 1, 2, 3. Elements 5-6 point to index 4 (since 96/16 = 6).

bin_infos_[pagesize] [Size: BIN_COUNT]

The script generates one of these arrays for every page size provided in the user input.

reg_size: The byte size for this bin.

slab_size: A calculated multiple of pagesize. The script must find a multiple that fits enough regions to keep the header/bitmap overhead low, without wasting excessive trailing bytes. Default logic: Target 1-4 pages per slab depending on the reg_size.

nregs: slab_size / reg_size.

bitmap_size: ceil(nregs / 64) * 8 (bytes required for the 64-bit hardware scan instructions).

tcache_bin_info [Size: BIN_COUNT]

Mapping: Size class index -> ncached_max (max items a thread can hold without locking the arena).

Default Logic: Scale inversely with size.

0 – 256 bytes: 200 items.

256 – 4096 bytes: 64 items.

4096 – 16384 bytes: 20 items.
*/