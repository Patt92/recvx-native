#!/usr/bin/env python3
"""Debug-Stream fuer Agenten: liest die autodump-Datei der Runtime (`autodump 5 auto`) und meldet nur Aenderungen.

  stream.py [auto.json] [--log build/play/stream.log] [--radar build/play/radar.txt] [--idle 120] [--once]

Nur eine Instanz je Log (Lock); beendet sich nach --idle Sekunden ohne neue Daten.

Laeuft dauerhaft (Hintergrund-Task oder Unteragent) und schreibt
  - Ereigniszeilen auf stdout und in stream.log (Raum, Eingabebesitzer, Dialogtext, HP, Griff,
    Gegner sichtbar/nah/Haltung/tot/Bodenkontakt, Items, Zonen und Waende an/aus, Route, Game Over),
  - radar.txt: immer aktueller Kurzlagebericht (~15 Zeilen) zum billigen Lesen statt eines Screenshots.
Gegnerhaltung kommt aus den Knotenmatrizen (body.posture: standing/low/lying, body.contact: ground/air/under).
"""
import json, math, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pathfind import Grid

args = [a for a in sys.argv[1:] if not a.startswith('--')]
opts = {sys.argv[i]: sys.argv[i + 1] for i in range(1, len(sys.argv) - 1) if sys.argv[i].startswith('--') and sys.argv[i] != '--once'}
AUTO = args[0] if args else 'build/play/auto.json'
LOG = opts.get('--log', os.path.join(os.path.dirname(AUTO), 'stream.log'))
RADAR = opts.get('--radar', os.path.join(os.path.dirname(AUTO), 'radar.txt'))
ONCE = '--once' in sys.argv


def load(path):
    s = open(path).read()
    try: return json.loads(s)
    except json.JSONDecodeError: return json.loads(re.sub(r'[\x00-\x1f]', ' ', s))


def owner(d):
    flags = [('DEBUGMENUE', d.get('debug_open')), ('GAMEOVER', d.get('gameover_active')), ('KINO', d['cinematic']),
             ('DIALOG', d['dialog_open']), ('INVENTAR', d['inventory_open']), ('BOX', d.get('box_open')), ('TUER', d['door_active'])]
    on = [k for k, v in flags if v]
    return ' '.join(on) if on else 'FREI'


def bearing(d, x, z):
    px, _, pz = d['actor']; yaw = d['yaw']
    target = math.degrees(math.atan2(-(x - px), -(z - pz))) % 360; cur = yaw * 360 / 65536
    delta = (target - cur + 180) % 360 - 180  # + = links (keys left erhoeht yaw)
    if abs(delta) < 20: return 'vorn'
    if abs(delta) > 150: return 'hinten'
    return f"{'links' if delta > 0 else 'rechts'} {abs(delta):.0f}°"


class Sight:
    def __init__(self): self.key = None; self.grid = None
    def los(self, d, x, z):
        key = (d['room'], tuple(w['fl'] for w in d.get('walls', [])), d.get('player_floor'))
        if key != self.key:
            self.key = key; self.grid = Grid(d, cell=1.0, radius=0.6)
        g = self.grid; px, _, pz = d['actor']
        return g.los(g.idx(px, pz), g.idx(x, z))


sight = Sight()


def enemies(d):
    px, _, pz = d['actor']; out = {}
    for e in d['enemies']:
        if e['inactive'] or e['hidden'] or e['identity'] == 0: continue
        if abs(e['position'][0]) < 0.05 and abs(e['position'][2]) < 0.05: continue  # Skriptgegner noch nicht platziert
        if 'work' not in e: continue
        x, z = e['position'][0], e['position'][2]; dist = math.hypot(x - px, z - pz)
        b = e.get('body') or {}
        m0 = e['work']['mode'][0]
        state = 'tot' if e['hp'] <= 0 and m0 in (3, 4) or m0 == 4 else {1: 'aktiv', 2: 'Griff', 3: 'getroffen', 5: 'Skript'}.get(m0, f'm{m0}')
        out[e['index']] = dict(id=e['identity'], x=x, z=z, dist=dist, hp=e['hp'], state=state,
                               posture=b.get('posture', '?'), contact=b.get('contact', '?'), top=b.get('top'),
                               sight=sight.los(d, x, z) if dist < 60 else False)
    return out


