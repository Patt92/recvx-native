#!/usr/bin/env python3
"""Turn HD texture packs (DDS) into content-keyed HD textures for the port.

Source: every *.dds below <mods>/hd-source/ (any subfolders). Two naming schemes are understood:
  * PCSX2   <TEX0 hash>-[<CLUT hash>-]<bits>.dds   PS2 size from TEX0 TW/TH in <bits>
  * Dolphin tex1_<w>x<h>_<...>.dds                  GameCube size in the name
The hashes in those names are computed by the emulators over their own GPU memory images; the port has no
such image and its cooked textures carry no TEX0, so it cannot use them as keys. Instead each DDS is
box-downscaled to the size in its name and matched by image content against the cooked textures of the
same size in <data>/textures (luma correlation, tiers A/B below). Redrawn atlases (inventory icons,
status screens) differ in the picture but keep the PS2 transparency layout; tier L matches those by their
alpha mask. Only clear matches are used.

Output: <mods>/textures/hd/<md5>.tga, <md5> = MD5 of the uncompressed cooked data TGA, RGB from the pack,
alpha from the pack when it has its own transparency mask (else from the PS2 texture), <scale> times the
PS2 size. <mods>/textures/hd-import.json lists every file this
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
# Tier L (layout): redrawn atlases (inventory icons, status screens) keep the PS2 transparency layout but not
# the picture. Accepted when the alpha masks correlate >= LAYOUT[0], every candidate with a different mask
# stays LAYOUT[1] below, the picture itself still correlates >= LAYOUT[2] and no other candidate with the
# same mask is within LAYOUT[1] in luma. Only textures with a real mask take part (has_layout).
LAYOUT = (0.97, 0.15, 0.50)
DUPLICATE = 0.98      # luma NCC above which two cooked textures count as the same picture
RECORD = 'hd-import.json'
VERSION = 2   # matching/conversion rules; a change re-imports


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
    return hashlib.sha256(json.dumps([VERSION, listing, scale, tiers]).encode()).hexdigest()


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


def alpha_values(alpha, limit=64):
    """Alpha mask in the cooked domain (0..255), reduced like luma_values."""
    w, h = alpha.size
    small = alpha.reduce((max(1, w // limit), max(1, h // limit))) if w > limit or h > limit else alpha
    return list(small.tobytes())


def visible_luma(rgb, mask):
    return luma_values(Image.composite(rgb, Image.new('RGB', rgb.size), mask))


def has_layout(values):
    """A mask with transparent and opaque parts (2..98 % covered), not a plain opaque or empty texture."""
    covered = sum(1 for v in values if v >= 128) / len(values)
    return 0.02 <= covered <= 0.98


def dds_alpha(image):
    """Pack alpha for mask matching: GS range (0x80 = opaque) doubled like the cooked textures. Only the mask
    shape counts there; the written alpha comes from pack_alpha()."""
    return image.getchannel('A').point(lambda a: min(255, 2 * a)) if 'A' in image.getbands() else None


def pack_alpha(image, base_alpha):
    """Pack alpha in the cooked domain (min(255, 2 * a)) for tier L. Dumped files keep the GS value
    (0x80 = opaque), redrawn files are often saved with full 0..255 alpha. The PS2 texture decides: over its
    opaque texels the pack's median alpha is either about 0x80 (GS range, doubled) or about 0xff (kept)."""
    alpha = image.getchannel('A')
    opaque = [a for a, b in zip(alpha.resize(base_alpha.size, Image.BOX).tobytes(), base_alpha.tobytes()) if b == 255]
    gs_range = not opaque or sorted(opaque)[len(opaque) // 2] <= 0xc0
    return alpha.point(lambda a: min(255, 2 * a)) if gs_range else alpha


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
        self.alphas, self.masks = [], []
        for d in digests:
            with Image.open(by_digest[d][0]) as image:
                image.load()
                mask = image.convert('RGBA').getchannel('A')
                self.masks.append(mask)
                self.alphas.append(alpha_values(mask))
        self.visible = {}

    def shortlist(self, query, count=8):
        tiled = Image.frombytes('RGB', self.strip.size, thumb(query).tobytes() * len(self.digests))
        per_tile = ImageChops.difference(self.strip, tiled).reduce(THUMB).tobytes()
        return [i for _, i in sorted((sum(per_tile[3 * i:3 * i + 3]), i) for i in range(len(self.digests)))[:count]]

    def layout_shortlist(self, query, count=8):
        return [i for _, i in sorted(((-ncc(query, a), i) for i, a in enumerate(self.alphas) if has_layout(a)))[:count]]

    def visible_luma_of(self, i):
        """Luma of what is drawn: transparent texels count as black (their RGB is never seen)."""
        if i not in self.visible:
            self.visible[i] = visible_luma(self.images[i], self.masks[i])
        return self.visible[i]

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
        with Image.open(path) as image:
            image.load()
            alpha = dds_alpha(image)
            hd = image.convert('RGB')
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
    if not found and alpha is not None:
        small_alpha = alpha.reduce(scale) if scale > 1 else alpha
        layout = layout_match(group, visible_luma(small, small_alpha), alpha_values(small_alpha), small)
        if layout is not None:
            best = (layout[1], layout[0])
            rms = sum(v * v for v in ImageStat.Stat(ImageChops.difference(small, group.images[best[1]])).rms) / 3
            psnr = 99.0 if rms == 0 else 20 * math.log10(255 / math.sqrt(rms))
            second, found = None, 'L'
            return path, {'status': 'matched', 'ps2': group.digests[best[1]], 'ncc': round(best[0], 4),
                          'alpha_ncc': round(layout[2], 4), 'psnr': round(psnr, 2), 'tier': found}
    return path, {'status': 'matched' if found else 'no clear match', 'ps2': group.digests[best[1]],
                  'ncc': round(best[0], 4), 'ncc2': round(second[0], 4) if second else None,
                  'psnr': round(psnr, 2), 'tier': found}


def layout_match(group, luma, mask, small):
    """(candidate, luma ncc, alpha ncc) for tier L, or None."""
    if not has_layout(mask):
        return None
    scored = [(ncc(mask, group.alphas[i]), ncc(luma, group.visible_luma_of(i)), i)
              for i in set(group.layout_shortlist(mask)) | set(group.shortlist(small))]
    same = [c for c in scored if c[0] >= LAYOUT[0]]
    if not same:
        return None
    other = max((c[0] for c in scored if c[0] < LAYOUT[0]), default=None)
    if other is not None and max(c[0] for c in same) - other < LAYOUT[1]:
        return None
    alpha_score, luma_score, index = max(same, key=lambda c: c[1])
    if luma_score < LAYOUT[2]:
        return None
    rivals = [c[1] for c in same if c[2] != index and ncc(group.visible_luma_of(index), group.visible_luma_of(c[2])) < DUPLICATE]
    if rivals and luma_score - max(rivals) < LAYOUT[1]:
        return None
    return index, luma_score, alpha_score


def convert(job):
    dds, ps2_path, scale, out_path = job
    with Image.open(dds) as image:
        image.load()
        hd = image.convert('RGBA')
        own = 'A' in image.getbands()
    with Image.open(ps2_path) as image:
        image.load()
        base = image.convert('RGBA')
    # Alpha: the pack's own mask when it has one (redrawn glyphs and icons have their own silhouettes, the PS2
    # mask would cut them), otherwise the PS2 mask (packs saved without alpha or fully opaque).
    hd_alpha = None
    if own and has_layout(alpha_values(dds_alpha(hd).resize(base.size, Image.BOX))):
        hd_alpha = pack_alpha(hd, base.getchannel('A'))
    hd = hd.convert('RGB')
    w, h = base.size
    target = (w * scale, h * scale)
    if hd.size != target:
        hd = hd.resize(target, Image.LANCZOS)
    alpha = base.getchannel('A')
    result = hd.convert('RGBA')
    if hd_alpha is not None:
        result.putalpha(hd_alpha if hd_alpha.size == target else hd_alpha.resize(target, Image.BILINEAR))
    else:
        result.putalpha(alpha.resize(target, Image.BILINEAR) if alpha.getextrema() != (255, 255) else alpha.resize(target))
    # Same layout as the cooked files (type 2, 32 bit, top-left, no footer), so a package can bake it into data/.
    header = bytes([0, 0, 2]) + bytes(9) + target[0].to_bytes(2, 'little') + target[1].to_bytes(2, 'little') + bytes([32, 0x28])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = out_path.with_suffix('.tmp')
    temporary.write_bytes(header + result.tobytes('raw', 'BGRA'))
    os.replace(temporary, out_path)
    return out_path.stat().st_size, 'pack' if hd_alpha is not None else 'ps2'


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
        converted = dict(zip((out.stem for *_, out in jobs_list), pool.map(convert, jobs_list)))
    size = sum(bytes_ for bytes_, _ in converted.values())
    record = {
        'stamp': key, 'source': str(source), 'scale': scale, 'tiers': tiers,
        'written': sorted(out.name for *_, out in jobs_list),
        'matched': {d: {'dds': p.relative_to(source).as_posix(), 'ps2': by_digest[d][0].relative_to(textures).as_posix(),
                        'files': len(by_digest[d]), 'ncc': r['ncc'], 'psnr': r['psnr'], 'tier': r['tier'],
                        **({'alpha_ncc': r['alpha_ncc']} if 'alpha_ncc' in r else {}),
                        **({'alpha': converted[d][1]} if d in converted else {})}
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
    parser.add_argument('--tiers', default='ABL', help='A/B: picture match, L: same transparency layout (redrawn atlases)')
    parser.add_argument('--force', action='store_true', help='re-import even if nothing changed')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    args = parser.parse_args()
    run(args.mods, args.data, args.scale, args.tiers, args.jobs, args.force)


if __name__ == '__main__':
    sys.exit(main())
