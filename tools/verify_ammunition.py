#!/usr/bin/env python3
"""Targeted RM_0070 shelf pickup with an equipped, empty pistol in pip[2].

Snapshot inventory/position are diagnostic preconditions; room scripts and
question/acknowledgement use the normal runtime path. No campaign proof.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import zlib


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--data', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    folder = a.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    def run(name, replay, frames, load=False):
        script = folder / (name + '.txt')
        script.write_text(replay)
        state = folder / (name + '.json')
        command = [str(a.runtime.resolve()), '--data', str(a.data.resolve()),
                   '--headless', '--room', 'RM_0070.RDX', '--frames', str(frames),
                   '--replay', str(script), '--state', str(state), '--snapshots',
                   str(folder / 'snapshots'), '--screenshot', str(folder / (name + '.bmp'))]
        if not load:
            command.append('--in-game')
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        assert not result['blocked_commands'], result['event_tasks']
        return result
    run('seed', '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n', 4)
    seed = folder / 'snapshots/snapshot-1.rcvxsnap'
    snapshot = bytearray(seed.read_bytes())
    assert snapshot[:8] == b'RCVXSNAP' and struct.unpack_from('<II', snapshot, 8) == (1, 0x838)
    # SAVEFILE itm: 0x14 header + 120 flag words. Keep every other player bank.
    at = 16 + 0x1f4
    struct.pack_into('<12I', snapshot, at, 2, 55 << 16 | 1, 5 << 16, *([0] * 9))
    struct.pack_into('<fffI', snapshot, 16 + 0x804, 53.0, 0.0, 11.0, 32768)
    struct.pack_into('<I', snapshot, len(snapshot)-4, zlib.crc32(snapshot[:-4]))
    (folder / 'snapshots/snapshot-2.rcvxsnap').write_bytes(snapshot)
    load = '0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
    empty = run('empty-trigger', load + '10 keys aim attack\n100 keys none\n110 inventory\n', 140, True)
    assert empty['items'][0] == 2 and empty['items'][2] == 5 << 16 and not empty['combat_shots']
    assert empty['inventory_open'] and empty['audio_played'].count('ARMS/0000/0004') > 1
    pickup = load + '10 position 53 0 11 32768\n40 confirm\n100 confirm\n'
    question = run('shelf-question', pickup, 80, True)
    assert question['dialog_index'] == 157 and question['items'][2] == 5 << 16
    received = run('shelf-received', pickup, 125, True)
    assert received['dialog_index'] == 158 and received['inventory_open']
    assert received['items'][:4] == [2, 55 << 16 | 1, 5 << 16, 12 << 16 | 15], received['items']
    assert sum(v & 65535 for v in received['items'][2:] if v >> 16 & 255 == 12) == 15
    taken = run('shelf-acknowledged', pickup + '130 confirm\n', 700, True)
    assert taken['items'][:4] == received['items'][:4]
    assert taken['item_flags'][4] & 0x1000000  # original shelf acquisition it_flg 135
    assert not taken['dialog_open'] and not taken['inventory_open']
    print('RM_0070: repeated empty triggers, occupied pip[2], 15 shelf rounds in pip[3], original question/ACK and zero screenshot passed.')


if __name__ == '__main__':
    main()
