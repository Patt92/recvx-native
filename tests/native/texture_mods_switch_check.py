#!/usr/bin/env python3
"""Textur-Mods sichtbar und schaltbar: Startmeldung, Raumzaehler und Debugmenue-Schalter.

Die Runtime meldet den Datenordner mit Texturformat, den Mods-Ordner mit der Zahl der HD-Dateien und je
Raum, woher die Texturen kamen (HD, Mod-Datei, data/, decodiert). Der Debugmenue-Eintrag "texture_mods"
schaltet beide Mod-Stufen von apply_texture_files ab und laedt den Raum an derselben Stelle neu; das Bild
ist dann dasselbe wie ohne Mods-Ordner mit demselben Neuladen.

Das Inventar (GameUi menu/fonts) folgt derselben Rangfolge: eine HD-Datei fuer menu/0001 (Inventarrahmen)
in anderer Groesse aendert das Inventarbild, der Schalter laedt die Atlanten sofort neu (PS2-Bild).

Aufbau: ein Mods-Ordner mit genau einer HD-Datei (magenta) unter dem MD5 von RM_1090/0000.tga und einer
(cyan, doppelte Groesse) unter dem MD5 von menu/0001.tga."""
import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

ROOM = 'RM_1090'
OFF = '60 debug_key F2\n61 debug_select texture_mods 0\n62 confirm\n63 debug_key F2\n'


def solid_tga(path, size=8, bgra=(255, 0, 255, 255)):
    header = bytes([0, 0, 2]) + bytes(9) + size.to_bytes(2, 'little') + size.to_bytes(2, 'little') + bytes([32, 0x28])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + bytes(bgra) * size * size)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-texmods-') as folder:
        root = Path(folder)
        source = data / 'textures/rooms' / ROOM / '0000.tga'
        solid_tga(root / 'mods/textures/hd' / (hashlib.md5(source.read_bytes()).hexdigest() + '.tga'))
        frame = data / 'textures/menu/0001.tga'
        solid_tga(root / 'mods/textures/hd' / (hashlib.md5(frame.read_bytes()).hexdigest() + '.tga'), 1024, (255, 255, 0, 255))
        (root / 'off.txt').write_text(OFF)

        def run(name, mods, replay=None, mode=('--in-game', '--room', ROOM + '.RDX')):
            state, shot = root / f'{name}.json', root / f'{name}.bmp'
            command = [runtime, '--data', str(data), '--mods', str(mods), '--snapshots', str(root / 'snap'),
                       '--user-dir', str(root / f'user-{name}'), '--headless', *mode,
                       '--frames', '120', '--state', str(state), '--screenshot', str(shot)]
            if replay:
                command += ['--replay', str(replay)]
            done = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            return done.stdout, json.loads(state.read_text()), shot.read_bytes()

        out, on, on_image = run('on', root / 'mods')
        assert f'Data: "{data}" (textures: uncompressed TGA)' in out, out
        assert 'textures/hd: 2 files' in out, out
        assert f'{ROOM}.RDX textures: 1 HD,' in out, out
        assert on['texture_mods'] and on['texture_files']['hd'] == 2, on['texture_files']  # Raum + Inventarrahmen

        out, off, off_image = run('off', root / 'mods', root / 'off.txt')
        assert f'{ROOM}.RDX textures: 0 HD,' in out and '(texture mods off)' in out, out
        assert not off['texture_mods'] and off['texture_files']['hd'] == 2, off['texture_files']  # nur der erste Ladevorgang
        assert on['actor'] == off['actor'], ('Neuladen hat die Lage veraendert', on['actor'], off['actor'])
        assert on_image != off_image, 'HD-Datei hat das Bild nicht veraendert'

        out, _, ps2_image = run('ps2', root / 'no-mods', root / 'off.txt')
        assert 'Mods: ' in out and '(missing)' in out, out
        assert off_image == ps2_image, 'Schalter aus zeigt nicht das PS2-Bild'

        inventory = ('--inventory',)
        _, _, inv_hd = run('inv-on', root / 'mods', mode=inventory)
        _, _, inv_ps2 = run('inv-ps2', root / 'no-mods', mode=inventory)
        _, _, inv_off = run('inv-off', root / 'mods', root / 'off.txt', mode=inventory)
        _, _, inv_ps2_replay = run('inv-ps2-replay', root / 'no-mods', root / 'off.txt', mode=inventory)  # gleicher Takt
        assert inv_hd != inv_ps2, 'HD-Datei fuer menu/0001 hat das Inventar nicht veraendert'
        assert inv_off == inv_ps2_replay, 'Schalter aus laedt die Inventaratlanten nicht neu'
    print(f'Texture mods: startup lines, {ROOM} HD count, inventory atlas HD and in-place switch to the PS2 image passed')


if __name__ == '__main__':
    main()
