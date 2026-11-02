#!/usr/bin/env python3
"""Regenerate the checked-in SPIR-V of the Vulkan raster backend from native/gpu_raster_vulkan.glsl.

Every build (desktop, cross, Android, CI) compiles the Vulkan backend from these files, so all
platforms run byte-identical shader modules and no build needs glslc. After editing
gpu_raster_vulkan.glsl run this script with the pinned glslc (GLSLC_VERSION) and commit the result;
the CTest vulkan_spirv_current checks the files against the GLSL wherever that glslc is installed.
--check only compares and exits 1 on a difference."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'native/gpu_raster_vulkan.glsl'
OUTPUT = ROOT / 'native/third_party/vulkan_spirv'
# name, shader stage, stage macro (gpu_raster_vulkan.glsl); same list as CMakeLists.txt.
STAGES = [('raster_vs', 'vert', 'RASTER_VS'), ('raster_fs', 'frag', 'RASTER_FS'), ('mask_vs', 'vert', 'MASK_VS'),
          ('mask_fs', 'frag', 'MASK_FS'), ('full_vs', 'vert', 'FULL_VS'), ('composite_fs', 'frag', 'COMPOSITE_FS'),
          ('resolve_cs', 'comp', 'RESOLVE_CS')]
GLSLC_VERSION = 'shaderc v2026.4'


def generate(glslc, directory):
    for name, kind, macro in STAGES:
        # -O0: der Optimierer darf keine Rechnung umformen; NoContraction/float_controls stehen im SPIR-V.
        subprocess.run([glslc, f'-fshader-stage={kind}', f'-D{macro}', '--target-env=vulkan1.1', '-O0', '-mfmt=num',
                        str(SOURCE), '-o', str(directory / f'gpu_raster_vulkan_{name}.inc')], check=True)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--glslc', default=shutil.which('glslc') or 'glslc')
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    version = subprocess.run([a.glslc, '--version'], capture_output=True, text=True, check=True).stdout.splitlines()[0]
    if not version.startswith(GLSLC_VERSION):
        print(f'glslc is "{version}", the checked-in SPIR-V is pinned to "{GLSLC_VERSION}"', file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as temporary:
        generate(a.glslc, Path(temporary))
        fresh = {path.name: path.read_bytes() for path in Path(temporary).iterdir()}
    if a.check:
        stale = [name for name, data in fresh.items() if not (OUTPUT / name).is_file() or (OUTPUT / name).read_bytes() != data]
        if stale:
            print('checked-in SPIR-V differs from gpu_raster_vulkan.glsl: ' + ', '.join(sorted(stale))
                  + ' (run tools/update_vulkan_spirv.py)', file=sys.stderr)
            return 1
        print(f'{len(fresh)} SPIR-V modules match gpu_raster_vulkan.glsl ({version})')
        return 0
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, data in fresh.items():
        (OUTPUT / name).write_bytes(data)
    print(f'wrote {len(fresh)} SPIR-V modules to {OUTPUT} ({version})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
