#!/usr/bin/env python3
"""Import a PCSX2 texture replacement pack as content-keyed HD textures.

PCSX2 names each replacement `<TEX0 hash>-<CLUT hash>-<bits>.dds` (or `<TEX0 hash>-<bits>.dds` without
palette). The hashes are xxHash values over the GS memory image of the texture; the port renders without a
GS memory and its cooked textures carry no TEX0, so it cannot recompute them. The name's `bits` field holds
TEX0 PSM (bits 0-5), TW (6-9) and TH (10-13), i.e. the PS2 texture size. Each DDS is box-downscaled to that
size and matched by image content against the cooked textures of the same size (the procedure of
tools/match_hd_textures.py, Pillow only).

Output: <output>/textures/hd/<md5>.tga, where <md5> is the MD5 of the cooked data TGA, plus
<output>/textures/hd-manifest-pcsx2.json. RGB comes from the pack, alpha from the cooked PS2 texture.
Existing HD files are kept (use --replace to overwrite). Nothing in data/ or the pack is modified.
"""
import argparse
import hashlib
import json
import math
import os
import re
import struct
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image, ImageChops, ImageStat

NAME = re.compile(r'^([0-9a-f]{1,16})-(?:([0-9a-f]{1,16})-)?([0-9a-f]{8})(?:-mip(\d+))?\.dds$', re.I)
THUMB = 16
TIER_A = (0.90, 0.05)   # min ncc, min gap to the runner-up (tools/build_hd_textures.py)
TIER_B = (0.80, 0.15)
DUPLICATE = 0.98      # luma NCC above which two cooked textures count as the same picture


def gs_size(bits):
    return 1 << ((bits >> 6) & 15), 1 << ((bits >> 10) & 15)


def pack_entries(pack):
    entries, skipped = [], []
    for path in sorted(pack.iterdir()):
        if not path.is_file() or path.suffix.lower() != '.dds':
            continue
        m = NAME.match(path.name)
        if not m or m.group(4):  # mip levels follow their base texture
            skipped.append(path.name)
            continue
        entries.append((path, gs_size(int(m.group(3), 16))))
    return entries, skipped


def tga_size(path):
    with path.open('rb') as f:
        head = f.read(18)
    return struct.unpack_from('<HH', head, 12)


def index_cooked(textures, sizes, jobs):
    """md5 of the cooked TGA -> (size, paths); only sizes the pack needs."""
    paths = [p for p in sorted(textures.rglob('*.tga')) if tga_size(p) in sizes]

    def digest(path):
        return hashlib.md5(path.read_bytes()).hexdigest(), path
    by_digest = {}
    with ThreadPoolExecutor(jobs) as pool:
        for key, path in pool.map(digest, paths, chunksize=64):
            by_digest.setdefault(key, []).append(path)
    return by_digest


def load_rgb(path):
    with Image.open(path) as image:
        image.load()
        return image.convert('RGB')


