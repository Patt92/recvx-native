#!/usr/bin/env python3
"""Gegner-Angriffskette im normalen Spiel: Zombies und Hunde verfolgen, greifen,
beissen; Hebelruetteln, Freigabe, Game Over — headless mit Originalressourcen,
ausdruecklich `--enemy-ai on --enemy-grab on`.

Zombies (RM_0080, fuenf Zombies id 1):
  natural   Spieler bleibt auf Spawn 0 (pos_no 0 wie die Originaltuer aus RM_0050)
            und tut nichts: das Raumskript setzt Zombies 0..2 per Sub_controll
            0x8f auf Typ 8 (ruhen bis Schaden), 3/4 warten in bhEne_Event.
            Kein Biss; die Angriffskette pruefen front/lever/death/RM_0060.
  front     Diagnose-Aufstellung vor Zombie 0 (`position -3 0 24 0`); zwischen
            zwei Griffen kommt die Steuerung zurueck (player_scripted wechselt).
  lever     Wie `front`, ab Tick 20 jeden Tick eine neue Richtung
            (sys->pad_ps -> bhEne_LeverCheck): messbar weniger HP-Verlust.
  death     `front` ueber 1800 Ticks: HP < 0, Kauen, natives Game Over (Phase 2).
Weitere natuerliche Zombie-Laeufe ohne Diagnoseplatzierung:
  RM_0060   vier Zombies InitType11 — Biss im natuerlichen Lauf.
  RM_0031   Zombies mit Skriptzustand Sub_controll 0x93 (Typ 10, mode0 5) neben
            normalen Zombies; Griff mit plp->mnwP = epw->mnwP des greifenden Works.
  RM_3070   Zombie type 6 (InitType06, mode2 4 = Kriechvariante) — fruehester
            Raum mit dieser Art in den Raumdaten; natuerlicher Lauf, Zombie 1
            per Skript Typ 8 (kein Biss ohne Eingabe).
Hunde (EN04):
  RM_1000   drei Hunde, einer type 0 aktiv: Griff (bhEne04_NG00, PlyDG00:
            Spielermotion aus der Hundebibliothek), Biss (HP -8 je Durchlauf),
            Freigabe (PlyDG00 Zweig 5 nach bhSetMotion(plp) hokan_count == 0),
            zweiter Angriff, schliesslich NG01 (plp->mode0 6), HP < 0, Game Over.
  RM_0050   Fallenereignis 15/17 (Diagnose: Snapshot ev_flg 117 + Vorhaengeschloss
            wie tools/verify_event_freeze.py, Platzierungen), danach greifen die
            freigelassenen Hunde den Spieler in den Wegezonen an.

Jeder Lauf verlangt 0 blockierte Skriptbefehle. Gemeldete Gegner-Luecken werden
gegen eine erlaubte Liste (Praefix) geprueft; jede andere Luecke ist ein Fehler,
damit kein stilles Abbrechen der Originalroutinen durchrutscht. Die erlaubten
Eintraege sind nachgewiesene Originaldefekte oder Mechanismen ausserhalb der
Kette (siehe work/native_port_enemy_ai.result.md).
"""
import argparse
import json
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ALLOWED_GAPS = (
    # Motion.c:131 liest den Key bei frm_no>>16 ohne Grenze gegen frm_num. Die
    # Nachbarkeys aus demselben MTN-Bereich werden nativ angehaengt
    # (decode_motion_asset, tail_keys); diese Meldung steht nur noch, wenn die
    # Keytafel am Rand ihres eigenen Bereichs endet.
    'bhSetMotion:frm_no ',
    'bhSetMotion: Motionkanal ohne Modellknoten',  # Motion mit mehr Kanaelen als Knoten: Original schreibt hinter objP
    'bhCheckRoute: keine Wegezone',           # rutchk.c:5 laesst pos uninitialisiert (Hund/Spieler ausserhalb rom->rutp)
    'en01:Nachbar ohne exp0',                 # en01.c:11440 liest EPP_EXP0 eines skript-inaktiven Works (exp0 NULL)
    # Erster Pass eines Raums: plp->mdl[i].owP kommt aus bhGetFreeMemory
    # (dread.c:161) und wird nicht geloescht, bhMlbBinRealize setzt den Zeiger
    # nur auf NULL (binfunc.c:28). Vor dem ersten bhCalcModel(plp) liest das
    # Original damit nicht initialisierten Speicher; nativ wird gemeldet statt
    # eine Pose zu erfinden. Gemessen nur im Tick 0 (danach 22 Knoten).
    'plp->mlwP->owP[0] (en01.c:2840)',
    'plp->mlwP->owP vor bhCalcModel(plp)',
    # Derselbe erste Pass in bhEne04_MVType00..02 (en04.c:1061/1126/1160): owP[1]
    # des Spielers vor bhCalcModel(plp); gemessen ab Bild 1 in RM_1000.
    'plp->mlwP->owP[1] (en04.c:1061)',
    # RM_0020: Gegner 4 ist id 91 (Maskenzombie); eneset.c:376 bhControlMask
    # (face.c) ist nicht portiert und liegt ausserhalb der Angriffskette.
    'bhControlMask',
    # RM_0020 Helikopterszene: bhEff352 (effsub) ist nicht portiert; nur gemeldet.
    'bhEff352',
)

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
RUNTIME = str(args.runtime.resolve())
DATA = str(args.data.resolve())


