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
import shutil
from pathlib import Path

from PIL import Image

TIER_A = (0.90, 0.05)   # min ncc, min gap to the runner-up
TIER_B = (0.80, 0.15)


def gap(match):
    return 1.0 if match['ncc2'] is None else match['ncc'] - match['ncc2']


def tier(match):
    if match.get('colour_ok') is False:
        return None
    if match.get('decisive_rgb'):
        return 'A'
    if match['ncc'] >= TIER_A[0] and gap(match) >= TIER_A[1]:
        return 'A'
    if TIER_B[0] <= match['ncc'] < TIER_A[0] and gap(match) >= TIER_B[1]:
        return 'B'
    return None


def select_matches(matches, existing=None, tiers='AB'):
    """Choose one DDS per target; never displace an existing assignment."""
    existing = existing or {}
    chosen, report = {}, {}
    existing_sources = {Path(entry['dds']).name: key for key, entry in existing.items()}
    for path, match in sorted(matches.items()):
        if Path(path).name in existing_sources:
            report[path] = {'status': 'existing-source-kept', 'target': existing_sources[Path(path).name]}
            continue
        if not match or match.get('status') != 'ok':
            report[path] = {'status': (match or {}).get('status', 'invalid')}
            continue
        t = tier(match)
        key = match['ps2']
        if t is None or t not in tiers:
            report[path] = {'status': 'colour-mismatch' if match.get('colour_ok') is False else 'ambiguous-or-low-similarity',
                            'ncc': match['ncc'], 'ncc2': match['ncc2'], 'target': key}
            continue
        if key in existing:
            report[path] = {'status': 'existing-target-kept', 'target': key}
            continue
        current = chosen.get(key)
        rank = (match['ncc'], gap(match), match.get('psnr', 0))
        if current and rank <= (current[1]['ncc'], gap(current[1]), current[1].get('psnr', 0)):
            report[path] = {'status': 'alternate-source', 'target': key}
            continue
        if current:
            report[current[0]] = {'status': 'alternate-source', 'target': key}
        chosen[key] = (path, match, t)
        report[path] = {'status': 'selected', 'target': key, 'tier': t}
    for item in report.values():
        if item['status'] == 'alternate-source':
            item['selected'] = chosen[item['target']][0]
    return chosen, report


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
    parser.add_argument('--existing-manifest', type=Path, help='preserve target assignments and extend this manifest')
    parser.add_argument('--report', type=Path, help='write outcome for every DDS, including unresolved sources')
    parser.add_argument('--only', default='', help='comma list of data/textures categories (default all)')
    parser.add_argument('--per-asset', default='', help='comma list of categories without cooked runtime files; write explicit identity overrides')
    parser.add_argument('--limit', type=int, default=0)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()
    table = json.loads(args.matches.read_text())
    ps2 = table['ps2']
    existing = json.loads(args.existing_manifest.read_text()) if args.existing_manifest else {}
    chosen, report = select_matches(table['matches'], existing, args.tiers)
    only = {c for c in args.only.split(',') if c}
    per_asset = {c for c in args.per_asset.split(',') if c}
    jobs, estimate = [], 0
    for digest, (dds, match, t) in sorted(chosen.items()):
        paths = ps2[digest]
        if only and paths[0].split('/')[0] not in only:
            continue
        with Image.open(args.textures / paths[0]) as image:
            w, h = image.size
            has_alpha = image.mode == 'RGBA'
        estimate += w * h * args.scale ** 2 * (4 if has_alpha else 3) + 18
        destinations = [args.output / 'textures' / path for path in paths if path.split('/')[0] in per_asset]
        if any(path.split('/')[0] not in per_asset for path in paths):
            destinations.append(args.output / 'textures' / 'hd' / (digest + '.tga'))
        for out in destinations:
            jobs.append((digest, dds, str(args.textures / paths[0]), args.scale, str(out), match['scale']))
    if args.limit:
        jobs = jobs[::max(1, len(jobs) // args.limit)][:args.limit]
    emitted = {job[0] for job in jobs}
    for item in report.values():
        if item['status'] == 'selected' and item['target'] not in emitted:
            item['status'] = 'filtered-out'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(emitted)} target identities, {len(jobs)} replacement files at {args.scale}x, about {estimate / 1e9:.1f} GB uncompressed TGA')
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
    manifest = dict(existing)
    manifest.update({digest: {'dds': Path(dds).name, 'ncc': m['ncc'], 'tier': t, 'ps2': ps2[digest][0]}
                     for digest, (dds, m, t) in chosen.items() if digest in emitted})
    for digest, entry in manifest.items():
        overrides = [str(Path(job[4]).relative_to(args.output)) for job in jobs
                     if job[0] == digest and Path(job[4]).parent.name != 'hd']
        # Existing content-hash replacements cannot reach an uncooked runtime
        # identity. Project their exact bytes into explicit overrides as well.
        if digest in existing and args.existing_manifest:
            source = args.existing_manifest.parent / 'hd' / (digest + '.tga')
            for relative in ps2.get(digest, []):
                if relative.split('/')[0] not in per_asset:
                    continue
                destination = args.output / 'textures' / relative
                if not source.is_file():
                    raise FileNotFoundError(f'existing HD replacement missing: {source}')
                if not destination.exists():
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    temporary = destination.with_suffix('.tga.tmp')
                    shutil.copyfile(source, temporary)
                    os.replace(temporary, destination)
                overrides.append(str(destination.relative_to(args.output)))
        if overrides:
            entry['overrides'] = sorted(set(entry.get('overrides', []) + overrides))
    (args.output / 'textures').mkdir(parents=True, exist_ok=True)
    (args.output / 'textures' / 'hd-manifest.json').write_text(json.dumps(manifest, indent=0))
    print(f'written {written}, already present {skipped}, {size / 1e9:.2f} GB in {args.output / "textures" / "hd"}')


if __name__ == '__main__':
    main()
