#!/usr/bin/env python3
"""Real-data snapshot check: play into RM_0000, equip the lighter, teleport and walk,
save a snapshot (bhPushGameData record), then load it in a fresh runtime from the
title screen. Room, position, yaw, items, equipment and HP must match."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', help='Optional executable such as wine64')
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []
    walk = [line for line in (ROOT / 'tests/native/replays/lighter-walk.txt').read_text().splitlines() if not line.startswith('#')]
    with tempfile.TemporaryDirectory(prefix='recvx-snapshot-') as folder:
        root = Path(folder)
        controls = root / 'controls.ini'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)

        def run(name, frames, replay):
            (root / (name + '.txt')).write_text(replay)
            state = root / (name + '.json')
            subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                     '--controls', str(controls), '--snapshots', str(root / 'slots'), '--headless',
                                     '--frames', str(frames), '--replay', str(root / (name + '.txt')), '--state', str(state)],
                           check=True, stdout=subprocess.DEVNULL)
            return json.loads(state.read_text())

        saved = run('save', 3310, '\n'.join(walk) + '\n3200 debug_key F2\n3201 debug_select room 5\n3202 confirm\n3203 debug_key F2\n'
                    '3260 keys forward\n3290 keys none\n3300 debug_key F2\n3301 debug_select snapshot_save 1\n3302 confirm\n3303 debug_key F2\n')
        assert saved['debug_status'].startswith('Snapshot 2 gespeichert'), saved['debug_status']
        loaded = run('load', 12, '0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n')
        assert loaded['debug_status'].startswith('Snapshot 2 geladen'), loaded['debug_status']
        assert not loaded['title_active'] and not loaded['debug_open']
        for key in ('room', 'actor', 'yaw', 'items', 'weapon', 'hp'):
            assert saved[key] == loaded[key], f'{key}: {saved[key]} != {loaded[key]}'
        assert not loaded['blocked_commands'] and not loaded['cinematic']
    print('Snapshot save after teleport/walk and fresh-runtime load passed.')


if __name__ == '__main__':
    main()
