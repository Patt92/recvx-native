#!/usr/bin/env python3
"""Turn HD texture packs (DDS) into content-keyed HD textures for the port.

Source: every *.dds below <mods>/hd-source/ (any subfolders). Two naming schemes are understood:
  * PCSX2   <TEX0 hash>-[<CLUT hash>-]<bits>.dds   PS2 size from TEX0 TW/TH in <bits>
  * Dolphin tex1_<w>x<h>_<...>.dds                  GameCube size in the name
The hashes in those names are computed by the emulators over their own GPU memory images; the port has no
such image and its cooked textures carry no TEX0, so it cannot use them as keys. Instead each DDS is
box-downscaled to the size in its name and matched by image content against the cooked textures of the
same size in <data>/textures (luma correlation; tiers below). Only clear matches are used.

Output: <mods>/textures/hd/<md5>.tga, <md5> = MD5 of the uncompressed cooked data TGA, RGB from the pack,
alpha from the PS2 texture, <scale> times the PS2 size. <mods>/textures/hd-import.json lists every file this
tool wrote and every DDS it could not use (with the reason). A later run removes only files listed there and
skips all work when the sources, --scale and --tiers are unchanged. Other files in textures/hd (e.g. built
by tools/build_hd_textures.py) are never touched. Nothing in data/ or hd-source/ is modified.
./build.sh runs this automatically when <mods>/hd-source exists.
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

PCSX2 = re.compile(r'^([0-9a-f]{1,16})-(?:([0-9a-f]{1,16})-)?([0-9a-f]{8})(?:-mip(\d+))?\.dds$', re.I)
DOLPHIN = re.compile(r'^tex1_(\d+)x(\d+)_.*?(_mip\d+)?\.dds$', re.I)
THUMB = 16
TIER_A = (0.90, 0.05)   # min ncc, min gap to the runner-up (tools/build_hd_textures.py)
TIER_B = (0.80, 0.15)
DUPLICATE = 0.98      # luma NCC above which two cooked textures count as the same picture
RECORD = 'hd-import.json'


def name_size(name):
    """(width, height) the name declares, or None (not a pack texture or a mip level)."""
    m = PCSX2.match(name)
    if m:
        if m.group(4):
            return None
        bits = int(m.group(3), 16)
        return 1 << ((bits >> 6) & 15), 1 << ((bits >> 10) & 15)
    m = DOLPHIN.match(name)
    if m and not m.group(3):
        return int(m.group(1)), int(m.group(2))
    return None


def source_entries(source):
    entries, ignored = [], []
    for path in sorted(p for p in source.rglob('*') if p.is_file() and p.suffix.lower() == '.dds'):
        size = name_size(path.name)
        (entries if size else ignored).append((path, size) if size else path)
    return entries, ignored


def stamp(source, entries, scale, tiers):
    listing = [(p.relative_to(source).as_posix(), p.stat().st_size, int(p.stat().st_mtime)) for p, _ in entries]
    return hashlib.sha256(json.dumps([listing, scale, tiers]).encode()).hexdigest()


def tga_size(path):
    with path.open('rb') as f:
        return struct.unpack_from('<HH', f.read(18), 12)


def index_cooked(textures, sizes, jobs):
    paths = [p for p in sorted(textures.rglob('*.tga')) if tga_size(p) in sizes]
    by_digest = {}
    with ThreadPoolExecutor(jobs) as pool:
        for key, path in pool.map(lambda p: (hashlib.md5(p.read_bytes()).hexdigest(), p), paths, chunksize=64):
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
    """Cooked textures of one size; thumbnails in one image strip for a C-speed shortlist."""

    def __init__(self, digests, by_digest):
        self.digests = digests
        self.images = [load_rgb(by_digest[d][0]) for d in digests]
        self.strip = Image.frombytes('RGB', (THUMB, THUMB * len(digests)), b''.join(thumb(i).tobytes() for i in self.images))
        self.luma = {}

    def shortlist(self, query, count=8):
        tiled = Image.frombytes('RGB', self.strip.size, thumb(query).tobytes() * len(self.digests))
        per_tile = ImageChops.difference(self.strip, tiled).reduce(THUMB).tobytes()
        return [i for _, i in sorted((sum(per_tile[3 * i:3 * i + 3]), i) for i in range(len(self.digests)))[:count]]

    def luma_of(self, i):
        if i not in self.luma:
            self.luma[i] = luma_values(self.images[i])
        return self.luma[i]


def match(entry, candidates):
    path, (w, h) = entry
    group = candidates.get((w, h))
    if group is None:
        return path, {'status': 'no cooked texture of this size', 'size': [w, h]}
    try:
        hd = load_rgb(path)
    except Exception as exc:  # a decoder failure is reported, not hidden
        return path, {'status': 'cannot decode', 'error': str(exc)}
    if hd.size[0] % w or hd.size[1] % h or hd.size[0] // w != hd.size[1] // h:
        return path, {'status': 'not an integer multiple of the named size', 'size': list(hd.size), 'named': [w, h]}
    scale = hd.size[0] // w
    small = hd.reduce(scale) if scale > 1 else hd
    query = luma_values(small)
    scored = sorted(((ncc(query, group.luma_of(i)), i) for i in group.shortlist(small)), reverse=True)
    best = scored[0]
    # A runner-up that is itself nearly the same cooked picture (variants of one texture) does not make the
    # match uncertain; the gap counts against the next really different candidate.
    distinct = [c for c in scored[1:] if ncc(group.luma_of(best[1]), group.luma_of(c[1])) < DUPLICATE]
    second = distinct[0] if distinct else None
    rms = sum(v * v for v in ImageStat.Stat(ImageChops.difference(small, group.images[best[1]])).rms) / 3
    psnr = 99.0 if rms == 0 else 20 * math.log10(255 / math.sqrt(rms))
    found = tier(best[0], second[0] if second else None)
    return path, {'status': 'matched' if found else 'no clear match', 'ps2': group.digests[best[1]],
                  'ncc': round(best[0], 4), 'ncc2': round(second[0], 4) if second else None,
                  'psnr': round(psnr, 2), 'tier': found}


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
    result = hd.convert('RGBA')
    result.putalpha(alpha.resize(target, Image.BILINEAR) if alpha.getextrema() != (255, 255) else alpha.resize(target))
    # Same layout as the cooked files (type 2, 32 bit, top-left, no footer), so a package can bake it into data/.
    header = bytes([0, 0, 2]) + bytes(9) + target[0].to_bytes(2, 'little') + target[1].to_bytes(2, 'little') + bytes([32, 0x28])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = out_path.with_suffix('.tmp')
    temporary.write_bytes(header + result.tobytes('raw', 'BGRA'))
    os.replace(temporary, out_path)
    return out_path.stat().st_size


def run(mods, data, scale, tiers, jobs, force=False, log=print):
    source, hd = mods / 'hd-source', mods / 'textures' / 'hd'
    record_path = mods / 'textures' / RECORD
    entries, ignored = source_entries(source) if source.is_dir() else ([], [])
    previous = json.loads(record_path.read_text()) if record_path.is_file() else {}
    key = stamp(source, entries, scale, tiers) if entries else None
    if previous.get('stamp') == key and key and not force and all((hd / f).is_file() for f in previous.get('written', [])):
        log(f'HD import up to date: {len(entries)} DDS in {source} -> {len(previous["written"])} HD textures in {hd}')
        return previous
    for name in previous.get('written', []):  # only what this tool wrote before
        (hd / name).unlink(missing_ok=True)
    if not entries:
        record_path.unlink(missing_ok=True)
        log(f'HD import: no DDS in {source}' + (f'; removed {len(previous.get("written", []))} imported textures' if previous else ''))
        return {}
    textures = data / 'textures'
    sample = next(textures.rglob('*.tga'), None)
    if sample is None or sample.read_bytes()[2] != 2:
        raise SystemExit(f'{textures} must be the uncompressed cooked tree (HD keys are MD5 of those files)')
    by_digest = index_cooked(textures, {s for _, s in entries}, jobs)
    by_size = {}
    for digest, paths in by_digest.items():
        by_size.setdefault(tga_size(paths[0]), []).append(digest)
    candidates = {size: Candidates(sorted(keys), by_digest) for size, keys in by_size.items()}
    with ThreadPoolExecutor(jobs) as pool:
        results = dict(pool.map(lambda e: match(e, candidates), entries))
    chosen = {}
    for path, result in results.items():
        if result['status'] != 'matched' or result['tier'] not in tiers:
            continue
        current = chosen.get(result['ps2'])
        if current is None or result['ncc'] > current[1]['ncc']:
            chosen[result['ps2']] = (path, result)
    jobs_list, foreign = [], []
    for digest, (path, _) in sorted(chosen.items()):
        out = hd / f'{digest}.tga'
        if out.exists():  # written by something else (e.g. tools/build_hd_textures.py): keep it
            foreign.append(digest)
            continue
        jobs_list.append((path, by_digest[digest][0], scale, out))
    with ThreadPoolExecutor(jobs) as pool:
        size = sum(pool.map(convert, jobs_list))
    record = {
        'stamp': key, 'source': str(source), 'scale': scale, 'tiers': tiers,
        'written': sorted(out.name for *_, out in jobs_list),
        'matched': {d: {'dds': p.relative_to(source).as_posix(), 'ps2': by_digest[d][0].relative_to(textures).as_posix(),
                        'files': len(by_digest[d]), 'ncc': r['ncc'], 'psnr': r['psnr'], 'tier': r['tier']}
                    for d, (p, r) in sorted(chosen.items())},
        'kept_existing': foreign,
        'unused': {p.relative_to(source).as_posix(): r for p, r in sorted(results.items())
                   if r['status'] != 'matched' or r['tier'] not in tiers},
        'ignored_names': [p.relative_to(source).as_posix() for p in ignored],
    }
    record_path.parent.mkdir(parents=True, exist_ok=True)
    record_path.write_text(json.dumps(record, indent=1) + '\n')
    files = sum(m['files'] for m in record['matched'].values())
    log(f'HD import: {len(entries)} DDS in {source}: {len(chosen)} matched ({files} cooked files), '
        f'{len(record["unused"])} unused, {len(ignored)} ignored names; wrote {len(jobs_list)} TGA at {scale}x '
        f'({size / 1e6:.0f} MB) to {hd}, kept {len(foreign)} existing; details in {record_path}')
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mods', type=Path, default=Path('mods'), help='mods folder: reads hd-source/, writes textures/hd/')
    parser.add_argument('--data', type=Path, default=Path('data'), help='uncompressed cooked data tree')
    parser.add_argument('--scale', type=int, choices=(1, 2, 4, 8), default=4, help='output size multiple of the PS2 texture')
    parser.add_argument('--tiers', default='AB')
    parser.add_argument('--force', action='store_true', help='re-import even if nothing changed')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    args = parser.parse_args()
    run(args.mods, args.data, args.scale, args.tiers, args.jobs, args.force)


if __name__ == '__main__':
    sys.exit(main())
