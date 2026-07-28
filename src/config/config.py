#!/bin/python3

"""
    config.py.

    configuration-script for the tsalloc library. configures the tsalloc library to support
    specified memory architectures. tsalloc provides default support for the following page-sizes:
        - 4 KiB
        - 16 KiB
        - 32 KiB
        - 64 KiB
        - 2 MiB
"""

import argparse
import math
import sys
import os


class configuration:
    def __init__(
        self,
        page_size,
        min_align,
        epoch,
        nbytes_slab_alloc_max,
        nbytes_alloc_max,
        nbytes_new_span
    ):
        if min_align > page_size:
            raise ValueError(f"min-align ({min_align}) must be <= page-size ({page_size})")

        max_blocks                  = page_size // min_align

        self.page_size              = page_size
        self.min_align              = min_align
        self.epoch                  = epoch
        self.nbytes_slab_alloc_max  = nbytes_slab_alloc_max
        self.nbytes_alloc_max       = nbytes_alloc_max
        self.nbytes_new_span        = nbytes_new_span
        self.nbytes_bitmap          = math.ceil(max_blocks / 64) * 8

        self.min_align_shift        = int(math.log2(self.min_align))
        self.epoch_shift            = int(math.log2(self.epoch))
        
    def get_size_classes(
        self
    ) -> list:
        sz_classes  = []
        size        = self.min_align
        step        = self.min_align
        
        while size <= self.nbytes_alloc_max:
            for _ in range(self.epoch):
                sz_classes.append(size)
                if size >= self.nbytes_alloc_max:
                    return sz_classes
                size += step
            step *= 2
        return sz_classes

    def get_slab_infos(
        self, 
        sizes   : list
    ) -> list:
        infos = []
        
        for block_size in sizes:
            if block_size > self.nbytes_slab_alloc_max:
                break
                
            slab_size   = (block_size * self.page_size) // math.gcd(block_size, self.page_size)
            nblocks     = slab_size // block_size
            
            infos.append((block_size, slab_size, nblocks))
            
        return infos

    def get_tcache_limits(
        self, 
        sizes       : list, 
        slab_infos  : list
    ) -> list:
        limits  = []

        for i, size in enumerate(sizes):
            if size > self.nbytes_slab_alloc_max:
                break
            
            nblocks = slab_infos[i][2]
            limit   = min(128, nblocks // 2)
            
            limits.append(limit)
            
        return limits


"""
    default configurations based on page-size profiles.
"""
DEFAULT_CONFIGS = {
    (1024 * 4): configuration(
        page_size               = (1024 * 4),
        min_align               = 16,
        epoch                   = 4 ,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_alloc_max        = (sys.maxsize - 1),
        nbytes_new_span         = (1024 * 1024 * 2)
    ),
    (1024 * 16): configuration(
        page_size               = (1024 * 16),
        min_align               = 16,
        epoch                   = 4 ,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_alloc_max        = (sys.maxsize - 1),
        nbytes_new_span         = (1024 * 1024 * 2)
    ),
    (1024 * 32): configuration(
        page_size               = (1024 * 32),
        min_align               = 16,
        epoch                   = 4 ,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_alloc_max        = (sys.maxsize - 1),
        nbytes_new_span         = (1024 * 1024 * 2)
    ),
    (1024 * 64): configuration(
        page_size               = (1024 * 64),
        min_align               = 16,
        epoch                   = 4 ,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_alloc_max        = (sys.maxsize - 1),
        nbytes_new_span         = (1024 * 1024 * 2)
    ),
    (1024 * 1024 * 2): configuration(
        page_size               = (1024 * 1024 * 2),
        min_align               = 256,
        epoch                   = 4,
        nbytes_slab_alloc_max   = (1024 * 512),
        nbytes_alloc_max        = (sys.maxsize - 1),
        nbytes_new_span         = (1024 * 1024 * 32)
    )
}


def type_bounded_pow2(
    max_val : int, 
    min_val : int
):
    def wrapper(arg: str) -> int:
        val = int(arg)
        if val <= 0 or (val & (val - 1)) != 0:
            raise argparse.ArgumentTypeError(f"'{arg}' must be a positive power of 2.")
        if val < min_val or val > max_val:
            raise argparse.ArgumentTypeError(f"'{arg}' out of bounds [{min_val}, {max_val}].")
        return val
    return wrapper

def type_bounded_int(
    max_val : int, 
    min_val : int
):
    def wrapper(arg: str) -> int:
        val = int(arg)
        if val <= 0:
            raise argparse.ArgumentTypeError(f"'{arg}' must be a positive integer.")
        if val < min_val or val > max_val:
            raise argparse.ArgumentTypeError(f"'{arg}' out of bounds [{min_val}, {max_val}].")
        return val
    return wrapper

def write_global_file_header(f) -> None:
    f.write("\n")
    f.write("#pragma\tonce\n")
    f.write("#ifndef\t_TSALLOC_CONFIG_H\n")
    f.write("#define\t_TSALLOC_CONFIG_H\n\n")
    f.write("\n")
    f.write("\n")
    f.write("#include\t\"../internal/common.h\"\n")
    f.write("\n")
    f.write("\n")

    f.write("struct tsalloc_slab_info\n")
    f.write("{\n")
    f.write("\tuint32_t\tblock_size;\n")
    f.write("\tuint32_t\tslab_size;\n")
    f.write("\tuint32_t\tnblocks;\n")
    f.write("};\n")
    f.write("typedef\tstruct tsalloc_slab_info\ttsalloc_slab_info_t;\n")
    f.write("\n")

    f.write("struct tsalloc_config\n")
    f.write("{\n")
    f.write("\tuint64_t\tpage_size;\n")
    f.write("\tuint64_t\tmin_align;\n")
    f.write("\tuint32_t\tepoch;\n")
    f.write("\tuint64_t\tslab_alloc_max;\n")
    f.write("\tuint64_t\talloc_max;\n")
    f.write("\tuint64_t\tnew_span_size;\n")
    f.write("\tuint32_t\tmin_align_shift;\n")
    f.write("\tuint32_t\tepoch_shift;\n")
    f.write("\tuint32_t\tnbytes_bitmap;\n")
    f.write("\tuint32_t\tnszclasses;\n")
    f.write("\tuint32_t\tnszclasses_slab;\n")
    f.write("\n")
    f.write("\tconst uint64_t*\t\t\t\tsz_class_max_nbytes;\n")
    f.write("\tconst uint16_t*\t\t\t\tsz_class_of_nbytes;\n")
    f.write("\tconst tsalloc_slab_info_t*\tslab_infos;\n")
    f.write("\tconst uint32_t*\t\t\t\ttcache_info;\n")
    f.write("};\n")
    f.write("typedef\tstruct tsalloc_config\ttsalloc_cfg_t;\n")
    f.write("\n\n")

def generate_config_c_block(
    cfg : configuration
) -> str:
    sizes           = cfg.get_size_classes()
    class_count     = len(sizes)
    slab_infos      = cfg.get_slab_infos(sizes)
    slab_count      = len(slab_infos)
    tcache_limits   = cfg.get_tcache_limits(sizes, slab_infos)
    
    size2index  = []
    curr_idx    = 0
    for req_bytes in range(cfg.min_align, cfg.nbytes_slab_alloc_max + 1, cfg.min_align):
        while sizes[curr_idx] < req_bytes:
            curr_idx += 1
        size2index.append(curr_idx)

    out = f"// -----[TSALLOC_CONFIG_START: {cfg.page_size}]-----\n\n"

    out += f"static const uint64_t\tsz_class_max_nbytes_{cfg.page_size}[{class_count}]\t= {{\n"
    out += "    " + ", ".join(map(str, sizes)) + "\n};\n\n"

    out += f"static const uint16_t\tsz_class_of_nbytes_{cfg.page_size}[{len(size2index)}]\t= {{\n"
    out += "    " + ", ".join(map(str, size2index)) + "\n};\n\n"

    out += f"static const tsalloc_slab_info_t\tslab_infos_{cfg.page_size}[{slab_count}]\t= {{\n"
    for info in slab_infos:
        out += f"    {{{info[0]}, {info[1]}, {info[2]}}},\n"
    out += "};\n\n"

    out += f"static const uint32_t\ttcache_info_{cfg.page_size}[{slab_count}]\t\t\t= {{\n"
    out += "    " + ", ".join(map(str, tcache_limits)) + "\n};\n\n"

    out += (
        f"static const tsalloc_cfg_t\tconfig_{cfg.page_size}\t= {{\n"
        f"\t.page_size              = {cfg.page_size},\n"
        f"\t.min_align              = {cfg.min_align},\n"
        f"\t.epoch                  = {cfg.epoch},\n"
        f"\t.slab_alloc_max         = {cfg.nbytes_slab_alloc_max},\n"
        f"\t.alloc_max              = {cfg.nbytes_alloc_max}ULL,\n"
        f"\t.new_span_size          = {cfg.nbytes_new_span},\n"
        f"\t.min_align_shift        = {cfg.min_align_shift},\n"
        f"\t.epoch_shift            = {cfg.epoch_shift},\n"
        f"\t.nbytes_bitmap          = {cfg.nbytes_bitmap},\n"
        f"\t.nszclasses             = {class_count},\n"
        f"\t.nszclasses_slab        = {slab_count},\n"
        f"\t.sz_class_max_nbytes    = sz_class_max_nbytes_{cfg.page_size},\n"
        f"\t.sz_class_of_nbytes     = sz_class_of_nbytes_{cfg.page_size},\n"
        f"\t.slab_infos             = slab_infos_{cfg.page_size},\n"
        f"\t.tcache_info            = tcache_info_{cfg.page_size}\n"
        f"}};\n\n"
    )

    out += f"// -----[TSALLOC_CONFIG_END: {cfg.page_size}]-----\n\n"
    
    return out

def remove_config_block(
    file_path   : str, 
    page_size   : int
) -> None:
    if not os.path.exists(file_path):
        return

    with open(file_path, "r") as f:
        lines = f.readlines()

    with open(file_path, "w") as f:
        skip_mode   = False
        start_tag   = f"// -----[TSALLOC_CONFIG_START: {page_size}]-----\n"
        end_tag     = f"// -----[TSALLOC_CONFIG_END: {page_size}]-----\n"
        
        for line in lines:
            if line == start_tag:
                skip_mode   = True
            
            if not skip_mode:
                f.write(line)
                
            if skip_mode and line == end_tag:
                skip_mode   = False


def main():
    parser = argparse.ArgumentParser(
        description     = "generate _tsalloc_config.h.",
        formatter_class = lambda prog: argparse.HelpFormatter(prog, max_help_position = 50)
    )
     
    parser.add_argument(
        "--clear", 
        action  = "store_true", 
        help    = "clear configurations for existing page-size profiles"
    )
    parser.add_argument(
        "--default",
        action  = "store_true",
        help    = "generate configurations for default page-size profiles"
    )
    parser.add_argument(
        "-rm", "--remove",
        type    = type_bounded_pow2((sys.maxsize - 1), 1),
        metavar = "PAGE_SIZE",
        help    = "remove configuration for specified page-size profile"
    )
    parser.add_argument(
        "-a", "--append",
        action  = "store_true",
        help    = "append configuration for specified page-size profile"
    )

    parser.add_argument(
        "-p", "--page-size",
        type    = type_bounded_pow2((sys.maxsize - 1), 1),
    )
    parser.add_argument(
        "-ma", "--min-align",
        type    = type_bounded_pow2((sys.maxsize - 1), 1),
    )
    parser.add_argument(
        "-e", "--epoch",
        type    = type_bounded_int((sys.maxsize - 1), 1),
    )
    parser.add_argument(
        "-sm", "--slab-alloc-max",
        type    = type_bounded_pow2((sys.maxsize - 1), 1),
    )
    parser.add_argument(
        "-am", "--alloc-max",
        type    = type_bounded_pow2((sys.maxsize - 1), 1),
    )
    parser.add_argument(
        "-ns", "--new-span-size",
        type    = type_bounded_pow2((sys.maxsize - 1), 1),
    )
    args        = parser.parse_args()
    file_path   = "src/config/_tsalloc_config.h"

    if args.clear:
        with open(file_path, "w") as f:
            write_global_file_header(f)
            f.write("#endif\t//_TSALLOC_CONFIG_H\n")
        sys.exit(0)

    if args.remove:
        remove_config_block(file_path, args.remove)
        sys.exit(0)

    mode = "a" if args.append else "w"
    file_exists = os.path.exists(file_path)

    if args.default:
        with open(file_path, mode) as f:
            if mode == "w" or not file_exists:
                write_global_file_header(f)
            for default_cfg in DEFAULT_CONFIGS.values():
                f.write(generate_config_c_block(default_cfg))
            if mode == "w" or not file_exists:
                f.write("#endif\t//_TSALLOC_CONFIG_H\n")
        sys.exit(0)

    config_args = [
        args.page_size, args.min_align, args.epoch, 
        args.slab_alloc_max, args.alloc_max, args.new_span_size
    ]

    if any(config_args):
        if None in config_args:
            print("Error: Incomplete configuration. Provide all arguments.")
            sys.exit(1)
            
        try:
            cfg = configuration(
                args.page_size, args.min_align, args.epoch,
                args.slab_alloc_max, args.alloc_max, args.new_span_size
            )
        except ValueError as e:
            print(f"Configuration Error: {e}")
            sys.exit(1)
        
        with open(file_path, mode) as f:
            if mode == "w" or not file_exists:
                write_global_file_header(f)
            f.write(generate_config_c_block(cfg))
            if mode == "w" or not file_exists:
                f.write("#endif\t//_TSALLOC_CONFIG_H\n")
    else:
        if not (args.clear or args.remove or args.default):
            parser.print_help()


if __name__ == "__main__":
    main()