def run(name, room, frames, replay, extra=(), ai=True):
    state = out / (name + '.json')
    log = out / (name + '.log')
    script = out / (name + '.txt')
    script.write_text(replay)
    if state.exists():
        state.unlink()
    command = [RUNTIME, '--data', DATA, '--mods', str(out / 'no-mods'),
               '--room', room, '--headless', '--in-game', '--frames', str(frames), '--ss', '1',
               '--replay', str(script.resolve()), '--state', str(state.resolve())] + list(extra)
    if ai:
        command += ['--enemy-ai', 'on', '--enemy-grab', 'on']
    with log.open('w') as stream:
        done = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    if done.returncode:
        raise RuntimeError(f'{name}: runtime exit {done.returncode}\n' + log.read_text())
    text = log.read_text()
    gaps = set()
    if 'Enemy gaps:' in text:
        for line in text.split('Enemy gaps:', 1)[1].splitlines()[1:]:
            if not line.startswith('  '):
                break
            gaps.add(line.strip())
    unexpected = sorted(g for g in gaps if not any(g.startswith(a) for a in ALLOWED_GAPS))
    assert not unexpected, (name, 'unerwartete Gegner-Luecken', unexpected)
    value = json.loads(state.read_text())
    assert value['blocked_commands'] == 0, (name, 'blocked commands', value['blocked_commands'])
    value['_gaps'] = sorted(gaps)
    value['_pending_opcodes'] = sorted({c['opcode'] for c in value['pending_commands']})
    return value


def enemy(state, index):
    for entry in state['enemies']:
        if entry['index'] == index:
            return entry
    raise AssertionError(f'enemy {index} missing from state')


def grabbed(state):
    """plp->mode0 4/6 with the enemy library installed (plp->mnwP = epw->mnwP)."""
    pw = state.get('player_work') or {}
    return state['player_scripted'] and pw.get('mode', [0])[0] in (4, 6) and pw.get('mtn_enemy') == 1


# Seit Sub_controll 0x8f den nativen Work erreicht (1c2934d), ruhen die Zombies
# bei pos_no 0 als Typ 8 (siehe `natural`). Die Diagnose-Aufstellung nutzt
# deshalb Spawn 2 (pos_no 2 -> Skriptzweig 0x8f type 0, Zombies aktiv) und steht
# knapp 3 Einheiten vor Zombie 0 an seiner Skriptlage: RM_0080 ev 0 off 1292-1312
# Common 12/7/11 -> (-3, 0, 21), ay 180 (event.c:12034/12068), danach Common 47
# bhCalcModel (event.c:10053). Die frueher hier stehende Lage (4.76, 0, -37.16)
# war die Raumtabellenlage, auf die der Work mangels Common 47 per
# bhCalcFixOffset zurueckgezogen wurde (enemies_bats_bander_dogs-20261014).
FRONT = '0 position -3 0 24 0\n'
FRONT_SPAWN = ['--spawn', '2']
results = {}
REPO = Path(__file__).resolve().parent.parent

