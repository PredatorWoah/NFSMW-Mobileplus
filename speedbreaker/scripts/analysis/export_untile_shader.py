#!/usr/bin/env python3
"""Export the actual game-free untile shader and its supported variants.

Uses a host C++ compiler to expand the same C++/GLSL macros as renderer.cpp.
Compile the resulting .comp files with glslangValidator or the NDK's glslc.
"""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = (root / "runtime/gpu/renderer.cpp").read_text()
    start = source.index("const char* kUntileGlsl =")
    end = source.index("VkPipeline s_untilePipeline", start)
    helper = output / "export_untile.cpp"
    helper.write_text(
        '#include <gpu/bc_decode_glsl.h>\n#include <gpu/shared_memory_glsl.h>\n'
        '#include <fstream>\n#include <string>\n' + source[start:end] +
        '\nint main(int argc,char** argv) { if(argc!=2)return 2; '
        'std::ofstream out(argv[1]);out<<kUntileGlsl;return out.good()?0:1;}\n'
    )
    executable = output / "export_untile"
    subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++20", "-O2",
                    "-I" + str(root / "runtime"), str(helper), "-o", str(executable)], check=True)
    raw = output / "untile-source.comp"
    subprocess.run([str(executable), str(raw)], check=True)
    text = raw.read_text()
    first, rest = text.split("\n", 1)
    for variant, defines in {"native": "", "bc": "#define BC_DECODE\n",
                             "image": "#define TO_IMAGE\n"}.items():
        for split in (False, True):
            preamble = defines + ("#define SPLIT_MEMORY\n" if split else "")
            name = "untile-" + variant + ("-split" if split else "") + ".comp"
            (output / name).write_text(first + "\n" + preamble + rest)
    print("Exported six actual untile variants to", output)


if __name__ == "__main__":
    main()
