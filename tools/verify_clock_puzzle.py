#!/usr/bin/env python3
"""RM_1090 grandfather-clock puzzle on the original room scripts.

Snapshot ev_flg 164 (the clock's interactive branch of scd1) and the player
placement are diagnostic preconditions, not a naturally walked campaign. The
room scripts stay unchanged. Checks: the choice dialogue opens, Left rotates,
Reset after Left releases the player again (events 8/9 finish), and Escape in
the choice closes the puzzle through the script's own pad check (event 13)
instead of confirming "Left".
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
                '--headless', '--enemy-ai', 'on', '--in-game', '--room', 'RM_1090.RDX', '--frames', str(frames),
                '--replay', str(script), '--state', str(state)], check=True,
                stdout=log, stderr=subprocess.STDOUT)
        v = json.loads(state.read_text())
        assert not v['blocked_commands'], (name, v['event_tasks'])
        return v

    run('seed', '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n', 4)
    b = bytearray((out / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
    assert b[:8] == b'RCVXSNAP' and struct.unpack_from('<II', b, 8) == (1, 0x838)
    at = 16 + 0x14 + (164 // 32) * 4
    struct.pack_into('<I', b, at, struct.unpack_from('<I', b, at)[0] | (0x80000000 >> (164 % 32)))
    struct.pack_into('<I', b, len(b) - 4, zlib.crc32(b[:-4]))
    (out / 'snapshots/snapshot-2.rcvxsnap').write_bytes(b)
    # Interaction 6 (type 3, attr 0x800a; prm0 5 -> message 9 "There is a button.", then
    # scd1 @041a starts event 13 and the Left/Right/Reset question).
    head = ('0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
            '60 position 5 0 62.5 0\n61 confirm\n'
            '100 confirm\n')
    tasks = lambda v: sorted(t['event'] for t in v['event_tasks'])
    puzzle = {8, 9, 13, 6, 7}

    # Reset right away: the script leaves the puzzle (ev_flg 4:7 clear).
    v = run('reset_only', head + '140 right\n141 right\n142 confirm\n', 260)
    assert not v['cinematic'] and not v['dialog_open'], ('reset_only', v['cinematic'], v['dialog_open'], v['event_tasks'])

    # Left, then Reset: events 8/9 turn the plate and the number disc back and release.
    v = run('left_reset', head + '140 confirm\n260 right\n261 right\n262 confirm\n', 520)
    assert not (puzzle & set(tasks(v))), ('left_reset tasks', v['event_tasks'])
    assert not v['cinematic'] and not v['dialog_open'], ('left_reset', v['cinematic'], v['dialog_open'], v['event_tasks'])

    # Escape in the choice: event 13's bhPadCheck (bit 12) closes the puzzle; it must not
    # confirm "Left" (event 6 would be running and the question would return).
    v = run('escape', head + '140 cancel\n', 260)
    assert not (puzzle & set(tasks(v))), ('escape tasks', v['event_tasks'])
    assert not v['cinematic'] and not v['dialog_open'], ('escape', v['cinematic'], v['dialog_open'], v['event_tasks'])
    print('clock puzzle: reset, left+reset and escape release the player')


if __name__ == '__main__':
    main()
