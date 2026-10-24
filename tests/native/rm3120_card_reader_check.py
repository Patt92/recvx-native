#!/usr/bin/env python3
"""RM_3120: Emblemkarte (Item 76) am Leser (Bodenausloeser 0) benutzen, ohne Hostabbruch.

Ereignis 0 @0x232 setzt per Common_controll 34 (event.c:11880) auf den versteckten Gegner 2 die
2-Bild-Raummotion 1 mit frm_no 9226 (Original-UB, Motion.c:131 liest hinter dem Clip). Die Karte
startet Ereignis 4 @0x8f0, das den Gegner mit 24 01 02 01 sichtbar macht; bhDrawEnemy zeichnet die
Pose im selben Bild. ORIGINAL-FIX Regel 3 (motion_set.cpp Pass::frame) klemmt auf das letzte Clipbild.

Vorher: der Klemmwert lag auf einem angehaengten Nachbarkey (Bytes des Folgeblocks, Position
-2.69e38), die Matrizen wurden inf und die Runtime brach mit "nonfinite native actor work matrix" ab.

Diagnoseaufstellung: Item per Debugmenue, Position auf dem Ausloeser."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

REPLAY = ('0 debug_key F2\n1 debug_select give 76\n2 confirm\n3 debug_key F2\n'
          '10 position 0 0 121 32768\n200 inventory\n240 right\n250 confirm\n260 confirm\n')

def run(runtime, data, root, frames):
    replay = root / 'replay.txt'
    replay.write_text(REPLAY)
    state = root / f'state-{frames}.json'
    result = subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
                             '--mods', str(root / 'no-mods'), '--user-dir', str(root / f'user-{frames}'),
                             '--headless', '--in-game', '--room', 'RM_3120.RDX', '--replay', str(replay),
                             '--frames', str(frames), '--state', str(state), '--enemy-ai', 'on'],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert result.returncode == 0, result.stdout[-2000:]
    return json.loads(state.read_text())

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-rm3120-card-') as folder:
        root = Path(folder)
        used = run(args.runtime, args.data, root, 300)
        assert (4, 1) in [(t['event'], t['level']) for t in used['event_tasks']], ('Ereignis 4 nicht gestartet', used['event_tasks'])
        enemy = [e for e in used['enemies'] if e.get('index') == 2][0]
        assert not enemy['hidden'], 'Gegner 2 nicht sichtbar'
        done = run(args.runtime, args.data, root, 700)
        assert not done['event_tasks'] and done['event_camera'] == -1, ('Ereignis 4 endet nicht', done['event_tasks'], done['event_camera'])
        assert done['blocked_commands'] == 0
    print('RM_3120 card reader: event 4 runs, enemy 2 drawn from the clamped clip frame.')

if __name__ == '__main__':
    main()
