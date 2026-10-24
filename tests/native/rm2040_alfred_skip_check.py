#!/usr/bin/env python3
"""RM_2040 Alfred-Szene mit SELECT-Skip: das Bild muss nach dem Skip wieder aufblenden.

scd1 @0x83a startet die Szene (ev 0xba gesetzt, 0xb4 frei, ATR 7, Items 0x12/0x18/0x09 -> ev 0xb5).
Der Skip-Zweig @0x970 (cb_flg 0x10000000, system.c:545) beendet Tasks 1..8, startet Ereignis 8 (Ausblende,
Schlaf, Einblende) und mit ev 0xb5 Event_exec 3/39 bei 34 Tabelleneintraegen. Event_init (event.c:12964)
liest den Offset ohne Laengenpruefung aus den Ereignisdaten.

Vorher: der Port warf 'event index outside table', der abgebrochene scd1-Pass liess bhIfelFlg/G_Sp
stehen, Ereignis 8 sprang aus seinem Schlaf auf das bhEnd von scd1, die Einblende lief nie, das Bild
blieb schwarz (screen_fade an 255) bis zum Raumwechsel.

Konstruierter Testzustand: frischer Snapshot in RM_2040, dazu ev 0xb3/0xba und die drei Items."""
import argparse
import json
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

REPLAY = ('5 debug_key F2\n6 debug_select snapshot_load 1\n7 confirm\n'
          '20 position 41 0 30 49152\n30 confirm\n300 select\n')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())
    with tempfile.TemporaryDirectory(prefix='recvx-rm2040-') as folder:
        root = Path(folder)
        (root / 'snapshots').mkdir()

        def run(name, frames, replay):
            script, state = root / (name + '.txt'), root / (name + '.json')
            script.write_text(replay)
            done = subprocess.run([runtime, '--data', data, '--mods', str(root / 'no-mods'),
                                   '--snapshots', str(root / 'snapshots'), '--user-dir', str(root / ('user-' + name)),
                                   '--headless', '--in-game', '--room', 'RM_2040.RDX', '--frames', str(frames),
                                   '--replay', str(script), '--state', str(state)],
                                  check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            s = json.loads(state.read_text())
            s['_log'] = done.stdout
            return s

        run('seed', 4, '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n')
        b = bytearray((root / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
        assert b[:8] == b'RCVXSNAP', 'snapshot header'
        ev, it = 16 + 0x14, 16 + 0x14 + 0x80  # SaveFile.ev_flg / it_flg
        for base, bit in ((ev, 0xb3), (ev, 0xba), (it, 0x09), (it, 0x12), (it, 0x18)):
            at = base + (bit // 32) * 4
            struct.pack_into('<I', b, at, struct.unpack_from('<I', b, at)[0] | (0x80000000 >> (bit % 32)))
        struct.pack_into('<I', b, len(b) - 4, zlib.crc32(b[:-4]))
        (root / 'snapshots/snapshot-2.rcvxsnap').write_bytes(b)

        done = run('skip', 700, REPLAY)
        assert 'event index outside table' not in done['_log'], done['_log'][-1500:]
        assert not done['cinematic'], 'Cinematic laeuft noch'
        assert done['screen_fade']['an'] == 0 and done['screen_fade']['target'] == 0, ('Bild bleibt schwarz', done['screen_fade'])
        assert 8 not in {t['event'] for t in done['event_tasks']}, ('Ereignis 8 haengt', done['event_tasks'])
    print('RM_2040 Alfred skip: fade back in, event 8 done, Event_exec 3/39 started from the event data.')

if __name__ == '__main__':
    main()
