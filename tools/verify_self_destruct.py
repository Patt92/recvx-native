#!/usr/bin/env python3
"""Selbstzerstoerungsszene (gameover.c bhMainGameOver gov_md1 10) am Laufzeitpfad.

Ausloeser ist das Originalskript: RM_4040 scd1 @0452 prueft ev_flg 257 gesetzt, 259 frei,
35 gesetzt, 36 frei und CkFlag Bank 10 Bit 23 extra 1 (etc_idx 1 mit cb_flg 0x100, also die
Untersuchung von Raumeintrag 1, Typ 3 bei x 18..24 / z 4..34), dann EvtStart 5 und 0xcb Modus 1
(bhSetEventTimer, system.c:2437: ev_flg 67, evt_tim 18000 VSync, evt_tdg 3600). Event 5 fuehrt
ueber die Tuer 0x33 nach RM_3230 (Fluchtszene) und weiter nach RM_3091, wo der Zaehler im freien
Spiel ablaeuft (sync.c:62: -2 je Bild bei sp_flg 0x200). bhCheckEvtTimer (player.c:1272) startet
dann mit gm_flg 0x400 das Game Over; bhMainGameOver zaehlt 70 Bilder in md1 0 und laeuft in
md1 10 die Todesszene (gameover.c:287-330, :443-480): Motion PlMtnAct[wpntp][dmlvl][3] ab Bild 0
ohne Mischung, gov_etb bei gov_ct 40/54/68/82/96/110, die fuenfte Explosion setzt Motion 25 mit
frm_no 0, hokan_count 8, hokan_rate 49152, Motion 25 wechselt beim Bildumbruch auf 26, nach
gov_ct > 150 md1 1.

Diagnose, kein natuerliches Durchspielen: Debug-Snapshot in RM_4040 mit gepatchten ev_flg
257/35 (gesetzt) und 259/36 (frei), Spielerplatzierung vor Raumeintrag 1. Die RM_3230-Fluchtszene
laeuft ohne Skip durch: Event 2 setzt EN19 (Art 1 Index 1) per Common_controll 24 (event.c:11299-11500:
mnwP = sys->rmthp, mtn_add 0x10000, mtn_no 1, mode0 5) 569 Bilder lang auf frm_no 0, dann warten die
Tasks mit bhFrameCheck (event.c:3237, Befehl 0x38) auf frm_no 35/80/463; die Motion treibt
bhEne19_Event (en19.c bhEne19_Mode0[5]) ueber bhSetMotion im Gegnerpass bhControlEnemy, der nur
bei sys->sp_flg 0x2 laeuft (eneset.c:352). Headless/Replay-Laeufe schalten diesen Pass per
Vorgabe ab (--enemy-ai off, runtime.cpp; world_enemies.cpp maskiert sp_flg 0x2), deshalb hier
ausdruecklich --enemy-ai on --enemy-grab on. Die zweite Aufstellung setzt ply_hp 100 (dmlvl 1),
damit die Todesmotion aus der zweiten PlMtnAct-Zeile (gameover.c:289 liest exp0->dmlvl beim
Eintritt) gemessen wird. RECVX_PLAYER_POSE_TRACE/RECVX_CAM_TRACE liefern je Bild die Knotenteile
des Spielers.

Explosionen (2026-10-17): gameover.c:539 bhSetExplosionEffect(&gov_etb[exp_ct].px) -> weapon.c:1812
(Feuerball 23 Art 1 Groesse 6 bei pos - 4 * cam.v, zwei Truemmer 24; 23 Art 1 streut alle vier Bilder
einen weiteren 23 und einen Funken 24, 24 setzt 16 Paesse lang je ein 25) laeuft am Weltbesitzer unter
sp_flg 0x6C (effect.c:750, bhControlEffect bei 0x8) und wird mit pt_flg 0x15 (game.c:339 bhDrawEffect)
unter der Szenenkamera cam.flg 0x40 (0/10/35, pers 11832) gezeichnet; gameover.c:278 bhClearEffect nullt
davor eff[512]. Geprueft werden die Effektliste des Zustands, das Lueckenprotokoll und zwei Endbilder
(--screenshot) mit warmen Explosionspixeln (R >= 160, G >= 70, B < R - 50).
"""
import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOM = 'RM_4040.RDX'
FREE_ROOM = 'RM_3091.RDX'
SCENE_FRAMES = 1100  # Bild innerhalb der RM_3230-Fluchtszene: EN19 hinter den Warten 35/80, vor 463
SCENE_DONE_FRAMES = 1500  # Bild nach dem Ende der 463er-Warten (Raumeintritt 450 + 569 Schleife + 463)
CHAIN_FRAMES = 2200  # Bild, an dem der Zaehler in RM_3091 abgelesen wird (Tuer ohne Skip, Eintritt ~1869)
EN19_SCENE_WAIT_463 = {1: 2583, 2: 2707, 5: 3073}  # EWHILE-Offsets der FrameCheck-Schleifen auf frm_no 463
# player.c:216-227 PlMtnAct[wpntp][dmlvl], Spalte 3 (Todesmotion); dieselbe Tafel in native/player_stairs.c.
PLMTNACT_DEATH = {(0, 0): 4, (0, 1): 7, (0, 2): 8, (1, 0): 6, (1, 1): 7, (1, 2): 8}


