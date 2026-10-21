#!/usr/bin/env python3
"""RM_1090: Computer mit falschem Passwort, danach muss die Kamera zum Raumschnitt zurueck.

Original: bhSysCall Modus 7 (system.c:2084) stoppt waehrend des Terminals nur Task 7
(ts_flg |= 0x80, :2121) und setzt sp_flg 0x30; Task 8 bhSysCallEvent (system.c:659) laeuft
weiter und bhEventScheduler2 (event.c:13025) unter sp_flg 0x10. Ereignis 5 (Offset 2690
bhEvtComSet 7, dann Sleep 30, bhInitCamSet, bhFixEventCamPly 1, bhCineSet 1) endet also, waehrend
der Computer den Schirm hat. Skript 7 setzt bei falschem Passwort ev_flg 0x3e (computer_test);
scd1 (Offset 1434: bhCk 1/0x3e -> Event_exec Ebene 0 Ereignis 4) zeigt "Nothing happens."
(Nachricht 5, bhCineSet 3/4) und endet.

Vorher (Arbeitsbaum bis 2026-10-21): der native Ereignistick ruhte waehrend des Terminals;
Ereignis 4 verdraengte das noch schlafende Ereignis 5 auf Ebene 0, bhInitCamSet lief nie,
die Ereigniskamera 6 (Monitor) blieb stehen, obwohl die Spielerin wieder frei war.

Replay: tests/native/replays/rm1090-terminal-wrong-password.txt (Diagnoseaufstellung)."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm1090-terminal-wrong-password.txt'


def run(runtime, data, root, frames):
    state = root / f'state-{frames}.json'
    if state.exists():
        state.unlink()
    result = subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
                             '--mods', str(root / 'no-mods'), '--user-dir', str(root / 'user'),
                             '--headless', '--in-game', '--room', 'RM_1090.RDX',
                             '--replay', str(REPLAY), '--frames', str(frames),
                             '--state', str(state), '--enemy-ai', 'on'],
                            check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return json.loads(state.read_text()), result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-rm1090-terminal-') as folder:
        root = Path(folder)
        typed, _ = run(args.runtime, args.data, root, 830)
        comp = typed['computer']
        assert comp and comp['active'] and comp['number'] == 7 and comp['words'][0] == '1972', ('Tastatur', comp)
        # Waehrend des Terminals laeuft Ereignis 5 weiter: nach Sleep 30 bhInitCamSet (5b) und
        # bhCineSet 1, die Aufgabe endet; Ereignis 4 darf noch nicht laufen (ev_flg 0x3e erst nach Enter).
        levels = [(t['event'], t['level']) for t in typed['event_tasks']]
        assert (5, 0) not in levels and (4, 0) not in levels, ('Ereignis 5 lief waehrend des Terminals nicht zu Ende', levels)
        assert typed['event_camera'] == -1 and not typed['cinematic'], ('Kamera vor dem Enter nicht zurueck', typed['event_camera'], typed['cinematic'])
        after, log = run(args.runtime, args.data, root, 900)
        assert not after['computer']['active'], 'Terminal nach Enter nicht beendet'
        assert after['dialog_open'] and after['dialog_index'] == 5, ('"Nothing happens." (Nachricht 5) fehlt', after['dialog_open'], after['dialog_index'])
        assert (4, 0) in [(t['event'], t['level']) for t in after['event_tasks']], ('Ereignis 4 nicht auf Ebene 0', after['event_tasks'])
        done, log = run(args.runtime, args.data, root, 1100)
        assert not done['dialog_open'], 'Nachricht nach OK nicht geschlossen'
        assert done['event_camera'] == -1 and not done['cinematic'], ('Ereigniskamera bleibt stehen', done['event_camera'], done['cinematic'])
        assert done['player_work']['mode'][0] == 1 and not done['player_scripted'], ('Spielerin nicht frei', done['player_work'])
        assert done['blocked_commands'] == 0, log
        print('RM_1090 wrong password: terminal ended, message 5 confirmed, room camera restored.')


if __name__ == '__main__':
    main()