# --- Friedhof RM_0020: Helikopter-Kriecher (Gegner 3) ----------------------
# Soll aus dem RM_0020-Skript (scd0-Init, base-rel 262-402, ev_flg 104 == 0):
#   bhInitModelSet 1/0,1,2,5/0 -> stflg 0x1000000 (event.c:2847) fuer die vier
#   Friedhofszombies, 1/4/0 fuer den Maskenzombie 4 (id 91); Gegner 3 (ed_flg 4
#   == 0): bhLoadWork, Common 12/7 (0,0,0)/13/11, Common 23 (0: flg &= ~0x118,
#   event.c:11981), Common 46 (0: flg &= ~0x20, :10362), Common 24 (Raum-MTN 4,
#   mode0 5 = bhEne_Event, ct1 0 = sofortiges bhSetMotion/bhCalcModel/
#   bhCalcLinkModel event.c:11601, ct2 6). Nach der Szene (ev 11/12): Common 25
#   (Bibliothek 40, mode0 5) als Kriechen ab (141,0,130), Common 23/46 (1),
#   Sub_controll 0x8f (event.c:9804) -> mode0 1 (bhEne01_Move); bhInitModelSet
#   1/0,1,2,5/1 weckt die vier Zombies, 1/4/0 deaktiviert den Maskenzombie.
# Ohne Greifen, weil die Zombies nach der Szene den Spieler sonst toeten und das
# native Game Over den Raum neu laedt (gemessen: Bild 2600 frisch initialisiert).
GRAB_OFF = ['--enemy-ai', 'on', '--enemy-grab', 'off']


def crawler_waits(state, name):
    for idx in (0, 1, 2, 5):
        w = enemy(state, idx)['work']
        assert w['stflg'] & 0x1000000, (name, 'Zombie', idx, 'nicht per bhInitModelSet deaktiviert', w)
    w4 = enemy(state, 4)['work']
    assert w4['stflg'] & 0x1000000, (name, 'Maskenzombie 4 nicht deaktiviert', w4)
    w3 = enemy(state, 3)['work']
    assert not (w3['stflg'] & 0x1000000), (name, 'Kriecher faelschlich deaktiviert', w3)
    assert w3['mode'][0] == 5 and w3['mode'][1] == 4 and w3['mtn_no'] == 4, (name, 'Kriecher nicht in bhEne_Event mit Raum-MTN 4', w3)
    assert all(abs(v) < .01 for v in w3['pos']), (name, 'Kriecher nicht am Skriptursprung (0,0,0), sondern in der KI unterwegs', w3)
    assert not (w3['flg'] & 0x118) and not (w3['flg'] & 0x20), (name, 'Common 23/46 (0) nicht auf dem Work', w3)
    assert not any(c['opcode'] == 0x69 and c['event'] == 0 for c in state['pending_commands']), \
        (name, 'Common_controll der Init-Szene ungebunden', state['_pending_opcodes'])


entry = run('graveyard-entry', 'RM_0020.RDX', 30, '', GRAB_OFF, ai=False)
crawler_waits(entry, 'graveyard-entry')
# Natuerlicher Tuereintritt RM_0050 -> RM_0030 -> RM_0020 (tools/verify_continuity.py RETURN).
door = run('graveyard-door', 'RM_0050.RDX', 1100,
           '60 position -33 0 110 16384\n61 confirm\n180 confirm\n500 skip\n700 position 32.5 0 70 49152\n701 confirm\n750 skip\n',
           GRAB_OFF, ai=False)
assert door['room'] == 'RM_0020.RDX' and door['room_changes'] == 2, ('graveyard-door', door['room'], door['room_changes'])
crawler_waits(door, 'graveyard-door')
WALK = (REPO / 'tests/native/replays/graveyard-walk.txt').read_text()
crawl = run('graveyard-crawl', 'RM_0020.RDX', 1000, WALK, GRAB_OFF, ai=False)
assert crawl['cinematic'] and crawl['event_flags'][3] & 0x800000, ('graveyard-crawl', 'Heli-Szene/ev_flg 104 bei Bild 1000', crawl['cinematic'])
w3 = enemy(crawl, 3)['work']
assert w3['mode'][0] == 5 and w3['mtn_no'] == 40 and not (w3['stflg'] & 0x1000000), ('graveyard-crawl', 'Kriechmotion 40 (Common 25, mode0 5) nicht aktiv', w3)
assert 120 < w3['pos'][0] < 150 and 120 < w3['pos'][2] < 140, ('graveyard-crawl', 'Kriecher nicht an der Skriptlage um (141,0,130)', w3)
after = run('graveyard-after', 'RM_0020.RDX', 1200, WALK, GRAB_OFF, ai=False)
assert not after['cinematic'] and not after['event_tasks'], ('graveyard-after', 'Szene nicht beendet', after['event_tasks'])
for idx in (0, 1, 2, 3, 5):
    w = enemy(after, idx)['work']
    assert not (w['stflg'] & 0x1000000) and w['mode'][0] == 1, ('graveyard-after', 'Zombie', idx, 'nicht in bhEne01_Move', w)
