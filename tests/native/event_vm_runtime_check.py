#!/usr/bin/env python3
"""EventVm scheduler/world binding through recvx-runtime; isolated script fixture.

The room geometry/assets are original. Replaced EVT_WORK is a mechanism test,
not a walkthrough, natural event trigger or enemy acceptance.
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
    data, output = args.data.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='recvx-event-vm-') as directory:
        folder = Path(directory)
        fixture = folder / 'data'
        fixture.mkdir()
        for source in data.iterdir():
            if source.name != 'rooms':
                (fixture / source.name).symlink_to(source, target_is_directory=source.is_dir())
        (fixture / 'rooms').mkdir()
        for source in (data / 'rooms').iterdir():
            if source.name != 'RM_0080.RDT':
                (fixture / 'rooms' / source.name).symlink_to(source)
        room = bytearray((data / 'rooms/RM_0080.RDT').read_bytes())
        scripts = [
            bytes([0x14, 0, 0, 0, 0x14, 0, 1, 2, 0x14, 0, 2, 3, 0]),  # scd0
            bytes([5, 6, 27, 0, 0, 0, 0]),  # scd1 enables the scheduler
            bytes([0x14, 0, 0, 1, 5, 1, 44, 1, 0, 0, 0xff, 0]),  # self replacement
            bytes([5, 1, 45, 1, 0, 0, 0xff, 0]),  # must NOT replace current cursor
            bytes([0xfc, 8, 4, 1, 100, 0, 0, 2, 0xfe, 0xfd,
                   5, 1, 47, 1, 0, 0, 0xff, 0]),  # WHILE return 2 never exits
            bytes([0x65, 0, 0, 0, 0x64, 13, 0, 1, 0, 0, 0x64, 137, 0, 0, 0xff, 0]),
        ]
        offsets = []
        size = 4 * len(scripts)
        for script in scripts:
            offsets.append(size)
            size += len(script)
        base = len(room)
        room += struct.pack('<' + 'I' * len(offsets), *offsets) + b''.join(scripts)
        struct.pack_into('<I', room, 28, base)
        (fixture / 'rooms/RM_0080.RDT').write_bytes(room)
        state = output / 'runtime.json'
        with (output / 'runtime.log').open('w') as log:
            subprocess.run([str(args.runtime.resolve()), '--data', str(fixture),
                            '--mods', str(folder / 'mods'), '--user-dir', str(folder / 'user'),
                            '--headless', '--in-game',
                            '--room', 'RM_0080.RDX', '--frames', '4', '--state', str(state)],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        result = json.loads(state.read_text())
        def flag(bit):
            return bool(result['event_flags'][bit // 32] & (0x80000000 >> (bit % 32)))
        assert flag(300) and not flag(301), 'Event_exec replaced the live cursor'
        assert not flag(303), 'WHILE rejected return 2'
        assert result['blocked_commands'] == 0, result
        assert result['pending_world_bindings'] == 0, result
        assert result['pending_commands'] == [], result
        assert result['player_work']['mode'][:3] == [7, 0, 1], result
        print('Native runtime fixture: self replacement, WHILE and player binding passed.')


if __name__ == '__main__':
    main()
