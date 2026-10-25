#!/usr/bin/env python3
"""RM_0110 Ereigniszombie: EN26 (Anatomist Zombie, en26.c) im nativen Gegnerpass.

Gegner 4 von RM_0110 hat die Identitaet 26. bhJumpEnemy[26] ist bhEne26
(en26.c:628); der Port hatte dafuer keinen Besitzer und meldete nur die Luecke
"bhEne26". Folgen (Nutzerbefund): Init (en26.c:1772) lief nie, der Work blieb in
mode0 0 ohne Motion (T-Pose beim Wiederbetreten), die Ereignisbefehle Common 24/34
(event.c:11299/:11632) schalten ihn auf mode0 5 (bhEne_Event), was ohne Routine nie
lief; nach der Szene stand der Zombie still und die Spielerin blieb im Ereignis-
Rueckwaertsgehen (pl_smove04, pl_evt.c:392), das erst der Griff des Zombies beendet.

Konstruierter Testzustand (keine Nutzerdaten): frischer Snapshot aus RM_0110
Eingang 1 (sys->pos_no 1 -> scd0 @256 values[15] == 1 setzt rm_flg 3), dazu nur
ev_flg 128 (scd0 @846: ev_flg 128 gesetzt, 129 frei, rm_flg 3 -> Fressszene mit
Ereignissen 10..12). Danach natuerliches Gehen bis zum Bodenausloeser der Szene
(Ereignisse 40..44), --enemy-ai on, --enemy-grab on. Diagnose: der Debug-Menue-
Ausruestungseintrag gibt die Waffe 32 (wpnr 6), der Raumwechsel fuer das
Wiederbetreten geht ueber das Debug-Menue (Raum/Teleport). Nur die Schussprobe
setzt nach der natuerlichen Szene eine Diagnoseposition; sie ist kein
Durchspiel-/Zielwechselnachweis."""
import argparse
import json
import os
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

WALK = ('0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
        '{equip}'
        '20 keys forward\n100 keys left\n103 keys forward\n'
        '300 keys right\n307 keys forward\n600 keys none\n')
