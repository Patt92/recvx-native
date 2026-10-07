#!/usr/bin/env python3
"""Check original RM_0060 exit music in the runtime, with normal and skipped doors.
Positions are diagnostic placements; room scripts and door/audio resources are
unaltered. Dog cues are exercised by recvx-bgm-room-tests using original events.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='recvx-bgm-room-') as directory:
        root = Path(directory)
        subprocess.run([str(args.runtime.resolve()), '--write-controls', str(root / 'controls.ini')], check=True, stdout=subprocess.DEVNULL)
        for name, extra in [('natural', ''), ('skip', '21 confirm\n')]:
            replay = output / (name + '.txt')
            replay.write_text('20 position 61.495 0 29.995 0\n20 confirm\n' + extra +
                              '220 position -39.5 0 54.5 0\n220 confirm\n')
            state = output / (name + '.json')
            with (output / (name + '.log')).open('w') as log:
                subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                    '--controls', str(root / 'controls.ini'), '--headless', '--in-game',
                    '--room', 'RM_0060.RDX', '--frames', '500', '--replay', str(replay),
                    '--state', str(state)], stdout=log, stderr=subprocess.STDOUT, check=True)
            value = json.loads(state.read_text())
            assert value['room'] == 'RM_0080.RDX' and value['room_changes'] == 2, value
            assert not value['bgm_active'] and value['bgm_id'] == -1, value
            assert not value['bgm_request'] and not value['blocked_commands'], value
            assert 'BGM1/0000' in value['audio_played'], value
        print('Original RM_0060 BGM starts, stops at exit 9 and stays stopped through RM_0050 to RM_0080; normal and skipped door paths passed.')


if __name__ == '__main__':
    main()