w3 = enemy(after, 3)['work']
assert (w3['flg'] & 0x118) == 0x118 and (w3['flg'] & 0x20), ('graveyard-after', 'Common 23/46 (1) vor Sub_controll 0x8f fehlt', w3)
assert enemy(after, 4)['work']['stflg'] & 0x1000000, ('graveyard-after', 'Maskenzombie 4 nach der Szene aktiv', enemy(after, 4)['work'])
results['graveyard'] = {'entry': enemy(entry, 3)['work'], 'door': enemy(door, 3)['work'], 'crawl': enemy(crawl, 3)['work'],
                        'after': {e['index']: e['work'] for e in after['enemies']}, 'gaps': sorted(set(after['_gaps']) | set(door['_gaps']))}

# --- Zombies RM_0080 -------------------------------------------------------
natural = run('natural', 'RM_0080.RDX', 1500, '')
# RM_0080-Skript (ev 0, off 1334/1436/1538): `06 0f 00 00` (bhCmpB pos_no == 0)
# -> Common 46 (0), Sub_controll 0x8f type 8; sonst 0x8f type 0. Auch die
# Originaltuer RM_0050 -> RM_0080 liefert pos_no 0. Typ 8 ueberspringt Brain/
# ActionModeCheck (en01.c:4873) und wird erst durch Schaden zu Typ 0
# (en01.c:3218). Die Zombies 3/4 warten in bhEne_Event (mode0 5). Ohne
# Spielereingabe bleibt der Raum deshalb original ruhig; vor 1c2934d erreichte
# Sub_controll den nativen Work nicht, daher die fruehere Biss-Erwartung.
for idx in (0, 1, 2):
    e = enemy(natural, idx)
    assert e['type'] & 0xff == 8 and e['work']['mode'][0] == 1, ('natural', 'Zombie', idx, 'nicht original Typ 8 (pos_no 0)', e)
for idx in (3, 4):
    assert enemy(natural, idx)['work']['mode'][0] == 5, ('natural', 'Zombie', idx, 'nicht in bhEne_Event', enemy(natural, idx))
assert natural['hp'] == 160 and not natural['player_scripted'], ('natural', 'Biss trotz ruhender Typ-8-Zombies', natural['hp'])
results['natural'] = {'hp': natural['hp'], 'grabbed_at_end': natural['player_scripted'], 'gaps': natural['_gaps'],
                      'pending_opcodes': natural['_pending_opcodes']}

early = run('front-90', 'RM_0080.RDX', 90, FRONT, FRONT_SPAWN)
# Bild 120 liegt zwischen erstem Griff und Folgegriff (gemessen 90/120/160).
mid = run('front-120', 'RM_0080.RDX', 120, FRONT, FRONT_SPAWN)
# Bei Spawn 2 greifen drei aktive Zombies nacheinander; Bild 600 liegt vor dem
# Game Over (gemessen HP 50), der Tod selbst ist `death`.
late = run('front-600', 'RM_0080.RDX', 600, FRONT, FRONT_SPAWN)
assert early['player_scripted'], ('front', 'Zombie 0 greift nicht innerhalb von 90 Ticks')
assert early['hp'] < 160, ('front', 'erster Biss fehlt', early['hp'])
assert enemy(early, 0)['work']['mode'][0] == 2, ('front', 'Zombie 0 nicht in mode0 2 (Angriff)', enemy(early, 0)['work'])
assert not mid['player_scripted'], ('front', 'Spieler nach dem ersten Griff nicht freigegeben')
assert mid['hp'] <= early['hp'] and late['hp'] < early['hp'], ('front', 'HP sinkt nicht ueber die Griffe', early['hp'], mid['hp'], late['hp'])
assert not late['gameover_active'], ('front', 'Game Over zu frueh')
results['front'] = {'hp': [early['hp'], mid['hp'], late['hp']], 'gaps': late['_gaps']}

