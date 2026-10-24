#!/usr/bin/env python3
"""RM_1090: Computer nach dem Oeffnen der Tastatur mehrfach abbrechen, danach muss die Spielerin frei sein.

Original: Waehrend des Terminals laeuft Task 8 (system.c:659/2121) weiter; Ereignis 5
(bhEvtComSet 7, Sleep 30, bhInitCamSet, bhCineSet 1) loescht cb_flg/st_flg 0x4, waehrend der
Computer den Schirm hat. Abbrechen (pad_ps 0x1000 -> com_md1 20, sp_evt.c:544) beendet das
Terminal ohne Ereignis 4. bhExitSpEvtComputer (sp_evt.c:762-777) aendert die laufenden sys-Worte
an Ort und Stelle.

Vorher: ComputerTerminal::tick schrieb die beim Oeffnen kopierten Worte zurueck und setzte damit
cb_flg 0x4 / st_flg 0x4 wieder; pad &= 0x1188F (system.c:531) sperrte Rennen und Aktionsknopf.

Replay: tests/native/replays/rm1090-terminal-cancel.txt (Diagnoseaufstellung)."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm1090-terminal-cancel.txt'

def run(runtime, data, root, frames):
    state = root / f'state-{frames}.json'
    subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
                    '--mods', str(root / 'no-mods'), '--user-dir', str(root / f'user-{frames}'),
                    '--headless', '--in-game', '--room', 'RM_1090.RDX',
                    '--replay', str(REPLAY), '--frames', str(frames),
                    '--state', str(state), '--enemy-ai', 'on'],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return json.loads(state.read_text())

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-rm1090-cancel-') as folder:
        root = Path(folder)
        opened = run(args.runtime, args.data, root, 745)
        assert opened['computer'] and opened['computer']['active'], ('Tastatur nicht offen', opened['computer'])
        closed = run(args.runtime, args.data, root, 800)
        flags = closed['work_flags']
        assert not closed['computer']['active'], 'Terminal nach Abbrechen nicht beendet'
        assert not flags[10] & 0x4 and not flags[5] & 0x4, ('cb_flg/st_flg 0x4 nach Abbrechen gesetzt', hex(flags[10]), hex(flags[5]))
        assert not flags[10] & 0x400000 and not flags[5] & 0x4000000, 'bhExitSpEvtComputer-Masken fehlen'
        assert flags[6] == 0xffffffff, ('sp_flg', hex(flags[6]))
        running = run(args.runtime, args.data, root, 1040)
        assert running['player_work']['mode'][0] == 1 and running['player_work']['mode'][2] == 6, ('Rennen gesperrt', running['player_work'])
    print('RM_1090 terminal cancel: flags as left by event 5, player can run again.')

if __name__ == '__main__':
    main()
