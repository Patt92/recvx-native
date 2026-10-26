#!/usr/bin/env python3
"""Hinweise fuer Agenten, die beim naechsten Schritt nicht weiterwissen: Raumskript und Code gezielt lesen.

Eskalation (billig -> teuer): Zonentext lesen -> `hint.py zone` -> `hint.py blocked` -> `hint.py op/grep` -> Screenshot.
Liest nur: data/rooms/*.RDT, native/ und die Semantikreferenz src/ps2/veronica/prog/ (nie archived/).

  hint.py room   [auto.json]          Was gibt es im aktuellen Raum? je Zone: Meldungstext oder Skriptstelle
  hint.py zone i [auto.json]          Was passiert bei Zone i (Meldung, Skriptblock mit Bedingungen, Flags jetzt)
  hint.py blocked x z [auto.json]     Warum kein Weg? welche Wand sperrt, welche Skriptstellen sie oeffnen
  hint.py event n [room]              Raumereignis n dekodiert (Namen, Texte, Items, Flags)
  hint.py msg n [room]                Raummeldung n
  hint.py op <hex|bhName>             wo ein Skriptbefehl implementiert ist (Port und Originalreferenz)
  hint.py grep <regex>                begrenzte Suche in native/ und src/ps2/veronica/prog/ (max 40 Treffer)
"""
import json, os, re, struct, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools')); sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import walk_event_scripts as W

ORIG_EVENT = os.path.join(ROOT, 'src/ps2/veronica/prog/event.c')
SEARCH = [os.path.join(ROOT, 'native'), os.path.join(ROOT, 'src/ps2/veronica/prog')]


def opnames():
    s = open(ORIG_EVENT).read(); i = s.index('(*bhScenarioJmpT[256])'); j = s.index('{', i); k = s.index('};', j)
    body = re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', '', s[j + 1:k], flags=re.S))
    return [t.strip() for t in body.split(',') if t.strip()]


NAMES = opnames()


def item_names():
    out = {}
    for line in open(os.path.join(ROOT, 'docs/INVENTORY-COVERAGE.md')):
        m = re.match(r'\| (\d+) \| ([^|]+?) \| \d+ \|', line)
        if m and int(m.group(1)) not in out: out[int(m.group(1))] = m.group(2)
        if line.startswith('## ') and out: break
    return out


ITEMS = item_names()


def load_state(path):
    s = open(path).read()
    try: return json.loads(s)
    except json.JSONDecodeError: return json.loads(re.sub(r'[\x00-\x1f]', ' ', s))


class Room:
    def __init__(self, name):
        self.name = name[:7]
        self.d = open(os.path.join(ROOT, 'data/rooms', self.name + '.RDT'), 'rb').read()
        u32 = lambda o: struct.unpack_from('<I', self.d, o)[0]; self.u32 = u32
        ev = u32(28); header = u32(ev); self.ev = ev
        self.starts = [('scd0', ev + header), ('scd1', ev + u32(ev + 4))] + [(f'event{i}', ev + u32(ev + p)) for i, p in enumerate(range(8, header, 4))]
        self.bounds = sorted(set(o for _, o in self.starts)) + [len(self.d)]
        ro = u32(16); self.msg = u32(ro + 0x38)

    def message(self, n):
        d, t = self.d, self.msg; cnt = self.u32(t)
        if n >= cnt: return f'<Meldung {n} ausserhalb>'
        p = t + self.u32(t + 4 + 4 * n); out = []
        for _ in range(4096):
            c = struct.unpack_from('<H', d, p)[0]; p += 2
            if c == 0xffff: break
            if c == 0xff00: out.append(' | ')
            elif c == 0xff01: out.append(' ')
            elif c in (0xff02, 0xff03): p += 2; out.append(' | ' if c == 0xff02 else '')
            elif c < 108: out.append(chr(c + 32))
        return re.sub(r'\s+', ' ', ''.join(out)).strip(' |')

    def commands(self, script):
        start = dict(self.starts)[script]; end = self.bounds[self.bounds.index(start) + 1]; p = start; depth = 0
        while p < end:
            n = W.length(self.d, p)
            if n is None: return
            op = self.d[p]
            if op in (0x03, 0xfb): depth -= 1
            yield p - self.ev, depth, bytes(self.d[p:p + n])
            if op in (0x01, 0xfa): depth += 1
            if op == 0x02: pass
            p += n
            if op == 0xff and script.startswith('event'): return

    def scripts(self): return [s for s, _ in self.starts]