lever_lines = [FRONT.strip()] + [f'{f} keys {"left" if f % 2 else "right"}' for f in range(20, 700)]
lever = run('lever-600', 'RM_0080.RDX', 600, '\n'.join(lever_lines) + '\n', FRONT_SPAWN)
assert lever['hp'] > late['hp'], ('lever', 'Hebelruetteln senkt den Schaden nicht', lever['hp'], late['hp'])
results['lever'] = {'hp': lever['hp'], 'hp_without_lever': late['hp']}

# An der Skriptlage (-3, 0, 21) erreicht der Tod Phase 2 gemessen bei Bild 1600
# (Phase 1 bei 1500); 1800 laesst Abstand, die Pruefung bleibt Tod + Phase 2.
death = run('death-1800', 'RM_0080.RDX', 1800, FRONT, FRONT_SPAWN)
assert death['hp'] < 0, ('death', 'Spieler stirbt nicht', death['hp'])
# Griff-Tod: NG00 case 2 (en01.c:6861) setzt plp->mode0 6 bei hp < 0, PlyDG00 case 6/7 spielt die
# Todesmotion Offset+12/13 (412) und setzt plp->flg 0x2 erst an ihrem letzten Bild (:7748);
# bhCPM0_enedie (player.c:6711) startet das Game Over dann, nicht schon mit hp < 0.
dying = run('death-1050', 'RM_0080.RDX', 1050, FRONT, FRONT_SPAWN)
assert dying['hp'] < 0 and dying['player_work']['mode'][0] == 6 and dying['player_work']['mtn_no'] == 412 and not dying['gameover_active'], ('death', 'Game Over vor dem Ende der Todesmotion (plp->flg 0x2, en01.c:7748)', dying['player_work'], dying['gameover_active'])
assert death['gameover_active'] and death['gameover_phase'] == 2, ('death', 'Game Over nicht aktiv/Phase 2', death['gameover_phase'])
assert enemy(death, 0)['work']['mode'][3] == 2 or enemy(death, 0)['work']['mode'][0] in (1, 2), ('death', enemy(death, 0)['work'])
results['death'] = {'hp': death['hp'], 'gameover_phase': death['gameover_phase'], 'gaps': death['_gaps']}

# --- natuerliche Zombie-Laeufe in weiteren Raeumen ---------------------------
r60 = run('rm0060-natural', 'RM_0060.RDX', 1500, '')
assert r60['hp'] < 160, ('rm0060', 'kein Biss im natuerlichen Lauf', r60['hp'])
assert all(e['type'] == 11 for e in r60['enemies'] if e['identity'] == 1), ('rm0060', 'InitType11 erwartet')
results['rm0060-natural'] = {'hp': r60['hp'], 'gaps': r60['_gaps']}

# Griffende mit spielergebundenem Effekt. bhEne01_PlyDG00 Zweig 3 (en01.c:7625-
# 7637) gibt plp->mnwP = plp->mnwPb zurueck und laesst plp->mtn_no auf dem
# Zombie-Eintrag En01_PlyMtn_OffsetTbl[0] + 10/11 = 410/411 stehen, bis
# bhControlPlayer ihn im selben Bild ueberschreibt (game.c:47). bhControlEffect
# (game.c:48) liest danach pp->mlwP->owP[op->lkono].mtx (effect.c:803-811).
# Loeste der Port stattdessen plp->mnwP[plp->mtn_no] auf, brach der Lauf im
# Freigabebild mit "actor motion unavailable" ab (Nutzermeldung Friedhof).
# Diagnose dieses Falls: das Debugmenue ruestet das Feuerzeug (Item 55) aus,
# damit bhEff009 als spielergebundene Arbeit (link 1, Knoten 9) im Effektpass
# liegt; keine Positionierung, keine Flags.
lamp = '0 debug_key F2\n1 debug_select equip 55\n2 confirm\n4 debug_key F2\n'
# Bild 1150 endet zwischen zwei Griffen: der Spieler lebt, ist freigegeben, und
# bhControlPlayer hat plp->mtn_no bereits zurueckgeholt. (2026-10-14: der
# Folgegriff beginnt vor Bild 1200, gleich mit dem Vorher-Binary; Fenster
# gemessen 1100/1150 frei, 1200 gegriffen.)
r60l = run('rm0060-lighter-release', 'RM_0060.RDX', 1150, lamp)
assert r60l['weapon'] == 1 and any(e['id'] == 9 and e['type'] == 2 and e['visible'] for e in r60l['effects']), \
    ('rm0060-lighter', 'kein sichtbarer spielergebundener bhEff009 im Effektpass', r60l['weapon'], r60l['effects'])
