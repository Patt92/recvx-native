#!/usr/bin/env python3
"""RM_1130: U-Boot-Steuerrad, Ruecksetzen der Spielerin am Ende der Fahrt ohne Satz zur Seite.

Ereignis 20/30 (Rad, ATR 2/3) setzt die Spielerin per Common 7 und Overhauser-Spline (event.c:12034,
:10644) mit der Plattform, ohne exp0 spx/spz nachzufuehren. Am Ende 69 29 01 (plp->flg 0x40000,
event.c:10346) und 64 80 (Player_controll 128). act_std case 0 (player.c:2127-2140) zieht danach mit
spxn = 0.3 * (spx - px) zur alten Sicherposition; bhFixPosition (player.c:1538-1558, Motion.c:857-916)
haelt unter flg 0x40000 den Fuss und hebt diesen Zug auf.

Vorher fehlte die Fussfixierung nativ: x sprang im Freigabebild von 61.12 auf 62.50.

Diagnoseaufstellung: Eingang 1, Position am Rad."""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

def replay_text():
    lines = ['5 position 63 0 120 16384', '20 confirm']
    lines += [f'{f} confirm' for f in range(30, 121, 6)]
    lines += [f'{f} confirm' for f in range(430, 557, 6)]
    return '\n'.join(lines) + '\n'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-rm1130-sub-') as folder:
        root = Path(folder)
        replay = root / 'replay.txt'
        replay.write_text(replay_text())
        env = dict(os.environ, RECVX_POSITION_TRACE='1')
        run = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                              '--mods', str(root / 'no-mods'), '--user-dir', str(root / 'user'),
                              '--headless', '--in-game', '--room', 'RM_1130.RDX', '--spawn', '1',
                              '--frames', '760', '--replay', str(replay)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
        assert run.returncode == 0, run.stdout[-2000:]
        xs, modes, flgs = [], [], []
        for line in run.stdout.splitlines():
            if line.startswith('POSITION_FRAME'):
                fields = dict(f.split('=', 1) for f in line.split()[1:] if '=' in f)
                xs.append(float(fields['pos'].split(',')[0]))
                modes.append(int(fields['mode']))
                flgs.append(int(fields['flg'], 16))
        assert len(xs) >= 740, len(xs)
        # Freigabe der zweiten Fahrt (Ereignis 20 zurueck nach x 61): das erste Bild nach Bild 600, in dem
        # Player_controll 128 mode0 1 setzt und flg 0x40000 (69 29 01) gesetzt ist.
        release = next((i for i in range(600, 740) if modes[i] & 0xff == 1 and modes[i - 1] & 0xff == 7 and flgs[i] & 0x40000), None)
        assert release is not None, 'Freigabe der Rueckfahrt nicht im Pruefbereich'
        assert xs[release] < 61.5, ('Fahrt zurueck nicht erreicht', xs[release])
        # Ab der Freigabe bis zum Ende der Mischung (flg 0x40000 geloescht) und darueber hinaus.
        settled = next(i for i in range(release, len(xs)) if not flgs[i] & 0x40000)
        assert settled + 10 < len(xs), settled
        jump = max(abs(xs[i + 1] - xs[i]) for i in range(release - 1, settled + 10))
        assert jump < 0.5, ('Satz nach dem Anhalten', jump, xs[release - 1:settled + 10])
    print('RM_1130 submarine wheel: no sideways jump after the platform stops.')

if __name__ == '__main__':
    main()