def zones(d):
    return {a['index']: (a['flags'] & 1) for a in d['interactions']}


def inv(d):
    return {v >> 16: v & 0xffff for v in d['items'][1:] if v}  # items[0] = Ausruestungsplatz


def band(dist): return 0 if dist < 10 else 1 if dist < 20 else 2 if dist < 35 else 3


def diff(prev, d, ev):
    t = d.get('event_clock') or ''
    if prev is None or prev['room'] != d['room']:
        doors = [a for a in d['interactions'] if a['type'] == 0 and a['flags'] & 1]
        ev(f"RAUM {d['room'][:7]} pos ({d['actor'][0]:.0f},{d['actor'][2]:.0f}) Tueren {len(doors)} Zonen {sum(1 for a in d['interactions'] if a['flags'] & 1)}")
        prev = None
    if prev is None or owner(prev) != owner(d): ev(f"BESITZ {owner(d)}")
    if d.get('dialog_text') and (prev is None or prev.get('dialog_text') != d['dialog_text']):
        ev(f"DIALOG {d['dialog_text'].strip().replace(chr(10), ' | ')}" + (' [JA/NEIN]' if d.get('dialog_choosing') else ''))
    if prev is not None and prev['hp'] != d['hp']: ev(f"HP {prev['hp']}→{d['hp']}")
    pw, ppw = d.get('player_work', {}).get('mode', [0])[0], (prev or {}).get('player_work', {}).get('mode', [0])[0]
    if pw == 4 and ppw != 4: ev("GRIFF beginnt (keys left/right im Wechsel)")
    if ppw == 4 and pw != 4: ev("GRIFF vorbei")
    if d.get('gameover_active') and not (prev or {}).get('gameover_active'): ev("GAMEOVER (Snapshot 2 laden)")
    E, P = enemies(d), enemies(prev) if prev else {}
    for i, e in E.items():
        p = P.get(i)
        tag = f"EN{i} id{e['id']} ({e['x']:.0f},{e['z']:.0f}) d{e['dist']:.0f} {bearing(d, e['x'], e['z'])}"
        if p is None:
            if e['state'] != 'tot': ev(f"GEGNER {tag} {e['posture']} {e['state']}{' SICHT' if e['sight'] else ''}")
            continue
        if p['state'] != e['state']: ev(f"GEGNER {tag} {p['state']}→{e['state']} hp{e['hp']}")
        if p['posture'] != e['posture'] and e['state'] != 'tot':
            ev(f"GEGNER {tag} {p['posture']}→{e['posture']}" + (' (Ziel senken: fire n low)' if e['posture'] == 'lying' else ''))
        if p['contact'] != e['contact'] and e['contact'] != 'ground': ev(f"AUFFAELLIG {tag} Bodenkontakt {e['contact']} (top {e['top']:.1f})")
        if e['state'] != 'tot' and band(e['dist']) < band(p['dist']) and band(e['dist']) <= 1:
            ev(f"NAH {tag} {e['posture']}{' SICHT' if e['sight'] else ' wand'}")
        if e['sight'] != p['sight'] and e['state'] != 'tot' and e['dist'] < 35: ev(f"SICHT {tag} {'frei' if e['sight'] else 'verdeckt'}")
    if prev is not None:
        I, PI = inv(d), inv(prev)
        for k in set(I) | set(PI):
            if I.get(k) != PI.get(k): ev(f"INVENTAR item{k} {PI.get(k, 0)}→{I.get(k, 0)}")
        Z, PZ = zones(d), zones(prev)
        for k in Z:
            if PZ.get(k) is not None and PZ[k] != Z[k]: ev(f"ZONE i{k} {'an' if Z[k] else 'aus'}")
        W = {w['i']: w['fl'] & 1 for w in d.get('walls', [])}; PW = {w['i']: w['fl'] & 1 for w in prev.get('walls', [])}
        changed = [k for k in W if PW.get(k) is not None and PW[k] != W[k]]
        if changed: ev("WAND " + ' '.join(f"w{k} {'zu' if W[k] else 'offen'}" for k in changed))
        RI = {i['index']: (i['hidden'], i['inactive']) for i in d['room_items']}; PRI = {i['index']: (i['hidden'], i['inactive']) for i in prev['room_items']}
        for k, v in RI.items():
            if PRI.get(k) and PRI[k][0] and not v[0]: ev(f"ITEM sichtbar raumitem{k}")
        c, pc = d.get('control', {}).get('result'), prev.get('control', {}).get('result')
        if c != pc and c in ('arrived', 'stalled', 'cancelled'): ev(f"ROUTE {c} bei ({d['actor'][0]:.0f},{d['actor'][2]:.0f})")


