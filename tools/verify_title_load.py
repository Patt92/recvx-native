#!/usr/bin/env python3
"""Titel-Navigation mit/ohne Host-Spielstand; isolierte Dateien, echte Runtime.

adv.c:1864-1898 sperrt Cursor 0 nur bei Flag 0; DisplayGameModePlate
(:1654) zeichnet unselektierte Eintraege mit 0x7f7f7f.
Der Snapshot dient nur als SAVEFILE-Fixture, nicht als Schreibmaschinenbeleg.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from verify_frontend import bmp_rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True, help='Runtime oder Start.command')
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-title-load-') as folder:
        root = Path(folder)
        saves = root / 'saves'
        saves.mkdir()
        subprocess.run([str(args.runtime.resolve()), '--write-controls', str(root / 'controls.ini')],
                       check=True, stdout=subprocess.DEVNULL)

        def run(name, replay, frames=120, extra=(), sdl=False):
            script = output / (name + '.txt')
            script.write_text(replay)
            state = output / (name + '.json')
            picture = output / (name + '.bmp')
            state.unlink(missing_ok=True)
            picture.unlink(missing_ok=True)
            command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                       '--mods', str(root / 'mods'), '--user-dir', str(root / 'user'),
                       '--controls', str(root / 'controls.ini'), '--debug-config', str(root / 'debug.ini'),
                       '--snapshots', str(root / 'snapshots'), '--saves', str(saves),
                       '--ss', '1', '--renderer', 'software', '--frames', str(frames),
                       '--replay', str(script), '--state', str(state), '--screenshot', str(picture), *extra]
            env = os.environ.copy()
            if sdl:
                env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
            else:
                command.append('--headless')
            with (output / (name + '.log')).open('w') as log:
                subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            return json.loads(state.read_text())

        empty = run('empty', '60 confirm\n90 up\n92 up\n')
        assert empty['title_mode'] == 9 and empty['title_cursor'] == 1, empty
        # Ein kurzer Entwicklungs-Snapshot stellt einen lesbaren Host-Slot bereit.
        fixture = run('fixture', '5 debug_key F2\n6 debug_select snapshot_save 0\n7 confirm\n8 debug_key F2\n',
                      frames=20, extra=('--in-game', '--room', 'RM_0010.RDX'))
        assert fixture['debug_status'].startswith('Snapshot 1 gespeichert'), fixture['debug_status']
        shutil.copyfile(root / 'snapshots/snapshot-1.rcvxsnap', saves / 'save-1.rcvxsav')
        initial = run('saved-initial', '60 confirm\n')
        down = run('saved-down', '60 confirm\n90 down\n')
        back = run('saved-back', '60 confirm\n90 down\n92 up\n94 up\n')
        assert initial['title_cursor'] == back['title_cursor'] == 0, (initial['title_cursor'], back['title_cursor'])
        assert down['title_cursor'] == 1, down['title_cursor']
        # Echte SDL-Tasten durch InputManager, nicht nur MenuInput-Replay.
        physical = run('sdl-back', '60 sdl_key down Return\n62 sdl_key up Return\n'
                       '90 sdl_key down Down\n92 sdl_key up Down\n'
                       '94 sdl_key down Up\n96 sdl_key up Up\n', frames=700, sdl=True)
        assert physical['title_mode'] == 9 and physical['title_cursor'] == 0, physical['title_cursor']
        loaded = run('sdl-load-open', '60 sdl_key down Return\n62 sdl_key up Return\n'
                     '90 sdl_key down Down\n92 sdl_key up Down\n'
                     '94 sdl_key down Up\n96 sdl_key up Up\n'
                     '100 sdl_key down Return\n102 sdl_key up Return\n', frames=1000, sdl=True)
        assert not loaded['title_active'] and loaded['typewriter']['active'], loaded['typewriter']
        # Ohne Saves bleibt LOAD grau, exakt wie die unselektierte Platte mit Saves.
        def load_band(name):
            _, _, rows = bmp_rows(output / (name + '.bmp'))
            return [pixel for row in rows[272:304] for pixel in row[64:577]]
        grey = load_band('empty')
        assert grey == load_band('saved-down'), 'unselected LOAD GAME differs with/without saves'
        bright = load_band('saved-back')
        assert sum(max(p) for p in bright) > sum(max(p) for p in grey), 'selected LOAD GAME is not brighter'
        assert sum(1 for r, g, b in grey if abs(r-g) < 14 and abs(g-b) < 14 and 95 < max(r,g,b) < 150) > 500, 'grey LOAD GAME plate missing'
        # Defekte Slots bleiben erreichbar, damit der Ladebildschirm den Defekt melden kann.
        (saves / 'save-1.rcvxsav').write_bytes(b'damaged fixture')
        damaged = run('damaged-back', '60 confirm\n90 down\n92 up\n')
        assert damaged['title_cursor'] == 0, damaged['title_cursor']
    print('PASS: Startauswahl, runter/hoch, obere Grenze, SDL-Tasten, graue Platte ohne Saves und defekte Slots.')


if __name__ == '__main__':
    main()
