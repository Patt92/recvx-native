#!/usr/bin/env python3
"""Wasserklang von dnd/kdd: CallPlayerActionSe(522/523) ist Bank 2, Liste 10/11.

Diagnostische Platzierung in RM_3060 (Wasserflaeche Bodensatz 8: type 3, prm0 1,
x 2..58, z 2..56, Etage 0; Etage 1 ab z 49 auf y 9):
- jump-down: auf Etage 1 (30, 9, 58), Blick -z, Confirm startet dnd (Aktion 17).
  Das Ende landet im Wasser; case 5 ruft bhCheckWater und CallPlayerActionSe(522, 1)
  (player.c:3666).
- climb-up: im Wasser (30, 0, 46), Blick +z, Confirm startet kdd (Aktion 16);
  case 5 ruft mit st_flg 0x40 und status 0x100000 CallPlayerActionSe(523, 1)
  (player.c:3454).

CallPlayerActionSe -> CallPlayerFootStepSeEx(SeNo, 3, ...) (sdfunc.c:1541) legt
SeNo in RequestInfo.ListNo ab (sdfunc.c:1494). ListNo ist char (types.h:1754),
die ELF speichert es mit `sb $s3,0x6F21` (0x294A88), PlaySe/sdShotPlay erhalten
also 522 & 0xff = 10 bzw. 523 & 0xff = 11 in Bank 2. Der Port gab die volle SeNo als
Programmindex an die Raumbank 2 weiter; decode_program_voices warf
"Sony sound-bank index outside table" und beendete die Runtime."""
import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CASES = (
    ('rm3060-water-jump-down.txt', 17, 0, '/2/0010'),
    ('rm3060-water-climb-up.txt', 16, 9, '/2/0011'),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    failures = []
    with tempfile.TemporaryDirectory(prefix='recvx-rm3060-water-') as folder:
        root = Path(folder)
        for name, action, landing_y, sound in CASES:
            state = root / (name + '.json')
            result = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                     '--in-game', '--headless', '--room', 'RM_3060.RDX', '--frames', '300',
                                     '--replay', str(ROOT / 'tests/native/replays' / name), '--state', str(state)],
                                    env=dict(os.environ), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            if result.returncode:
                failures.append(f'{name}: Runtime endet mit {result.returncode}: {result.stderr.strip().splitlines()[-1:]}')
                continue
            s = json.loads(state.read_text())
            assert s['stairs'] and s['stairs']['mode2'] == action and not s['stairs']['active'], (name, s['stairs'])
            assert s['actor'][1] == landing_y, (name, s['actor'])
            played = [p for p in s['audio_played'] if p.endswith(sound)]
            print(f'{name}: Aktion {action}, Ende y {s["actor"][1]}, Wasserklang {played}')
            if not played:
                failures.append(f'{name}: Bank 2 Liste {sound[-2:]} nicht gespielt: {s["audio_played"]}')
    assert not failures, '; '.join(failures)
    print('RM_3060: CallPlayerActionSe 522/523 spielen Bank 2 Liste 10/11 (ListNo char).')


if __name__ == '__main__':
    main()
