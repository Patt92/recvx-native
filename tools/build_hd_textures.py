#!/usr/bin/env python3
"""Build content-keyed HD replacement textures from a match table.

Input: JSON from tools/match_hd_textures.py. Output: <output>/textures/hd/<md5>.tga,
where <md5> is the MD5 of the cooked PS2 TGA file (data/textures/...). The runtime
(`apply_texture_files`) replaces every texture with that content, in any room or
asset, when `--mods <output>` (or the mods directory beside the app) contains it.

RGB comes from the HD image; alpha always comes from the cooked PS2 texture so
cut-outs keep the original coverage. Nothing in data/ is modified.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path

from PIL import Image

TIER_A = (0.90, 0.05)   # min ncc, min gap to the runner-up
TIER_B = (0.80, 0.15)


def gap(match):
    return 1.0 if match['ncc2'] is None else match['ncc'] - match['ncc2']


def tier(match):
    if match['ncc'] >= TIER_A[0] and gap(match) >= TIER_A[1]:
        return 'A'
    if TIER_B[0] <= match['ncc'] < TIER_A[0] and gap(match) >= TIER_B[1]:
        return 'B'
    return None


def convert(job):
    key, dds, ps2_path, scale, out_path, scale_hd = job
    if os.path.exists(out_path):
        return key, 'exists', os.path.getsize(out_path)
    with Image.open(dds) as image:
        image.load()
        hd = image.convert('RGB')
    with Image.open(ps2_path) as image:
        image.load()
        base = image.convert('RGBA')
    w, h = base.size
    target = (w * scale, h * scale)
    if hd.size != target:
        hd = hd.resize(target, Image.LANCZOS)
    alpha = base.getchannel('A')
    if alpha.getextrema() == (255, 255):
        result = hd
    else:
        result = hd.convert('RGBA')
        result.putalpha(alpha.resize(target, Image.BILINEAR))
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    temporary = out_path + '.tmp'
    result.save(temporary, 'TGA')
    os.replace(temporary, out_path)
    return key, 'written', os.path.getsize(out_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--matches', type=Path, required=True)
    parser.add_argument('--textures', type=Path, default=Path('data/textures'))
    parser.add_argument('--output', type=Path, required=True, help='mods directory to create/extend')
    parser.add_argument('--scale', type=int, choices=(1, 2, 4), default=2, help='output size multiple of the PS2 texture')
    parser.add_argument('--tiers', default='AB')
    parser.add_argument('--only', default='', help='comma list of data/textures categories (default all)')
    parser.add_argument('--limit', type=int, default=0)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()
    table = json.loads(args.matches.read_text())
    ps2 = table['ps2']
    chosen = {}
    for dds, match in table['matches'].items():
        if not match or match.get('status') != 'ok':
            continue
        t = tier(match)
        if t is None or t not in args.tiers:
            continue
        current = chosen.get(match['ps2'])
        if current is None or match['ncc'] > current[1]['ncc']:
            chosen[match['ps2']] = (dds, match, t)
    only = {c for c in args.only.split(',') if c}
    jobs, estimate = [], 0
    for digest, (dds, match, t) in sorted(chosen.items()):
        paths = ps2[digest]
        if only and paths[0].split('/')[0] not in only:
            continue
        with Image.open(args.textures / paths[0]) as image:
            w, h = image.size
            has_alpha = image.mode == 'RGBA'
        estimate += w * h * args.scale ** 2 * (4 if has_alpha else 3) + 18
        out = args.output / 'textures' / 'hd' / (digest + '.tga')
        jobs.append((digest, dds, str(args.textures / paths[0]), args.scale, str(out), match['scale']))
    if args.limit:
        jobs = jobs[::max(1, len(jobs) // args.limit)][:args.limit]
    print(f'{len(jobs)} replacement textures at {args.scale}x, about {estimate / 1e9:.1f} GB uncompressed TGA')
    if args.dry_run:
        return
    written = skipped = size = 0
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for i, (key, status, nbytes) in enumerate(pool.map(convert, jobs, chunksize=8), 1):
            written += status == 'written'
            skipped += status == 'exists'
            size += nbytes
            if i % 500 == 0:
                print(f'{i}/{len(jobs)}')
    manifest = {digest: {'dds': Path(dds).name, 'ncc': m['ncc'], 'tier': t, 'ps2': ps2[digest][0]}
                for digest, (dds, m, t) in chosen.items()}
    (args.output / 'textures').mkdir(parents=True, exist_ok=True)
    (args.output / 'textures' / 'hd-manifest.json').write_text(json.dumps(manifest, indent=0))
    print(f'written {written}, already present {skipped}, {size / 1e9:.2f} GB in {args.output / "textures" / "hd"}')


if __name__ == '__main__':
    main()
