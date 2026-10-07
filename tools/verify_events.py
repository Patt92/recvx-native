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
        # Dialogue advances on its own; OK would request the original cinematic
        # skip (bhSysCallGame), which is checked separately below.
        steve_replay.write_text('60 position 12 0 70 0\n3200 confirm\n')
        def gift(result):
            guns = [packed for packed in result['items'][2:12] if (packed >> 16) & 255 == 9]
            assert guns == [(9 << 16) | 12] and result['weapon'] == 4, ('original M93R gift/ammo/equipment', result['items'])
        # Without input the scene must stop at the original received-item
        # confirmation, with the item already granted exactly once.
        unconfirmed = folder / 'steve-unconfirmed.txt'
        unconfirmed.write_text('60 position 12 0 70 0\n')
        state = folder / 'steve-unconfirmed.json'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--headless', '--in-game',
                                 '--room', 'RM_0030.RDX', '--replay', str(unconfirmed), '--frames', '3600', '--state', str(state)], check=True, cwd=ROOT)
        waiting = json.loads(state.read_text());gift(waiting)
        assert waiting['inventory_open'] and waiting['dialog_open'] and not waiting['blocked_commands'], 'script gift did not open the modal item result'
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
                gift(result)
                assert not result['bgm_active'], 'non-looping Steve cue still plays after original end'
                assert any(command['opcode'] == 0x6d for command in result['pending_commands']), 'missing particle binding hidden'
        skip = folder / 'steve-skip.txt'
        skip.write_text('60 position 12 0 70 0\n300 confirm\n600 confirm\n')
        state = folder / 'steve-skip.json'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--headless', '--in-game',
                                 '--room', 'RM_0030.RDX', '--replay', str(skip), '--frames', '700', '--state', str(state)], check=True, cwd=ROOT)
        skipped = json.loads(state.read_text())
        gift(skipped)
        assert not skipped['cinematic'] and not skipped['player_scripted'] and not skipped['blocked_commands'], 'OK did not skip the Steve cinematic'
        assert not skipped['inventory_open'] and not skipped['dialog_open'] and not skipped['event_tasks'], 'skipped gift acknowledgement did not restore control'
        for spawn, replay in [(0, 'steve-walk.txt'), (1, 'steve-walk-spawn1.txt')]:
            state = folder / ('steve-walk-spawn%d.json' % spawn)
            subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--headless', '--in-game',
                                    '--room', 'RM_0030.RDX', '--spawn', str(spawn), '--replay', str(ROOT / 'tests/native/replays' / replay),
                                    '--frames', '3600', '--state', str(state)], check=True, cwd=ROOT)
            walked = json.loads(state.read_text())
            gift(walked)
            assert walked['events'] == list(range(10)) and not walked['event_tasks'], ('walked Steve tasks', spawn)
            assert not walked['blocked_commands'] and not walked['cinematic'] and not walked['player_hidden'] and not walked['player_scripted'], ('walked Steve control', spawn)
            assert all(abs(a-b)<.02 for a,b in zip(walked['actor'], [-12.8,0,115.1])), ('walked Steve result', spawn)
        # bhCmpB key 2 is cam.ncut: RM_0070's scan script unlocks the shelf ammunition (ETC 4) per camera.
        shelf = folder / 'shelf-ammo.txt'
        shelf.write_text('10 position 53 0 11 32768\n40 confirm\n100 confirm\n130 confirm\n')
        state = folder / 'shelf-ammo.json'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--headless', '--in-game',
                                 '--validate-poses', '--room', 'RM_0070.RDX', '--replay', str(shelf), '--frames', '700', '--state', str(state)], check=True, cwd=ROOT)
        taken = json.loads(state.read_text())
        assert taken['items'][3] == 786447 and not taken['blocked_commands'], 'RM_0070 shelf ammunition cannot be taken'
        assert not taken['cinematic'] and not taken['player_scripted'] and not taken['dialog_open'] and all(t['event'] != 0 for t in taken['event_tasks']), 'window sequence does not return control'
    print('Original cemetery and both courtyard spawns walked; Steve M93R gift, 12 rounds, equipment, modal acknowledgement and cinematic skip and the camera-gated RM_0070 shelf ammunition passed. Enemy AI, facial morphs and effects remain incomplete.')

if __name__ == '__main__':
    main()
