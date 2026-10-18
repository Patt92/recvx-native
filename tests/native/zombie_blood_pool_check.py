#!/usr/bin/env python3
"""Blutlache unter einem getoeteten Zombie (bhEne01_DD00, en01.c:9973-9995).

Auf trockenem Boden ruft der erste Sterbezustand bhSetBloodPoolLnk(epw, &epw->px,
epw->ay, &en01_BldTbl, 0) bzw. mit en01_BldTbl2 (effsub3.c:145). Das belegt zwei
eff[]-Arbeiten, die Wurzel mit id 300 und flg 0x200001 (effsub3.c:158-162); bhEff300
laesst sie danach stehen (case 2, effsub3.c:385-397).

Der Port meldete den Aufruf in allen EN01-Sterbezustaenden nur als Luecke
"bhSetBloodPoolLnk (effsub3.c:145)" -- unter toten Zombies entstand nie eine Lache.

Replay: tests/native/replays/rm0080-zombie-blood-pool.txt (Diagnose-Aufstellung im
Raum RM_0080, --enemy-ai on). Geprueft: ein Zombie ist im Sterbezustand (mode0 4),
die Luecke wird nicht gemeldet und mindestens eine Lachenwurzel lebt."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm0080-zombie-blood-pool.txt'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-zombie-pool-') as folder:
        root = Path(folder)
        state = root / 'state.json'
        result = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                                 '--mods', str(root / 'no-mods'), '--headless', '--in-game',
                                 '--room', 'RM_0030.RDX', '--replay', str(REPLAY), '--frames', '6200',
                                 '--state', str(state), '--enemy-ai', 'on'],
                                check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        s = json.loads(state.read_text())
    assert s['room'].split('.')[0] == 'RM_0080', s['room']
    dead = [e for e in s['enemies'] if e['identity'] == 1 and e['work']['mode'][0] == 4]
    assert dead, 'kein Zombie im Sterbezustand: ' + str([(e['hp'], e['work']['mode']) for e in s['enemies']])
    assert not any(d['work']['stflg'] & 0x100000 for d in dead), 'Zombie liegt im Wasser (stflg 0x100000)'
    assert 'bhSetBloodPoolLnk' not in result.stdout, \
        'bhSetBloodPoolLnk wird als Luecke gemeldet, die Sterbezustaende legen keine Lache an'
    assert s['blood_pools'] >= len(dead), f"{s['blood_pools']} Lachen fuer {len(dead)} tote Zombies"
    print(f"RM_0080: {len(dead)} toter Zombie(s), {s['blood_pools']} Blutlache(n) (eff id 300).")


if __name__ == '__main__':
    main()
