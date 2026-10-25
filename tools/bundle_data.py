"""Data tree an app bundles: profile excludes, mods baked in, optional compression.

stage_bundle_data() builds a private staging tree from data/ or data_compressed/: files are hard-linked
(or copied when the build folder is on another file system), the profile's unread subtrees are left out,
and recvx-mod-bake replaces exactly the files the runtime would take from the mods folder. The source tree
is never written: the bake tool replaces files by rename, which breaks the link instead of writing through.
"""
import json
import os
import shutil
import subprocess
from pathlib import Path

from data_profiles import profile

MOD_CONTENT = ('textures', 'rooms', 'players', 'weapons', 'doors', 'items', 'movies', 'audio')


def mods_with_content(mods: Path | None) -> Path | None:
    """The mods folder if it holds anything recvx-mod-bake would apply, else None."""
    if mods is None or not mods.is_dir():
        return None
    for part in MOD_CONTENT:
        folder = mods / part
        if folder.is_dir() and any(p.is_file() and p.suffix.lower() in {'.tga', '.rdt', '.bin', '.rvv', '.mlt'}
                                   for p in folder.rglob('*')):
            return mods
    return None


def _link_tree(source: Path, destination: Path, excluded: set[Path]) -> int:
    count = 0
    for folder, names, files in os.walk(source):
        folder = Path(folder)
        names[:] = [n for n in names if folder / n not in excluded]
        target = destination / folder.relative_to(source)
        target.mkdir(parents=True, exist_ok=True)
        for name in files:
            if folder / name in excluded or name == '.DS_Store':
                continue
            try:
                os.link(folder / name, target / name)
            except OSError:
                shutil.copy2(folder / name, target / name)
            count += 1
    return count


def stage_bundle_data(source: Path, staging: Path, target: str, mods: Path | None, bake_tool: Path | None,
                      compressed: bool) -> dict:
    if not (source / 'native-data.version').is_file():
        raise RuntimeError(f'bundle data source is not a prepared data tree: {source}')
    if staging.exists():
        shutil.rmtree(staging)
    excluded = {source / part for part in profile(target)['exclude']}
    files = _link_tree(source, staging, excluded)
    result = {'source': str(source), 'files': files, 'excluded': list(profile(target)['exclude']),
              'compressed': compressed, 'mods': None, 'bake': 'no mods folder content'}
    applied = mods_with_content(mods)
    if applied is not None:
        if bake_tool is None or not bake_tool.is_file():
            raise RuntimeError(f'mods in {applied} need the host tool recvx-mod-bake (build the macOS/desktop target first)')
        command = [str(bake_tool), str(staging), str(applied)] + (['--compress'] if compressed else [])
        result['bake'] = subprocess.run(command, check=True, capture_output=True, text=True).stdout.strip()
        result['mods'] = str(applied)
    # asset-manifest.json beschreibt weiter den Quellbaum; was ersetzt wurde, steht hier.
    (staging / 'bundle-data.json').write_text(json.dumps(result, indent=1) + '\n')
    return result
