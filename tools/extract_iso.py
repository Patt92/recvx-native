#!/usr/bin/env python3
"""Prepare the game data of a downloaded runtime from your own disc images, without a build.

Put the disc image(s) into the folder iso/ next to the runtime (disc 1 and disc 2 may both lie
there; they are merged) and run this script. It writes data/ into the main folder of the runtime
and creates an empty mods/ beside it; the runtime finds both without --data. The same script
works in a source checkout (main folder = repository root, exporters from build/).

Needs Python 3.12+ and FFmpeg (ffmpeg and ffprobe on PATH, or on Windows build-tools/ffmpeg.exe
and build-tools/ffprobe.exe in the main folder) for the movies."""
import argparse
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import prepare_native_data  # noqa: E402

SUFFIX = '.exe' if os.name == 'nt' else ''


def find_exporter(root):
    """recvx-assets of the downloaded package (main folder or bin/) or of a source build."""
    for directory in ('.', 'bin', 'build/native', 'build/macos-arm64', 'build/windows-native',
                      'build/windows-arm64-native', 'build/native/Release'):
        candidate = root / directory / f'recvx-assets{SUFFIX}'
        world = candidate.with_name(f'recvx-world-assets{SUFFIX}')
        if candidate.is_file() and world.is_file():
            return candidate.resolve()
    return None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--iso', type=Path, action='append',
                   help='disc image; repeat for both discs (default: every .iso in <main folder>/iso)')
    p.add_argument('--root', type=Path, default=ROOT, help='main folder (default: the folder above tools/)')
    p.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    p.add_argument('--ffmpeg', default='ffmpeg')
    p.add_argument('--ffprobe', default='ffprobe')
    a = p.parse_args()
    root = a.root.resolve()
    data = root / 'data'
    if (data / 'native-data.version').is_file():
        print(f'{data} is already prepared; delete it to extract again.')
        return 0
    if data.exists():
        p.error(f'{data} exists without a completed export; delete it and run again')
    images = a.iso or sorted(path for path in (root / 'iso').glob('*') if path.suffix.lower() == '.iso')
    if not images:
        p.error(f'no disc image: put your .iso file(s) into {root / "iso"} (or pass --iso)')
    for image in images:
        if not image.is_file():
            p.error(f'disc image not found: {image}')
    exporter = find_exporter(root)
    if not exporter:
        p.error(f'recvx-assets{SUFFIX} and recvx-world-assets{SUFFIX} not found in {root} or {root / "bin"}')
    if a.jobs < 1:
        p.error('--jobs must be at least 1')
    print(f'Extracting {", ".join(str(i) for i in images)} into {data} ...', flush=True)
    prepare_native_data.export_iso([i.resolve() for i in images], data, exporter, a.ffmpeg, a.ffprobe, a.jobs)
    (root / 'mods').mkdir(exist_ok=True)
    print(f'Done. Game data: {data}\nMods: {root / "mods"}\nStart the runtime from {root}; it finds data/ by itself.')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, OSError) as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)
