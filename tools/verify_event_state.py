#!/usr/bin/env python3
"""Check original VSync time units through the runtime, menu and debug pause.
Uses exported room geometry/assets with a small isolated event-table fixture;
this is a mechanism test, not campaign progression evidence.
"""
import argparse
import json
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', default='')
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    prefix = shlex.split(args.emulator)
    with tempfile.TemporaryDirectory(prefix='recvx-clock-') as directory:
        temp = Path(directory)
        fixture = temp / 'data'
        fixture.mkdir()
        for source in data.iterdir():
            if source.name != 'rooms':
                (fixture / source.name).symlink_to(source, target_is_directory=source.is_dir())
        (fixture / 'rooms').mkdir()
        for source in (data / 'rooms').iterdir():
            if source.name != 'RM_0000.RDT':
                (fixture / 'rooms' / source.name).symlink_to(source)
        room = bytearray((data / 'rooms/RM_0000.RDT').read_bytes())
        offset = len(room)
        # EVT_WORK with no room tasks: init flag 3 + stv_tm=100; idle scan.
        room += struct.pack('<II', 8, 19) + bytes([5, 1, 3, 0, 0, 0, 9, 9, 100, 0, 0, 0])
        struct.pack_into('<I', room, 28, offset)
        (fixture / 'rooms/RM_0000.RDT').write_bytes(room)
        with (temp / 'controls.log').open('w') as log:
            subprocess.run(prefix + [str(runtime), '--write-controls', str(temp / 'controls.ini')],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        replay = temp / 'pause.txt'
        replay.write_text('0 debug_key F2\n30 debug_key F2\n')

        def run(name, extra):
            state, log = temp / (name + '.json'), temp / (name + '.log')
            with log.open('w') as stream:
                result = subprocess.run(prefix + [str(runtime), '--data', str(fixture),
                    '--mods', str(temp / 'mods'), '--controls', str(temp / 'controls.ini'),
                    '--debug-config', str(temp / 'debug.ini'), '--headless',
                    '--state', str(state)] + extra, stdout=stream, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError(log.read_text())
            value = json.loads(state.read_text())
            assert value['blocked_commands'] == 0, (name, value)
            return value

        assert run('title', ['--frames', '30'])['play_time'] == 0
        assert run('running', ['--in-game', '--frames', '30'])['play_time'] == 160
        assert run('running-double', ['--in-game', '--frames', '60'])['play_time'] == 220
        paused = run('paused', ['--in-game', '--frames', '60', '--replay', str(replay)])
        assert paused['play_time'] == 160 and not paused['debug_open'], paused
        print('Original game-time gate, 60-Hz units, title exclusion and debug pause passed.')


if __name__ == '__main__':
    main()
