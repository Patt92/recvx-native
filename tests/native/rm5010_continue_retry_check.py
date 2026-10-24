#!/usr/bin/env python3
"""RM_5010 Tyrant: Continue nach dem Tod stellt den geladenen Stand her, nicht den Spielstart.

Original: bhInitPlayer (player.c:848) ruft bhPushGameData bei jedem Start ueber bhFirstGameStart
(system.c:1394-1397) - Neues Spiel, Schreibmaschinen-Laden und bhPopGameData (room.c:1053,
tk_flg 0x300020). Der Retry-Punkt ist damit immer der zuletzt geladene Stand. bhPopGameData nullt
ts_flg (room.c:1050), bhFirstGameStart stellt 0x3DF80 wieder her (system.c:453); die Task-Tore
(ts_flg & 0x3DE00) == 0x3DE00 (system.c:668, sync.c:275) sind nach dem Continue also offen.

Vorher: ein stehender Retry-Punkt vom Spielstart wurde beim Laden eines Snapshots nicht ersetzt,
Continue holte RM_0000 mit der Startausruestung zurueck; zugleich blieb ts_flg nach dem Abbild bei
0xC000, Hintergrund und Spielerfreigabe blieben gesperrt.

Ablauf: Snapshot nach dem Intro in RM_5010 mit zusaetzlichem Item 21 anlegen (Debugmenue), dann
Neues Spiel (RM_0000), Snapshot laden, der Tyrant toetet die stehende Claire, Continue bestaetigen."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ITEM = 21
PREP = ('1800 confirm\n1850 debug_key F2\n1851 debug_select give 21\n1852 confirm\n1853 debug_key F2\n'
        '1870 debug_key F2\n1871 debug_select snapshot_save 0\n1872 confirm\n1873 debug_key F2\n')
MAIN = ('1 new_game\n20 debug_key F2\n21 debug_select snapshot_load 0\n22 confirm\n23 debug_key F2\n'
        + ''.join(f'{f} confirm\n' for f in range(1800, 3100, 120)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())
    with tempfile.TemporaryDirectory(prefix='recvx-rm5010-retry-') as folder:
        root = Path(folder)
        snapshots = root / 'snapshots'

        def run(name, replay, frames, extra):
            script = root / f'{name}.txt'
            script.write_text(replay)
            state = root / f'{name}-{frames}.json'
            done = subprocess.run([runtime, '--data', data, '--mods', str(root / 'no-mods'),
                                   '--snapshots', str(snapshots), '--user-dir', str(root / f'user-{name}-{frames}'),
                                   '--headless', '--enemy-ai', 'on', '--enemy-grab', 'on', '--frames', str(frames),
                                   '--replay', str(script), '--state', str(state)] + extra,
                                  check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            s = json.loads(state.read_text())
            assert s['blocked_commands'] == 0, ('blockierte Eventbefehle', s['blocked_commands'])
            return s, done.stdout

        prep, _ = run('prep', PREP, 1900, ['--in-game', '--room', 'RM_5010.RDX'])
        assert (snapshots / 'snapshot-1.rcvxsnap').is_file(), 'Snapshot nicht geschrieben'
        assert any((v >> 16) & 0xff == ITEM for v in prep['items']), ('Item fehlt im Snapshot', prep['items'])

        loaded, _ = run('main', MAIN, 1300, [])
        assert loaded['room'] == 'RM_5010.RDX' and loaded['hp'] > 0 and not loaded['gameover_active'], loaded['room']
        assert any((v >> 16) & 0xff == ITEM for v in loaded['items']), ('Item fehlt nach dem Laden', loaded['items'])

        dead, _ = run('main', MAIN, 2600, [])
        assert dead['gameover_active'] and dead['hp'] < 0, ('Tyrant toetet nicht', dead['hp'], dead['gameover_active'])

        after = None
        for frames in range(2700, 3500, 100):
            s, _ = run('main', MAIN, frames, [])
            if s['retry_count'] == 1 and not s['gameover_active']:
                after = (frames, s)
                break
        assert after, 'Continue erreicht keinen Retry'
        frames, s = after
        ts = s['work_flags'][12]
        assert s['room'] == 'RM_5010.RDX', ('Continue laedt den Spielstart statt des geladenen Stands', s['room'])
        assert any((v >> 16) & 0xff == ITEM for v in s['items']), ('Continue holt die Startausruestung zurueck', s['items'])
        assert s['hp'] > 0, ('HP nach dem Continue nicht hergestellt', s['hp'])
        assert ts & 0x3de00 == 0x3de00, ('ts_flg nach bhPopGameData ohne bhFirstGameStart-Wert', hex(ts))
        assert s['background_enabled'], 'Hintergrund nach dem Continue gesperrt'
    print(f'RM_5010 continue: retry restores the loaded state in RM_5010 with item {ITEM}, ts_flg {hex(ts)} at frame {frames}')


if __name__ == '__main__':
    main()
