#!/usr/bin/env python3
"""Replay original detector/box scripts with diagnostic trigger placement.

This checks script handover and the real input path, not a complete playthrough.
Resource tests in recvx-progression-tests also cover the original fire atlases,
segmented zombie motions, Cerberus activation and non-looping Steve music.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPLAY = '''0 debug_key F2
1 debug_select give 5
2 confirm
3 debug_key F2
60 position 34.5 0 5 0
100 position 44.5 0 5 0
180 confirm
220 confirm
320 position 44.5 0 10 0
321 confirm
350 right
351 confirm
352 confirm
360 up
361 confirm
362 down
363 confirm
370 cancel
400 position 34.5 0 5 0
420 position 44.5 0 5 0
560 position 44.5 0 10 0
561 confirm
590 right
591 confirm
592 confirm
600 up
601 confirm
602 down
603 confirm
610 cancel
'''


def check(runtime, data, folder, prefix):
    replay = folder / 'security-path.txt'
    replay.write_text(REPLAY)
    for name, frames in [('announcement', 110), ('handover', 270),
                         ('box', 364), ('deposit-pass', 550), ('withdraw', 680)]:
        state = folder / (name + '.json')
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run(prefix + [str(runtime.resolve()), '--data', str(data.resolve()),
                '--headless', '--in-game', '--room', 'RM_0090.RDX', '--frames', str(frames),
                '--replay', str(replay), '--state', str(state)], cwd=ROOT,
                stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        assert not result['blocked_commands'], (name, result['event_tasks'])
        assert result['audio_played'].count('VOICE1/0455') == 1, (name, 'announcement missing/restarted')
        if name == 'announcement':
            # Original message 16 is FF02/1/FFFF (one-tick empty message).
            # The announcement is VOICE1/0455, not an invented subtitle.
            assert result['dialog_index'] == 16, 'original message not dispatched'
            assert result['fixed_camera'] == 9 and result['player_hidden'], result
        elif name == 'box':
            assert result['box_open'] and result['inventory_open'] and result['box_base'] == 224, result
        else:
            for field in ['player_hidden', 'player_scripted', 'cinematic', 'dialog_open', 'inventory_open', 'box_open']:
                assert not result[field], (name, field)
            assert result['fixed_camera'] == -1, name
        if name in ('box', 'deposit-pass'):
            assert result['security_items_2'][:2] == [327681, 3604481], (name, 'gun/lighter deposit')
            assert result['items'][1] == result['items'][3] == 0, name
        if name == 'withdraw':
            assert result['items'][1] == 3604481 and result['items'][3] == 327681, 'withdrawal'
            assert not any(result['security_items_2']), 'storage not emptied'
    print('Original detector announcement/camera, control handover, box deposit, pass and withdrawal verified. Trigger positions are diagnostic; box drawing remains a host view.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-progression-') as temporary:
        folder = args.output.resolve() if args.output else Path(temporary)
        folder.mkdir(parents=True, exist_ok=True)
        check(args.runtime, args.data, folder, [args.emulator] if args.emulator else [])


if __name__ == '__main__':
    main()