def flag_now(state, m, b):
    if not state or 'vm_flags' not in state: return '?'
    for mm, w, v in state.get('vm_flags', []):
        if mm == m and w == b // 32: return int(bool(v & (0x80000000 >> (b % 32))))
    return 0


def annotate(room, c, state=None):
    op = c[0]; name = NAMES[op] if op < len(NAMES) else f'op{op:02x}'
    u8 = lambda i: c[i] if i < len(c) else 0; u16 = lambda i: c[i] | (c[i + 1] << 8) if i + 1 < len(c) else 0
    note = ''
    if op == 0x04:
        m, b, mode = u8(1), u16(2) & 0x3ff, u8(5); note = f'wenn F[{m}:{b}]=={1 - mode} (jetzt {flag_now(state, m, b)})'
    elif op == 0x05:
        m, b, mode = u8(1), u16(2) & 0x3ff, u8(5); note = f'F[{m}:{b}] := {1 - mode} (jetzt {flag_now(state, m, b)})'
    elif op == 0x06:
        key, mode, val = u8(1), u8(2), u8(3); cmp = ['==', '>', '>=', '<', '<=', '!='][mode] if mode < 6 else '?'
        note = f"wert[{key}{' Zone' if key == 20 else ''}] {cmp} {val}"
    elif op == 0x0a: note = f"Wand w{u8(1)} {'aus (offen)' if u8(2) else 'an (zu)'}"
    elif op == 0x0b: note = f"Zone i{u8(1)} {'aus' if u8(2) else 'an'}"
    elif op == 0x0c: note = f"Boden {u8(1)} {'aus' if u8(2) else 'an'}"
    elif op == 0x25: note = f"Zone i{u8(1)} neu belegt (attr {u16(2):#x}, prm {list(c[4:8])})"
    elif op == 0x0e: note = f"Zone i{u8(4)} Aufnahme-Bit {u16(2) & 0x3ff}"
    elif op in (0x10, 0x11): note = f"Item {u8(1)} {ITEMS.get(u8(1), '?')}" + (' benutzt' if op == 0x10 else ' im Inventar')
    elif op == 0x14: note = f"startet Ereignis {u8(3)} auf Ebene {u8(2)}"
    elif op == 0x1f: note = f'"{room.message(u8(2))}"' if not u8(1) else 'Meldung schliessen'
    elif op in (0xb2, 0xc2): note = f"gibt Item {u8(1)} {ITEMS.get(u8(1), '?')}"
    elif op == 0x01: note = f'IF (Laenge {u8(1)})'
    return f"{name:<20s} {c.hex(' ')}" + (f'   -> {note}' if note else '')


def block_after(room, script, off, state, maxn=30):
    out = []; started = False; base = None
    for o, depth, c in room.commands(script):
        if o == off: started = True; base = depth
        if not started: continue
        if out and depth < base - 1: break
        out.append(f"  {script} off=0x{o:x} {'  ' * max(depth - base + 1, 0)}{annotate(room, c, state)}")
        if len(out) >= maxn: out.append('  ...'); break
    return out


def find_refs(room, pred):
    for s in room.scripts():
        prev = []
        for o, depth, c in room.commands(s):
            if pred(c): yield s, o, prev[-4:]
            prev.append((o, depth, c))


