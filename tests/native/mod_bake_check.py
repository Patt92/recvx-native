#!/usr/bin/env python3
"""App-Daten mit eingebackenen Mods (tools/bundle_data.py + recvx-mod-bake).

Prueft: der Paketbaum enthaelt genau die Dateien, die die Runtime unter --mods nehmen wuerde (Textur je Asset
vor HD-Datei vor data/, Raum, Tuer-, Item- und SYSTEM-Archive), die Profilausschluesse fehlen, der Quellbaum
bleibt trotz Hardlinks bytegleich, --compress schreibt Farbtabellen-TGA und hd-source/ wird nicht verwendet."""
import argparse
import hashlib
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from bundle_data import stage_bundle_data  # noqa: E402


def tga(path, pixel, size=4):
    header = bytes([0, 0, 2]) + bytes(9) + size.to_bytes(2, 'little') + size.to_bytes(2, 'little') + bytes([32, 0x28])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + bytes(pixel) * size * size)


def tree_digest(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bake', type=Path, required=True)
    bake = parser.parse_args().bake.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-bake-') as folder:
        root = Path(folder)
        data, mods = root / 'data', root / 'mods'
        (data / 'native-data.version').parent.mkdir(parents=True)
        (data / 'native-data.version').write_text('recvx-assets-1\n')
        (data / 'asset-manifest.json').write_text('{}\n')
        tga(data / 'textures/rooms/RM_1000/0000.tga', (10, 20, 30, 255))   # per-asset Mod
        tga(data / 'textures/rooms/RM_1000/0001.tga', (40, 50, 60, 255))   # HD-Treffer
        tga(data / 'textures/rooms/RM_2000/0003.tga', (40, 50, 60, 255))   # derselbe Inhalt in einem anderen Raum
        tga(data / 'textures/rooms/RM_1000/0002.tga', (70, 80, 90, 255))   # bleibt
        for name, payload in (('rooms/RM_1000.RDT', b'room'), ('archives/MRY/0072.bin', b'door'),
                              ('archives/ITEM1/0004.bin', b'item'), ('archives/SYSTEM/0001.bin', b'player'),
                              ('files/MOVIE/0000.bin', b'cooked source'), ('archives/RDX_LNK/0000.bin', b'cooked source')):
            (data / name).parent.mkdir(parents=True, exist_ok=True)
            (data / name).write_bytes(payload)
        before = tree_digest(data)
        hd_key = hashlib.md5((data / 'textures/rooms/RM_1000/0001.tga').read_bytes()).hexdigest()
        tga(mods / 'textures/rooms/RM_1000/0000.tga', (1, 2, 3, 255), 8)
        tga(mods / f'textures/hd/{hd_key}.tga', (4, 5, 6, 255), 16)
        tga(mods / 'textures/hd/0123456789abcdef0123456789abcdef.tga', (7, 7, 7, 255), 16)  # trifft nichts
        for name, payload in (('rooms/RM_1000.RDT', b'mod room'), ('doors/0001.bin', b'mod door'),
                              ('items/0005.bin', b'mod item'), ('players/0001.bin', b'mod player')):
            (mods / name).parent.mkdir(parents=True, exist_ok=True)
            (mods / name).write_bytes(payload)
        (mods / 'hd-source').mkdir()
        (mods / 'hd-source/pack.dds').write_bytes(b'DDS not used')

        staged = root / 'stage'
        result = stage_bundle_data(data, staged, 'ios', mods, bake, False)
        assert tree_digest(data) == before, 'Quellbaum wurde beim Backen veraendert'
        assert 'textures from 2 HD files' in result['bake'] and '1 per-asset' in result['bake'], result['bake']
        read = lambda p: (staged / p).read_bytes()
        assert read('textures/rooms/RM_1000/0000.tga')[12] == 8, 'Textur je Asset nicht eingesetzt'
        assert read('textures/rooms/RM_1000/0001.tga')[12] == 16 and read('textures/rooms/RM_2000/0003.tga')[12] == 16, 'HD nicht eingesetzt'
        assert read('textures/rooms/RM_1000/0002.tga') == (data / 'textures/rooms/RM_1000/0002.tga').read_bytes()
        assert (read('rooms/RM_1000.RDT'), read('archives/MRY/0072.bin'), read('archives/ITEM1/0004.bin'),
                read('archives/SYSTEM/0001.bin')) == (b'mod room', b'mod door', b'mod item', b'mod player')
        assert not (staged / 'files/MOVIE').exists() and not (staged / 'archives/RDX_LNK').exists(), 'Profilausschluss fehlt'
        assert not any(p.suffix in {'.dds', '.bake-tmp'} for p in staged.rglob('*')), 'DDS/Zwischendatei im Paketbaum'
        assert json.loads(read('bundle-data.json'))['mods'] == str(mods)

        compressed = root / 'stage-compressed'
        stage_bundle_data(data, compressed, 'macos', mods, bake, True)
        assert (compressed / 'textures/rooms/RM_1000/0001.tga').read_bytes()[2] in (1, 9), 'nicht komprimiert'
        assert (compressed / 'files/MOVIE/0000.bin').is_file(), 'Desktop schliesst nichts aus'
        assert tree_digest(data) == before

        plain = stage_bundle_data(data, root / 'stage-plain', 'macos', root / 'no-mods', None, False)
        assert plain['mods'] is None and tree_digest(root / 'stage-plain') == {**before, 'bundle-data.json': tree_digest(root / 'stage-plain')['bundle-data.json']}
    print('Mod bake: per-asset/HD/room/door/item/player replacements in the app tree, source untouched, compressed and profile excludes passed')


if __name__ == '__main__':
    main()
