#!/usr/bin/env python3
"""RM_1100 Palasttreppe: Event 0 (Offset 1082) mit ENDIF ohne IF bei 0x44c.

Original (event.c:425 bhEndif) senkt nur G_Sp/bhIfelFlg; die Szene laeuft weiter:
bhPlayerKaidanMotion (Treppe hoch), Spieler ausblenden, Fade, bhSetDoorCall nach
RM_2000 mit Tuer-/Treppendemo. Diagnose-Aufstellung am Ausloeser (Spawn 1, Position
12.539/0/23.9413), keine Flag-/Skriptaenderung.
"""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
(out / 'replay.txt').write_text('5 position 12.539 0 23.9413 0\n10 confirm\n')


def run(frames):
    state = out / f'state-{frames}.json'
    if state.exists():
        state.unlink()
    log = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(out / 'no-mods'),
                          '--room', 'RM_1100.RDX', '--spawn', '1', '--headless', '--in-game', '--frames', str(frames),
                          '--ss', '1', '--replay', str(out / 'replay.txt'), '--state', str(state)],
                         capture_output=True, text=True, check=True)
    assert 'ENDIF without IF' not in log.stdout + log.stderr, ('ENDIF ohne IF bricht das Event ab', frames)
    value = json.loads(state.read_text())
    assert value['blocked_commands'] == 0, (frames, 'blockierte Skriptbefehle', value['blocked_commands'])
    return value


climb = run(180)
assert climb['room'] == 'RM_1100.RDX' and climb['cinematic'] and climb['motion'] == 35, ('Treppenmotion 35 fehlt', climb['motion'])
assert climb['actor'][1] > 30, ('Spieler steigt die Treppe nicht hoch', climb['actor'])
assert climb['player_hidden'], ('Spieler nach der Treppe nicht ausgeblendet (24 00 00 00)',)
door = run(400)
assert door['door_active'] and door['door']['time'] > 0, ('Tuer-/Treppendemo nach bhSetDoorCall fehlt', door['door'])
after = run(900)
assert after['room'] == 'RM_2000.RDX' and after['room_changes'] == 1, ('Raumwechsel nach RM_2000 fehlt', after['room'])
assert not after['door_active'] and not after['player_hidden'], ('Steuerung nach der Demo nicht zurueck',)
print('RM_1100 palace stairs: climb, hide, door demo and RM_2000 passed (diagnostic placement).')