def cmd_zone(i, state):
    room = Room(state['room']); a = next((z for z in state['interactions'] if z['index'] == i), None)
    if a is None: print('keine Zone', i); return
    kind = {0: 'TUER', 1: 'TREPPE', 2: 'TREPPE2', 3: 'TEXT', 4: 'ITEM/OBJEKT'}.get(a['type'], a['type'])
    locks = [n for bit, n in ((0x400, 'Blick +z gesperrt'), (0x800, '-x gesperrt'), (0x1000, '-z gesperrt'), (0x2000, '+x gesperrt')) if a['attribute'] & bit]
    print(f"Zone i{i} {kind} {'an' if a['flags'] & 1 else 'AUS'} attr {a['attribute']:#x} prm {a['parameters']} {' '.join(locks)}")
    if a['type'] == 3 and a['attribute'] & 0x8000: print(f"  Direkte Meldung {a['parameters'][1]}: \"{room.message(a['parameters'][1])}\"")
    if a['type'] == 3 and a['attribute'] & 9: print(f"  Kameraschnitt cut {a['parameters'][0]} – Skript reagiert ueber wert[20]=={i}")
    if a['type'] == 4: print(f"  Raumitem/Objekt Platz {a['parameters'][0]}")
    if a['type'] == 0: print(f"  Tuer zu Raum-Nr {a['parameters'][1]} (Stufe {a['parameters'][0]}), Spawn {a['parameters'][2]}")
    refs = list(find_refs(room, lambda c: (c[0] == 0x06 and c[1] == 20 and c[3] == i) or (c[0] == 0x0e and len(c) > 4 and c[4] == i)))
    tog = list(find_refs(room, lambda c: c[0] in (0x0b, 0x25) and c[1] == i))
    if not refs and not tog: print('  keine Skriptstelle prueft diese Zone')
    for s, o, _ in refs[:4]:
        print(f'  Skript reagiert ({s} 0x{o:x}):'); print('\n'.join(block_after(room, s, o, state, 24)))
    for s, o, ctx in tog[:6]:
        print(f'  Zone wird geschaltet in {s} 0x{o:x}; davor:')
        for oo, dd, cc in ctx: print(f'    0x{oo:x} {annotate(room, cc, state)}')


def cmd_blocked(x, z, state):
    from pathfind import Grid
    pts, info = Grid(state).path(state['actor'][0], state['actor'][2], x, z)
    if pts: print(f'Weg existiert ({info:.0f}) – nicht blockiert'); return
    room = Room(state['room']); px, pz = state['actor'][0], state['actor'][2]
    cands = []
    for w in state['walls']:
        if not (w['fl'] & 1) or w['ty'] > 5: continue
        test = dict(state); test['walls'] = [dict(v, fl=0) if v['i'] == w['i'] else v for v in state['walls']]
        p2, _ = Grid(test).path(px, pz, x, z)
        if p2: cands.append(w)
    if not cands: print('Keine einzelne Wand sperrt – mehrere Waende/Etage/Boden pruefen (hint.py room)'); return
    scripted = [w for w in cands if any(True for _ in find_refs(room, lambda c, wi=w['i']: c[0] == 0x0a and c[1] == wi))]
    fixed = [w for w in cands if w not in scripted]
    if fixed: print('Feste Waende auf dem Weg (Umweg/anderer Zugang noetig): ' + ' '.join(f"w{w['i']}({w['p'][0]:.0f},{w['p'][2]:.0f})" for w in fixed))
    for w in scripted:
        print(f"Sperrende Wand w{w['i']} ty{w['ty']} p=({w['p'][0]:.0f},{w['p'][2]:.0f}) e=({w['e'][0]:.0f},{w['e'][2]:.0f}) attr {w['attr']:#x}")
        refs = list(find_refs(room, lambda c, wi=w['i']: c[0] == 0x0a and c[1] == wi))
        if not refs: print('  kein Skript schaltet sie (fest oder Objekt/Gegner-Wand)')
        for s, o, ctx in refs:
            print(f"  {s} 0x{o:x} {annotate(room, room.d[room.ev + o:room.ev + o + 4], state)}; davor:")
            for oo, dd, cc in ctx: print(f'    0x{oo:x} {annotate(room, cc, state)}')
        trig = [s for s, _, _ in refs]
        for s in sorted(set(trig)):
            if s.startswith('event'):
                n = int(s[5:]); starters = list(find_refs(room, lambda c, n=n: c[0] == 0x14 and c[3] == n))
                for ss, oo, ctx in starters[:2]:
                    print(f'  {s} wird gestartet in {ss} 0x{oo:x}; Bedingungen davor:')
                    for o3, d3, c3 in ctx: print(f'    0x{o3:x} {annotate(room, c3, state)}')


