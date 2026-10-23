#!/usr/bin/env python3
"""Titel -> LOAD GAME -> Abbruch bzw. Laden (bup_00.c:425-437, adv.c:1953).

Vom Titel aus laeuft der Ladebildschirm mit ss_flg 0x200 und ohne Raum. Ein Abbruch setzt
tk_flg 0x300008: der Titeltask laeuft wieder, mit dem FromTitle 1 von LOAD GAME direkt im
Hauptmenue (Mode 8/9), Cursor auf LOAD GAME. Vorher blieb der Titel versteckt, der Runtime
zeichnete ohne Raum die Spielszene und brach mit "actor texture outside scene table" ab.
Der Spielstand fuer Slot 1 entsteht aus einem Host-Snapshot (dasselbe Dateiformat).
"""
import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-title-load-') as temporary:
        root = Path(temporary)

        def run(name, frames, inputs, extra):
            replay, state = root / (name + '.txt'), root / (name + '.json')
            replay.write_text('\n'.join(inputs) + '\n')
            result = subprocess.run([str(runtime), '--data', str(data), '--headless', '--frames', str(frames),
                                     '--user-dir', str(root / 'user'), '--snapshots', str(root / 'snaps'),
                                     '--saves', str(root / 'saves'), '--replay', str(replay), '--state', str(state)] + extra,
                                    capture_output=True, text=True, timeout=300)
            assert result.returncode == 0, (name, result.stderr[-600:])
            return json.loads(state.read_text()), result.stdout

        run('snapshot', 70, ['40 debug_key F2', '42 debug_select snapshot_save 0', '44 confirm', '50 debug_key F2'],
            ['--in-game', '--room', 'RM_0010.RDX'])
        (root / 'saves').mkdir()
        shutil.copy(root / 'snaps' / 'snapshot-1.rcvxsnap', root / 'saves' / 'save-1.rcvxsav')

        opened, out = run('open', 400, ['100 confirm', '200 confirm'], [])
        assert opened['typewriter']['active'] and 'Ladebildschirm' in out, ('LOAD GAME oeffnet den Ladebildschirm nicht', opened['typewriter'])

        back, out = run('cancel', 600, ['100 confirm', '200 confirm', '400 cancel'], [])
        assert not back['typewriter']['active'] and back['title_active'], ('Abbruch fuehrt nicht in den Titel', back['typewriter'], back['title_active'])
        assert back['title_mode'] == 9 and back['title_cursor'] == 0, ('Titel nicht im Hauptmenue auf LOAD GAME (FromTitle 1)', back['title_mode'], back['title_cursor'])

        again, out = run('again', 900, ['100 confirm', '200 confirm', '400 cancel', '650 confirm'], [])
        assert again['typewriter']['active'], 'nach dem Abbruch laesst sich LOAD GAME nicht erneut waehlen'

        loaded, out = run('load', 700, ['100 confirm', '200 confirm', '400 confirm', '460 confirm'], [])
        assert loaded['room'] == 'RM_0010.RDX' and not loaded['title_active'] and not loaded['typewriter']['active'], \
            ('Laden vom Titel erreicht den Raum nicht', loaded['room'], loaded['title_active'])
    print('title LOAD GAME: cancel returns to the main menu, load reaches the saved room')


if __name__ == '__main__':
    main()
