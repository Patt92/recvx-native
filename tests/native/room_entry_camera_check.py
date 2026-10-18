#!/usr/bin/env python3
"""Raumeintritt ohne Kameraschwenk (cut.c:663-710 bhInitActiveCamera).

RM_1000 Spawn 2 steht laut Raumtabelle bei z 115.9, knapp ausserhalb der
Schnittflaeche von Kamera 0 (z bis 115.0). Das Original rechnet die
Verfolgungspose px + w ohne Bereichsgrenze; der Port liess dort die rohe
CAM_WRK-Lage stehen und schwenkte danach ueber rund 24 Einheiten herein."""
import argparse
import math
import os
import re
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-entry-camera-') as folder:
        root = Path(folder)
        # Titelstart ohne Raum: keine Schnitttabelle, die Kameranachfuehrung darf nichts rechnen
        # (2026-10-18: "active camera outside table" beim Start der App).
        title = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                '--headless', '--frames', '60'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert 'Native runtime error' not in title.stdout and 'Rendered 60 frames' in title.stdout, title.stdout[-400:]
        for spawn in (0, 1, 2):
            env = dict(os.environ, RECVX_CAM_TRACE='1')
            result = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                     '--in-game', '--headless', '--room', 'RM_1000.RDX', '--spawn', str(spawn), '--frames', '30',
                                     '--state', str(root / f'state-{spawn}.json')],
                                    env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            poses = [tuple(map(float, re.search(r'pos=([-\d.]+),([-\d.]+),([-\d.]+)', line).groups()))
                     for line in result.stderr.splitlines() if line.startswith('CAMOUTPUT')]
            assert len(poses) >= 20, (spawn, len(poses))
            travel = sum(math.dist(poses[i], poses[i + 1]) for i in range(len(poses) - 1))
            assert travel < 2.0, f'RM_1000 Spawn {spawn}: Kamera schwenkt nach dem Eintritt {travel:.2f} Einheiten (bhInitActiveCamera setzt die Verfolgungspose sofort)'
    print('RM_1000 Spawn 0/1/2: Kamera steht ab dem ersten Bild auf der Verfolgungspose, kein Eintrittsschwenk.')


if __name__ == '__main__':
    main()
