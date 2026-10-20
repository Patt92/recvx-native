#!/usr/bin/env python3
"""RM_5840 Tyrant (EN19 Typ 2): Trefferzuweisung einer Sofortwaffe an das native Gegnerwork.

Originalkette: bhCheckEnemies (weapon.c:620ff, nativ native/player_combat_core.c) schreibt
flg 0x4, dpx/dvx, wpnr_no, comb_flg 0x10/0x20/0x40 und dam[kno] += EneDam{Near,Mid,Far}
[id][wno] (weapon.c:759/765/771) in den Gegnerwork; der naechste Gegnerpass rechnet
bhEne19_DmgCheck (en19.c:3193): bhEne_CalcDamage (zonzon1.c:725) und hp -= total_dam
(en19.c:3241). Der Ledger fuehrte das als offene "Trefferzuweisung weapon.c an native
Works (A04)" (RoomWorld::damage meldet A04 noch, hat aber keinen Aufrufer).

Erwartung aus Originaldaten: Waffe 6 (Debug-Ausruestung Item 32) gegen id 19 macht je
Treffer EneDamNear[19][6] = 100, EneDamMid[19][6] = 80 oder EneDamFar[19][6] = 60
(weapon.c:16/50/84). En19CombWepTbl[6] = {500, {10,10,10}, 600, 0} (en19.c:102): drei
Treffer erreichen crit 500 nicht, bonus 0; En19CombJointTbl ist genullt (correct 0).
Also ist jede hp-Differenz genau einer dieser drei Werte.

Diagnose: Waffe per Debug-Menue-Ausruestung (Originalmenge), Zielen/Feuern per Replay,
--enemy-ai on. Kein Eingriff in Gegner- oder Trefferfelder."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

REPLAY = ('5 debug_key F2\n6 debug_select equip 32\n7 confirm\n8 debug_key F2\n20 keys aim\n'
          '40 keys aim attack\n43 keys aim\n60 keys aim attack\n63 keys aim\n80 keys aim attack\n83 keys aim\n'
          '100 keys none\n')
DAMAGE = {100, 80, 60}  # EneDamNear/Mid/Far[19][6], weapon.c:16/50/84


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())
    with tempfile.TemporaryDirectory(prefix='recvx-rm5840-gun-') as folder:
        root = Path(folder)
        subprocess.run([runtime, '--write-controls', str(root / 'controls.ini')], check=True,
                       stdout=subprocess.DEVNULL)
        (root / 'replay.txt').write_text(REPLAY)

        def run(frames):
            state = root / ('state-%d.json' % frames)
            done = subprocess.run([runtime, '--data', data, '--mods', str(root / 'no-mods'),
                                   '--controls', str(root / 'controls.ini'), '--debug-config', str(root / 'debug.ini'),
                                   '--snapshots', str(root / 'snapshots'), '--user-dir', str(root / 'user'),
                                   '--saves', str(root / 'saves'), '--headless', '--in-game',
                                   '--room', 'RM_5840.RDX', '--spawn', '0', '--frames', str(frames),
                                   '--replay', str(root / 'replay.txt'), '--state', str(state),
                                   '--enemy-ai', 'on', '--enemy-grab', 'on'],
                                  check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            s = json.loads(state.read_text())
            s['_log'] = done.stdout
            found = [e for e in s['enemies'] if e['identity'] == 19]
            assert len(found) == 1, ('RM_5840 ohne genau einen Tyrant', [e['identity'] for e in s['enemies']])
            return s, found[0]

        hp = []
        hits = []
        for frames in (39, 45, 65, 85):  # vor dem ersten Schuss, dann je nach Schuss 40/60/80 + DmgCheck
            s, t = run(frames)
            assert 'Enemy gaps' not in s['_log'] and 'A04' not in s['_log'], ('Gegnerluecke', s['_log'][-2000:])
            assert s['weapon'] == 6, ('Item 32 ist nicht Waffe 6', s['weapon'])
            hp.append(t['hp'])
            hits.append(s['combat_hits'])
        assert hits == [0, 1, 2, 3], ('Treffer je Schuss', hits)
        assert hp[0] in (300, 420, 540, 900), ('En19HpTbl[gm_mode][2]', hp[0])
        steps = [hp[i] - hp[i + 1] for i in range(3)]
        assert all(d in DAMAGE for d in steps), ('hp -= total_dam ist kein EneDam[19][6]-Wert', hp, steps)
        print('RM_5840 Tyrant: Waffe 6, Treffer %s, hp %s, Schaden je Treffer %s (EneDam[19][6]).' % (hits, hp, steps))


if __name__ == '__main__':
    main()