def cmd_room(state):
    room = Room(state['room']); print(f"{room.name}: {len(state['interactions'])} Zonen, Meldungen {room.u32(room.msg)}")
    for a in state['interactions']:
        if not (a['flags'] & 1): continue
        k = {0: 'TUER', 1: 'TREPPE', 2: 'TREPPE2', 3: 'TEXT', 4: 'ITEM'}.get(a['type'], a['type'])
        cx, cz = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        what = ''
        if a['type'] == 3 and a['attribute'] & 0x8000: what = '"' + room.message(a['parameters'][1])[:70] + '"'
        elif a['type'] == 3:
            refs = list(find_refs(room, lambda c, i=a['index']: c[0] == 0x06 and c[1] == 20 and c[3] == i))
            what = f"Skript {refs[0][0]} 0x{refs[0][1]:x}" if refs else 'kein Skript'
        elif a['type'] == 4:
            it = next((r for r in state['room_items'] if r['index'] == a['parameters'][0]), None)
            what = (f"Item {it['identity']} {ITEMS.get(it['identity'], '?')}" + (' (genommen)' if it['inactive'] else '') + (' (versteckt)' if it['hidden'] else '')) if it else f"Objekt {a['parameters'][0]}"
        elif a['type'] == 0: what = f"Tuer -> Raum {a['parameters'][1]}"
        print(f" i{a['index']:<2} {k:<7} ({cx:.0f},{cz:.0f}) {what}")


def cmd_op(arg):
    op = int(arg, 16) if re.fullmatch(r'(0x)?[0-9a-fA-F]{1,2}', arg) else NAMES.index(arg)
    name = NAMES[op]; print(f'Opcode 0x{op:02x} = {name}')
    pats = [(os.path.join(ROOT, 'native/events.cpp'), rf'case 0x{op:x}:|case {op}:'), (os.path.join(ROOT, 'native/world.cpp'), rf'op==0x{op:x}\b'),
            (ORIG_EVENT, rf'^[a-z].*\b{name}\(')]
    for f, pat in pats:
        r = subprocess.run(['rg', '-n', '-m', '3', pat, f], capture_output=True, text=True).stdout
        for line in r.splitlines(): print(f'  {os.path.relpath(f, ROOT)}:{line[:220]}')


def cmd_grep(pat):
    r = subprocess.run(['rg', '-n', '-m', '8', '--max-columns', '200', pat] + SEARCH, capture_output=True, text=True).stdout.splitlines()
    for line in r[:40]: print(os.path.relpath(line.split(':', 1)[0], ROOT) + ':' + line.split(':', 1)[1] if ':' in line else line)
    if len(r) > 40: print(f'... {len(r) - 40} weitere')


def main():
    a = sys.argv[1:]; auto = os.path.join(ROOT, 'build/play/auto.json')
    if not a: print(__doc__); return
    c = a[0]
    if c == 'room': cmd_room(load_state(a[1] if len(a) > 1 else auto))
    elif c == 'zone': cmd_zone(int(a[1]), load_state(a[2] if len(a) > 2 else auto))
    elif c == 'blocked': cmd_blocked(float(a[1]), float(a[2]), load_state(a[3] if len(a) > 3 else auto))
    elif c == 'event':
        st = load_state(auto) if os.path.exists(auto) else None; room = Room(a[2] if len(a) > 2 else st['room'])
        for o, d, cc in room.commands(f'event{a[1]}'): print(f"0x{o:x} {'  ' * max(d, 0)}{annotate(room, cc, st)}")
    elif c == 'msg': room = Room(a[2] if len(a) > 2 else load_state(auto)['room']); print(room.message(int(a[1])))
    elif c == 'op': cmd_op(a[1])
    elif c == 'grep': cmd_grep(a[1])
    else: print(__doc__)


if __name__ == '__main__': main()
