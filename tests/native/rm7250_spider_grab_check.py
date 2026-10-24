#!/usr/bin/env python3
"""RM_7250 Schwarze Witwe: Griff mit plp->mnwP = epw->mnwP (en03.c:6825).

Die Spielerin geht kurz auf die Spinne zu, Gegner 1 packt sie (Headless, --enemy-ai on). Ohne
Eingabe steht sie in act_sta (flg 0x40000, player.c:2234); bhFixPosition (player.c:1538-1558) hebt dort den
Schub aus bhCheckPlayer (hitchk.c:5765) wieder auf, die Spinne schiebt sie also nicht in Reichweite. Der Griff
haengt die Spinnenbibliothek ueber den Dienst player_library ein und schreibt
danach den vorher gelesenen Spielerstand zurueck. Vorher setzte genau dieses
Zurueckschreiben plp->mnwP wieder auf mnwPb, und die Griffmotion 62/63 fehlte
dort ("bhSetMotion(plp): mnwPb[mtn_no] fehlt"). Gepruefter Zustand: der Lauf
erreicht den Griff (Vibration beim Zupacken) und meldet diese Luecke nicht."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        state = Path(tmp) / 'state.json'
        replay = Path(tmp) / 'walk.txt'
        replay.write_text('200 keys forward\n225 keys none\n')
        run = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--in-game', '--headless',
                              '--enemy-ai', 'on', '--room', 'RM_7250.RDX', '--frames', '600', '--replay', str(replay), '--state', str(state)],
                             capture_output=True, text=True, timeout=600)
        assert run.returncode == 0, run.stderr[-2000:]
        assert 'bhSetMotion(plp)' not in run.stdout, 'Griff spielt die Spielermotion aus der falschen Bibliothek:\n' + run.stdout[-1500:]
        out = json.loads(state.read_text())
        # bhEne03_PlayerControl Fall 0 (en03.c:6838): StartVibrationEx(1, 11) beim Zupacken.
        assert out['vibration_requests'] > 0, 'RM_7250: der Griff der Spinne fand nicht statt'
    print('RM_7250 spider grab PASS')


if __name__ == '__main__':
    main()
