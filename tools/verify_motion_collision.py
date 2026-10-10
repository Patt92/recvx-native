#!/usr/bin/env python3
"""Native Runtime movement against an actual original-room wall.

Placements are diagnostic. ATR records, collision tables, movement curves,
input and event scheduler remain the actual runtime path, not a fake owner.
An optional pre-change binary must cross the same wall to prove this check
detects the formerly neutral movement service. This is not a playthrough.
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
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    room = (args.data / 'rooms/RM_0050.RDT').read_bytes()
    header = struct.unpack_from('<I', room, 16)[0]
    base = struct.unpack_from('<I', room, header + 0x18)[0]
    wall = struct.unpack_from('<BBBbI6f4B', room, base + 36 * 11)
    flags, kind, _, floor, attr, x, y, z, width, height, depth, *_ = wall
    assert flags & 1 and kind == 0 and floor == 0 and attr == 0 and height > 17.5
    edge = x + width
    start = (edge + 9, y, z + depth * .25)

    def run(runtime, name, keys, yaw, frames=160, placement=True):
        replay = args.output / (name + '.txt')
        replay.write_text((f'20 position {start[0]} {start[1]} {start[2]} {yaw}\n' if placement else '') +
                          f'21 keys {keys}\n')
        state = args.output / (name + '.json')
        if state.exists():
            raise RuntimeError(f'output already exists: {state}')
        with (args.output / (name + '.log')).open('w') as log:
            subprocess.run([str(runtime.resolve()), '--data', str(args.data.resolve()),
                            '--room', 'RM_0050.RDX', '--in-game', '--headless',
                            '--frames', str(frames), '--replay', str(replay.resolve()),
                            '--state', str(state.resolve()), '--validate-poses'],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        value = json.loads(state.read_text())
        assert value['room'] == 'RM_0050.RDX' and value['room_changes'] == 0, name
        assert value['blocked_commands'] == 0, name
        assert not value['dialog_open'] and not value['cinematic'] and not value['player_scripted'], name
        assert value['hp'] == 160, (name, 'HP changed', value['hp'])
        return value

    # Forward arp=2, run arp=3, backward arp=-1. The original advances arn
    # halfway to arp each tick, so the leading point sets the stopping gap.
    for name, keys, yaw, lead in [('walk', 'forward', 16384, 2),
                                 ('run', 'forward run', 16384, 3),
                                 ('back', 'back', 49152, 1)]:
        value = run(args.runtime, name, keys, yaw)
        expected = edge + 3.5 + lead
        assert abs(value['actor'][0] - expected) < .01, (name, value['actor'], expected)
        assert abs(value['actor'][2] - start[2]) < .01, (name, 'unwanted lateral movement')
        # bhControlPlayer restores stflg after the leading-point query; bit1
        # records only the subsequent body contact, not a front-only stop.
        if args.baseline:
            before = run(args.baseline, name + '-before', keys, yaw)
            assert before['actor'][0] < x - 3.5, (name, 'baseline did not cross the actual wall', before['actor'])
            assert value['pending_commands'] == before['pending_commands'], (name, 'event gaps changed')

    # This direction travels away from the nearby scripted-zombie trigger.
    value = run(args.runtime, 'oblique', 'forward run', 8192, 50)
    assert value['actor'][0] >= edge + 3.5 - .01 and value['actor'][2] < start[2] - 5, value['actor']
    # Natural POS spawn plus ordinary keys, no diagnostic placement.
    run(args.runtime, 'room-spawn', 'forward', 0, 80, False)
    print('Native RM_0050 wall stops walk/run/back and permits tangential movement; '
          'HP retained. Natural spawn/input smoke run also passed. '
          'Preexisting event gaps are compared with the baseline, not claimed closed.')

if __name__ == '__main__':
    main()