EQUIP = '5 debug_key F2\n6 debug_select equip 32\n7 confirm\n8 debug_key F2\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())
    with tempfile.TemporaryDirectory(prefix='recvx-rm0110-en26-') as folder:
        root = Path(folder)
        (root / 'snapshots').mkdir()
        subprocess.run([runtime, '--write-controls', str(root / 'controls.ini')], check=True,
                       stdout=subprocess.DEVNULL)

        def run(name, frames, replay, trace=False):
            script, state = root / (name + '.txt'), root / (name + '.json')
            script.write_text(replay)
            done = subprocess.run([runtime, '--data', data, '--mods', str(root / 'no-mods'),
                                   '--controls', str(root / 'controls.ini'), '--debug-config', str(root / 'debug.ini'),
                                   '--snapshots', str(root / 'snapshots'), '--user-dir', str(root / 'user'),
                                   '--saves', str(root / 'saves'), '--headless', '--in-game',
                                   '--room', 'RM_0110.RDX', '--spawn', '1', '--frames', str(frames),
                                   '--replay', str(script), '--state', str(state),
                                   '--enemy-ai', 'on', '--enemy-grab', 'on'],
                                  check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                  env=dict(os.environ, RECVX_EVENT_TRACE='1') if trace else None)
            s = json.loads(state.read_text())
            s['_log'] = done.stdout
            return s

        def en26(s):
            found = [e for e in s['enemies'] if e['identity'] == 26]
            assert len(found) == 1 and found[0]['index'] == 4, ('EN26 ist nicht Raumgegner 4',
                                                                [(e['index'], e['identity']) for e in s['enemies']])
            return found[0]

        def events(s):
            return {t['event'] for t in s['event_tasks']}

        run('seed', 4, '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n')
        b = bytearray((root / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
        assert b[:8] == b'RCVXSNAP' and struct.unpack_from('<II', b, 8) in ((1, 0x838), (2, 0x838)), 'snapshot header'
        assert b[16 + 12] == 1, 'Snapshot nicht an Eingang 1 (SaveFile.pos_no)'
        at = 16 + 0x14 + (128 // 32) * 4  # SaveFile.ev_flg, Flag 128
        struct.pack_into('<I', b, at, struct.unpack_from('<I', b, at)[0] | (0x80000000 >> (128 % 32)))
        struct.pack_into('<I', b, len(b) - 4, zlib.crc32(b[:-4]))
        (root / 'snapshots/snapshot-2.rcvxsnap').write_bytes(b)

        walk = WALK.format(equip='')
        # Fressszene vor dem Ausloeser: Common 24/34 setzen mode0 5, bhEne_Event laeuft in bhEne26.
        idle = run('idle', 200, walk)
        z = en26(idle)
        assert 'bhEne26' not in idle['_log'], 'bhJumpEnemy[26] ohne nativen Besitzer (Luecke bhEne26)'
        assert z['work']['mode'][0] == 5 and events(idle) & {10, 11, 12}, \
            ('Fressszene: EN26 nicht im Ereignismodus', z['work']['mode'], idle['event_tasks'])
        # Szene am Bodenausloeser (Ereignisse 40..44).
        scene = run('scene', 600, walk)
        z = en26(scene)
        assert scene['cinematic'] and 40 in events(scene) and z['work']['mode'][0] == 5, \
            ('Szene nicht gestartet', scene['event_tasks'], z['work']['mode'])
        # Nach der Szene: EN26 laeuft seine Zustandsmaschine (Move/Nage), greift und beisst.
        after = run('after', 900, walk)
        z = en26(after)
        assert not events(after) & {40, 41, 42, 43, 44}, ('Szene endet nicht', after['event_tasks'])
        assert z['work']['mode'][0] in (1, 2), ('EN26 bleibt nach der Szene stehen', z['work']['mode'], z['work']['mtn_no'])
        assert after['hp'] < 160, ('EN26 greift nicht an (NG00, en26.c:2605)', after['hp'], after['player_work'])
        # Wiederbetreten: bhEne26_Init (en26.c:1772) setzt mode0 1 und eine Motion.
        back = run('reenter', 1200, walk + '1070 debug_key F2\n1071 debug_select room 14\n1072 confirm\n'
                   '1120 debug_select room 13\n1121 confirm\n1122 debug_key F2\n')
        assert back['room'] == 'RM_0110.RDX', back['room']
        z = en26(back)
        # Init setzt hp 200 (en26.c:1785) und mode0 1 (:1852); ohne Init bleibt beides 0.
        assert z['work']['mode'][0] >= 1 and z['work']['hp'] == 200, \
            ('EN26 nach Wiederbetreten ohne Init (T-Pose)', z['work'])
        # Toetbar: explizite Diagnoseposition nach dem ersten Griff, dann echter
        # Zieleingabe-/Schusspfad. Kopfplatzen braucht nr < 15 (weapon.c:757 ->
        # comb_flg 0x10, en26.c:954-985). Die alte Probe verliess sich auf den
        # genauen Kampfstand nach 1080 Takten: der erste Game-Pass vor Event-0
        # veraendert ihn, next_target fuehrte dann zu einem Fehlschuss
        # (combat_shots=1, combat_hits=0), nicht zu fehlenden Kopfeffekten.
        # Gleiche Diagnoseposition gegen beide Task-Startreihenfolgen geprueft;
        # Szenenablauf/Angriff oben bleiben ohne Positionsinjektion getestet.
        kill_replay = (WALK.format(equip=EQUIP) +
                       '1080 position 26 0 25 16384\n'
                       '1081 keys forward\n1085 keys aim\n1091 keys aim next_target\n1094 keys aim\n1099 keys aim attack\n1102 keys aim\n'
                       '1115 keys aim attack\n1118 keys aim\n1131 keys aim attack\n1134 keys none\n')
        # Kopfplatzen (bhEne26_DmgCheckType00, en26.c:864-872): hp = -1, ExpHeadEffect legt viermal
        # bhEne_SetNikuhenEffect an (en26.c:3350-3356 -> zonzon.c:768, eff id 362), NeckBloodEffect(0)
        # je Winkel zweimal bhEne_SetBlood3 (en26.c:3369-3384 -> zonzon.c:634, eff id 370), also acht.
        # EXP0_I(0x78) = -1 sperrt danach SetNikuhenEffect2 (en26.c:1085), es kommen keine weiteren 362.
        burst = run('burst', 1105, kill_replay)
        z = en26(burst)
        ids = [e['id'] for e in burst['effects']]
        assert burst['combat_shots'] >= 1 and burst['combat_hits'] >= 1, \
            ('Kopfplatzprobe ohne echten Schusstreffer', burst['combat_shots'], burst['combat_hits'])
        assert z['hp'] == -1 and ids.count(362) == 4 and ids.count(370) == 8, \
            ('Kopfplatzen ohne 4x362/8x370 (en26.c:868/869)', z['hp'], sorted(set(ids)), ids.count(362), ids.count(370))
        assert 'Nikuhen' not in burst['_log'] and 'Blood3' not in burst['_log'] and 'bhEff362' not in burst['_log'] \
            and 'bhEff370' not in burst['_log'], 'Kopfplatzen meldet eine Luecke'
        kill = run('kill', 1154, kill_replay, trace=True)
        z = en26(kill)
        assert kill['combat_hits'] >= 1, ('kein Treffer', kill['combat_shots'], kill['combat_hits'])
        assert z['hp'] < 0 and z['work']['mode'][0] == 4 and z['work']['flg'] & 2, \
            ('EN26 stirbt nicht (DmgCheckType00/Die, en26.c:856/3195)', z['hp'], z['work'])
        # Schluesselgegenstand: scd1 @06ec bhEtcAtariEnePosSet (0xb3, event.c:7794) setzt nach dem Tod
        # etcp 12 und itwp[4] an owP[9] des Gegners 4 (siehe event_lifecycle RM_0110).
        assert 'op=b3: 04 0c 03 3b 00 09' in kill['_log'], 'bhEtcAtariEnePosSet fuer Gegner 4 nach dem Tod nicht erreicht'
        print('RM_0110 EN26: Fressszene mode0 5, Szene endet, Angriff (hp %d), Init beim Wiederbetreten '
              '(mtn %d), Tod nach Treffer (hp %d), Gegenstandsplatz 0xb3.' % (after['hp'], en26(back)['work']['mtn_no'], z['hp']))


if __name__ == '__main__':
    main()
