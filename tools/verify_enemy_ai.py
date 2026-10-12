#!/usr/bin/env python3
"""Gegner-Angriffskette im normalen Spiel: Zombies und Hunde verfolgen, greifen,
beissen; Hebelruetteln, Freigabe, Game Over — headless mit Originalressourcen,
ausdruecklich `--enemy-ai on --enemy-grab on`.

Zombies (RM_0080, fuenf Zombies id 1, kein Skriptereignis vor dem Kontakt):
  natural   Spieler bleibt auf Spawn 0 und tut nichts (keine Diagnoseplatzierung).
            Ein Zombie findet ihn ueber bhSearchPlayer/Brain/MV, greift
            (bhEne01_EatCheck -> Nage, Spielermotion aus der Gegnerbibliothek)
            und beisst (PlyDG00/01: Inventar-HP sinkt).
  front     Diagnose-Aufstellung vor Zombie 0 (`position -3 0 24 0`); zwischen
            zwei Griffen kommt die Steuerung zurueck (player_scripted wechselt).
  lever     Wie `front`, ab Tick 20 jeden Tick eine neue Richtung
            (sys->pad_ps -> bhEne_LeverCheck): messbar weniger HP-Verlust.
  death     `front` ueber 1500 Ticks: HP < 0, Kauen, natives Game Over (Phase 2).
Weitere natuerliche Zombie-Laeufe ohne Diagnoseplatzierung:
  RM_0060   vier Zombies InitType11 — Biss im natuerlichen Lauf.
  RM_0031   Zombie type 9 (bhEne01_InitType02, liegend, mtn 3, HP/2) neben
            normalen Zombies; Griff mit plp->mnwP = epw->mnwP des greifenden Works.
  RM_3070   Zombie type 6 (InitType06, mode2 4 = Kriechvariante) — fruehester
            Raum mit dieser Art in den Raumdaten; natuerlicher Lauf.
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
    'bhSetEffect(17)',                        # bhEne00 (Fass), nicht Teil dieser Kette
    'bhSetMotion:frm_no ausserhalb von frm_num',  # bhGetFrameNum/ChgWalkMtn: Original liest einen Key hinter frm_num
    'bhSetMotion: Motionkanal ohne Modellknoten',  # Motion mit mehr Kanaelen als Knoten: Original schreibt hinter objP
    'bhCheckRoute: keine Wegezone',           # rutchk.c:5 laesst pos uninitialisiert (Hund/Spieler ausserhalb rom->rutp)
    'bhEne04_MV02 rot undefiniert',           # en04.c:1825: rot bleibt im Zweig 0x1000 uninitialisiert
    'en01:Nachbar ohne exp0',                 # en01.c:11440 liest EPP_EXP0 eines skript-inaktiven Works (exp0 NULL)
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


FRONT = '0 position -3 0 24 0\n'
results = {}

# --- Zombies RM_0080 -------------------------------------------------------
natural = run('natural', 'RM_0080.RDX', 1500, '')
assert natural['hp'] < 160, ('natural', 'kein Biss: HP unveraendert', natural['hp'])
assert any(e.get('work', {}).get('mode', [0])[0] in (1, 2) for e in natural['enemies'] if e['identity'] == 1), \
    ('natural', 'kein Zombie im Verfolgungs-/Angriffsmodus')
results['natural'] = {'hp': natural['hp'], 'grabbed_at_end': natural['player_scripted'], 'gaps': natural['_gaps'],
                      'pending_opcodes': natural['_pending_opcodes']}

early = run('front-90', 'RM_0080.RDX', 90, FRONT)
mid = run('front-200', 'RM_0080.RDX', 200, FRONT)
late = run('front-1000', 'RM_0080.RDX', 1000, FRONT)
assert early['player_scripted'], ('front', 'Zombie 0 greift nicht innerhalb von 90 Ticks')
assert early['hp'] < 160, ('front', 'erster Biss fehlt', early['hp'])
assert enemy(early, 0)['work']['mode'][0] == 2, ('front', 'Zombie 0 nicht in mode0 2 (Angriff)', enemy(early, 0)['work'])
assert not mid['player_scripted'], ('front', 'Spieler nach dem ersten Griff nicht freigegeben')
assert mid['hp'] <= early['hp'] and late['hp'] < early['hp'], ('front', 'HP sinkt nicht ueber die Griffe', early['hp'], mid['hp'], late['hp'])
assert not late['gameover_active'], ('front', 'Game Over zu frueh')
results['front'] = {'hp': [early['hp'], mid['hp'], late['hp']], 'gaps': late['_gaps']}

lever_lines = [FRONT.strip()] + [f'{f} keys {"left" if f % 2 else "right"}' for f in range(20, 1200)]
lever = run('lever-1000', 'RM_0080.RDX', 1000, '\n'.join(lever_lines) + '\n')
assert lever['hp'] > late['hp'], ('lever', 'Hebelruetteln senkt den Schaden nicht', lever['hp'], late['hp'])
results['lever'] = {'hp': lever['hp'], 'hp_without_lever': late['hp']}

death = run('death-1500', 'RM_0080.RDX', 1500, FRONT)
assert death['hp'] < 0, ('death', 'Spieler stirbt nicht', death['hp'])
assert death['gameover_active'] and death['gameover_phase'] == 2, ('death', 'Game Over nicht aktiv/Phase 2', death['gameover_phase'])
assert enemy(death, 0)['work']['mode'][3] == 2 or enemy(death, 0)['work']['mode'][0] in (1, 2), ('death', enemy(death, 0)['work'])
results['death'] = {'hp': death['hp'], 'gameover_phase': death['gameover_phase'], 'gaps': death['_gaps']}

# --- natuerliche Zombie-Laeufe in weiteren Raeumen ---------------------------
r60 = run('rm0060-natural', 'RM_0060.RDX', 1500, '')
assert r60['hp'] < 160, ('rm0060', 'kein Biss im natuerlichen Lauf', r60['hp'])
assert all(e['type'] == 11 for e in r60['enemies'] if e['identity'] == 1), ('rm0060', 'InitType11 erwartet')
results['rm0060-natural'] = {'hp': r60['hp'], 'gaps': r60['_gaps']}

r31 = run('rm0031-natural', 'RM_0031.RDX', 1500, '')
assert r31['hp'] < 160, ('rm0031', 'kein Biss im natuerlichen Lauf', r31['hp'])
assert any(e['identity'] == 1 and e['type'] == 9 for e in r31['enemies']), ('rm0031', 'Zombie type 9 fehlt')
results['rm0031-natural'] = {'hp': r31['hp'], 'grabbed_at_end': grabbed(r31), 'gaps': r31['_gaps']}

r3070 = run('rm3070-crawl', 'RM_3070.RDX', 1500, '')
crawler = [e for e in r3070['enemies'] if e['identity'] == 1 and e['type'] == 6]
assert crawler, ('rm3070', 'Zombie type 6 (Kriechvariante) fehlt')
assert crawler[0]['work']['mode'][0] >= 1 and crawler[0]['work']['mode'][2] == 4, ('rm3070', 'Kriecher nicht in mode2 4', crawler[0]['work'])
assert r3070['hp'] < 160, ('rm3070', 'kein Schaden im natuerlichen Lauf', r3070['hp'])
results['rm3070-crawl'] = {'hp': r3070['hp'], 'crawler': crawler[0]['work']['mode'], 'gaps': r3070['_gaps']}

# --- Hunde RM_1000 (natuerlich, keine Eingabe) ------------------------------
d350 = run('dogs-350', 'RM_1000.RDX', 350, '')
d500 = run('dogs-500', 'RM_1000.RDX', 500, '')
d3000 = run('dogs-3000', 'RM_1000.RDX', 3000, '')
assert grabbed(d350) and d350['player_work']['mode'][0] == 4, ('dogs', 'kein Hundegriff (plp->mode0 4) bei Tick 350', d350.get('player_work'))
assert d350['hp'] < 160, ('dogs', 'kein Biss bis Tick 350', d350['hp'])
assert enemy(d350, 1)['work']['mode'][0] == 2, ('dogs', 'Hund 1 nicht in mode0 2 (Nage)', enemy(d350, 1)['work'])
assert not d500['player_scripted'] and d500['player_work']['mtn_enemy'] == 0, ('dogs', 'Spieler nach dem Hundegriff nicht freigegeben', d500.get('player_work'))
assert enemy(d500, 1)['work']['mode'][0] == 1, ('dogs', 'Hund 1 nach der Freigabe nicht im Bewegungsmodus', enemy(d500, 1)['work'])
assert d3000['hp'] < 0 and d3000['gameover_active'] and d3000['gameover_phase'] == 2, ('dogs', 'kein Game Over durch Hunde', d3000['hp'], d3000['gameover_phase'])
assert d3000['player_work']['mode'][0] == 6, ('dogs', 'Todesbiss NG01 (plp->mode0 6) fehlt', d3000['player_work'])
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
trap = ('0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
        '60 position -186 0 86 0\n61 confirm\n'
        '260 position -175.5 8 85.8489 0\n261 confirm\n'
        '550 position -142 0 0 0\n'
        '700 position -131 0 10 0\n')
t = run('rm0050-trap-1100', 'RM_0050.RDX', 1100, trap, trap_extra)
assert 15 in t['events'] or 17 in t['events'], ('rm0050', 'Fallenereignis nicht ausgeloest', t['events'])
dogs = [e for e in t['enemies'] if e['identity'] == 4]
assert len(dogs) == 2 and all(d['work']['mode'][0] >= 1 for d in dogs), ('rm0050', 'Hunde nicht freigelassen', [d['work'] for d in dogs])
assert any(d['work']['mode'][2] == 2 or d['work']['mode'][0] == 2 for d in dogs), ('rm0050', 'kein Hund in Verfolgung/Angriff', [d['work'] for d in dogs])
assert t['hp'] < 160, ('rm0050', 'Hunde beissen nicht', t['hp'])
results['rm0050-trap'] = {'hp': t['hp'], 'dogs': [d['work']['mode'] for d in dogs], 'events': t['events'], 'gaps': t['_gaps']}

(out / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
print('verify_enemy_ai: PASS')