def flag_bit(words, bit):
    return bool(words[bit // 32] & (0x80000000 >> (bit % 32)))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--data', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    runtime = str(a.runtime.resolve())
    for stale in [out / 'controls.ini'] + list((out / 'snapshots').glob('snapshot-*.rcvxsnap')):
        stale.unlink(missing_ok=True)  # die Runtime ueberschreibt weder controls.ini noch Snapshot-Plaetze
    subprocess.run([runtime, '--write-controls', str(out / 'controls.ini')], check=True, stdout=subprocess.DEVNULL)
    shutil.copyfile(Path(__file__).resolve().parents[1] / 'debug.ini', out / 'debug.ini')
    results = {}

    def run(name, frames, replay, trace=False, screenshot=False):
        script, state, log = out / (name + '.txt'), out / (name + '.json'), out / (name + '.log')
        script.write_text(replay)
        if state.exists():
            state.unlink()  # die Runtime ueberschreibt kein Zustandsziel
        shot = out / (name + '.bmp')
        shot.unlink(missing_ok=True)
        env = dict(os.environ)
        if trace:
            env['RECVX_PLAYER_POSE_TRACE'] = '1'
            env['RECVX_CAM_TRACE'] = '1'
        with log.open('w') as stream:
            done = subprocess.run([runtime, '--data', str(a.data.resolve()), '--mods', str(out / 'no-mods'),
                                   '--controls', str(out / 'controls.ini'), '--debug-config', str(out / 'debug.ini'),
                                   '--snapshots', str(out / 'snapshots'), '--headless', '--in-game', '--room', ROOM,
                                   '--frames', str(frames), '--replay', str(script), '--state', str(state),
                                   '--enemy-ai', 'on', '--enemy-grab', 'on']  # Gegnerpass (eneset.c:352) auch headless
                                  + (['--screenshot', str(shot)] if screenshot else []),
                                  stdout=stream, stderr=subprocess.STDOUT, env=env)
        if done.returncode:
            raise RuntimeError(f'{name}: runtime exit {done.returncode}\n' + log.read_text())
        v = json.loads(state.read_text())
        assert v['blocked_commands'] == 0, (name, 'blocked commands', v['event_tasks'])
        v['_log'] = log
        v['_shot'] = shot if screenshot else None
        return v

    def warm_pixels(shot):
        # 24/32-Bit-BMP ohne Kompression (SoftwareRenderer::save_bmp); Zeilen von unten nach oben.
        b = shot.read_bytes()
        off, w, h, bpp = struct.unpack_from('<I', b, 10)[0], struct.unpack_from('<i', b, 18)[0], struct.unpack_from('<i', b, 22)[0], struct.unpack_from('<H', b, 28)[0]
        assert bpp in (24, 32), ('BMP', bpp)
        step, row = bpp // 8, (w * bpp // 8 + 3) & ~3
        warm = 0
        for y in range(abs(h)):
            base = off + y * row
            for x in range(w):
                bb, g, r = b[base + x * step], b[base + x * step + 1], b[base + x * step + 2]
                if r >= 160 and g >= 70 and bb < r - 50:
                    warm += 1
        return warm

    def explosion_gaps(log):
        return [l for l in log.read_text().splitlines() if re.search(r'\bbhEff0*(23|24|25)\b|bhSetExplosionEffect', l)]

    # --- Diagnose-Snapshot: Raum-Init von RM_4040, dann die Flags der Selbstzerstoerung --------
    run('seed', 4, '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n')
    seed = bytearray((out / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
    assert seed[:8] == b'RCVXSNAP' and struct.unpack_from('<II', seed, 8) == (1, 0x838), 'snapshot header'
    ev_base = 16 + 0x14  # SaveFile.ev_flg
    for bit, on in ((257, 1), (35, 1), (259, 0), (36, 0)):
        at = ev_base + (bit // 32) * 4
        word = struct.unpack_from('<I', seed, at)[0]
        mask = 0x80000000 >> (bit % 32)
        struct.pack_into('<I', seed, at, (word | mask) if on else (word & ~mask))
    struct.pack_into('<I', seed, len(seed) - 4, zlib.crc32(seed[:-4]))
    (out / 'snapshots/snapshot-2.rcvxsnap').write_bytes(seed)
    caution = bytearray(seed)
    struct.pack_into('<h', caution, 16 + 0x818, 100)  # SaveFile.ply_hp[0]: dmlvl 1 (player.c:803-821)
    struct.pack_into('<I', caution, len(caution) - 4, zlib.crc32(caution[:-4]))
    (out / 'snapshots/snapshot-3.rcvxsnap').write_bytes(caution)

    def replay(slot):
        # Nach der Untersuchung keine weitere Eingabe: die RM_3230-Szene und ihre Tuer laufen natuerlich.
        lines = ['0 debug_key F2', '1 debug_select snapshot_load %d' % slot, '2 confirm',
                 '60 position 21 0 19 0', '61 confirm', '90 confirm', '120 confirm', '150 confirm']
        return '\n'.join(lines) + '\n'

    def en19(v):
        found = [e for e in v['enemies'] if e['identity'] == 19]
        assert len(found) == 1 and found[0]['index'] == 1, ('EN19 ist nicht Art 1 Index 1 (rom->enep[1])', [(e['index'], e['identity']) for e in v['enemies']])
        return found[0]['work']

    def scene(label, slot):
        # RM_3230 Event 2 (rel 0a22): Common_controll 24 -> mode0 5, mtn_no 1, mtn_add 0x10000 (event.c:11299-11500),
        # 569 Bilder frm_no 0, dann bhFrameCheck (event.c:3237) auf 35, 80 und 463 bei einem Bild je Pass.
        s = run(label + '-scene', SCENE_FRAMES, replay(slot))
        assert s['room'] == 'RM_3230.RDX' and s['room_changes'] == 1 and s['cinematic'], (label, 'RM_3230-Szene', s['room'], s['room_changes'], s['cinematic'])
        w = en19(s)
        assert w['mode'][0] == 5 and w['mode'][1] == 1 and w['mtn_no'] == 1 and w['mtn_add'] == 0x10000, (label, 'EN19 nach Common_controll 24', w['mode'], w['mtn_no'], hex(w['mtn_add']))
        assert 80 * 65536 < w['frm_no'] < 463 * 65536, (label, 'EN19 frm_no zwischen den Warten 80 und 463 (bhEne19_Event/bhSetMotion)', w['frm_no'] / 65536)
        waiting = {t['event']: t['offset'] for t in s['event_tasks']}
        assert all(waiting.get(ev) == off for ev, off in EN19_SCENE_WAIT_463.items()), (label, 'Tasks in der 463er-Warte', waiting)
        d = run(label + '-scene-done', SCENE_DONE_FRAMES, replay(slot))
        assert d['room'] == 'RM_3230.RDX' and d['room_changes'] == 1, (label, 'noch RM_3230 nach den 463er-Warten', d['room'])
        wd = en19(d)
        assert wd['mode'][0] == 5 and wd['mtn_no'] == 1 and wd['frm_no'] >= 463 * 65536, (label, 'EN19 frm_no >= 463', wd['frm_no'] / 65536)
        done = {t['event']: t['offset'] for t in d['event_tasks']}
        assert all(done.get(ev) != off for ev, off in EN19_SCENE_WAIT_463.items()), (label, 'Tasks noch in der 463er-Warte', done)
        assert 1 not in done and 2 not in done, (label, 'Tasks 1/2 nach EEVT (0a1e/0a94) noch aktiv; 5 schlaeft 2x45 Bilder bis 0c0a', done)
        return w['frm_no'] // 65536, wd['frm_no'] // 65536

    def timeline(label, slot, hp):
        # Ausloeser: Block scd1 @0452 nach der Untersuchung von Eintrag 1.
        t = run(label + '-trigger', 200, replay(slot))
        assert t['hp'] == hp, (label, 'ply_hp aus dem Snapshot', t['hp'])
        assert 5 in t['events'], (label, 'EvtStart 5 des scd1-Blocks @048a fehlt', t['events'])
        assert all(flag_bit(t['event_flags'], b) for b in (257, 35, 259, 36, 67)), (label, 'Flags des Blocks @0452', t['event_flags'][:3])
        assert t['event_deadline_mode'] == 2 and 0 < t['event_deadline_ticks'] <= 18000, (label, 'bhSetEventTimer Modus 1', t['event_deadline_mode'], t['event_deadline_ticks'])
        # Raumkette RM_4040 -> RM_3230 -> RM_3091: der Zaehler ueberlebt bhInitRoomChangeSystem.
        c = run(label + '-chain', CHAIN_FRAMES, replay(slot))
        assert c['room'] == FREE_ROOM and c['room_changes'] == 2, (label, 'Raumkette', c['room'], c['room_changes'])
        assert not c['cinematic'] and not c['dialog_open'] and not c['player_scripted'] and not c['gameover_active'], (label, 'kein freies Spiel in RM_3091', c['cinematic'], c['dialog_open'], c['player_scripted'])
        assert c['event_deadline_mode'] == 2 and 0 < c['event_deadline_ticks'] < 18000 and c['event_deadline_ticks'] % 2 == 0, (label, 'evt_tim nach dem Raumwechsel', c['event_deadline_mode'], c['event_deadline_ticks'])
        assert c['work_flags'][6] & 0x200 and c['work_flags'][11] & 0x20000000 and (c['work_flags'][12] & 0x15e00) == 0x15e00 and not (c['work_flags'][10] & 1), (label, 'bhCheckEvtTimer-Tor (player.c:1272)', [hex(c['work_flags'][i]) for i in (6, 10, 11, 12)])
        assert flag_bit(c['event_flags'], 67), (label, 'ev_flg 67 vor dem Ablauf geloescht')
        # sync.c:62: evt_tim -= 2 je Bild; Bild e bringt den Zaehler auf 0 und bhCheckEvtTimer (gleicher Pass) auf gov_md0 0.
        e = CHAIN_FRAMES + c['event_deadline_ticks'] // 2 - 1
        return t, c, e

    # --- Aufstellung 1: volle HP (dmlvl 0, Motion 4) -----------------------------------------
    scene_frames = scene('full', 1)
    t1, c1, e1 = timeline('full', 1, 160)
    ticks_at_chain = c1['event_deadline_ticks']
    before = run('full-before', e1 - 2, replay(1))
    assert not before['gameover_active'] and before['event_deadline_ticks'] == 6 and flag_bit(before['event_flags'], 67), ('before', before['gameover_active'], before['event_deadline_ticks'])
    expiry = run('full-expiry', e1 + 2, replay(1))
    assert expiry['gameover_active'] and expiry['gameover_phase'] == 0, ('expiry', 'bhCheckEvtTimer startete kein Game Over', expiry['gameover_active'], expiry['gameover_phase'])
    assert expiry['event_deadline_mode'] == 0 and expiry['event_deadline_ticks'] == 0, ('expiry', 'bhInitGameOver loescht evt_tmd/evt_tim (gameover.c:114)')
    assert not flag_bit(expiry['event_flags'], 67), ('expiry', 'ev_flg 67 nach bhCheckEvtTimer noch gesetzt')
    assert expiry['work_flags'][11] & 0x400 and not (expiry['work_flags'][12] & 0x4000) and expiry['work_flags'][12] & 0x100 and not (expiry['work_flags'][5] & 0x200), ('expiry', 'player.c:1289-1303 Flags', hex(expiry['work_flags'][11]), hex(expiry['work_flags'][12]), hex(expiry['work_flags'][5]))
    assert expiry['hp'] == 160, ('expiry', 'Selbstzerstoerung aendert hp nicht', expiry['hp'])
    # Bild e+1: bhSelectContinue/bhInitGameOver; md1 0 zaehlt gov_ct 1..71 (Bilder e+2..e+72); Bild e+73 ist der
    # erste Pass in md1 10 (gov_ct 0). Der Pass gov_ct 150 (Bild e+223) schaltet nach md1 1.
    s = e1 + 73
    scene = run('full-scene', s + 10, replay(1))
    assert scene['gameover_active'] and scene['gameover_phase'] == 10 and scene['gameover_blasts'] == 0, ('scene', scene['gameover_phase'], scene['gameover_blasts'])
    # gameover.c:278 bhClearEffect: vor der ersten Explosion ist kein Satz (auch kein Raumeffekt 122 aus RM_3091) mehr belegt.
    assert all(e['id'] == 0 for e in scene['effects']), ('scene', 'eff[512] nach bhClearEffect nicht leer', [e['id'] for e in scene['effects'] if e['id']][:10])
    assert scene['motion'] == PLMTNACT_DEATH[(0, 0)] and scene['animation_frame'] == 9 * 65536, ('scene', 'Motion Spalte 3 / frm_no nach 10 Paessen', scene['motion'], scene['animation_frame'])
    assert scene['work_flags'][6] == 0x6C and (scene['work_flags'][5] & 0x5) == 0x5 and (scene['work_flags'][11] & 0x200480) == 0x200480 and scene['work_flags'][10] & 0x40000000, ('scene', 'gameover.c:301-330 Flags', [hex(scene['work_flags'][i]) for i in (5, 6, 10, 11)])
    assert scene['actor'] == [0, 0, 0] and scene['yaw'] == 35499, ('scene', 'plp->px/py/pz 0, ay 35499', scene['actor'], scene['yaw'])
    # gameover.c:242 njFogDisable in md1 0, :323 gm_flg 0x200000 -> sync.c:143 bhControlEOR njFogDisable am Ende
    # jedes Bildes: md1 10 hat keinen Raumnebel (RM_3091 near 128 / far -200 bleiben nur Kamerawerte).
    assert not scene['fog']['enabled'], ('scene', 'Nebel in md1 10 trotz gm_flg 0x200000 (sync.c:143)', scene['fog'])
    blast5 = run('full-blast5', s + 100, replay(1), screenshot=True)
    assert blast5['gameover_phase'] == 10 and blast5['gameover_blasts'] == 5, ('blast5', blast5['gameover_phase'], blast5['gameover_blasts'])
    assert blast5['motion'] == 25 and blast5['animation_frame'] == 2 * 65536, ('blast5', 'Motion 25 ab frm_no 0 nach der fuenften Explosion', blast5['motion'], blast5['animation_frame'])
    # weapon.c:1812 am Weltbesitzer: nach der fuenften Explosion (gov_ct 96, Pass gov_ct 100) leben Feuerball 23
    # (15 Bilder), Truemmer 24 (47 Paesse) und deren Funken 25 im geteilten eff[512]; nichts davon als Luecke.
    ids = [e['id'] for e in blast5['effects']]
    assert ids.count(23) >= 1 and ids.count(24) >= 2 and ids.count(25) >= 1, ('blast5', 'Effektsaetze 23/24/25 der Explosionen fehlen', sorted(set(ids)))
    assert not explosion_gaps(blast5['_log']), ('blast5', 'Explosionsluecken im Protokoll', explosion_gaps(blast5['_log']))
    warm5 = warm_pixels(blast5['_shot'])
    assert warm5 >= 5000, ('blast5', 'Endbild ohne Explosionspixel (Feuerball 23 Groesse 6 bei (3,10,-6), 41 Einheiten vor der Kamera)', warm5)
    blast1 = run('full-blast1', s + 45, replay(1), screenshot=True)
    assert blast1['gameover_phase'] == 10 and blast1['gameover_blasts'] == 1, ('blast1', blast1['gameover_phase'], blast1['gameover_blasts'])
    warm1 = warm_pixels(blast1['_shot'])
    assert warm1 >= 20, ('blast1', 'Endbild ohne Explosionspixel (Feuerball 23 bei (-25,10,-76), 115 Einheiten vor der Kamera)', warm1)
    final = run('full-mask', s + 151 + 20, replay(1), trace=True)
    assert final['gameover_active'] and final['gameover_phase'] == 1 and final['gameover_blasts'] == 6, ('mask', final['gameover_phase'], final['gameover_blasts'])
    assert not explosion_gaps(final['_log']), ('mask', 'Explosionsluecken im Protokoll', explosion_gaps(final['_log']))
    assert final['work_flags'][6] == 0x20, ('mask', 'sp_flg 0x20 nach md1 10', hex(final['work_flags'][6]))

    # --- POSETRACE: je Pass genau ein Knotenteil, Original-Mischfolge --------------------------
    ticks = {}
    current = None
    for line in final['_log'].read_text().splitlines():
        m = re.match(r'CAMTICK tick=(\d+)', line)
        if m:
            current = int(m.group(1))
            ticks.setdefault(current, [])
            continue
        m = re.match(r'POSETRACE mtn=(\d+) frm=(-?\d+) mode=(\d+) hokan=(\d+) rate=(-?\d+)', line)
        if m and current is not None:
            ticks[current].append(tuple(int(x) for x in m.groups()))
    assert ticks, 'keine CAMTICK-Zeilen (RECVX_CAM_TRACE)'
    death = PLMTNACT_DEATH[(0, 0)]
    nodes = [ticks.get(s + k, []) for k in range(151)]
    assert all(len(n) == 1 for n in nodes), ('md1 10', 'nicht genau ein Knotenteil je Pass', [(s + k, len(n)) for k, n in enumerate(nodes) if len(n) != 1][:10])
    nodes = [n[0] for n in nodes]
    assert all(len(ticks.get(s + k, [])) == 0 for k in range(151, 160)), ('md1 1', 'Knotenteile nach gov_ct 150', [ticks.get(s + k) for k in range(151, 160)])
    # player.c:1292 setzt sp_flg = 0x20; das Passtor von bhControlPlayer (player.c:1365, sp_flg 0x1) bleibt
    # damit von Bild e bis zum Ende von md1 0 zu: kein Knotenteil, waehrend Bild e-1 noch einen hatte.
    assert len(ticks.get(e1 - 1, [])) == 1, ('frei', 'Spielerpass vor dem Ablauf fehlt', ticks.get(e1 - 1))
    assert all(len(ticks.get(e1 + k, [])) == 0 for k in range(73)), ('md1 0', 'Knotenteil trotz sp_flg 0x20 (player.c:1292/1365)', [(e1 + k, ticks[e1 + k]) for k in range(73) if ticks.get(e1 + k)][:5])
    frames_death = None
    for k in range(97):
        mtn, frm, _, hokan, rate = nodes[k]
        assert mtn == death and hokan == 0 and rate == 0, ('md1 10', 'Pass %d: Motion Spalte 3 ohne Mischung erwartet' % k, nodes[k])
        if k == 0:
            assert frm == 0, ('md1 10', 'Pass 0 nicht bei frm_no 0', nodes[0])
        elif frm == 0:
            if frames_death is None:
                frames_death = k  # Bildumbruch: frm_no -= frm_num (Motion.c:103)
            assert nodes[k - 1][1] == (frames_death - 1) * 65536, ('md1 10', 'Pass %d: Umbruch vor dem letzten Bild' % k, nodes[k - 1], nodes[k])
        else:
            assert frm == nodes[k - 1][1] + 65536, ('md1 10', 'Pass %d: frm_no schiebt nicht um ein Bild' % k, nodes[k - 1], nodes[k])
    for k in range(97, 105):
        mtn, frm, _, hokan, rate = nodes[k]
        assert (mtn, frm, hokan, rate) == (25, (k - 97) * 65536, 8 - (k - 97), 49152), ('md1 10', 'Mischpass %d nach der fuenften Explosion (gameover.c:470-478, Motion.c:21)' % (k - 97), nodes[k])
    switch = None
    for k in range(105, 151):
        mtn, frm, _, hokan, rate = nodes[k]
        assert hokan == 0 and rate == 49152, ('md1 10', 'Pass %d: Mischung laeuft laenger als 8 Paesse' % k, nodes[k])
        if nodes[k - 1][0] == 25 and mtn == 26:
            assert frm == 0 and switch is None, ('md1 10', 'Motion 26 nicht beim Bildumbruch von 25 (gameover.c:448)', nodes[k - 1], nodes[k])
            switch = k
        elif frm == 0:
            assert mtn == nodes[k - 1][0] == 26, ('md1 10', 'Umbruch ohne Wechsel 25 -> 26', nodes[k - 1], nodes[k])
        else:
            assert mtn == nodes[k - 1][0] and frm == nodes[k - 1][1] + 65536, ('md1 10', 'Pass %d: Motion/frm_no' % k, nodes[k - 1], nodes[k])
    results['full'] = {'ticks_at_chain': ticks_at_chain, 'expiry_tick': e1, 'scene_first_tick': s,
                       'rm3230_en19_frm_at_%d_%d' % (SCENE_FRAMES, SCENE_DONE_FRAMES): list(scene_frames),
                       'death_motion': death, 'death_motion_frames': frames_death, 'motion_26_pass': switch,
                       'audio_dead_voice': [n for n in final['audio_played'] if n.startswith('ADV/')],
                       'explosion_effects_at_gov_ct_100': {str(i): ids.count(i) for i in (23, 24, 25)},
                       'explosion_pixels': {'gov_ct_45': warm1, 'gov_ct_100': warm5},
                       'unsupported': final.get('audio_unavailable', [])}

    # --- Aufstellung 2: ply_hp 100 -> dmlvl 1, Todesmotion aus der zweiten PlMtnAct-Zeile ------
    t2, c2, e2 = timeline('caution', 2, 100)
    s2 = e2 + 73
    scene2 = run('caution-scene', s2 + 10, replay(2), trace=True)
    assert scene2['gameover_active'] and scene2['gameover_phase'] == 10, ('caution', scene2['gameover_phase'])
    assert scene2['hp'] == 100 and scene2['motion'] == PLMTNACT_DEATH[(0, 1)], ('caution', 'PlMtnAct[0][1][3] (gameover.c:289 liest exp0->dmlvl beim Eintritt)', scene2['hp'], scene2['motion'])
    first = [l for l in scene2['_log'].read_text().splitlines() if l.startswith('POSETRACE')][-10]
    assert first.startswith('POSETRACE mtn=%d frm=0 ' % PLMTNACT_DEATH[(0, 1)]), ('caution', 'erster Knotenteil der Szene', first)
    results['caution'] = {'ticks_at_chain': c2['event_deadline_ticks'], 'expiry_tick': e2, 'scene_first_tick': s2, 'death_motion': scene2['motion']}

    (out / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2))
    print('verify_self_destruct: PASS (Selbstzerstoerung RM_4040 -> RM_3230 -> RM_3091, RM_3230-Szene ohne Skip (EN19 frm_no %d/%d bei Bild %d/%d), Ablauf bei Bild %d, md1 10 ab Bild %d, Motion %d dann 25 (hokan 8..1) %s; Explosionen 23/24/25 = %d/%d/%d bei gov_ct 100, warme Pixel %d/%d bei gov_ct 45/100; Diagnose-Snapshot und Platzierung, --enemy-ai on)' % (
        scene_frames[0], scene_frames[1], SCENE_FRAMES, SCENE_DONE_FRAMES, e1, s, death, 'und 26 ab Pass %d' % switch if switch is not None else 'ohne Umbruch in 150 Paessen',
        ids.count(23), ids.count(24), ids.count(25), warm1, warm5))


if __name__ == '__main__':
    main()
