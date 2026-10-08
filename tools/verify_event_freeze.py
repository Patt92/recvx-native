#!/usr/bin/env python3
"""Original RM_0050 stairs, trap event and door after release.

Snapshot ev_flg 117, padlock inventory and player placements are diagnostic
preconditions, not a naturally walked campaign. Original room scripts remain
unchanged. Both trap branches and padlock use are also covered by the C++
event-lifecycle test.
"""
import argparse
import json
from pathlib import Path
import struct
import shutil
import subprocess
import zlib


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--data', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(a.runtime.resolve()), '--write-controls', str(out / 'controls.ini')], check=True, stdout=subprocess.DEVNULL)
    shutil.copyfile(Path(__file__).resolve().parents[1] / 'debug.ini', out / 'debug.ini')

    def run(name, replay, frames):
        script, state = out / (name + '.txt'), out / (name + '.json')
        script.write_text(replay)
        with (out / (name + '.log')).open('w') as log:
            subprocess.run([str(a.runtime.resolve()), '--data', str(a.data.resolve()),
                '--mods', str(out / 'no-mods'), '--controls', str(out / 'controls.ini'),
                '--debug-config', str(out / 'debug.ini'), '--snapshots', str(out / 'snapshots'),
                '--headless', '--in-game', '--room', 'RM_0050.RDX', '--frames', str(frames),
                '--replay', str(script), '--state', str(state)], check=True,
                stdout=log, stderr=subprocess.STDOUT)
        v = json.loads(state.read_text())
        assert not v['blocked_commands'], (name, v['event_tasks'])
        return v

    run('seed', '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n', 4)
    b = bytearray((out / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
    assert b[:8] == b'RCVXSNAP' and struct.unpack_from('<II', b, 8) == (1, 0x838)
    at = 16 + 0x14 + (117 // 32) * 4
    struct.pack_into('<I', b, at, struct.unpack_from('<I', b, at)[0] | (0x80000000 >> (117 % 32)))
    struct.pack_into('<I', b, 16 + 0x1f4 + 2 * 4, (84 << 16) | 1)
    struct.pack_into('<I', b, len(b) - 4, zlib.crc32(b[:-4]))
    (out / 'snapshots/snapshot-2.rcvxsnap').write_bytes(b)
    replay = ('0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
              '60 position -186 0 86 0\n61 confirm\n'
              '260 position -175.5 8 85.8489 0\n261 confirm\n'
              '550 position -142 0 0 0\n'
              '1200 position -166 8 72 32768\n1200 confirm\n')
    for name, frames in [('up', 240), ('down', 520), ('dog-finished', 1100), ('exit', 1500)]:
        v = run(name, replay, frames)
        for k in ['cinematic', 'player_scripted', 'player_hidden', 'dialog_open', 'movie_active']:
            assert not v[k], (name, k, v['event_tasks'])
        if name in ('up', 'down'):
            assert abs(v['actor'][1] - (8 if name == 'up' else 0)) < .01, (name, v['actor'])
        if name == 'dog-finished':
            assert 15 in v['events'] or 16 in v['events'], 'original trap event not exercised'
            assert not any(t['event'] in (15, 16) for t in v['event_tasks'])
        if name == 'exit':
            assert v['room'] == 'RM_0060.RDX' and v['room_changes'] == 1, v['room']
    print('Original stairs up/down, dog trap completion and subsequent exit passed (diagnostic snapshot/placements).')


if __name__ == '__main__':
    main()
