#!/usr/bin/env python3
"""Original RM_1020 ID-card terminal, discard question and control return.

Event 27 runs the security terminal (bhEvtComSet 2, code NTC0394), event 29
sets st_flg 0x4 itself (bhSet 5,0x1d) before asking to discard the ID card,
and scd1 drops the card on Yes (bhPlItemLost 73). In the original the next
bhCPM0_action pass ends st_flg 0x4 in bhControlPlayerPad (player.c:1838).
Both answers must hand control back. The ID card in the inventory and the
player placement are diagnostic preconditions, not a naturally walked campaign.
"""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

from verify_computer import Replay

ID_CARD = 73


class Slow(Replay):
    def act(self, action, spacing=6):
        super().act(action, spacing)


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

    def run(name, lines, frames):
        script, state = out / (name + '.txt'), out / (name + '.json')
        script.write_text('\n'.join(lines) + '\n')
        with (out / (name + '.log')).open('w') as log:
            subprocess.run([str(a.runtime.resolve()), '--data', str(a.data.resolve()),
                '--mods', str(out / 'no-mods'), '--controls', str(out / 'controls.ini'),
                '--debug-config', str(out / 'debug.ini'), '--snapshots', str(out / 'snapshots'),
                '--headless', '--in-game', '--room', 'RM_1020.RDX', '--frames', str(frames),
                '--replay', str(script), '--state', str(state)], check=True,
                stdout=log, stderr=subprocess.STDOUT)
        v = json.loads(state.read_text())
        assert not v['blocked_commands'], (name, v['event_tasks'])
        return v

    run('seed', ['0 debug_key F2', '1 debug_select snapshot_save 0', '2 confirm'], 4)
    b = bytearray((out / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
    assert b[:8] == b'RCVXSNAP'
    struct.pack_into('<I', b, 16 + 0x1f4 + 3 * 4, (ID_CARD << 16) | 1)
    struct.pack_into('<I', b, len(b) - 4, zlib.crc32(b[:-4]))
    (out / 'snapshots/snapshot-2.rcvxsnap').write_bytes(b)

    # Interaction 13 (ATR 0xb800) starts event 27; the terminal opens its
    # keyboard after the waits, then the ID from comevt_message_no2[16].
    r = Slow(2)
    r.lines = ['0 debug_key F2', '1 debug_select snapshot_load 1', '2 confirm',
               '100 position 53 0 41 32768', '120 confirm']
    r.frame = 200
    r.confirm_until(322)
    r.frame, r.key = 345, (1, 1)
    r.word('NTC0394')
    lines = r.lines + [f'{f} confirm' for f in range(900, 1101, 30)]
    for name, answer, kept in [('discard', [], False), ('keep', ['1105 right'], True)]:
        v = run(name, lines + answer + ['1110 confirm', '1300 keys back', '1400 keys none'], 1500)
        assert v['computer'] and not v['computer']['active'], (name, 'terminal still running')
        assert v['event_flags'][0] & (0x80000000 >> 4), (name, 'ID verification did not open the lock')
        assert (ID_CARD in [x >> 16 for x in v['items']]) == kept, (name, v['items'])
        assert not v['dialog_open'] and not v['cinematic'], (name, 'st_flg 0x4 kept the player locked', hex(v['work_flags'][5]))
        assert not v['work_flags'][5] & 0x4, (name, hex(v['work_flags'][5]))
        assert v['actor'][2] < 30, (name, 'control was not returned', v['actor'])
    print('Original RM_1020 ID-card terminal, discard question (Yes/No) and control return passed (diagnostic inventory/placement).')


if __name__ == '__main__':
    main()
