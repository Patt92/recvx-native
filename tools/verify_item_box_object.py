#!/usr/bin/env python3
"""Exercise an unchanged original room lid through the production input path.

Replay placement is diagnostic and derived from the original ATR; no room
scripts or object flags are replaced. This is not a story progression test.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    room = (args.data / 'rooms/RM_1080.RDT').read_bytes()
    header = struct.unpack_from('<I', room, 16)[0]
    base = struct.unpack_from('<I', room, header + 0x1c)[0]
    count = struct.unpack_from('<I', room, header + 0x9c)[0]
    areas = []
    for index in range(count):
        p = base + index * 36
        flags, kind, _, floor = struct.unpack_from('<BBBb', room, p)
        attribute = struct.unpack_from('<I', room, p + 4)[0]
        if flags & 1 and kind == 4 and attribute & 2 and room[p+32] != 255:
            areas.append((p, floor))
    assert len(areas) == 1, areas
    p, floor = areas[0]
    assert floor == 0 and not struct.unpack_from('<I', room, p + 4)[0] & 0x1000
    x, y, z = struct.unpack_from('<3f', room, p + 8)
    w, _, d = struct.unpack_from('<3f', room, p + 20)
    replay = args.output / 'lid-input.txt'
    replay.write_text(f'1 position {x+w/2} {y} {z+d/2+2} 0\n3 confirm\n90 cancel\n150 confirm\n')
    states = {}
    for name, frames in [('closed', 3), ('opening', 20), ('menu', 70),
                         ('released', 140), ('reopened', 220)]:
        state = args.output / (name + '.json')
        assert not state.exists(), 'Use a fresh output directory'
        with (args.output / (name + '.log')).open('w') as log:
            subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                            '--room', 'RM_1080.RDX', '--in-game', '--headless', '--ss', '1',
                            '--frames', str(frames), '--replay', str(replay.resolve()),
                            '--state', str(state.resolve()), '--screenshot', str((args.output / (name+'.bmp')).resolve())],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        states[name] = json.loads(state.read_text())
        value = states[name]
        assert not value['blocked_commands'] and not value['pending_commands'], name
        assert not value['audio_unavailable'], (name, value['audio_unavailable'])
        assert value['hp'] == states['closed']['hp'] and value['items'] == states['closed']['items'], (name, 'inventory/HP changed')
    assert not states['closed']['box_open'] and not states['opening']['box_open']
    assert states['opening']['work_flags'][14] & 0x18000 == 0x18000
    assert states['opening']['work_flags'][13] & 0x10000
    assert states['opening']['audio_played'].count('SYSTEM/MIDI/2/64') == 1
    for name in ['menu', 'reopened']:
        assert states[name]['box_open'] and states[name]['inventory_open'] and states[name]['box_base'] == 64, name
    value = states['released']
    assert not value['box_open'] and not value['inventory_open']
    assert not value['work_flags'][14] & 0x18000 and not value['work_flags'][5] & 4
    assert not value['work_flags'][10] & 0x1c0000
    assert states['reopened']['audio_played'].count('SYSTEM/MIDI/2/64') == 2
    assert (args.output / 'opening.bmp').read_bytes() != (args.output / 'closed.bmp').read_bytes(), 'lid image unchanged'
    print('Original RM_1080 lid opens once, requests the real menu, releases control and opens again via runtime input.')


if __name__ == '__main__':
    main()
