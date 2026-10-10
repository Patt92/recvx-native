#!/usr/bin/env python3
"""Diagnose camera/flag transitions through native room events and rendering.

Uses the unmodified RM_0020 geometry and camera resources with an isolated
event table. This proves production bindings, not natural story progression.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    phases = [
        ('event1', bytes([0x13, 0, 1, 0]), [24, 5000]),
        ('event4', bytes([0x13, 0, 4, 0]), [25, 20000]),
        ('non-game-miss', bytes([5, 12, 22, 0, 0, 1, 0x13, 0, 0, 0]), [1, 99]),
        ('flag-only-retains-clip', bytes([5, 12, 22, 0, 0, 0]), [1, 99]),
        ('game-miss', bytes([0x13, 0, 0, 0]), [2, 20000]),
        ('non-game-hit', bytes([5, 12, 22, 0, 0, 1, 0x13, 0, 1, 0]), [24, 5000]),
        ('rcase-only-retains-clip', bytes([0x37, 1]), [24, 5000]),
        ('rcase-miss', bytes([0x13, 0, 1, 0]), [1, 99]),
        ('fixed-cut-uses-room-cut-key', bytes([0x37, 0, 5, 12, 22, 0, 0, 0, 0x2a, 0, 4, 0]), [2, 20000]),
        ('room-return', bytes([0x13, 1, 0, 0]), [2, 20000]),
        ('non-game-return', bytes([5, 12, 22, 0, 0, 1, 0x13, 1, 0, 0]), [1, 99]),
        ('eye-view-return', bytes([5, 12, 22, 0, 0, 0, 5, 11, 25, 0, 0, 0, 0x13, 1, 0, 0]), [1, 20000]),
    ]
    with tempfile.TemporaryDirectory(prefix='recvx-clip-events-') as folder:
        mods = Path(folder)
        (mods / 'rooms').mkdir()
        for index, (name, _, expected) in enumerate(phases):
            # Each prefix ends in a long wait. Command counts differ between
            # phases; a guessed 13-frame cadence can sample the next phase.
            room = bytearray((args.data / 'rooms/RM_0020.RDT').read_bytes())
            script = b''.join(command + bytes([0xf8]) for _, command, _ in phases[:index+1])
            script += bytes([0xf9, 250, 0, 0, 0])
            table = len(room)
            room += struct.pack('<III', 12, 18, 20) + bytes([0x14, 0, 0, 0, 0, 0, 0, 0]) + script
            struct.pack_into('<I', room, 28, table)
            (mods / 'rooms/RM_0020.RDT').write_bytes(room)
            state = args.output / (name + '.json')
            if state.exists():
                raise RuntimeError('Use a fresh output directory: ' + str(state))
            with (args.output / (name + '.log')).open('w') as log:
                subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                                '--mods', str(mods), '--headless', '--in-game', '--room', 'RM_0020.RDX',
                                '--frames', '50', '--ss', '1', '--state', str(state),
                                '--screenshot', str(args.output / (name + '.bmp'))],
                               check=True, stdout=log, stderr=subprocess.STDOUT)
            value = json.loads(state.read_text())
            assert not value['blocked_commands'] and not value['pending_commands'], (name, value)
            assert value['render_clip'] == expected, (name, value['render_clip'], expected)
            assert value['hp'] == 160, (name, 'unrelated HP regression')
        # Original self-destruct game-over branch: gm_flg 0x400, then
        # bhGameOverSet. Preserve the explicit njClipZ of gameover.c:244;
        # setting cam.flg 0x40 alone does not invoke the room-table query.
        room = bytearray((args.data / 'rooms/RM_0020.RDT').read_bytes())
        table = len(room)
        script = bytes([0xf8, 0xf9, 20, 0, 5, 11, 21, 0, 0, 0, 0xc6, 0, 0])
        room += struct.pack('<III', 12, 18, 20) + bytes([0x14, 0, 0, 0, 0, 0, 0, 0]) + script
        struct.pack_into('<I', room, 28, table)
        (mods / 'rooms/RM_0020.RDT').write_bytes(room)
        state = args.output / 'monitor-death.json'
        with (args.output / 'monitor-death.log').open('w') as log:
            subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                            '--mods', str(mods), '--headless', '--in-game', '--room', 'RM_0020.RDX',
                            '--frames', '120', '--ss', '1', '--state', str(state),
                            '--screenshot', str(args.output / 'monitor-death.bmp')],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        value = json.loads(state.read_text())
        assert value['gameover_active'] and value['gameover_phase'] == 10, value
        assert value['render_clip'] == [2, 20000], value['render_clip']
        assert not value['blocked_commands'] and not value['pending_commands'], value
    print(f'{len(phases)} native camera invocation/latch transitions reached the renderer with the original clip ranges.')
    print('Native self-destruct death scene preserved its explicit clip range 2/20000.')


if __name__ == '__main__':
    main()
