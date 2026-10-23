#!/usr/bin/env python3
"""ORIGINAL-FIX (QUALITY.md) in the original rooms that reach each defect.

Each room runs headless with enemy AI and grab on, unmodified data, no input.
The check requires a finite player position and no remaining gap report from
the fixed mechanisms. A fix that regresses brings back the report (or NaN).

    RM_3210/RM_7060  bhObj005 neighbour cells before wt_wvp (water), id 88 motion
                     with more channels than model objects
    RM_5830          en02 SetSandEffectMain case 8 uninitialized ang
    RM_3011/RM_3020  en04 bhCheckRoute without route zone
    RM_3120          Common_controll 34 start frame 9226 on a 2-frame clip
    RM_7030          small-screen monitor before its first render cut
    RM_9321          bhEff_E12_Fire divides by ln == 0 (player on the fire)
"""
import argparse
import json
import math
import re
import subprocess
import tempfile
from pathlib import Path

ROOMS = {
    'RM_3210': 300, 'RM_7060': 300, 'RM_5830': 900, 'RM_3011': 600,
    'RM_3020': 600, 'RM_3120': 120, 'RM_7030': 120, 'RM_9321': 700,
}
FIXED = ('bhObj005: Nachbarzelle', 'Motionkanal ohne Modellknoten', 'ang uninitialisiert',
         'keine Wegezone', 'ausserhalb von frm_num', 'ohne gezeichnetes Bild')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    failures = []
    with tempfile.TemporaryDirectory(prefix='recvx-original-fix-') as temporary:
        root = Path(temporary)
        for room, frames in ROOMS.items():
            state = root / f'{room}.json'
            result = subprocess.run(
                [str(args.runtime), '--data', str(args.data), '--room', f'{room}.RDX', '--in-game',
                 '--headless', '--enemy-ai', 'on', '--enemy-grab', 'on', '--frames', str(frames),
                 '--ss', '1', '--user-dir', str(root / f'user-{room}'), '--state', str(state)],
                capture_output=True, text=True, timeout=600)
            if result.returncode != 0:
                failures.append(f'{room}: runtime exit {result.returncode}: {result.stderr.strip()[-200:]}')
                continue
            # The state writer prints non-finite floats bare; keep them visible as strings.
            text = re.sub(r'(?<![A-Za-z"])(-?nan|-?inf)(?![A-Za-z"])', r'"\1"', state.read_text())
            data = json.loads(text)
            actor = data['actor']
            if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in actor):
                failures.append(f'{room}: player position not finite: {actor}')
            for gap in data.get('enemy_gaps', []):
                if any(key in gap for key in FIXED):
                    failures.append(f'{room}: fixed defect reported again: {gap}')
            print(f'{room}: {frames} frames, actor {actor}, gaps {len(data.get("enemy_gaps", []))}')
    if failures:
        raise SystemExit('\n'.join(failures))
    print('ORIGINAL-FIX rooms passed')


if __name__ == '__main__':
    main()
