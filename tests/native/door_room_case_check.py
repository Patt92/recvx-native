#!/usr/bin/env python3
"""Door destination uses live sys->rcase at room-read time, not at DoorCall.

room.c:883-918 bhSetDoorDemo stores no rcase. system.c:1623 consumes it
later. RM_0021 scd1 @0448 sets rcase 0 before its own DoorCall, but the
interaction table requests the same door first, while rcase is still 1.
No RM_0011 file exists, and no fallback is part of the original mechanism.
"""
import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def scenario_room(source, destination, target_case, exit_case, direct=False):
    """Diagnostic EVT only: change case after DoorCall, or in exit scd1.

    Geometry, spawns and destination files remain original. This deliberately
    isolated scenario also checks a nonzero target case; always forcing case 0
    would hide the cemetery crash but fail the return to RM_0021.
    """
    data = bytearray(source.read_bytes())
    init = bytes([0x14, 0, 0, 0, 0])
    scan = bytes([5, 6, 27, 0, 0, 0])  # scd1 enables bhEventScheduler2 (sp_flg 0x10)
    scan += (bytes([1, 8, 4, 10, 31, 0, 0, 0, 0x37, target_case, 3, 0, 0])
             if exit_case else bytes([0]))
    event = bytes([0xF9, 5, 0, 0x33, 0, 0, 0, 0, 2, 0, 255 if direct else 1])
    if not exit_case:
        event += bytes([0x37, target_case])
    event += bytes([0xFF, 0])
    base = len(data)
    data += struct.pack('<3I', 12, 12 + len(init), 12 + len(init) + len(scan))
    data += init + scan + event
    struct.pack_into('<I', data, 28, base)
    destination.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-door-case-') as temporary:
        root = Path(temporary)
        mods = root / 'mods'
        (mods / 'rooms').mkdir(parents=True)

        def run(name, room, frames, replay='', modified=False, success=True):
            script, state = root / (name + '.txt'), root / (name + '.json')
            script.write_text(replay)
            result = subprocess.run([str(runtime), '--data', str(data), '--mods',
                                     str(mods if modified else root / 'no-mods'),
                                     '--user-dir', str(root / 'user'), '--headless', '--in-game',
                                     '--enemy-ai', 'on', '--ss', '1', '--room', room,
                                     '--frames', str(frames), '--replay', str(script), '--state', str(state)],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if not success:
                assert result.returncode != 0 and 'door room missing: RM_0029.RDX' in result.stdout, result.stdout
                return
            assert result.returncode == 0, (name, result.stdout)
            value = json.loads(state.read_text())
            assert not value['blocked_commands'], (name, result.stdout)
            assert value['room_changes'] == 1 and not value['door_active'], (name, value['room'], value['room_changes'])
            return value

        # Original files/scripts, diagnostic placement beside the stair door
        # (etc[1], x 102..108.99 / z 144..158.99). Confirm requests it with the
        # source case 1; scd1 then writes case 0 and repeats the request.
        replay = '30 position 108 0 150 16384\n31 confirm\n'
        for skip in (False, True):
            name = 'cemetery-skip' if skip else 'cemetery-regular'
            value = run(name, 'RM_0021.RDX', 400, replay + ('40 confirm\n' if skip else ''))
            assert value['room'] == 'RM_0010.RDX', (name, value['room'])
            assert value['door_skipped'] == skip, (name, value['door_skipped'])
            assert value['actor'] != [108, 0, 150], (name, 'source position survived room load')
            print(name + ': original exit script selects RM_0010, spawn 1, no missing-file fallback')

        # Same-pass change and next exit-scenario pass: both must be visible to
        # the loader, for normal demo, host skip and direct type-255 transition.
        for exit_case in (False, True):
            for direct in (False, True):
                scenario_room(data / 'rooms/RM_0010.RDT', mods / 'rooms/RM_0010.RDT', 1, exit_case, direct)
                name = f'late-case-{exit_case}-direct-{direct}'
                value = run(name, 'RM_0010.RDX', 400, modified=True)
                assert value['room'] == 'RM_0021.RDX', (name, value['room'])
                print(name + ': live case 1 selects the actual RM_0021 variant')

        # Truly absent destinations must still fail, not silently load case 0
        # or leave room_index pointing at the previous room.
        scenario_room(data / 'rooms/RM_0010.RDT', mods / 'rooms/RM_0010.RDT', 9, False, True)
        run('missing-case', 'RM_0010.RDX', 40, modified=True, success=False)
        print('missing-case: RM_0029 rejected; no fabricated variant fallback')


if __name__ == '__main__':
    main()