assert r60l['hp'] < 160, ('rm0060-lighter', 'kein Biss', r60l['hp'])
assert not r60l['player_scripted'], ('rm0060-lighter', 'Lauf endet nicht nach einer Freigabe', r60l.get('player_work'))
assert r60l['motion'] < 400, ('rm0060-lighter', 'bhControlPlayer holt plp->mtn_no nach der Freigabe nicht zurueck', r60l['motion'])
results['rm0060-lighter-release'] = {'hp': r60l['hp'], 'motion': r60l['motion'], 'gaps': r60l['_gaps']}

# Derselbe Raum ueber mehrere vollstaendige Griff-/Freigabefolgen bis zum Tod.
# `run` wirft bei jedem Abbruch der Laufzeit, der Fall ist also die eigentliche
# Absturzsicherung: der Effektpass loest den Spielerlink in jedem dieser Bilder
# auf. plp->mtn_no bleibt hier zu Recht auf dem Zombie-Eintrag stehen, weil
# bhControlPlayer nach dem Tod nicht mehr laeuft — darum keine Motionzusage.
r60d = run('rm0060-lighter-death', 'RM_0060.RDX', 3000, lamp)
assert r60d['hp'] < 0 and r60d['gameover_active'], ('rm0060-lighter-death', 'kein Tod ueber die Griffkette', r60d['hp'], r60d['gameover_active'])
assert any(e['id'] == 9 and e['type'] == 2 for e in r60d['effects']), \
    ('rm0060-lighter-death', 'spielergebundener bhEff009 ueberlebt die Griffkette nicht', r60d['effects'])
results['rm0060-lighter-death'] = {'hp': r60d['hp'], 'motion': r60d['motion'], 'gaps': r60d['_gaps']}

r31 = run('rm0031-natural', 'RM_0031.RDX', 1500, '')
assert r31['hp'] < 160, ('rm0031', 'kein Biss im natuerlichen Lauf', r31['hp'])
# Das Raumskript schreibt im ersten Bild Sub_controll 0x93 (event.c:9889: mode0..3,
# Typbyte) mit Typ 10 und mode0 5 auf die Zombies 1/4 (EVTRACE off 748/806/1354);
# der Spawn-Typ 9 ist danach nicht mehr sichtbar (vor 1c2934d erreichte
# Sub_controll den nativen Work nicht). Zombie 4 wird spaeter skriptseitig auf
# Typ 0 freigegeben und greift an.
z4 = enemy(r31, 4)
assert enemy(r31, 1)['type'] == 10 and enemy(r31, 1)['work']['mode'][0] == 5, ('rm0031', 'Zombie 1 nicht im Skriptzustand 0x93', enemy(r31, 1))
assert z4['type'] == 0 and z4['work']['mode'][0] in (1, 2), ('rm0031', 'Zombie 4 nicht freigegeben', z4)
results['rm0031-natural'] = {'hp': r31['hp'], 'grabbed_at_end': grabbed(r31), 'gaps': r31['_gaps']}

r3070 = run('rm3070-crawl', 'RM_3070.RDX', 1500, '')
crawler = [e for e in r3070['enemies'] if e['identity'] == 1 and e['type'] == 6]
assert crawler, ('rm3070', 'Zombie type 6 (Kriechvariante) fehlt')
assert crawler[0]['work']['mode'][0] >= 1 and crawler[0]['work']['mode'][2] == 4, ('rm3070', 'Kriecher nicht in mode2 4', crawler[0]['work'])
# Erstbesuch: Skriptblock off 536 (bhCk ed_flg 125 == 0, rm_flg 2 == 0) ->
# Common 46 (0), Sub_controll 0x8f Typ 8 fuer Zombie 1 (ruht bis Schaden,
# en01.c:3218/4873). Der Biss vom 2026-10-12 kam von diesem Zombie, solange
# Sub_controll den nativen Work nicht erreichte; Kriecher (MV14 haelt, Brain02
# nur mit mode1 & 1) und Zombie 2 erreichen den Spieler ohne Eingabe nicht.
assert enemy(r3070, 1)['type'] == 8, ('rm3070', 'Zombie 1 nicht im Skriptzustand Typ 8', enemy(r3070, 1))
results['rm3070-crawl'] = {'hp': r3070['hp'], 'crawler': crawler[0]['work']['mode'], 'gaps': r3070['_gaps']}

