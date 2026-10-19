#!/usr/bin/env python3
"""bhControlPlayerHead und bhCalcModel(plp) liegen im Passtor von bhControlPlayer.

player.c:1365: `if ((sys->sp_flg & 0x1) && !(plp->stflg & 0x1000000))` umschliesst den ganzen
Spielerpass bis :1766, darunter bhControlPlayerHead (:1683-1686) und bhCalcModel(plp) (:1688).
Bei geschlossenem Tor bleiben plp->exp1 (EXP1_I(8) Blinkuhr, EXP1_I(60) Blickuhr, EXP1_F(12..20)
Kopfknoten) und owP unveraendert. bhSetDispObj (event.c:2283-2330) setzt nur plp->mdflg 0x1 und
sperrt den Pass nicht; das Tor fuer den Ereignisstillstand ist bhInitModelSet (event.c:2826-2840).

Aufstellung (Diagnose, kein natuerliches Durchspielen): RM_1140-Hebelreplay, Bild 10 Untersuchen.
Waehrend der Meldung steht sp_flg 0xb8 (Bit 0x1 aus). Messung: Kopfzustand der Bilder 40 und 70
gleich. Gegenprobe im selben Lauf: vor der Meldung (Bilder 2 und 8, sp_flg 0x1 an) laeuft die
Blickuhr weiter - die Messung erkennt also einen laufenden Kopfpass.

Der Port rief den Kopf- und Modellpass ohne dieses Tor auf (nur `player_hidden`); die Uhren
liefen waehrend jeder Meldung weiter."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm1140-lever.txt'


def run(runtime, data, root, frames):
    state = root / f'f{frames}.json'
    subprocess.run([str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'), '--user-dir', str(root / 'user'),
                    '--in-game', '--headless', '--room', 'RM_1140.RDX', '--frames', str(frames),
                    '--replay', str(REPLAY), '--state', str(state)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.loads(state.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-head-gate-') as folder:
        root = Path(folder)
        (root / 'user').mkdir()
        s = {f: run(runtime, data, root, f) for f in (2, 8, 40, 70)}
        for f in (2, 8):
            assert s[f]['work_flags'][6] & 1 and not s[f]['work_flags'][14] & 0x1000000, (f, s[f]['work_flags'][6], s[f]['work_flags'][14])
        assert s[2]['player_head']['look_timer'] != s[8]['player_head']['look_timer'], \
            f"Gegenprobe: offener Spielerpass ohne Fortschritt der Blickuhr {s[2]['player_head']} {s[8]['player_head']}"
        for f in (40, 70):
            assert s[f]['dialog_open'] and not s[f]['work_flags'][6] & 1, (f, s[f]['dialog_open'], hex(s[f]['work_flags'][6]))
        assert s[40]['player_head'] == s[70]['player_head'], \
            f"Kopfpass lief trotz sp_flg ohne 0x1 (player.c:1365): {s[40]['player_head']} -> {s[70]['player_head']}"
    print('player.c:1365: Kopf- und Modellpass ruhen unter sp_flg ohne 0x1; offen laufen die Uhren weiter.')


if __name__ == '__main__':
    main()
