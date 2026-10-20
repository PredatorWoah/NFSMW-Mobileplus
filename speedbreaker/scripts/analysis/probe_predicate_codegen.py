#!/usr/bin/env python3
"""Compiler discrimination probe using synthetic shader_variants_test output.

Not a game shader or GPU performance test. See ANDROID_60FPS_INVESTIGATION.md.
"""
import argparse
import collections
import json
from pathlib import Path
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('shader_directory', type=Path)
    p.add_argument('--glslang', required=True)
    p.add_argument('--dis', required=True)
    p.add_argument('--val', required=True)
    args = p.parse_args()
    directory = args.shader_directory
    source = (directory / 'texture-early.frag').read_text()
    prefix = source[:source.index('void main()')]
    body = ('void main(){ vec4 sum=vec4(0); bool pred=u.bools[0].x!=0u; '
            'vec2 uv=u.c[0].xy;\n')
    samples = [
        f'sum += textureLod(t2d_1,uv+vec2({i % 3 - 1},{i // 3 - 1})'
        '*u.textureReciprocal[1].xy,0.0);'
        for i in range(9)
    ]
    probes = {
        'predicate-separate': body + '\n'.join('if(pred){' + x + '}' for x in samples) + '\no_c0=sum;}\n',
        'predicate-merged': body + 'if(pred){' + ''.join(samples) + '}\no_c0=sum;}\n',
        'max-copy': 'void main(){o_c0=vmax(u.c[0],u.c[0]);}\n',
        'direct-copy': 'void main(){o_c0=u.c[0];}\n',
    }
    records = []
    for target in ['vulkan1.1', 'vulkan1.2']:
        for name, body in probes.items():
            glsl = directory / f'probe-{name}.frag'
            glsl.write_text(prefix + body)
            spv = glsl.with_suffix('.' + target + '.spv')
            subprocess.run([args.glslang, '-V', '--target-env', target, '-Os', '-g0',
                            str(glsl), '-o', str(spv)], check=True, stdout=subprocess.DEVNULL)
            subprocess.run([args.val, '--target-env', target, str(spv)], check=True)
            asm = subprocess.check_output([args.dis, str(spv)], text=True)
            glsl.with_suffix('.' + target + '.spvasm').write_text(asm)
            ops = collections.Counter(
                line.strip().split(' = ')[-1].split()[0]
                for line in asm.splitlines() if line.strip() and not line.strip().startswith(';')
            )
            row = dict(probe=name, target=target, words=spv.stat().st_size // 4,
                       branches=ops['OpBranchConditional'], selections=ops['OpSelectionMerge'],
                       samples=ops['OpImageSampleExplicitLod'], select=ops['OpSelect'])
            records.append(row)
            print(json.dumps(row))
    (directory / 'compiler-probe-results.json').write_text(json.dumps(records, indent=2) + '\n')


if __name__ == '__main__':
    main()
