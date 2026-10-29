#!/usr/bin/env python3
"""Assemble the static web site of the browser build and the data index it needs.

Site (build/web/site): index.html (web/shell.html as built), recvx.js, recvx.wasm, favicon.png.
Data index: the WasmFS fetch backend (native/web_main.cpp) cannot list directories, so the runtime reads
web-files.txt (one relative path per line) from the served data/ root and creates the tree from it.
Game data itself is never copied into the site or an image; the server mounts data/ (see docs/WEB.md).
"""
import argparse
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import data_profiles  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
LISTING = 'web-files.txt'
MODS_LISTING = 'mods-files.txt'
# Files the runtime never reads; hidden files (.DS_Store) and the index itself stay out of the listing.
_IGNORED_NAMES = {LISTING, MODS_LISTING}


def tree_listing(root: Path, excluded=()) -> list[str]:
    """Sorted POSIX relative paths of every regular file below root, minus excluded subtrees."""
    excluded = [Path(part) for part in excluded]
    paths = []
    for path in root.rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if any(part.startswith('.') for part in relative.parts) or relative.name in _IGNORED_NAMES:
            continue
        if any(relative == part or part in relative.parents for part in excluded):
            continue
        paths.append(relative.as_posix())
    return sorted(paths)


def write_listing(root: Path, name: str, excluded=()) -> int:
    lines = tree_listing(root, excluded)
    target = root / name
    text = ''.join(line + '\n' for line in lines)
    if not target.is_file() or target.read_text() != text:
        target.write_text(text)
    return len(lines)


def assemble_site(build: Path, site: Path) -> Path:
    for name in ('recvx.js', 'recvx.wasm'):
        if not (build / name).is_file():
            raise RuntimeError(f'web build output missing: {build / name}')
    site.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / 'recvx.js', site / 'recvx.js')
    shutil.copy2(build / 'recvx.wasm', site / 'recvx.wasm')
    shutil.copy2(ROOT / 'web/shell.html', site / 'index.html')
    icon = ROOT / 'android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png'
    if icon.is_file():
        shutil.copy2(icon, site / 'favicon.png')
    return site


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/web')
    parser.add_argument('--site', type=Path, help='output directory (default: <build>/site)')
    parser.add_argument('--data', type=Path, default=ROOT / 'data', help='prepared data tree to index')
    parser.add_argument('--mods', type=Path, default=ROOT / 'mods', help='mods tree to index (optional)')
    parser.add_argument('--index-only', action='store_true', help='only write the data/mods index files')
    args = parser.parse_args()
    data = args.data.resolve()
    if not (data / 'native-data.version').is_file():
        parser.error(f'prepared data tree required (missing {data / "native-data.version"})')
    count = write_listing(data, LISTING, data_profiles.profile('web')['exclude'])
    print(f'Data index: {data / LISTING} ({count} files)')
    mods = args.mods.resolve()
    if mods.is_dir():
        # hd-source/ is a build-time import source (tools/import_hd_textures.py), never read at run time.
        print(f'Mods index: {mods / MODS_LISTING} ({write_listing(mods, MODS_LISTING, ("hd-source",))} files)')
    if not args.index_only:
        site = assemble_site(args.build.resolve(), (args.site or args.build / 'site').resolve())
        print(f'Site: {site}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
