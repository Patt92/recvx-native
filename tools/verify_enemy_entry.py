#!/usr/bin/env python3
"""Check the first enemy drawing across unchanged original room doors.

Door approaches are diagnostically positioned. Destination resources, scenario
scripts, enemy works, door packets and control passes are not replaced. The
optional before runtime must be built from the same source snapshot with ONLY
runtime.cpp's room-task handover reverted, never from an older parallel build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
# Original ETC type-0 records: source, index, destination, approach, yaw.
DOORS = [
    ('RM_0030', 1, 'RM_0020', (32.5, 0, 70), 49152),
    ('RM_0050', 2, 'RM_0060', (-166, 8, 72), 32768),
    ('RM_0050', 1, 'RM_0080', (-33, 0, 50), 16384),
    ('RM_1020', 10, 'RM_1000', (40, 8, 108), 32768),
]
DRAW = re.compile(r'DBG kind=1 idx=(\d+) id=(\d+).*? tris=(\d+) bbox=(.*?) motion=(\d+) room=(\d+) frame=(-?\d+) flg=([0-9a-f]+) stflg=([0-9a-f]+) calculated=(\d+) workpos=\((.*?)\) root=\((.*?)\)')


def bmp_pixels(path):
    import struct
    b = path.read_bytes()
    offset = struct.unpack_from('<I', b, 10)[0]
    width, height, planes, bits = struct.unpack_from('<iiHH', b, 18)
    assert planes == 1 and bits in (24, 32)
    stride = ((width * bits + 31) // 32) * 4
    size = bits // 8
    rows = [b[offset + y * stride:offset + (y + 1) * stride] for y in range(abs(height))]
    if height > 0:
        rows.reverse()
    return [tuple(row[x * size:x * size + 3]) for row in rows for x in range(width)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--before-runtime', type=Path)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    data = args.data.resolve()
    results = {}
    env = dict(os.environ, RODRIGO_DBG='1')
    cache = {}

    def run(side, runtime, source, replay, frames, label):
        key = side, label, frames
        if key in cache:
            return cache[key]
        folder = output / side / label
        folder.mkdir(parents=True, exist_ok=True)
        script = folder / 'approach.txt'
        script.write_text(replay)
        stem = folder / f'f{frames:04}'
        state, log, shot = stem.with_suffix('.json'), stem.with_suffix('.log'), stem.with_suffix('.bmp')
        state.unlink(missing_ok=True)
        shot.unlink(missing_ok=True)
        command = [str(runtime.resolve()), '--data', str(data), '--mods', str(output / 'no-mods'),
                   '--debug-config', str(output / 'debug.ini'), '--headless', '--in-game',
                   '--enemy-ai', 'on', '--enemy-grab', 'off', '--room', source + '.RDX',
                   '--replay', str(script), '--frames', str(frames), '--state', str(state),
                   '--screenshot', str(shot)]
        with log.open('w') as stream:
            subprocess.run(command, cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
        s = json.loads(state.read_text())
        assert not s['blocked_commands'], (label, frames, 'blocked original script')
        draws = []
        for match in DRAW.finditer(log.read_text()):
            index, identity, tris, bounds, motion, room, frame, flg, stflg, calculated, pos, root = match.groups()
            draws.append(dict(index=int(index), identity=int(identity), triangles=int(tris), bounds=bounds,
                              motion=int(motion), room_motion=bool(int(room)), frame=int(frame), flg=int(flg, 16),
                              stflg=int(stflg, 16), calculated=bool(int(calculated)),
                              work_position=[float(v) for v in pos.split(',')],
                              matrix_root=[float(v) for v in root.split(',')]))
        result = dict(state=s, draws=draws, screenshot=str(shot), log=str(log))
        cache[key] = result
        return result

    for source, index, destination, position, yaw in DOORS:
        replay = f'60 position {position[0]} {position[1]} {position[2]} {yaw}\n61 confirm\n'
        # Check the recorded connection against original ETC bytes, not a guessed room ID.
        import struct
        room = (data / 'rooms' / (source + '.RDT')).read_bytes()
        header = struct.unpack_from('<I', room, 16)[0]
        at = struct.unpack_from('<I', room, header + 0x1c)[0] + index * 36
        record = struct.unpack_from('<BBBBI6f4B', room, at)
        assert record[1] == 0 and f'RM_{record[11]}{record[12]:02}0' == destination
        label = destination
        early = run('after', args.runtime, source, replay, 62, label)
        assert early['state']['door_active'] and early['state']['room'] == source + '.RDX'
        lo, hi = 62, 1000
        assert run('after', args.runtime, source, replay, hi, label)['state']['room_changes'] == 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if run('after', args.runtime, source, replay, mid, label)['state']['room_changes']:
                hi = mid
            else:
                lo = mid
        loaded = hi
        samples = [loaded - 1, loaded, loaded + 1, loaded + 2, loaded + 5, loaded + 30]
        after = {str(f): run('after', args.runtime, source, replay, f, label) for f in samples}
        handover = after[str(loaded)]
        assert handover['state']['room'] == destination + '.RDX' and handover['state']['door_active']
        assert not handover['draws'], (destination, 'new enemy tree drawn in room loader task')
        visible = after[str(loaded + 1)]
        assert visible['state']['room_changes'] == 1 and not visible['state']['door_active']
        assert visible['draws'], (destination, 'first control pass did not draw any enemy work')
        for draw in visible['draws']:
            assert draw['calculated'] and not (draw['stflg'] & 0x41000000)
            assert draw['flg'] & 1
        pair = dict(source=source, etc_index=index, destination=destination, loaded_at_frames=loaded,
                    first_game_draw_frames=loaded + 1, approach_is_diagnostic=True, after=after)
        if args.before_runtime:
            before = {str(f): run('before', args.before_runtime, source, replay, f, label) for f in samples}
            wrong = before[str(loaded)]
            assert wrong['state']['room'] == destination + '.RDX' and not wrong['state']['door_active']
            assert wrong['draws'], (destination, 'negative control did not reproduce early enemy draw')
            # Enemy init, flags and motion are unchanged. The erroneous old path
            # can also advance the player once in the handover frame; record that
            # separately rather than silently claiming all state is identical.
            assert handover['state']['enemies'] == wrong['state']['enemies'], (destination, 'handover changed enemy init')
            changed_fields = [key for key in handover['state'] if handover['state'][key] != wrong['state'][key]]
            pair['handover_changed_fields'] = changed_fields
            changes = sum(x != y for x, y in zip(bmp_pixels(Path(wrong['screenshot'])), bmp_pixels(Path(handover['screenshot']))))
            assert changes > 0, (destination, 'early drawing produced no pixel difference')
            pair.update(before=before, early_changed_pixels=changes)
            pair['settled_comparison'] = {}
            for f in samples[2:]:
                a, b = after[str(f)], before[str(f)]
                fields = [key for key in a['state'] if a['state'][key] != b['state'][key]]
                assert set(fields) <= {'player_work', 'animation_frame', 'hair', 'head'}, (destination, f, fields)
                assert a['state']['enemies'] == b['state']['enemies'], (destination, f, 'settled enemy state differs')
                assert a['draws'] == b['draws'], (destination, f, 'settled enemy pose differs')
                pixels = sum(x != y for x, y in zip(bmp_pixels(Path(a['screenshot'])), bmp_pixels(Path(b['screenshot']))))
                pair['settled_comparison'][str(f)] = {'changed_fields': fields, 'changed_pixels': pixels,
                                                     'enemy_state_pose_triangles_identical': True}
        results[destination] = pair
        print(f'{source} ETC {index} -> {destination}: load at {loaded}, first controlled draw at {loaded+1}, '
              f'{len(visible["draws"])} enemy models; loader draw suppressed; settled states/poses/pixels checked.')
    results['binaries'] = {side: {'path': str(rt.resolve()), 'sha256': hashlib.sha256(rt.read_bytes()).hexdigest()}
                           for side, rt in [('after', args.runtime), ('before', args.before_runtime)] if rt}
    (output / 'measurements.json').write_text(json.dumps(results, indent=2))
    print('Unchanged original doors: first enemy draw ordering passed. Approaches are diagnostic, not a full story playthrough.')


if __name__ == '__main__':
    main()
