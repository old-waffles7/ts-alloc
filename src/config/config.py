#!/bin/python3


import math
from enum import Enum

# Configuration Constants
QUANTUM = 16
CLASSES_PER_GROUP = 4
MAX_SMALL_SIZE = 16384
MAX_ALLOC_SIZE = 7 * (1 << 60)

class PageSize(Enum):
    X86_4K = 4096
    ARM_16K = 16384
    ARM_64K = 65536

def generate_size_classes():
    sizes = []
    size = QUANTUM
    step = QUANTUM
    
    while size <= MAX_ALLOC_SIZE:
        for _ in range(CLASSES_PER_GROUP):
            sizes.append(size)
            if size >= MAX_ALLOC_SIZE:
                return sizes
            size += step
        step *= 2
    return sizes

def calculate_slab_info(reg_size, page_size):
    target_pages = 1
    if reg_size > page_size / 2:
        target_pages = 2
    if reg_size > page_size:
        target_pages = 4
        
    slab_size = target_pages * page_size
    nregs = slab_size // reg_size
    bitmap_size = math.ceil(nregs / 64) * 8
    
    return slab_size, nregs, bitmap_size

def get_tcache_limit(reg_size):
    if reg_size <= 256:
        return 200
    elif reg_size <= 4096:
        return 64
    elif reg_size <= MAX_SMALL_SIZE:
        return 20
    return 0

def main():
    sizes = generate_size_classes()
    bin_count = len(sizes)
    
    small_bin_count = sum(1 for s in sizes if s <= MAX_SMALL_SIZE)
    
    with open("src/config/tsalloc_config.h", "w") as f:
        f.write("#pragma once\n")
        f.write("#include   \"../internal/common.h\" \n\n")
        f.write(f"#define BIN_COUNT {bin_count}\n")
        f.write(f"#define SMALL_BIN_COUNT {small_bin_count}\n")
        f.write(f"#define MAX_SMALL_SIZE {MAX_SMALL_SIZE}\n\n")
        
        f.write("typedef struct {\n")
        f.write("    uint32_t reg_size;\n")
        f.write("    uint32_t slab_size;\n")
        f.write("    uint32_t nregs;\n")
        f.write("    uint32_t bitmap_size;\n")
        f.write("} bin_info_t;\n\n")

        f.write("static const uint64_t sz_index2size_tab[BIN_COUNT] = {\n")
        f.write("    " + ", ".join(map(str, sizes)) + "\n")
        f.write("};\n\n")

        size2index = []
        curr_idx = 0
        for req_bytes in range(QUANTUM, MAX_SMALL_SIZE + 1, QUANTUM):
            while sizes[curr_idx] < req_bytes:
                curr_idx += 1
            size2index.append(curr_idx)
            
        f.write(f"static const uint16_t sz_size2index_tab[{len(size2index)}] = {{\n")
        f.write("    " + ", ".join(map(str, size2index)) + "\n")
        f.write("};\n\n")

        for ps in PageSize:
            f.write(f"static const bin_info_t bin_infos_{ps.value}[SMALL_BIN_COUNT] = {{\n")
            for i in range(small_bin_count):
                slab_sz, nregs, bmp_sz = calculate_slab_info(sizes[i], ps.value)
                f.write(f"    {{{sizes[i]}, {slab_sz}, {nregs}, {bmp_sz}}},\n")
            f.write("};\n\n")

        tcache_limits = [get_tcache_limit(s) for s in sizes]
        f.write("static const uint32_t tcache_bin_info[BIN_COUNT] = {\n")
        f.write("    " + ", ".join(map(str, tcache_limits)) + "\n")
        f.write("};\n\n")

if __name__ == "__main__":
    main()