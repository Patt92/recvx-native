#!/usr/bin/env python3
"""Check original cemetery/courtyard tasks and control handover with cooked data."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# RECVX_CAM_TRACE=1 makes RoomWorld::tick_camera log one line per frame
# (cut.c:1986 bhControlEventCamera state plus the resulting cam pose), which is
# what the camera-path checks below read.  Event-camera lines carry evc/key/ct0/
# mode0, room-camera lines carry the cut.
def camera_trace(stderr_text):
    frames = []
    for line in stderr_text.splitlines():
        if not line.startswith('CAMTRACE'):
            continue
        fields = dict(part.split('=', 1) for part in line.split()[1:] if '=' in part)
        pose = [float(v) for v in fields['pos'].split(',')]
        angles = [int(v) for v in fields['ang'].split(',')]
        frames.append({'event': 'evc' in fields,
                       'track': int(fields.get('evc', -1)), 'key': int(fields.get('key', -1)),
                       'ct0': int(fields.get('ct0', -1)), 'mode0': int(fields.get('mode0', -1)),
                       'pos': pose, 'ang': angles, 'pers': int(fields['pers'])})
    assert frames, 'no camera trace produced (RECVX_CAM_TRACE)'
    return frames


def assert_no_camera_jump(frames, label, move=0.5, turn=300):
    """No event-camera frame may move/turn more than the original data allows.

    The thresholds are measured, not chosen to pass: the RM_0040 and RM_0090
    shots checked here are stills (bhCamPauseSet / single-key tracks), so the
    original per-frame delta is 0.  Before bhControlEventCamera's mode 3 and the
    cam.ian control slots were bound, the same frames moved up to 0.9 units and
    turned 1500-2100 units per frame.
    """
    previous = None
    for index, frame in enumerate(frames):
        if not frame['event']:
            previous = None
            continue
        if previous is not None and previous['track'] == frame['track'] and previous['key'] == frame['key']:
            step = max(abs(a - b) for a, b in zip(frame['pos'], previous['pos']))
            spin = max(abs((a - b + 32768) % 65536 - 32768) for a, b in zip(frame['ang'], previous['ang']))
            assert step < move and spin < turn, (label, 'camera jump', index, step, spin, previous, frame)
        previous = frame


def assert_camera_still(frames, label, track, key, pos, ang, tolerance=0.02):
    """Every frame on this event-camera key must sit on the original key values.

    Angles get one unit of slack because the original round-trips them through its
    own constants: bhInitEventCamera scales a key angle by 0.005493164 and
    bhControlEventCamera scales back by 182.04445, and that product is 0.99999994,
    so a key angle can come back one unit short (cut.c:1878-1882, :2063-2067).
    The fault this guards against moved the same angles by tens of thousands.
    """
    seen = [f for f in frames if f['event'] and f['track'] == track and f['key'] == key]
    assert seen, (label, 'event camera track/key never active', track, key)
    for frame in seen:
        assert all(abs(a - b) < tolerance for a, b in zip(frame['pos'], pos)), (label, 'position', track, key, frame)
        assert all(abs((a - b + 32768) % 65536 - 32768) <= 1 for a, b in zip(frame['ang'], ang)), (label, 'angles', track, key, frame)
    return seen


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
        # Dialogue advances on its own; OK only acknowledges the final item
        # message. The cinematic skip belongs to SELECT alone (system.c:545
        # bhSysCallGame, pad_ps 0x10000) and is checked separately below.
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
        # system.c:545 bhSysCallGame: only pad_ps 0x10000 (SELECT) requests the skip.
        # OK during the running scene must leave it running (it only acknowledges text).
        no_skip = folder / 'steve-ok-no-skip.txt'
        no_skip.write_text('60 position 12 0 70 0\n300 confirm\n400 confirm\n500 confirm\n')
        state = folder / 'steve-ok-no-skip.json'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--headless', '--in-game',
                                 '--room', 'RM_0030.RDX', '--replay', str(no_skip), '--frames', '700', '--state', str(state)], check=True, cwd=ROOT)
        unskipped = json.loads(state.read_text())
        assert unskipped['cinematic'] and not unskipped['blocked_commands'], 'OK skipped the Steve cinematic (only SELECT may)'
        assert not any((packed >> 16) & 255 == 9 for packed in unskipped['items'][2:12]), 'OK during the scene reached the gift early'
        skip = folder / 'steve-skip.txt'
        skip.write_text('60 position 12 0 70 0\n300 select\n600 confirm\n')
        state = folder / 'steve-skip.json'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--headless', '--in-game',
                                 '--room', 'RM_0030.RDX', '--replay', str(skip), '--frames', '700', '--state', str(state)], check=True, cwd=ROOT)
        skipped = json.loads(state.read_text())
        gift(skipped)
        assert not skipped['cinematic'] and not skipped['player_scripted'] and not skipped['blocked_commands'], 'SELECT did not skip the Steve cinematic'
        assert not skipped['inventory_open'] and not skipped['dialog_open'] and not skipped['event_tasks'], 'skipped gift acknowledgement (OK) did not restore control'
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
        # RM_0040 power switch: ETC 4 starts event 5, which runs bhFixEventCamPly(1),
        # bhCamSet(0, 10, 1) and then bhCamPauseSet(0) -> cam.mode0 = 3 (event.c:1032).
        # bhControlEventCamera has no case 3 (cut.c:1995), so pose, cam.ct0 and
        # cam.keyf_no hold: the shot is a still on track 10 key 1 while the lamp
        # animation plays.  Track 10's ax crosses zero (6864, -198, 12074, 7398), so
        # a spline over the raw keys instead of the accumulated cam.ian slots turned
        # the camera through roughly -290 degrees in 30 frames.
        power = folder / 'power-switch.txt'
        power.write_text('60 position 5 0 5.5 16384\n120 confirm\n160 confirm\n200 confirm\n240 confirm\n')
        state = folder / 'power-switch.json'
        log = subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                                       '--headless', '--in-game', '--room', 'RM_0040.RDX', '--replay', str(power),
                                       '--frames', '600', '--state', str(state)], check=True, cwd=ROOT,
                             env={**os.environ, 'RECVX_CAM_TRACE': '1'}, capture_output=True, text=True)
        (folder / 'power-switch.cam').write_text(log.stderr)
        switched = json.loads(state.read_text())
        assert not switched['blocked_commands'], ('RM_0040 power switch', switched['event_tasks'])
        assert 5 in switched['events'], 'RM_0040 power switch never started event 5'
        frames = camera_trace(log.stderr)
        # CAM_KEYF_WORK of RM_0040 track 10 key 1 (RDT 0x25f4 + 10 * 0x808 + 8 + 0x80).
        held = assert_camera_still(frames, 'RM_0040 power switch', 10, 1,
                                   [7.48, 11.51, 4.47], [-198, -17736, 0])
        assert all(f['pers'] == 9102 for f in held), ('RM_0040 power switch pers', held[0])
        # mode 3 freezes cam.ct0; mode 1 ran exactly the frames before the pause.
        assert {f['mode0'] for f in held} == {1, 3}, ('RM_0040 pause states', sorted({f['mode0'] for f in held}))
        assert {f['ct0'] for f in held if f['mode0'] == 3} == {1}, ('cam.ct0 advanced while paused', held)
        # Measured: one mode-1 frame (the bhCamSet pass) and 40 paused frames until
        # the script releases the camera; cam.ct0 stays on 1 through all of them.
        assert len([f for f in held if f['mode0'] == 3]) >= 40, ('the paused shot did not hold', len(held))
        assert_no_camera_jump(frames, 'RM_0040 power switch')
    print('Original cemetery and both courtyard spawns walked; Steve M93R gift, 12 rounds, equipment, modal acknowledgement, SELECT-only cinematic skip (OK leaves the scene running), the camera-gated RM_0070 shelf ammunition and the RM_0040 power switch (bhCamPauseSet still on track 10 key 1, no camera drift) passed. Enemy AI, facial morphs and effects remain incomplete.')

if __name__ == '__main__':
    main()