def thumb(image):
    w, h = image.size
    return image.reduce((max(1, w // THUMB), max(1, h // THUMB))).resize((THUMB, THUMB), Image.BOX)


def luma_values(image, limit=64):
    w, h = image.size
    small = image.reduce((max(1, w // limit), max(1, h // limit))) if w > limit or h > limit else image
    return list(small.convert('L').tobytes())


def ncc(a, b):
    n = len(a)
    ma, mb = sum(a) / n, sum(b) / n
    sab = saa = sbb = 0.0
    for x, y in zip(a, b):
        dx, dy = x - ma, y - mb
        sab += dx * dy
        saa += dx * dx
        sbb += dy * dy
    return 1.0 if saa == 0 and sbb == 0 else (0.0 if saa == 0 or sbb == 0 else sab / math.sqrt(saa * sbb))


def tier(best, second):
    gap = 1.0 if second is None else best - second
    if best >= TIER_A[0] and gap >= TIER_A[1]:
        return 'A'
    if TIER_B[0] <= best < TIER_A[0] and gap >= TIER_B[1]:
        return 'B'
    return None


class Candidates:
    """Cooked textures of one size: thumbnails in one image strip for a C-speed shortlist."""

    def __init__(self, digests, by_digest):
        self.digests = digests
        self.paths = [by_digest[d][0] for d in digests]
        self.images = [load_rgb(p) for p in self.paths]
        self.strip = Image.frombytes('RGB', (THUMB, THUMB * len(digests)), b''.join(thumb(i).tobytes() for i in self.images))
        self.luma = {}

    def shortlist(self, query, count=8):
        tiled = Image.frombytes('RGB', self.strip.size, thumb(query).tobytes() * len(self.digests))
        per_tile = ImageChops.difference(self.strip, tiled).reduce(THUMB).tobytes()
        scores = sorted((sum(per_tile[3 * i:3 * i + 3]), i) for i in range(len(self.digests)))
        return [i for _, i in scores[:count]]

    def luma_of(self, i):
        if i not in self.luma:
            self.luma[i] = luma_values(self.images[i])
        return self.luma[i]


def match(entry, candidates):
    path, (w, h) = entry
    group = candidates.get((w, h))
    if group is None:
        return path, {'status': 'no-candidate', 'gs_size': [w, h]}
    try:
        hd = load_rgb(path)
    except Exception as exc:  # a decoder failure is reported, not hidden
        return path, {'status': 'decode-error', 'error': str(exc)}
    if hd.size[0] % w or hd.size[1] % h or hd.size[0] // w != hd.size[1] // h:
        return path, {'status': 'bad-scale', 'size': list(hd.size), 'gs_size': [w, h]}
    scale = hd.size[0] // w
    small = hd.reduce(scale) if scale > 1 else hd
    query = luma_values(small)
    scored = sorted(((ncc(query, group.luma_of(i)), i) for i in group.shortlist(small)), reverse=True)
    best = scored[0]
    # Ein Zweitplatzierter, der selbst fast dasselbe gekochte Bild ist (Varianten derselben Textur), macht die
    # Zuordnung nicht unsicher; der Abstand zaehlt erst gegen den naechsten wirklich anderen Kandidaten.
    distinct = [c for c in scored[1:] if ncc(group.luma_of(best[1]), group.luma_of(c[1])) < DUPLICATE]
    second = distinct[0] if distinct else None
    rms = sum(v * v for v in ImageStat.Stat(ImageChops.difference(small, group.images[best[1]])).rms) / 3
    psnr = 99.0 if rms == 0 else 20 * math.log10(255 / math.sqrt(rms))
    return path, {'status': 'ok', 'ps2': group.digests[best[1]], 'ncc': round(best[0], 4),
                  'ncc2': round(second[0], 4) if second else None, 'psnr': round(psnr, 2), 'scale': scale,
                  'tier': tier(best[0], second[0] if second else None)}


def convert(job):
    dds, ps2_path, scale, out_path = job
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
    out_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = out_path.with_suffix('.tmp')
    result.save(temporary, 'TGA')
    os.replace(temporary, out_path)
    return out_path.stat().st_size


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--pack', type=Path, required=True, help='folder with the PCSX2 <hash>-<hash>-<bits>.dds files')
    parser.add_argument('--data', type=Path, default=Path('data'), help='cooked data tree (uncompressed textures)')
    parser.add_argument('--output', type=Path, required=True, help='mods directory to create/extend (textures/hd)')
    parser.add_argument('--scale', type=int, choices=(1, 2, 4, 8), default=4, help='output size multiple of the PS2 texture')
    parser.add_argument('--tiers', default='AB')
    parser.add_argument('--replace', action='store_true', help='overwrite existing HD files')
    parser.add_argument('--dry-run', action='store_true', help='only match and write the manifest')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    args = parser.parse_args()

    entries, skipped = pack_entries(args.pack)
    if not entries:
        raise SystemExit(f'no PCSX2 replacement DDS files in {args.pack}')
    textures = args.data / 'textures'
    sample = next(textures.rglob('*.tga'), None)
    if sample is None or sample.read_bytes()[2] != 2:
        raise SystemExit(f'{textures} must be an uncompressed cooked tree (HD keys are MD5 of those files)')
    sizes = {size for _, size in entries}
    by_digest = index_cooked(textures, sizes, args.jobs)
    by_size = {}
    for key, paths in by_digest.items():
        by_size.setdefault(tga_size(paths[0]), []).append(key)
    candidates = {size: Candidates(sorted(keys), by_digest) for size, keys in by_size.items()}
    print(f'{len(entries)} pack textures, {len(by_digest)} unique cooked textures of the same sizes', flush=True)

    with ThreadPoolExecutor(args.jobs) as pool:
        results = dict(pool.map(lambda e: match(e, candidates), entries))
    chosen = {}
    for path, result in results.items():
        if result['status'] != 'ok' or result['tier'] is None or result['tier'] not in args.tiers:
            continue
        current = chosen.get(result['ps2'])
        if current is None or result['ncc'] > current[1]['ncc']:
            chosen[result['ps2']] = (path, result)
    counts = {}
    for result in results.values():
        key = result['status'] if result['status'] != 'ok' else f"tier {result['tier'] or '-'}"
        counts[key] = counts.get(key, 0) + 1

    hd = args.output / 'textures' / 'hd'
    jobs, kept = [], 0
    for digest, (path, result) in sorted(chosen.items()):
        out = hd / f'{digest}.tga'
        if out.exists() and not args.replace:
            kept += 1
            continue
        jobs.append((path, by_digest[digest][0], args.scale, out))
    manifest = {digest: {'dds': path.name, 'ncc': r['ncc'], 'psnr': r['psnr'], 'tier': r['tier'],
                         'ps2': by_digest[digest][0].relative_to(textures).as_posix(), 'files': len(by_digest[digest])}
                for digest, (path, r) in sorted(chosen.items())}
    unmatched = {p.name: r for p, r in results.items() if r['status'] != 'ok' or r['tier'] is None or r['tier'] not in args.tiers}
    print(f'matched {len(chosen)} cooked textures ({sum(m["files"] for m in manifest.values())} files); '
          f'{", ".join(f"{k}: {v}" for k, v in sorted(counts.items()))}; skipped names: {len(skipped)}', flush=True)
    if args.dry_run:
        return
    size = 0
    with ThreadPoolExecutor(args.jobs) as pool:
        for written in pool.map(convert, jobs):
            size += written
    (args.output / 'textures').mkdir(parents=True, exist_ok=True)
    (args.output / 'textures' / 'hd-manifest-pcsx2.json').write_text(
        json.dumps({'matched': manifest, 'unmatched': unmatched}, indent=1) + '\n')
    print(f'written {len(jobs)} HD textures at {args.scale}x ({size / 1e6:.0f} MB), kept {kept} existing, in {hd}')


if __name__ == '__main__':
    sys.exit(main())
