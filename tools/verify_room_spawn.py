#!/usr/bin/env python3
"""Check room-load presentation ordering with an original door and saved height.

The door approach uses diagnostic positioning; scripts and door timing are original.
"""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())

    def run(name, replay, frames, *extra):
        script, state = folder / (name + '.txt'), folder / (name + '.json')
        script.write_text(replay)
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run([runtime, '--data', data, '--headless', '--replay', str(script),
                            '--frames', str(frames), '--state', str(state),
                            '--snapshots', str(folder / 'snapshots'), *extra],
                           cwd=ROOT, stdout=log, stderr=log, check=True)
        return json.loads(state.read_text())

    door = '10 skip\n60 position 50 0 34 32768\n61 confirm\n'
    before = run('door-last', door, 225, '--in-game', '--room', 'RM_0040.RDX', '--spawn', '1')
    assert before['door_active'] and before['room'] == 'RM_0040.RDX'
    # main.c:195-201: Task 20 releases Task 7 after its turn in the table.
    # At room load the door task still owns presentation; the following pass
    # controls the destination before its first game drawing (game.c:48-74).
    handover = run('door-226', door, 226, '--in-game', '--room', 'RM_0040.RDX', '--spawn', '1')
    assert handover['room'] == 'RM_0090.RDX' and handover['room_changes'] == 1
    assert handover['door_active'] and handover['actor'][1] == 0 and not handover['blocked_commands']
    for frames in [227, 228, 240]:
        s = run('door-' + str(frames), door, frames, '--in-game', '--room', 'RM_0040.RDX', '--spawn', '1')
        assert s['room'] == 'RM_0090.RDX' and s['room_changes'] == 1 and not s['door_active']
        assert s['actor'][1] == 0 and not s['blocked_commands'], s
    # The restore branch must preserve a saved height between rom->grand entries.
    saved = run('save-height', '0 position 69.9715 3 3.96232 32776\n0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n',
                3, '--in-game', '--room', 'RM_0090.RDX', '--spawn', '1')
    loaded = run('load-height', '0 debug_key F2\n1 debug_select snapshot_load 0\n2 confirm\n2 debug_key F2\n', 3)
    assert saved['actor'] == loaded['actor'] and loaded['actor'][1] == 3
    assert not loaded['blocked_commands']
    print('Original RM_0040 → RM_0090 first visible frame, following ticks and saved Y preservation passed.')


if __name__ == '__main__':
    main()
