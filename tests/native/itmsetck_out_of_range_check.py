#!/usr/bin/env python3
"""bhItmSetCk (event.c:2776-2794) schreibt rom->etcp[v4].flg ohne Grenzpruefung.

Erreichbar (tools/scan_script_reach.py): RM_3100 @00ae (Interaktion 9 von 9), RM_3140 @003e (255 von 16),
RM_7030 @0022/@0028/@002e (8/9/10 von 1). Das Original traf damit Bit 0 fremder Raumtabellen (RM_3100:
flrp[0].flg). ORIGINAL-FIX (QUALITY.md Regel 2): der Schreibzugriff entfaellt, der Befehl ist ausgefuehrt und
bleibt nicht als offene Bindung stehen; der Itemteil (itwp stflg 0x1000000) bleibt.
Vorherstand 2026-11-04 (alte Fassung, gleiche Laeufe): offene 0x23-Bindungen RM_7030 3, RM_3100 1, RM_3140 1."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def run(runtime, data, root, room):
    state = root / f'{room}.json'
    subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()), '--mods', str(root / 'no-mods'),
                    '--user-dir', str(root / 'user'), '--in-game', '--headless', '--room', room + '.RDX',
                    '--enemy-ai', 'on', '--frames', '60', '--state', str(state)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return json.loads(state.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-itmsetck-') as folder:
        root = Path(folder)
        for room in ('RM_7030', 'RM_3100', 'RM_3140'):
            state = run(args.runtime, args.data, root, room)
            pending = [c for c in state['pending_commands'] if c['opcode'] == 0x23]
            assert not pending, (room, 'bhItmSetCk ausserhalb rom->etcp bleibt offen', pending)
            assert state['blocked_commands'] == 0, (room, state['blocked_commands'])
    print('itmsetck_out_of_range PASS')


if __name__ == '__main__':
    main()
