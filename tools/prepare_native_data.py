#!/usr/bin/env python3
"""Cook all locally extracted disc data and hash the resulting standalone asset tree."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


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


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--exporter', type=Path, default=Path('build/native/recvx-assets'))
    mode = p.add_mutually_exclusive_group()
    mode.add_argument('--resume', action='store_true', help='Hash/verify an already completed export against the source dump.')
    mode.add_argument('--refresh', action='store_true', help='Rebuild the manifest after cooking new loose assets; no source dump is read.')
    mode.add_argument('--verify', action='store_true', help='Verify a packaged tree against its existing manifest.')
    a = p.parse_args()
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
    if not a.source:
        p.error('--source is required to export or verify the source dump')
    if root.exists():
        if not a.resume or not (root / 'native-data.version').is_file():
            raise RuntimeError('destination exists; --resume requires a completed native export')
    else:
        subprocess.run([str(a.exporter.resolve()), str(a.source.resolve()), str(root)], check=True)
    verify_source(a.source.resolve(), root)
    result = manifest(root)
    (root / 'asset-manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f"Cooked {result['rooms']} rooms, {result['textures']} editable textures; {result['bytes']} bytes.")

if __name__ == '__main__':
    main()
