#!/usr/bin/env python3
"""Match Dolphin HD texture-pack DDS files (GameCube) to cooked PS2 textures.

The pack's file name carries the ORIGINAL GameCube texture size (tex1_<w>x<h>_...);
the DDS itself is an integer multiple of it. Each DDS is box-downscaled to the
original size and compared with cooked PS2 textures of the same size by RGB error.
Output is a JSON match table, never a modification of data/.

Needs Pillow + numpy (see docs/NATIVE_ASSETS.md "HD texture packs").
"""
import argparse
import collections
import concurrent.futures
import hashlib
import json
import os
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

NAME = re.compile(r'tex1_(\d+)x(\d+)_')
THUMB = 8


def load_rgb(path):
    with Image.open(path) as im:
        im.load()
        return im.convert('RGB')


def thumb(arr):
    h, w, _ = arr.shape
    t = THUMB
    return arr.reshape(t, h // t, t, w // t, 3).mean(axis=(1, 3)).reshape(-1)


def index_ps2(textures_root, categories):
    """cooked TGA content hash -> {size, paths, thumb}"""
    unique = {}
    for category in categories:
        for path in sorted((textures_root / category).rglob('*.tga')):
            digest = hashlib.md5(path.read_bytes()).hexdigest()
            entry = unique.get(digest)
            if entry is None:
                rgb = load_rgb(path)
                w, h = rgb.size
                if w < THUMB or h < THUMB or w % THUMB or h % THUMB:
                    unique[digest] = {'size': (w, h), 'paths': [], 'thumb': None}
                else:
                    unique[digest] = {'size': (w, h), 'paths': [], 'thumb': thumb(np.asarray(rgb, dtype=np.float32))}
                entry = unique[digest]
            entry['paths'].append(str(path.relative_to(textures_root)))
    return unique


def luma(arr):
    return arr @ np.array([0.299, 0.587, 0.114], dtype=np.float32)


def ncc(a, b):
    a = a - a.mean()
    b = b - b.mean()
    denominator = float(np.sqrt((a * a).sum() * (b * b).sum()))
    if denominator < 1e-3:  # both flat: decide by mean colour elsewhere
        return 1.0 if abs(float(a.std()) - float(b.std())) < 1.0 else 0.0
    return float((a * b).sum() / denominator)


def worker(args):
    path, candidates_by_size, textures_root = args
    match = NAME.search(os.path.basename(path))
    if not match:
        return path, None
    w, h = int(match.group(1)), int(match.group(2))
    cands = candidates_by_size.get((w, h))
    if not cands:
        return path, {'status': 'no-size'}
    try:
        hd = load_rgb(path)
    except Exception as exc:  # decoder failure is reported, not hidden
        return path, {'status': 'decode-error', 'error': str(exc)}
    if hd.size[0] % w or hd.size[1] % h or hd.size[0] // w != hd.size[1] // h:
        return path, {'status': 'bad-scale', 'size': hd.size}
    scale = hd.size[0] // w
    small = np.asarray(hd.reduce(scale) if scale > 1 else hd, dtype=np.float32)
    key = thumb(small)
    # Shortlist by thumbnail correlation plus colour distance, then verify at full size.
    ranked = sorted(((-ncc(key, c[1]) + float(np.mean((key - c[1]) ** 2)) / 20000.0, c[0]) for c in cands))[:8]
    luma_small = luma(small)
    scored = []
    for _, digest in ranked:
        ref = ps2_pixels(textures_root, digest, cands)
        mse = float(np.mean((small - ref) ** 2))
        # Colour checks keep e.g. red and green herb variants apart: luma alone ignores hue.
        rgb = float(np.mean([ncc(small[..., c], ref[..., c]) for c in range(3)]))
        mean_delta = float(np.max(np.abs(small.reshape(-1, 3).mean(0) - ref.reshape(-1, 3).mean(0))))
        scored.append((ncc(luma_small, luma(ref)), mse, digest, rgb, mean_delta))
    scored.sort(reverse=True)
    best, second = scored[0], scored[1] if len(scored) > 1 else None
    psnr = 99.0 if best[1] == 0 else 10 * np.log10(255 * 255 / best[1])
    return path, {'status': 'ok', 'ps2': best[2], 'ncc': round(best[0], 4), 'psnr': round(float(psnr), 2), 'scale': scale,
                  'ncc2': round(second[0], 4) if second else None,
                  'top': [[d, round(n, 4), round(r, 4), round(md, 2)] for n, _, d, r, md in scored[:4]]}


_PIXELS = {}


def ps2_pixels(root, digest, cands):
    if digest not in _PIXELS:
        rel = next(c[2] for c in cands if c[0] == digest)
        _PIXELS[digest] = np.asarray(load_rgb(Path(root) / rel), dtype=np.float32)
    return _PIXELS[digest]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, required=True, help='.../Load/Textures/GCDE08')
    parser.add_argument('--textures', type=Path, default=Path('data/textures'))
    parser.add_argument('--categories', nargs='+', default=['rooms', 'items', 'menu', 'fonts', 'players', 'npcs', 'objects', 'room-items'])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=0, help='first N DDS files (sampling)')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()
    ps2 = index_ps2(args.textures, args.categories)
    by_size = collections.defaultdict(list)
    for digest, entry in ps2.items():
        if entry['thumb'] is not None:
            by_size[entry['size']].append((digest, entry['thumb'], entry['paths'][0]))
    print(f'PS2: {len(ps2)} unique textures, {sum(len(e["paths"]) for e in ps2.values())} files', file=sys.stderr)
    files = sorted(str(p) for p in args.pack.rglob('*.dds'))
    if not files:
        raise SystemExit(f'no DDS files under {args.pack} (pack missing or volume not mounted); output left unchanged')
    if args.limit:
        step = max(1, len(files) // args.limit)
        files = files[::step][:args.limit]
    results = {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        jobs = [(f, by_size, str(args.textures)) for f in files]
        for i, (path, result) in enumerate(pool.map(worker, jobs, chunksize=4), 1):
            results[path] = result
            if i % 200 == 0:
                print(f'{i}/{len(files)}', file=sys.stderr)
    table = {'ps2': {d: e['paths'] for d, e in ps2.items()}, 'matches': results}
    args.output.write_text(json.dumps(table))
    ok = [r for r in results.values() if r and r.get('status') == 'ok']
    print(f'{len(files)} DDS; matched candidates {len(ok)}; statuses', collections.Counter((r or {}).get('status') for r in results.values()))
    nccs = sorted(r['ncc'] for r in ok)
    if nccs:
        print('NCC percentiles', {p: nccs[int(len(nccs) * p / 100)] for p in (5, 25, 50, 75, 95)})


if __name__ == '__main__':
    main()
