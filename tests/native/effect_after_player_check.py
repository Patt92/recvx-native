#!/usr/bin/env python3
"""bhControlEffect liest owP des bhCalcModel(plp) desselben Bildes.

game.c:48-51: bhControlEnemy, bhControlPlayer, bhControlEffect, bhControlObjItm. bhControlPlayer endet
mit bhControlPlayerHead und bhCalcModel(plp) (player.c:1683-1688); erst danach setzt bhControlEffect
fuer einen Satz mit flg 0x80 op->px = owP[lkono].mtx * (lox, loy, loz) (effect.c:800-812).

Aufstellung (Diagnose): RM_1140, Feuerzeug ausgeruestet, Rennbewegung. Die Flamme bhEff009 haengt
an Spielerknoten 9 (bhControlPlayer, WpnEffTab[1], lkono 9). Messung im Zustand nach dem Bild:
op->px gleich owP[9] * lo des zuletzt veroeffentlichten bhCalcModel(plp). Der Port lief den
Effektpass vor dem Spielerpass; die Flamme stand dann am Knoten des Vorbildes (Abstand bis 0.59).
Gegenprobe im selben Lauf: owP[9] aendert sich zwischen Bild 109 und 110 um mehr als 0.1, ein
um ein Bild verspaeteter Effekt faellt also auf."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm1140-lighter-run.txt'


def run(runtime, data, root, frames):
    state = root / f'f{frames}.json'
    subprocess.run([str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'), '--user-dir', str(root / 'user'),
                    '--in-game', '--headless', '--room', 'RM_1140.RDX', '--frames', str(frames),
                    '--replay', str(REPLAY), '--state', str(state)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.loads(state.read_text())


def calc(m, p):
    x, y, z = p  # njCalcPoint: NJS_MATRIX zeilenweise, Verschiebung in m[12..14]
    return [m[0] * x + m[4] * y + m[8] * z + m[12], m[1] * x + m[5] * y + m[9] * z + m[13], m[2] * x + m[6] * y + m[10] * z + m[14]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-effect-order-') as folder:
        root = Path(folder)
        (root / 'user').mkdir()
        states = {f: run(runtime, data, root, f) for f in (95, 109, 110, 130)}
        moved = max(abs(a - b) for a, b in zip(calc(states[109]['player_joints'][9], (0, 0, 0)), calc(states[110]['player_joints'][9], (0, 0, 0))))
        assert moved > 0.1, f'Gegenprobe: Knoten 9 bewegt sich zwischen Bild 109 und 110 nur um {moved}'
        for f, s in states.items():
            assert s['weapon'] == 1 and not s['inventory_open'], (f, s['weapon'], s['inventory_open'])
            flames = [e for e in s['effects'] if e['id'] == 9 and e['link'] == 0 and e['lkono'] == 9]
            assert len(flames) == 1, (f, s['effects'])
            e = flames[0]
            want = calc(s['player_joints'][9], e['lo'])
            d = max(abs(a - b) for a, b in zip(e['pos'], want))
            assert d < 0.002, f'Bild {f}: Flamme {e["pos"]} statt owP[9]*lo {want} (Abstand {d}): Effektpass vor bhCalcModel(plp)'
    print('game.c:49-50: bhControlEffect setzt die Feuerzeugflamme an owP[9] des bhCalcModel(plp) desselben Bildes.')


if __name__ == '__main__':
    main()
