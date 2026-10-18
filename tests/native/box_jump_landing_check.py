#!/usr/bin/env python3
"""Kistensprung ohne Versatz (bhCPM2_act_dnd, player.c:3508-3724).

Diagnostische Platzierung (wie tools/verify_crate_edge.py) auf der Oberseite der
RM_0100-Kiste 7 (bhObj001-Dansa-Flaeche -16.5..-7.5 / -110.1..-101.1, Etage 2),
Blick ueber eine freie Kante, Confirm startet dnd (Aktion 17).

Original: dnd case 4 setzt plp->flr_no auf die untere Etage (player.c:3601), bevor
plp->py sinkt. bhCheckFloorP (player.c:1664) prueft deshalb in jedem folgenden
Pass die Dansa-Flaechen der unteren Etage und loescht stflg 0x20000
(hitchk.c:5357-5389). Im Abschlussbild setzt dnd plp->px/pz = owP[0]
(player.c:3688-3690); der Koerperpass (player.c:1600-1655, bhCheckWallEx) baut ohne
stflg 0x20000 keine Stufenkanten-Begrenzung (bhSetDansaLimitAtari, hitchk.c:666)
und die Figur bleibt auf dem Knoten stehen.

Der Port nahm die Etage aus plp->py (obere Etage) und schrieb ausserdem den
stflg-Stand vom Aktionsbeginn zurueck: das Abschlussbild schob die Figur 5.6
Einheiten von der Kiste weg (RM_0100 Nord: z -116.74 -> -122.35), act_std zog sie
in drei Bildern zurueck (-120.67, -118.99, -117.31) - der sichtbare Ruck."""
import argparse
import json
import math
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm0100-crate-jump-down.txt'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    base = REPLAY.read_text()
    assert '30 position -12 13 -105.6 0\n' in base, base
    failures = []
    with tempfile.TemporaryDirectory(prefix='recvx-box-jump-') as folder:
        root = Path(folder)
        # Nord (yaw 0, Replaydatei), Sued und Ost sind an Kiste 7 frei; West grenzt an die Wand.
        for yaw in (0, 32768, 49152):
            replay = root / f'jump-{yaw}.txt'
            replay.write_text(base.replace('-105.6 0\n', f'-105.6 {yaw}\n', 1))
            state = root / f'jump-{yaw}.json'
            result = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                     '--in-game', '--headless', '--room', 'RM_0100.RDX', '--frames', '200',
                                     '--replay', str(replay), '--state', str(state)],
                                    env=dict(os.environ, RECVX_POSITION_TRACE='1'), check=True,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            s = json.loads(state.read_text())
            assert s['stairs'] and s['stairs']['mode2'] == 17 and not s['stairs']['active'], (yaw, s['stairs'])
            assert s['motion'] == 42 and s['actor'][1] == 4, (yaw, s['motion'], s['actor'])
            pos = [tuple(map(float, m.groups())) for m in
                   re.finditer(r'POSITION_FRAME pos=([-\d.e+]+),([-\d.e+]+),([-\d.e+]+)', result.stderr)]
            top = max(i for i, p in enumerate(pos) if p[1] == 13)
            landed = pos[top + 1:]
            assert landed and all(p[1] == 4 for p in landed), (yaw, landed[:3])
            rest = landed[-1]
            drift = max(math.hypot(p[0] - rest[0], p[2] - rest[2]) for p in landed)
            path = ', '.join(f'({p[0]:.3f}, {p[2]:.3f})' for p in landed[:5])
            print(f'yaw {yaw}: Landung {path} ... Ruhe ({rest[0]:.3f}, {rest[2]:.3f}), groesste Abweichung {drift:.3f}')
            if drift > 0.001:
                failures.append(f'yaw {yaw}: Figur springt nach dem Abschlussbild um {drift:.3f} Einheiten und wird zurueckgezogen')
    assert not failures, '; '.join(failures)
    print('RM_0100 Kiste 7: dnd landet auf owP[0] und bleibt dort (keine Stufenkanten-Begrenzung im Abschlussbild).')


if __name__ == '__main__':
    main()
