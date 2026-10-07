#!/usr/bin/env python3
"""Check original Event_init -> RoomSoundCase -> room-bank request ordering.

An isolated event-table fixture exercises the mechanism with actual room/audio
resources. It is not an invented gameplay event or a rain-stop proof.
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
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-audio-order-') as directory:
        temporary = Path(directory)
        output = args.output.resolve() if args.output else temporary
        output.mkdir(parents=True, exist_ok=True)
        fixture = temporary / 'mods'
        (fixture / 'rooms').mkdir(parents=True)
        for case, pack in [(0, 4), (1, 5)]:
            room = bytearray((data / 'rooms/RM_0030.RDT').read_bytes())
            offset = len(room)
            # Init: bhRoomSoundCase. Scan: bhSeOn (list 0, slot 0, mode 1).
            room += struct.pack('<II', 8, 11) + bytes([0xb6, case, 0, 0x17, 0, 0, 0, 0, 0, 1, 0, 0])
            struct.pack_into('<I', room, 28, offset)
            (fixture / 'rooms/RM_0030.RDT').write_bytes(room)
            state = output / ('case-%d.json' % case)
            with (output / ('case-%d.log' % case)).open('w') as log:
                result = subprocess.run([str(runtime), '--data', str(data), '--mods', str(fixture),
                    '--headless', '--in-game', '--room', 'RM_0030.RDX', '--frames', '2', '--state', str(state)],
                    stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError((output / ('case-%d.log' % case)).read_text())
            value = json.loads(state.read_text())
            assert not value['blocked_commands'] and not value['pending_commands'], value
            assert value['audio_played'] and all(sound.startswith('MULTSPQ1/%04d/2/seq0/' % pack)
                for sound in value['audio_played']), (case, value['audio_played'])
        print('Original init-set RoomSoundCase selects the current room bank (cases 0/1, real SPQ/SQ/HD/BD resources). Indoor rain is not proven fixed.')


if __name__ == '__main__':
    main()
