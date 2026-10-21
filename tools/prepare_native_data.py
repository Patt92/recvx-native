#!/usr/bin/env python3
"""Cook disc data into the standalone asset tree.

The source is either an already extracted disc directory (--source) or one or
more PS2 ISO9660 images (--iso) extracted for this build. --iso produces the
complete tree in one step: disc export, per-room actor/object/item textures and
native movie packets. The runtime only sees the loose cooked tree."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from iso9660 import extract
from host_tools import find as find_host_tool


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        while chunk := f.read(4 * 1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def manifest(root):
    records = []
    for path in sorted(root.rglob('*')):
        if not path.is_file() or path.name == 'asset-manifest.json':
            continue
        if path.is_symlink() or path.suffix.lower() in {'.iso', '.afs'}:
            raise RuntimeError(f'cooked data must contain regular loose assets: {path}')
        records.append({'path': path.relative_to(root).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest(path)})
    return {'format': 'recvx-assets-1', 'files': len(records), 'bytes': sum(r['bytes'] for r in records),
            'rooms': sum(r['path'].startswith('rooms/') for r in records),
            'textures': sum(r['path'].startswith('textures/') for r in records), 'assets': records}


def verify_source(source, root):
    count = 0
    for path in sorted(source.rglob('*')):
        if not path.is_file():
            continue
        if path.suffix.upper() != '.AFS':
            target = root / 'files' / path.relative_to(source)
            if digest(path) != digest(target):
                raise RuntimeError(f'disc file mismatch: {path}')
        else:
            with path.open('rb') as afs:
                magic, entries = struct.unpack('<II', afs.read(8))
                if magic & 0xFFFFFF != 0x534641 or entries > 100000:
                    raise RuntimeError(f'invalid AFS: {path}')
                index = afs.read(entries * 8)
                for i in range(entries):
                    offset, size = struct.unpack_from('<II', index, i * 8)
                    afs.seek(offset)
                    h = hashlib.sha256()
                    remaining = size
                    while remaining:
                        data = afs.read(min(remaining, 4 * 1024 * 1024))
                        if not data:
                            raise RuntimeError(f'truncated archive: {path}')
                        h.update(data)
                        remaining -= len(data)
                    target = root / 'archives' / path.stem.upper() / f'{i:04}.bin'
                    if h.hexdigest() != digest(target):
                        raise RuntimeError(f'archive entry mismatch: {path}, {i}')
        count += 1
    print(f'Verified all {count} extracted disc files and every archive entry.', flush=True)


def export(source, root, exporter, resume):
    if root.exists():
        if not resume or not (root / 'native-data.version').is_file():
            raise RuntimeError('destination exists; --resume requires a completed native export')
    else:
        subprocess.run([str(exporter.resolve()), str(source.resolve()), str(root)], check=True)
    verify_source(source.resolve(), root)
    result = manifest(root)
    (root / 'asset-manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f"Cooked {result['rooms']} rooms, {result['textures']} editable textures; {result['bytes']} bytes.")


def world_exporter_for(exporter):
    return exporter.with_name('recvx-world-assets' + exporter.suffix)


def export_world_textures(world_exporter, root, jobs):
    rooms = sorted(path.stem for path in (root / 'rooms').glob('RM_*.RDT'))
    if not rooms:
        raise RuntimeError(f'no exported rooms for world textures in {root}')
    # Per-room subdirectories are disjoint; only the shared parents could race.
    for kind in ('npcs', 'objects', 'room-items'):
        (root / 'textures' / kind).mkdir(parents=True, exist_ok=True)

    def one(room):
        result = subprocess.run([str(world_exporter), str(root), room], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f'world texture export failed for {room}: '
                               f'{(result.stderr or result.stdout).strip()}')
        match = re.search(r'Exported (\d+) ', result.stdout)
        if not match:
            raise RuntimeError(f'unexpected world texture exporter output for {room}: {result.stdout.strip()}')
        return int(match.group(1))

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        total = sum(pool.map(one, rooms))
    print(f'Exported {total} actor/object/item textures for {len(rooms)} rooms.', flush=True)


def cook_movies(root, ffmpeg, ffprobe):
    subprocess.run([sys.executable, str(ROOT / 'tools/cook_movies.py'), '--data', str(root),
                    '--ffmpeg', ffmpeg, '--ffprobe', ffprobe], check=True)


def staging_parent(root):
    # A rename into place must stay on one volume; prefer the ignored build tree.
    build = ROOT / 'build'
    build.mkdir(parents=True, exist_ok=True)
    return build if build.stat().st_dev == root.parent.stat().st_dev else root.parent


def export_iso(images, root, exporter, ffmpeg, ffprobe, jobs):
    if root.exists():
        raise RuntimeError(f'destination exists; --iso only creates a new data tree: {root}')
    exporter = exporter.resolve()
    world = world_exporter_for(exporter)
    for tool in (exporter, world):
        if not tool.is_file():
            raise RuntimeError(f'missing built exporter: {tool}')
    tools = [find_host_tool(ffmpeg, ROOT), find_host_tool(ffprobe, ROOT)]
    if not all(tools):
        raise RuntimeError('--iso cooks the movies and needs ffmpeg and ffprobe on PATH '
                           'or, on Windows, build-tools/ffmpeg.exe and build-tools/ffprobe.exe '
                           '(see --ffmpeg/--ffprobe for explicit paths)')
    root.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=f'.{root.name}-staging-', dir=staging_parent(root)))
    try:
        cooked = stage / root.name
        with tempfile.TemporaryDirectory(prefix='iso-source-', dir=stage) as source:
            files, total, count = extract(images, source)
            print(f'Extracted {files} disc files / {total} bytes from {count} ISO image(s).', flush=True)
            subprocess.run([str(exporter), source, str(cooked)], check=True)
            verify_source(Path(source), cooked)
        export_world_textures(world, cooked, jobs)
        cook_movies(cooked, *tools)
        result = manifest(cooked)
        (cooked / 'asset-manifest.json').write_text(json.dumps(result, indent=2) + '\n')
        cooked.rename(root)
    finally:
        shutil.rmtree(stage, ignore_errors=True)
    print(f"Cooked {result['rooms']} rooms, {result['textures']} editable textures; "
          f"{result['files']} files / {result['bytes']} bytes in {root}.", flush=True)


def default_exporter():
    # build.sh builds into build/macos-arm64 on macOS, build/native elsewhere.
    for directory in ('build/native', 'build/macos-arm64'):
        candidate = ROOT / directory / 'recvx-assets'
        if candidate.is_file():
            return candidate
    return ROOT / 'build/native/recvx-assets'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path)
    p.add_argument('--iso', type=Path, action='append',
                   help='PS2 ISO9660 image to extract at build time; repeat to merge discs')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--exporter', type=Path, default=default_exporter(),
                   help='recvx-assets; --iso also uses recvx-world-assets from the same directory')
    p.add_argument('--ffmpeg', default='ffmpeg', help='used by --iso to cook the movies')
    p.add_argument('--ffprobe', default='ffprobe', help='used by --iso to cook the movies')
    p.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8),
                   help='parallel world texture exports for --iso')
    mode = p.add_mutually_exclusive_group()
    mode.add_argument('--resume', action='store_true', help='Hash/verify an already completed export against the source dump.')
    mode.add_argument('--refresh', action='store_true', help='Rebuild the manifest after cooking new loose assets; no source dump is read.')
    mode.add_argument('--verify', action='store_true', help='Verify a packaged tree against its existing manifest.')
    a = p.parse_args()
    if a.iso and a.source:
        p.error('--source and --iso are mutually exclusive')
    if a.iso and (a.refresh or a.verify or a.resume):
        p.error('--iso creates a new tree and cannot be combined with --resume, --refresh or --verify')
    if not 1 <= a.jobs <= 256:
        p.error('--jobs must be between 1 and 256')
    root = a.output.resolve()
    if a.refresh:
        if not (root / 'native-data.version').is_file():
            raise RuntimeError('--refresh requires an existing completed native export')
        result = manifest(root)
        (root / 'asset-manifest.json').write_text(json.dumps(result, indent=2) + '\n')
        print(f"Manifest refreshed: {result['files']} files / {result['bytes']} bytes.")
        return
    if a.verify:
        saved = json.loads((root / 'asset-manifest.json').read_text())
        current = manifest(root)
        if saved != current:
            raise RuntimeError('asset manifest differs from the actual file tree')
        print(f"Verified {current['files']} files / {current['bytes']} bytes.")
        return
    if a.iso:
        export_iso(a.iso, root, a.exporter, a.ffmpeg, a.ffprobe, a.jobs)
        return
    if not a.source:
        p.error('--source or --iso is required to export or verify the source dump')
    export(a.source, root, a.exporter, a.resume)

if __name__ == '__main__':
    main()
