#!/usr/bin/env python3
"""Check original cemetery/courtyard tasks and control handover with cooked data."""
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
    parser.add_argument('--emulator')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []
    with tempfile.TemporaryDirectory(prefix='recvx-event-runtime-') as temporary:
        folder = Path(temporary)
        if args.output:
            folder = args.output.resolve()
            folder.mkdir(parents=True, exist_ok=True)
        steve_replay = folder / 'steve-trigger.txt'
        # Diagnostic trigger placement after room initialization. This is
        # deliberately distinct from the walked cemetery regression.
        steve_replay.write_text('60 position 12 0 70 0\n' + ''.join(
            f'{tick} confirm\n' for tick in range(100, 3600, 100)))
        for name, room, replay, position in [
            ('cemetery-walk', 'RM_0020.RDX', ROOT / 'tests/native/replays/graveyard-walk.txt', [140, 0, 90]),
            ('steve-trigger', 'RM_0030.RDX', steve_replay, [-12.8, 0, 115.1]),
        ]:
            state = folder / (name + '.json')
            subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                '--headless', '--in-game', '--room', room, '--replay', str(replay),
                '--frames', '3600', '--state', str(state)], check=True, cwd=ROOT)
            result = json.loads(state.read_text())
            assert not result['blocked_commands'], (name, result['event_tasks'])
            for field in ['player_hidden', 'player_scripted', 'cinematic', 'movie_active', 'dialog_open']:
                assert not result[field], (name, field)
            assert result['event_camera'] == -1 and not result['event_tasks'], name
            assert all(abs(a - b) < .02 for a, b in zip(result['actor'], position)), (name, result['actor'])
            assert 0 in result['events'], (name, 'original task never dispatched')
            if name == 'steve-trigger':
                assert any(command['opcode'] == 0x6d for command in result['pending_commands']), 'missing particle binding hidden'
    print('Original cemetery trigger walked with tank controls/collision; Steve trigger placed diagnostically. Both task groups restore control. Enemy AI, facial morphs and effects remain incomplete.')

if __name__ == '__main__':
    main()
