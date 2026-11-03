#!/usr/bin/env python3
"""Match Dolphin HD texture-pack DDS files (GameCube) to cooked PS2 textures.

The pack's file name carries the ORIGINAL GameCube texture size (tex1_<w>x<h>_...).
Replacement images may use any resolution with the same aspect ratio. --cross-size
also compares different original dimensions, verified at the PS2 target size. --alpha-mask
searches alpha-premultiplied content and verifies only visible pixels (pack alpha and
original alpha both >= 128), so colour noise under transparent texels no longer hides a
match; nearly invisible sources must also reproduce the original alpha channel. --claimed
takes an existing manifest: an original already assigned to a DIFFERENT source with a higher
score is no longer a competitor (one pack file per original), which resolves sources whose
best candidates are near-identical variants. Eliminated candidates stay in the result. Candidates
are compared by image content; a reported candidate is NOT an accepted match.
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

NAME = re.compile(r'^tex1_(\d+)x(\d+)_(?:m_)?', re.I)
THUMB = 8
SEARCH_SIZE = 32


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
    difference = float(np.mean((a - b) ** 2))
    a = a - a.mean()
    b = b - b.mean()
    denominator = float(np.sqrt((a * a).sum() * (b * b).sum()))
    if denominator < 1e-3:
        return 1.0 if difference < 1e-3 else 0.0
    return float((a * b).sum() / denominator)


def worker(args):
    path, candidates_by_size, textures_root = args[:3]
    cross_size = bool(args[3]) if len(args) > 3 else False
    alpha_mask = bool(args[4]) if len(args) > 4 else False
    claimed = args[5] if len(args) > 5 and args[5] else {}
    match = NAME.search(os.path.basename(path))
    if not match:
        return path, None
    w, h = int(match.group(1)), int(match.group(2))
    cands = ([candidate for (cw, ch), group in candidates_by_size.items()
              if cw * h == ch * w for candidate in group]
             if cross_size else candidates_by_size.get((w, h)))
    if not cands:
        return path, {'status': 'no-size'}
    try:
        hd = load_rgb(path)
    except Exception as exc:  # decoder failure is reported, not hidden
        return path, {'status': 'decode-error', 'error': str(exc)}
    if hd.width * h != hd.height * w:
        return path, {'status': 'bad-scale', 'size': hd.size}
    scale = hd.width / w
    with Image.open(path) as source_image:
        hd_rgba = np.asarray(source_image.convert('RGBA'), dtype=np.uint8)
    pack_alpha = hd_rgba[..., 3]
    if alpha_mask:
        # Garbage colours under transparent texels (pack upscaler noise versus the
        # original's fill) must not dominate the search: compare premultiplied content.
        hd = Image.fromarray(premultiply(hd_rgba).astype(np.uint8))
    # Fixed-size thumbnail, including when the declared GC size has no PS2
    # equivalent. Full comparisons below use each candidate's OWN dimensions.
    key = np.asarray(hd.resize((THUMB, THUMB), Image.Resampling.BOX), dtype=np.float32).reshape(-1)
    # Shortlist by thumbnail correlation plus colour distance, then verify at full size.
    ranked = sorted(((-ncc(key, c[1]) + float(np.mean((key - c[1]) ** 2)) / 20000.0, c[0]) for c in cands))[:8]
    # The old eight-colour-thumbnail shortlist drops genuine matches. Search ALL
    # candidates by spatial luminance as well, then verify the union at full size.
    matrix = search_matrix(textures_root, cands, alpha_mask)
    query = search_vector(hd)
    scores = matrix @ query
    selected = {d for _, d in ranked}
    selected.update(cands[int(i)][0] for i in np.argsort(scores)[-32:])
    scored = []
    details = {}
    queries = {}
    for digest in sorted(selected):
        ref = ps2_pixels(textures_root, digest, cands)
        size = (ref.shape[1], ref.shape[0])
        if size not in queries:
            queries[size] = np.asarray(hd.resize(size, Image.Resampling.BOX), dtype=np.float32)
        small = queries[size]
        if alpha_mask:
            comparison = visible_comparison(hd_rgba, size, ref, ps2_alpha(textures_root, digest, cands))
            small, ref = comparison['pack'], comparison['original']
            details[digest] = {k: comparison[k] for k in ('coverage', 'masked', 'alpha_full_ncc')}
        mse = float(np.mean((small - ref) ** 2))
        # Colour checks keep e.g. red and green herb variants apart: luma alone ignores hue.
        rgb = float(np.mean([ncc(small[..., c], ref[..., c]) for c in range(3)]))
        mean_delta = float(np.max(np.abs(small.reshape(-1, 3).mean(0) - ref.reshape(-1, 3).mean(0))))
        scored.append((ncc(luma(small), luma(ref)), mse, digest, rgb, mean_delta))
    scored.sort(key=lambda item: (-item[0], item[1], item[2]))
    # One pack file per original: a candidate already taken by another source that
    # fits it better than this one cannot be this source's identity.
    eliminated = [item for item in scored if item[2] in claimed
                  and claimed[item[2]]['dds'] != os.path.basename(path) and claimed[item[2]]['ncc'] > item[0] + CLAIM_MARGIN]
    if eliminated:
        scored = [item for item in scored if item not in eliminated]
        if not scored:
            return path, {'status': 'all-claimed', 'eliminated': [[d, round(n, 4), claimed[d]['dds']] for n, _, d, _, _ in eliminated]}
    # Correlation ignores offsets and can prefer a wrong colour variant. A close
    # RGB reconstruction with at least a 2:1 squared-error lead is independent
    # evidence; record it separately instead of lowering the correlation gap.
    by_error = sorted(scored, key=lambda item: (item[1], item[2]))
    error_best = by_error[0]
    error_second = by_error[1] if len(by_error) > 1 else None
    error_ratio = error_best[1] / max(error_second[1], 1e-9) if error_second else 0.0
    error_psnr = 99.0 if error_best[1] == 0 else 10 * np.log10(255 * 255 / error_best[1])
    decisive_rgb = (error_best[0] >= .95 and error_best[3] >= .90 and error_best[4] <= 15
                    and error_psnr >= 26 and (error_second is None or error_ratio <= .5)
                    and (error_second is None or error_second[1] > 1e-9))
    best = error_best if decisive_rgb else scored[0]
    # Tiny export/lighting variants of the SAME original picture are not a
    # competing identity. Do not merge colour variants or propagate replacements:
    # only the selected digest is written; all alternates stay in the report.
    ref_best = ps2_pixels(textures_root, best[2], cands)
    distinct = []
    equivalent = []
    for candidate in scored:
        if candidate[2] == best[2]:
            continue
        ref = ps2_pixels(textures_root, candidate[2], cands)
        if ref.shape != ref_best.shape:
            distinct.append(candidate)
            continue
        alpha_candidate = ps2_alpha(textures_root, candidate[2], cands)
        alpha_best = ps2_alpha(textures_root, best[2], cands)
        if alpha_mask:
            # Variants only coincide where they are visible; masks may differ in isolated texels.
            ref = ref * (alpha_candidate[..., None] / 255.0)
            ref_best_compared = ref_best * (alpha_best[..., None] / 255.0)
            same_alpha = float(np.mean((alpha_candidate >= 128) == (alpha_best >= 128))) >= .995
        else:
            ref_best_compared = ref_best
            same_alpha = np.array_equal(alpha_candidate, alpha_best)
        colour_delta = float(np.max(np.abs(ref.mean((0, 1)) - ref_best_compared.mean((0, 1)))))
        rgb_similarity = min(ncc(ref[..., c], ref_best_compared[..., c]) for c in range(3))
        if ncc(luma(ref), luma(ref_best_compared)) >= .98 and rgb_similarity >= .98 and colour_delta <= 8 and same_alpha:
            equivalent.append(candidate[2])
        else:
            distinct.append(candidate)
    second = distinct[0] if distinct else None
    psnr = 99.0 if best[1] == 0 else 10 * np.log10(255 * 255 / best[1])
    target_size = [int(ref_best.shape[1]), int(ref_best.shape[0])]
    changed_size = target_size != [w, h]
    original_alpha = ps2_alpha(textures_root, best[2], cands)
    original_coverage = float(np.mean(original_alpha >= 128))
    pack_coverage = float(np.mean(pack_alpha >= 128))
    alpha_layout_ncc = None
    if .02 <= original_coverage <= .98 and .02 <= pack_coverage <= .98:
        original_mask = np.asarray(Image.fromarray(original_alpha).resize((SEARCH_SIZE, SEARCH_SIZE), Image.Resampling.BOX), dtype=np.float32)
        pack_mask = np.asarray(Image.fromarray(pack_alpha).resize((SEARCH_SIZE, SEARCH_SIZE), Image.Resampling.BOX), dtype=np.float32)
        alpha_layout_ncc = ncc(original_mask, pack_mask)
    alpha_layout_ok = not changed_size or alpha_layout_ncc is None or alpha_layout_ncc >= .90
    # Cross-platform scale changes need strong full-size RGB evidence too.
    # A good tiny thumbnail alone must never authorize an unrelated replacement.
    cross_size_ok = (not changed_size or (best[0] >= .95 and best[3] >= .90
                     and best[4] <= 15 and psnr >= 26 and alpha_layout_ok))
    extra = {}
    if eliminated:
        extra['eliminated'] = [[d, round(n, 4), claimed[d]['dds']] for n, _, d, _, _ in eliminated]
    if alpha_mask:
        detail = details[best[2]]
        # A source that is nearly invisible was compared premultiplied; its alpha
        # channel itself must then reproduce the original's, or the evidence is void.
        alpha_ok = detail['coverage'] >= VISIBLE_MINIMUM or (detail['alpha_full_ncc'] is not None and detail['alpha_full_ncc'] >= .90)
        extra.update({'alpha_mask': True, 'coverage': round(detail['coverage'], 4), 'masked': detail['masked'],
                      'alpha_full_ncc': round(detail['alpha_full_ncc'], 5) if detail['alpha_full_ncc'] is not None else None,
                      'alpha_ok': bool(alpha_ok)})
    return path, {**extra, 'status': 'ok', 'ps2': best[2], 'ncc': round(best[0], 4), 'psnr': round(float(psnr), 2), 'scale': scale,
                  'source_size': [w, h], 'target_size': target_size, 'cross_size_ok': bool(cross_size_ok),
                  'alpha_layout_ncc': round(alpha_layout_ncc, 5) if alpha_layout_ncc is not None else None,
                  'ncc2': round(second[0], 4) if second else None,
                  'rgb_ncc': round(best[3], 4), 'mean_delta': round(best[4], 2),
                  'equivalent_candidates': equivalent,
                  'colour_ok': best[3] >= .80 and best[4] <= 25,
                  'decisive_rgb': bool(decisive_rgb), 'rgb_error_ratio': round(float(error_ratio), 5),
                  'top': [[d, round(n, 4), round(r, 4), round(md, 2)] for n, _, d, r, md in scored[:4]]}


_PIXELS = {}
_SEARCH = {}
_ALPHAS = {}
CLAIM_MARGIN = .005     # a claimant must fit the original better than the displaced source by this much
VISIBLE_MINIMUM = .03   # below this visible fraction the premultiplied image is compared instead


def premultiply(rgba):
    return rgba[..., :3].astype(np.float32) * (rgba[..., 3:4].astype(np.float32) / 255.0)


def visible_comparison(hd_rgba, size, ref, ref_alpha):
    """Pack and original pixels restricted to texels both images show.

    Returns straight RGB rows of the shared visible mask, or the premultiplied
    full images when the mask covers less than VISIBLE_MINIMUM of the texture,
    plus the full-resolution alpha correlation (None for a constant original alpha).
    """
    rgb = np.asarray(Image.fromarray(hd_rgba[..., :3]).resize(size, Image.Resampling.BOX), dtype=np.float32)
    pack_alpha = np.asarray(Image.fromarray(hd_rgba[..., 3]).resize(size, Image.Resampling.BOX), dtype=np.float32)
    mask = (ref_alpha >= 128) & (pack_alpha >= 128)
    coverage = float(mask.mean())
    alpha_full_ncc = None
    if ref_alpha.min() != ref_alpha.max():
        alpha_full_ncc = ncc(pack_alpha, ref_alpha.astype(np.float32))
    if coverage >= VISIBLE_MINIMUM:
        return {'pack': rgb[mask], 'original': ref[mask], 'coverage': coverage, 'masked': True, 'alpha_full_ncc': alpha_full_ncc}
    pack = rgb * (pack_alpha[..., None] / 255.0)
    original = ref * (ref_alpha[..., None].astype(np.float32) / 255.0)
    return {'pack': pack.reshape(-1, 3), 'original': original.reshape(-1, 3), 'coverage': coverage, 'masked': False, 'alpha_full_ncc': alpha_full_ncc}


def ps2_alpha(root, digest, candidates):
    key = (str(root), digest)
    if key not in _ALPHAS:
        rel = next(c[2] for c in candidates if c[0] == digest)
        with Image.open(Path(root) / rel) as image:
            _ALPHAS[key] = np.asarray(image.convert('RGBA').getchannel('A'))
    return _ALPHAS[key]


def search_vector(image):
    pixels = np.asarray(image.resize((SEARCH_SIZE, SEARCH_SIZE), Image.Resampling.BOX), dtype=np.float32)
    values = luma(pixels).reshape(-1)
    values -= values.mean()
    return values / max(float(np.linalg.norm(values)), 1e-8)


def search_matrix(root, candidates, alpha_mask=False):
    key = (str(root), tuple(c[0] for c in candidates), alpha_mask)
    if key not in _SEARCH:
        if alpha_mask:
            images = [Image.fromarray((ps2_pixels(root, c[0], candidates) * (ps2_alpha(root, c[0], candidates)[..., None] / 255.0)).astype(np.uint8)) for c in candidates]
        else:
            images = [Image.fromarray(ps2_pixels(root, c[0], candidates).astype(np.uint8)) for c in candidates]
        _SEARCH[key] = np.stack([search_vector(image) for image in images])
    return _SEARCH[key]


def ps2_pixels(root, digest, cands):
    key = (str(root), digest)
    if key not in _PIXELS:
        rel = next(c[2] for c in cands if c[0] == digest)
        _PIXELS[key] = np.asarray(load_rgb(Path(root) / rel), dtype=np.float32)
    return _PIXELS[key]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, required=True, help='.../Load/Textures/GCDE08')
    parser.add_argument('--textures', type=Path, default=Path('data/textures'))
    parser.add_argument('--categories', nargs='+', help='default: every category under --textures')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=0, help='first N DDS files (sampling)')
    parser.add_argument('--cross-size', action='store_true', help='also compare different original sizes with the same aspect ratio')
    parser.add_argument('--alpha-mask', action='store_true', help='premultiplied search and visible-pixel verification (see module docstring)')
    parser.add_argument('--claimed', type=Path, help='hd-manifest.json whose assignments exclude taken originals (see module docstring)')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()
    claimed = {}
    if args.claimed:
        claimed = {digest: {'dds': entry['dds'], 'ncc': entry['ncc']} for digest, entry in json.loads(args.claimed.read_text()).items()}
    categories = args.categories or sorted(p.name for p in args.textures.iterdir() if p.is_dir() and not p.name.startswith('.'))
    ps2 = index_ps2(args.textures, categories)
    by_size = collections.defaultdict(list)
    for digest, entry in ps2.items():
        if entry['thumb'] is not None:
            by_size[entry['size']].append((digest, entry['thumb'], entry['paths'][0]))
    print(f'PS2: {len(ps2)} unique textures, {sum(len(e["paths"]) for e in ps2.values())} files', file=sys.stderr)
    files = sorted(str(p) for p in args.pack.rglob('*') if p.is_file() and p.suffix.lower() == '.dds')
    if not files:
        raise SystemExit(f'no DDS files under {args.pack} (pack missing or volume not mounted); output left unchanged')
    if args.limit:
        step = max(1, len(files) // args.limit)
        files = files[::step][:args.limit]
    results = {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        jobs = [(f, by_size, str(args.textures), args.cross_size, args.alpha_mask, claimed) for f in files]
        for i, (path, result) in enumerate(pool.map(worker, jobs, chunksize=4), 1):
            results[path] = result
            if i % 200 == 0:
                print(f'{i}/{len(files)}', file=sys.stderr)
    table = {'ps2': {d: e['paths'] for d, e in ps2.items()}, 'matches': results}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(table))
    ok = [r for r in results.values() if r and r.get('status') == 'ok']
    print(f'{len(files)} DDS; matched candidates {len(ok)}; statuses', collections.Counter((r or {}).get('status') for r in results.values()))
    nccs = sorted(r['ncc'] for r in ok)
    if nccs:
        print('NCC percentiles', {p: nccs[int(len(nccs) * p / 100)] for p in (5, 25, 50, 75, 95)})


if __name__ == '__main__':
    main()