def radar(d, recent):
    px, py, pz = d['actor']
    lines = [f"{time.strftime('%H:%M:%S')} {d['room'][:7]} ({px:.1f},{pz:.1f}) yaw {d['yaw']} hp {d['hp']} {owner(d)} route={d.get('control', {}).get('result')}"]
    if d.get('dialog_text'): lines.append("DIALOG " + d['dialog_text'].strip().replace('\n', ' | '))
    E = sorted(enemies(d).items(), key=lambda kv: kv[1]['dist'])
    for i, e in E[:5]:
        lines.append(f" EN{i} id{e['id']} d{e['dist']:.0f} {bearing(d, e['x'], e['z'])} {e['posture']}/{e['contact']} {e['state']} hp{e['hp']}{' SICHT' if e['sight'] else ' wand'} ({e['x']:.0f},{e['z']:.0f})")
    zs = []
    for a in d['interactions']:
        if not (a['flags'] & 1) or a['type'] in (1, 2): continue
        cx, cz = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        zs.append((math.hypot(cx - px, cz - pz), a['index'], {0: 'TUER', 3: 'TEXT', 4: 'ITEM'}.get(a['type'], '?'), cx, cz))
    for dist, i, k, cx, cz in sorted(zs)[:5]: lines.append(f" i{i} {k} d{dist:.0f} {bearing(d, cx, cz)} ({cx:.0f},{cz:.0f})")
    lines.append(" inv " + ' '.join(f"{k}x{v}" for k, v in inv(d).items()))
    lines += [' > ' + r for r in recent[-4:]]
    return '\n'.join(lines) + '\n'


def main():
    import fcntl
    lock = open(LOG + '.lock', 'w')
    try: fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        if not ONCE: print('stream laeuft schon (Lock ' + LOG + '.lock)'); return
    idle = float(opts.get('--idle', 120)); last_update = time.time()
    prev, mtime, recent = None, 0, []
    def ev(msg):
        line = f"{time.strftime('%H:%M:%S')} {msg}"; recent.append(line); print(line, flush=True)
        with open(LOG, 'a') as f: f.write(line + '\n')
    while True:
        try:
            m = os.path.getmtime(AUTO)
            if m != mtime:
                mtime = m; last_update = time.time(); d = load(AUTO)
                diff(prev, d, ev); prev = d
                tmp = RADAR + '.tmp'; open(tmp, 'w').write(radar(d, recent)); os.replace(tmp, RADAR)
                if ONCE: print(radar(d, recent), end=''); return
        except (FileNotFoundError, json.JSONDecodeError, KeyError): pass
        if not ONCE and time.time() - last_update > idle: print(f'stream beendet: {idle:.0f} s keine neuen Daten'); return
        time.sleep(0.05)


if __name__ == '__main__': main()