# --- Hunde RM_1000 (natuerlich, keine Eingabe) ------------------------------
d350 = run('dogs-350', 'RM_1000.RDX', 350, '')
d500 = run('dogs-500', 'RM_1000.RDX', 500, '')
d2520 = run('dogs-2520', 'RM_1000.RDX', 2520, '')
d2560 = run('dogs-2560', 'RM_1000.RDX', 2560, '')
d2595 = run('dogs-2595', 'RM_1000.RDX', 2595, '')
d3000 = run('dogs-3000', 'RM_1000.RDX', 3000, '')
assert grabbed(d350) and d350['player_work']['mode'][0] == 4, ('dogs', 'kein Hundegriff (plp->mode0 4) bei Tick 350', d350.get('player_work'))
assert d350['hp'] < 160, ('dogs', 'kein Biss bis Tick 350', d350['hp'])
assert enemy(d350, 1)['work']['mode'][0] == 2, ('dogs', 'Hund 1 nicht in mode0 2 (Nage)', enemy(d350, 1)['work'])
assert not d500['player_scripted'] and d500['player_work']['mtn_enemy'] == 0, ('dogs', 'Spieler nach dem Hundegriff nicht freigegeben', d500.get('player_work'))
assert enemy(d500, 1)['work']['mode'][0] == 1, ('dogs', 'Hund 1 nach der Freigabe nicht im Bewegungsmodus', enemy(d500, 1)['work'])
assert d3000['hp'] < 0 and d3000['gameover_active'] and d3000['gameover_phase'] == 2, ('dogs', 'kein Game Over durch Hunde', d3000['hp'], d3000['gameover_phase'])
# Zweiter Sprungangriff bei Bild ~2497 (hp 0 -> -6): Fallmotion 25 laeuft ohne Game Over, erst ihr
# Bildwechsel (~2537) loescht ts_flg 0x4000 und startet bhSelectContinue (bhCPM0_die mode3 1).
assert d2520['hp'] < 0 and d2520['player_work']['mode'][0] == 3 and d2520['player_work']['mtn_no'] == 25 and not d2520['gameover_active'], ('dogs', 'Game Over vor dem Ende der Fallmotion (player.c:6422)', d2520['player_work'], d2520['gameover_active'])
assert d2560['gameover_active'] and d2560['gameover_phase'] == 0 and d2560['player_work']['mode'][3] == 2 and d2560['player_work']['mtn_no'] == 26, ('dogs', 'Liegemotion 26 in gov_md1 0 fehlt', d2560['player_work'], d2560['gameover_phase'])
# main.c:197: Task 7 laeuft in den 70 Bildern von gov_md1 0 weiter (nur Task 8 gesperrt, sp_flg
# unveraendert): die Hunde bewegen sich zwischen zwei Bildern dieser Phase.
assert d2595['gameover_active'] and d2595['gameover_phase'] == 0, ('dogs', 'Bild 2595 nicht mehr in gov_md1 0', d2595['gameover_phase'])
moved = [(enemy(d2560, i)['work']['mtn_no'], enemy(d2560, i)['work']['frm_no'], enemy(d2560, i)['position']) != (enemy(d2595, i)['work']['mtn_no'], enemy(d2595, i)['work']['frm_no'], enemy(d2595, i)['position']) for i in (0, 1, 2)]
assert all(moved), ('dogs', 'Gegner stehen in gov_md1 0 still (Task 7 muss weiterlaufen)', moved)
# Die Hunde setzen nur plp->mode0 4 und klemmen beide Bisse auf hp 1 (en07.c:838/922, :899/:1005);
# der Tod kommt aus dem Sprungangriff (Schadensmodus 2). hp < 0 schaltet player.c:1405 nach
# bhCPM0_die (mode0 3): Fallmotion 25, beim Bildwechsel Liegemotion 26 (mode3 2) und ts_flg 0x4000
# -> gov_md0 0. Ein mode0 6 (bhCPM0_enedie) hat fuer EN07 keine Originalquelle.
assert d3000['player_work']['mode'][0] == 3 and d3000['player_work']['mode'][3] == 2 and d3000['player_work']['mtn_no'] == 26, ('dogs', 'bhCPM0_die (plp->mode0 3, Liegemotion 26) fehlt', d3000['player_work'])
results['dogs-rm1000'] = {'hp': [d350['hp'], d500['hp'], d3000['hp']], 'gameover_phase': d3000['gameover_phase'], 'gaps': d3000['_gaps']}

