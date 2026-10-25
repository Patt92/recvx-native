#!/usr/bin/env python3
"""RM_7160 lift arrival: diagnostic placement, real climb and walking input.

Supply an existing snapshot after the RM_7110 lift trip. Copy it into an
isolated user directory; never mutate the user's snapshot. The first game
pass must register crate terrain before the next event tick lowers the works.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--snapshot', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-lift-crate-') as folder:
        root = Path(folder)
        (root / 'snapshots').mkdir()
        shutil.copyfile(args.snapshot, root / 'snapshots/snapshot-2.rcvxsnap')
        replay = root / 'input.txt'
        replay.write_text('1 debug_key F2\n2 debug_select snapshot_load 1\n3 confirm\n'
                          '110 position 81.1 0 100 16384\n120 confirm\n'
                          '210 keys forward\n280 keys none\n300 confirm\n')
        for frames in (205, 285, 390):
            state = root / f'{frames}.json'
            subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                            '--mods', str(root / 'no-mods'), '--user-dir', str(root),
                            '--snapshots', str(root / 'snapshots'), '--headless', '--in-game',
                            '--replay', str(replay), '--frames', str(frames), '--state', str(state)],
                           check=True, stdout=subprocess.DEVNULL)
            result = json.loads(state.read_text())
            assert result['room'] == 'RM_7160.RDX', result['room']
            x, y, z = result['actor']
            if frames == 390:
                assert abs(y) < .001 and x < 68.5, result['actor']
                print(f'{frames}: deliberate descent returns to floor 0: {result["actor"]}')
                continue
            assert abs(y - 9) < .001, (frames, result['actor'])
            assert 70 <= x <= 77.5 and 95.5 <= z <= 104.5, (frames, result['actor'])
            assert not result['blocked_commands'], result['blocked_commands']
            if frames == 285:
                assert result['audio_played'][-1].endswith('/0003'), result['audio_played']
            print(f'{frames}: crate contact {result["actor"]}; original edge/material retained')


if __name__ == '__main__':
    main()
