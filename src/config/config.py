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
import re

UINT16_MAX = (1 << 16) - 1
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1

class configuration:
    def __init__(
        self,
        page_size,
        min_align,
        steps_per_pow2,
        nbytes_slab_alloc_max,
        nbytes_epoch_max,
        nbytes_default_span_size,
    ):
        if min_align > page_size:
            raise ValueError(f"min-align ({min_align}) must be <= page-size ({page_size})")

        if page_size > UINT64_MAX: raise ValueError(f"page-size exceeds uint64_t max ({UINT64_MAX})")
        if min_align > UINT64_MAX: raise ValueError(f"min-align exceeds uint64_t max ({UINT64_MAX})")
        if nbytes_slab_alloc_max > UINT64_MAX: raise ValueError(f"slab-alloc-max exceeds uint64_t max ({UINT64_MAX})")
        if nbytes_epoch_max > UINT64_MAX: raise ValueError(f"epoch-max exceeds uint64_t max ({UINT64_MAX})")
        if nbytes_default_span_size > UINT64_MAX: raise ValueError(f"default-span-size exceeds uint64_t max ({UINT64_MAX})")
        if steps_per_pow2 > UINT32_MAX: raise ValueError(f"steps-per-pow2 exceeds uint32_t max ({UINT32_MAX})")

        max_blocks                      = page_size // min_align

        self.page_size                  = page_size
        self.min_align                  = min_align
        self.steps_per_pow2             = steps_per_pow2
        self.nbytes_slab_alloc_max      = nbytes_slab_alloc_max
        self.nbytes_epoch_max           = nbytes_epoch_max
        self.nbytes_default_span_size   = nbytes_default_span_size
        self.nbytes_bitmap              = math.ceil(max_blocks / 64) * 8

        self.min_align_shift            = int(math.log2(self.min_align))
        self.steps_per_pow2_shift       = int(math.log2(self.steps_per_pow2))
        
    def get_slab_size_classes(
        self
    ) -> list:
        szclasses  = []
        size        = self.min_align
        
        while size <= self.nbytes_slab_alloc_max:
            szclasses.append(size)
            p2          = 1 << int(math.log2(size))
            step        = max(self.min_align, p2 // self.steps_per_pow2)
            size       += step
            
        if len(szclasses) > UINT16_MAX:
            raise ValueError(f"Slab size classes count ({len(szclasses)}) exceeds uint16_t max ({UINT16_MAX})")
            
        return szclasses

    def get_span_size_classes(
        self
    ) -> list:
        szclasses  = []
        size        = self.page_size
        loop_max    = (1 << 62)
        
        while size <= loop_max:
            szclasses.append(size)
            p2          = 1 << int(math.log2(size))
            step        = max(self.page_size, p2 // self.steps_per_pow2)
            step        = (step // self.page_size) * self.page_size
            size       += step
            
        if len(szclasses) > UINT16_MAX:
            raise ValueError(f"Span size classes count ({len(szclasses)}) exceeds uint16_t max ({UINT16_MAX})")
            
        return szclasses

    def get_slab_infos(
        self, 
        sizes   : list
    ) -> list:
        infos = []
        min_blocks   = 4
        target_bytes = self.page_size
        prev_nblocks = float('inf')
        
        for i,block_size in enumerate(sizes):
            target_n    = max(min_blocks, target_bytes // block_size)
            req_bytes   = target_n * block_size
            slab_size   = ((req_bytes + self.page_size - 1) // self.page_size) * self.page_size
            
            nblocks     = slab_size // block_size
            nblocks     = min(nblocks, prev_nblocks)
            
            infos.append((block_size, slab_size, nblocks, i))
            prev_nblocks = nblocks
            
        return infos

    def get_tcache_limits(
        self, 
        sizes       : list, 
        slab_infos  : list
    ) -> list:
        limits  = []
        for i, size in enumerate(sizes):
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
        steps_per_pow2          = 4,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_epoch_max        = (1024 * 32),
        nbytes_default_span_size = (1024 * 1024 * 2)
    ),
    (1024 * 16): configuration(
        page_size               = (1024 * 16),
        min_align               = 16,
        steps_per_pow2          = 4,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_epoch_max        = (1024 * 32),
        nbytes_default_span_size = (1024 * 1024 * 2)
    ),
    (1024 * 32): configuration(
        page_size               = (1024 * 32),
        min_align               = 16,
        steps_per_pow2          = 4,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_epoch_max        = (1024 * 32),
        nbytes_default_span_size = (1024 * 1024 * 2)
    ),
    (1024 * 64): configuration(
        page_size               = (1024 * 64),
        min_align               = 16,
        steps_per_pow2          = 4,
        nbytes_slab_alloc_max   = (1024 * 16),
        nbytes_epoch_max        = (1024 * 32),
        nbytes_default_span_size = (1024 * 1024 * 2)
    ),
    (1024 * 1024 * 2): configuration(
        page_size               = (1024 * 1024 * 2),
        min_align               = 256,
        steps_per_pow2          = 4,
        nbytes_slab_alloc_max   = (1024 * 512),
        nbytes_epoch_max        = (1024 * 1024 * 16),
        nbytes_default_span_size = (1024 * 1024 * 32)
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


def write_global_file_header(f, maxn_globs=64) -> None:
    f.write("\n")
    f.write("#pragma\tonce\n")
    f.write("#ifndef\t_TSALLOC_CONFIG_H\n")
    f.write("#define\t_TSALLOC_CONFIG_H\n\n")
    f.write("\n")
    f.write("\n")
    f.write("#include\t\"../internal/common.h\"\n")
    f.write("\n")
    f.write("\n")
    f.write(f"#define\tTSALLOC_MAXN_GLOBS\t{maxn_globs}\n")
    f.write("\n")
    f.write("\n")

    f.write("struct tsalloc_slab_info\n")
    f.write("{\n")
    f.write("\tuint32_t\tblock_size;\n")
    f.write("\tuint32_t\tslab_size;\n")
    f.write("\tuint32_t\tnblocks;\n")
    f.write("\tuint32_t\tszclass;\n")
    f.write("};\n")
    f.write("typedef\tstruct tsalloc_slab_info\ttsalloc_slab_info_t;\n")
    f.write("\n")

    f.write("struct tsalloc_config\n")
    f.write("{\n")
    f.write("\tuint64_t\tpage_size;\n")
    f.write("\tuint64_t\tmin_align;\n")
    f.write("\tuint64_t\tslab_alloc_max;\n")
    f.write("\tuint64_t\tepoch_max;\n")
    f.write("\tuint64_t\tdefault_span_size;\n")
    f.write("\tuint32_t\tsteps_per_pow2;\n")
    f.write("\tuint32_t\tmin_align_shift;\n")
    f.write("\tuint32_t\tsteps_per_pow2_shift;\n")
    f.write("\tuint32_t\tnbytes_bitmap;\n")
    f.write("\tuint16_t\tnszclasses_slab;\n")
    f.write("\tuint16_t\tnszclasses_span;\n")
    f.write("\n")
    f.write("\tconst uint64_t*\t\t\t\tszclass_max_nbytes_slab;\n")
    f.write("\tconst uint64_t*\t\t\t\tszclass_max_nbytes_span;\n")
    f.write("\tconst uint16_t*\t\t\t\tszclass_of_nbytes_slab;\n")
    f.write("\tconst tsalloc_slab_info_t*\tslab_infos;\n")
    f.write("\tconst uint32_t*\t\t\t\ttcache_info;\n")
    f.write("};\n")
    f.write("typedef\tstruct tsalloc_config\ttsalloc_cfg_t;\n")
    f.write("\n\n")


def generate_config_c_block(
    cfg : configuration
) -> str:
    slab_sizes      = cfg.get_slab_size_classes()
    span_sizes      = cfg.get_span_size_classes()
    slab_count      = len(slab_sizes)
    span_count      = len(span_sizes)
    slab_infos      = cfg.get_slab_infos(slab_sizes)
    tcache_limits   = cfg.get_tcache_limits(slab_sizes, slab_infos)
    
    size2index  = []
    curr_idx    = 0
    for req_bytes in range(cfg.min_align, cfg.nbytes_slab_alloc_max + 1, cfg.min_align):
        while slab_sizes[curr_idx] < req_bytes:
            curr_idx += 1
        size2index.append(curr_idx)

    out = f"// -----[TSALLOC_CONFIG_START: {cfg.page_size}]-----\n\n"

    out += f"static const uint64_t\tszclass_max_nbytes_slab_{cfg.page_size}[{slab_count}]\t= {{\n"
    out += "    " + ", ".join(map(str, slab_sizes)) + "\n};\n\n"

    out += f"static const uint64_t\tszclass_max_nbytes_span_{cfg.page_size}[{span_count}]\t= {{\n"
    out += "    " + ", ".join(map(str, span_sizes)) + "\n};\n\n"

    out += f"static const uint16_t\tszclass_of_nbytes_slab_{cfg.page_size}[{len(size2index)}]\t= {{\n"
    out += "    " + ", ".join(map(str, size2index)) + "\n};\n\n"

    out += f"static const tsalloc_slab_info_t\tslab_infos_{cfg.page_size}[{slab_count}]\t= {{\n"
    for info in slab_infos:
        out += f"    {{{info[0]}, {info[1]}, {info[2]}, {info[3]}}},\n"
    out += "};\n\n"

    out += f"static const uint32_t\ttcache_info_{cfg.page_size}[{slab_count}]\t\t\t= {{\n"
    out += "    " + ", ".join(map(str, tcache_limits)) + "\n};\n\n"

    out += (
        f"static const tsalloc_cfg_t\tconfig_{cfg.page_size}\t= {{\n"
        f"\t.page_size                  = {cfg.page_size},\n"
        f"\t.min_align                  = {cfg.min_align},\n"
        f"\t.slab_alloc_max             = {cfg.nbytes_slab_alloc_max},\n"
        f"\t.epoch_max                  = {cfg.nbytes_epoch_max},\n"
        f"\t.default_span_size          = {cfg.nbytes_default_span_size},\n"
        f"\t.steps_per_pow2             = {cfg.steps_per_pow2},\n"
        f"\t.min_align_shift            = {cfg.min_align_shift},\n"
        f"\t.steps_per_pow2_shift       = {cfg.steps_per_pow2_shift},\n"
        f"\t.nbytes_bitmap              = {cfg.nbytes_bitmap},\n"
        f"\t.nszclasses_slab            = {slab_count},\n"
        f"\t.nszclasses_span            = {span_count},\n"
        f"\t.szclass_max_nbytes_slab    = szclass_max_nbytes_slab_{cfg.page_size},\n"
        f"\t.szclass_max_nbytes_span    = szclass_max_nbytes_span_{cfg.page_size},\n"
        f"\t.szclass_of_nbytes_slab     = szclass_of_nbytes_slab_{cfg.page_size},\n"
        f"\t.slab_infos                 = slab_infos_{cfg.page_size},\n"
        f"\t.tcache_info                = tcache_info_{cfg.page_size}\n"
        f"}};\n\n"
    )

    out += f"#define    TSALLOC_PAGESIZE_{cfg.page_size}\tconfig_{cfg.page_size}\n\n"
    out += f"// -----[TSALLOC_CONFIG_END: {cfg.page_size}]-----\n\n"
    
    return out


def remove_config_block(
    file_path   : str, 
    page_size   : int
) -> bool:
    if not os.path.exists(file_path):
        return False

    with open(file_path, "r") as f:
        lines = f.readlines()

    removed = False
    with open(file_path, "w") as f:
        skip_mode   = False
        start_tag   = f"// -----[TSALLOC_CONFIG_START: {page_size}]-----\n"
        end_tag     = f"// -----[TSALLOC_CONFIG_END: {page_size}]-----\n"
        
        for line in lines:
            if line == start_tag:
                skip_mode = True
                removed = True
            
            if not skip_mode:
                f.write(line)
                
            if skip_mode and line == end_tag:
                skip_mode = False
                
    return removed


def generate_registry(file_path: str) -> None:
    if not os.path.exists(file_path):
        return

    with open(file_path, "r") as f:
        content = f.read()

    page_sizes = re.findall(r"// -----\[TSALLOC_CONFIG_START: (\d+)\]-----", content)
    
    registry_c  = "// -----[TSALLOC_REGISTRY_START]-----\n\n"
    registry_c += "static inline const tsalloc_cfg_t*\n"
    registry_c += "tsconfig_get_cfg(\n"
    registry_c += "\tsize_t  page_size\n"
    registry_c += "){\n"
    registry_c += "\tswitch (page_size)\n\t{\n"
    
    for ps in page_sizes:
        registry_c += f"\t\tcase {ps}:\n\t\t\treturn &config_{ps};\n"
        
    registry_c += "\t\tdefault:\n\t\t\treturn NULL;\n"
    registry_c += "\t}\n}\n\n"
    registry_c += "// -----[TSALLOC_REGISTRY_END]-----\n"

    if "// -----[TSALLOC_REGISTRY_START]-----" in content:
        content = re.sub(
            r"// -----\[TSALLOC_REGISTRY_START\]-----.*?// -----\[TSALLOC_REGISTRY_END\]-----\n", 
            registry_c, 
            content, 
            flags=re.DOTALL
        )
    else:
        content = content.replace("#endif\t//_TSALLOC_CONFIG_H", registry_c + "#endif\t//_TSALLOC_CONFIG_H")

    with open(file_path, "w") as f:
        f.write(content)


def main():
    parser = argparse.ArgumentParser(
        description     = "generate _tsalloc_config.h.",
        formatter_class = lambda prog: argparse.HelpFormatter(prog, max_help_position = 50)
    )
     
    parser.add_argument(
        "-c", "--clear", 
        action  = "store_true", 
        help    = "clear configurations for existing page-size profiles"
    )
    parser.add_argument(
        "-d", "--default",
        action  = "store_true",
        help    = "generate configurations for default page-size profiles"
    )
    parser.add_argument(
        "-rm", "--remove",
        type    = type_bounded_pow2(UINT64_MAX, 1),
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
        type    = type_bounded_pow2(UINT64_MAX, 1),
    )
    parser.add_argument(
        "-ma", "--min-align",
        type    = type_bounded_pow2(UINT64_MAX, 1),
    )
    parser.add_argument(
        "-sp2", "--steps-per-pow2",
        type    = type_bounded_int(UINT32_MAX, 1),
    )
    parser.add_argument(
        "-sm", "--slab-alloc-max",
        type    = type_bounded_pow2(UINT64_MAX, 1),
    )
    parser.add_argument(
        "-em", "--epoch-max",
        type    = type_bounded_pow2(UINT64_MAX, 1),
    )
    parser.add_argument(
        "-dss", "--default-span-size",
        type    = type_bounded_pow2(UINT64_MAX, 1),
    )
    parser.add_argument(
        "-mg", "--max-globals",
        type    = type_bounded_int(UINT16_MAX, 1),
        default = 64,
        help    = "define maximum number of global arenas"
    )
    
    args        = parser.parse_args()
    script_path = os.path.dirname(os.path.abspath(__file__))
    file_path   = os.path.join(script_path, "_tsalloc_config.h")

    if args.clear:
        with open(file_path, "w") as f:
            write_global_file_header(f, args.max_globals)
            f.write("#endif\t//_TSALLOC_CONFIG_H\n")
        sys.exit(0)

    if args.remove:
        remove_config_block(file_path, args.remove)
        generate_registry(file_path)
        sys.exit(0)

    mode = "a" if args.append else "w"
    file_exists = os.path.exists(file_path)

    if args.default:
        with open(file_path, mode) as f:
            if mode == "w" or not file_exists:
                write_global_file_header(f, args.max_globals)
            for default_cfg in DEFAULT_CONFIGS.values():
                f.write(generate_config_c_block(default_cfg))
            if mode == "w" or not file_exists:
                f.write("#endif\t//_TSALLOC_CONFIG_H\n")
        generate_registry(file_path)
        sys.exit(0)

    config_args = [
        args.page_size, args.min_align, args.steps_per_pow2, 
        args.slab_alloc_max, args.epoch_max, args.default_span_size
    ]

    if any(config_args):
        if None in config_args:
            print("Error: Incomplete configuration. Provide all configuration arguments.")
            sys.exit(1)
            
        try:
            cfg = configuration(
                args.page_size, args.min_align, args.steps_per_pow2,
                args.slab_alloc_max, args.epoch_max, args.default_span_size
            )
        except ValueError as e:
            print(f"Configuration Error: {e}")
            sys.exit(1)
            
        if mode == "a" and file_exists:
            was_removed = remove_config_block(file_path, args.page_size)
            if was_removed:
                print(f"[Notice] Overwriting existing configuration for page size {args.page_size}.")
                
            if args.max_globals != 64:
                with open(file_path, "r") as f:
                    content = f.read()
                content = re.sub(r"#define\tTSALLOC_MAXN_GLOBS\t\d+", f"#define\tTSALLOC_MAXN_GLOBS\t{args.max_globals}", content)
                with open(file_path, "w") as f:
                    f.write(content)
        
        with open(file_path, mode) as f:
            if mode == "w" or not file_exists:
                write_global_file_header(f, args.max_globals)
            f.write(generate_config_c_block(cfg))
            if mode == "w" or not file_exists:
                f.write("#endif\t//_TSALLOC_CONFIG_H\n")
    else:
        if not (args.clear or args.remove or args.default):
            parser.print_help()

    if not args.clear:
        generate_registry(file_path)


if __name__ == "__main__":
    main()