# --- Hunde RM_0050: Fallenereignis (Diagnose-Snapshot wie verify_event_freeze) --
subprocess.run([RUNTIME, '--write-controls', str(out / 'controls.ini')], check=True, stdout=subprocess.DEVNULL)
shutil.copyfile(Path(__file__).resolve().parents[1] / 'debug.ini', out / 'debug.ini')
trap_extra = ['--controls', str(out / 'controls.ini'), '--debug-config', str(out / 'debug.ini'), '--snapshots', str(out / 'snapshots')]
run('rm0050-seed', 'RM_0050.RDX', 4, '0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n', trap_extra, ai=False)
b = bytearray((out / 'snapshots/snapshot-1.rcvxsnap').read_bytes())
assert b[:8] == b'RCVXSNAP' and struct.unpack_from('<II', b, 8) == (1, 0x838)
at = 16 + 0x14 + (117 // 32) * 4
struct.pack_into('<I', b, at, struct.unpack_from('<I', b, at)[0] | (0x80000000 >> (117 % 32)))
struct.pack_into('<I', b, 16 + 0x1f4 + 2 * 4, (84 << 16) | 1)
struct.pack_into('<I', b, len(b) - 4, zlib.crc32(b[:-4]))
(out / 'snapshots/snapshot-2.rcvxsnap').write_bytes(b)
# scd0 @708: bhRandamSet 10 (event.c:4930) und bhCmpB 17 wuerfeln rm_flg 9; gesetzt -> Falle an
# flr 23 (x -194, Ereignis 16), frei -> flr 7 (x -142, Ereignis 15) (siehe verify_event_freeze.py).
# Die Lockposition folgt dem Wurf; eine feste -142 traf nur den freien Fall.
up = run('rm0050-up', 'RM_0050.RDX', 240, '0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
         '60 position -186 0 86 0\n61 confirm\n', trap_extra)
rm9 = bool(up['work_flags'][4] & (0x80000000 >> 9))
trap_event, lure = (16, -191) if rm9 else (15, -142)
trap = ('0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
        '60 position -186 0 86 0\n61 confirm\n'
        '260 position -175.5 8 85.8489 0\n261 confirm\n'
        '550 position %d 0 0 0\n'
        '700 position %d 0 10 0\n' % (lure, lure + 11))
t = run('rm0050-trap-1100', 'RM_0050.RDX', 1100, trap, trap_extra)
assert trap_event in t['events'], ('rm0050', 'Fallenereignis nicht ausgeloest', trap_event, rm9, t['events'])
dogs = [e for e in t['enemies'] if e['identity'] == 4]
assert len(dogs) == 2 and all(d['work']['mode'][0] >= 1 for d in dogs), ('rm0050', 'Hunde nicht freigelassen', [d['work'] for d in dogs])
# Verfolgung/Angriff: MV02 (en04.c:1792), MV07 Angriffswahl (:2205) und MV08 Sprungangriff (:2415,
# EXP0 0x800, Sprunghoehe 2.2) oder Griff mode0 2. Seit der Headless-Lauf jeden Takt zeichnet
# (bhEff106-Regen zieht rand() wie im Spiel), liegt Bild 1100 in einem Sprung (MV08).
assert any(d['work']['mode'][2] in (2, 7, 8) or d['work']['mode'][0] == 2 for d in dogs), ('rm0050', 'kein Hund in Verfolgung/Angriff', [d['work'] for d in dogs])
assert t['hp'] < 160, ('rm0050', 'Hunde beissen nicht', t['hp'])
results['rm0050-trap'] = {'hp': t['hp'], 'dogs': [d['work']['mode'] for d in dogs], 'events': t['events'], 'gaps': t['_gaps']}

(out / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
print('verify_enemy_ai: PASS')
