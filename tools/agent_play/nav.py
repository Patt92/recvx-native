#!/usr/bin/env python3
"""Navigationshilfe fuer --control-Laeufe (siehe work/live-play/PLAYBOOK.md).

  nav.py <dump.json>                     Kompaktzustand: Spieler, Dialog, Interaktionen, Gegner, Items
  nav.py <dump.json> map [cell [x0 z0 x1 z1]] ASCII-Karte (Waende #, Boeden ., Interaktionen Ziffer/Buchstabe, Gegner Z, Spieler P+Blick)
  nav.py <dump.json> face <tx> <tz>      Drehbefehl: Ziel-Yaw, Delta, Takte links/rechts (1310 Einheiten/Takt)
  nav.py <dump.json> goto <tx> <tz> [run] Steuerzeilen (relativ, ab 2): drehen, gehen/laufen bis Abstand, dump
  nav.py <dump.json> walls [x0 z0 x1 z1] Wandliste (gefiltert auf Rechteck)
  nav.py <dump.json> path tx tz [run] [face fx fz] [then ...]  Wegsuche -> route-Zeile fuer live.txt

Koordinaten: x nach rechts, z nach vorn; Blick (actor.cpp:312) = (-sin(yaw), -cos(yaw)), yaw 0..65535.
yaw 0 -> -z, 16384 -> -x, 32768 -> +z, 49152 -> +x. Gehen 0.49/Takt, Laufen ~1.0/Takt (RM_0000 gemessen).
"""
import json, math, re, sys

TURN_PER_TICK = 1310          # Einheiten je Takt bei "keys left/right"
WALK_PER_TICK = 0.49
RUN_PER_TICK = 1.0

def load(path):
    s = open(path).read()
    try: return json.loads(s)
    except json.JSONDecodeError:
        return json.loads(re.sub(r'[\x00-\x1f]', ' ', s))

def deg(v): return v * 360.0 / 65536.0
def facing(yaw):
    a = yaw / 65536.0 * 2 * math.pi
    return -math.sin(a), -math.cos(a)
def yaw_to(px, pz, tx, tz):
    return int(round(math.degrees(math.atan2(-(tx - px), -(tz - pz))) / 360.0 * 65536.0)) % 65536
def delta_yaw(cur, target):
    return (target - cur + 32768) % 65536 - 32768

def summary(d):
    px, py, pz = d['actor']; yaw = d['yaw']
    fx, fz = facing(yaw); pw = d.get('player_work', {}).get('mode')
    own = 'FREI' if not (d['cinematic'] or d['dialog_open'] or d['inventory_open'] or d['door_active'] or d.get('debug_open') or d.get('gameover_active')) else \
        ' '.join(k for k, v in [('DEBUGMENUE', d.get('debug_open')), ('GAMEOVER', d.get('gameover_active')), ('KINO', d['cinematic']), ('DIALOG', d['dialog_open']), ('INVENTAR', d['inventory_open']), ('TUER', d['door_active'])] if v)
    print(f"{d['room'][:7]} ({px:.1f},{pz:.1f}) yaw {yaw} blick({fx:+.1f},{fz:+.1f}) hp {d['hp']} {own} pw{pw} mtn{d.get('motion')}")
    if d.get('dialog_text'):
        print("DLG:", d['dialog_text'].replace('\n', ' | ').strip(), f"[wahl {d.get('dialog_choices')} sel {d.get('dialog_selection')} {'close' if d.get('dialog_closable') else ''}{'turn' if d.get('dialog_turnable') else ''}]")
    if d.get('menu_message_text'): print("MENU:", d['menu_message_text'].replace('\n', ' | ').strip())
    if d.get('debug_status'): print("DBG:", d['debug_status'])
    for a in d['interactions']:
        if not (a['flags'] & 1) or a['type'] in (1, 2): continue
        cx, cz = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        kind = {0: 'TUER', 3: 'TEXT', 4: 'ITEM'}.get(a['type'], f"ty{a['type']}")
        print(f" i{a['index']} {kind} ({cx:.0f},{cz:.0f}) d{math.hypot(cx-px,cz-pz):.0f} y{yaw_to(px,pz,cx,cz)} x{a['position'][0]:.0f}..{a['position'][0]+a['extent'][0]:.0f} z{a['position'][2]:.0f}..{a['position'][2]+a['extent'][2]:.0f} a{a['attribute']:#x} p{a['parameters'][:2]}")
    los = None
    try:
        from pathfind import Grid
        g = Grid(d, cell=1.0, radius=0.6); los = lambda x, z: g.los(g.idx(px, pz), g.idx(x, z))
    except Exception: pass
    for e in d['enemies']:
        if e['inactive'] and e['hp'] == 0: continue
        ex, ez = e['position'][0], e['position'][2]
        sight = '' if los is None else (' SICHT' if los(ex, ez) else ' wand')
        b = e.get('body') or {}
        post = f" {b['posture']}{'' if b.get('contact') in (None, 'ground') else '/' + b['contact']}" if b else ''
        print(f" EN{e['index']} id{e['identity']} hp{e['hp']}{' off' if e['inactive'] else ''} ({ex:.0f},{ez:.0f}) d{math.hypot(ex-px,ez-pz):.0f} y{yaw_to(px,pz,ex,ez)} m{e['work']['mode']}{post}{sight}")
    print(" items", [(i['index'], i['identity'], ('H' if i['hidden'] else '') + ('X' if i['inactive'] else ''), tuple(round(v) for v in i['position'])) for i in d['room_items'] if 'position' in i],
          "| inv", [(v >> 16, v & 0xffff) for v in d['items'] if v], "wpn", d.get('weapon'))

def ascii_map(d, cell=4.0, box=None):
    from pathfind import active_walls
    walls = active_walls(d)  # dieselbe Auswahl wie Wegsuche/move_walls (Etage, Hoehe, attr 4)
    floors = d.get('floors', [])
    px, _, pz = d['actor']
    if box: x0, z0, x1, z1 = box
    else:
        xs = [px]; zs = [pz]
        for w in walls + floors:
            xs += [w['p'][0], w['p'][0] + w['e'][0]]; zs += [w['p'][2], w['p'][2] + w['e'][2]]
        for a in d['interactions']:
            xs += [a['position'][0], a['position'][0] + a['extent'][0]]; zs += [a['position'][2], a['position'][2] + a['extent'][2]]
        x0, x1, z0, z1 = min(xs), max(xs), min(zs), max(zs)
    x0, z0 = math.floor(x0 / cell) * cell, math.floor(z0 / cell) * cell
    W, H = int(math.ceil((x1 - x0) / cell)) + 1, int(math.ceil((z1 - z0) / cell)) + 1
    grid = [[' '] * W for _ in range(H)]
    def idx(x, z): return int(math.floor((x - x0) / cell)), int(math.floor((z - z0) / cell))
    def put(x, z, ch, over=' .'):
        i, j = idx(x, z)
        if 0 <= i < W and 0 <= j < H and (over is None or grid[j][i] in over): grid[j][i] = ch
    def rect(p, e, ch, over=' .'):
        ax, bx = sorted([p[0], p[0] + e[0]]); az, bz = sorted([p[2], p[2] + e[2]])
        i0, j0 = idx(ax, az); i1, j1 = idx(bx - 1e-6, bz - 1e-6)
        for j in range(max(j0, 0), min(j1, H - 1) + 1):
            for i in range(max(i0, 0), min(i1, W - 1) + 1):
                if over is None or grid[j][i] in over: grid[j][i] = ch
    for f in floors: rect(f['p'], f['e'], '.')
    for a in d['interactions']:
        if a['flags'] & 1: rect(a['position'], a['extent'], {0: 'D', 3: 'T', 4: 'I'}.get(a['type'], '?'))
    for w in walls:
        if w['ty'] in (2, 3):
            r = abs(w['e'][0]); cx, cz = w['p'][0], w['p'][2]
            i0, j0 = idx(cx - r, cz - r); i1, j1 = idx(cx + r, cz + r)
            for j in range(max(j0, 0), min(j1, H - 1) + 1):
                for i in range(max(i0, 0), min(i1, W - 1) + 1):
                    if math.hypot(x0 + (i + .5) * cell - cx, z0 + (j + .5) * cell - cz) <= r + cell * .5: grid[j][i] = 'o'
        else: rect(w['p'], w['e'], '#', None)
    for a in d['interactions']:
        if a['flags'] & 1: put(a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2, str(a['index'] % 10), None)
    for e in d['enemies']:
        if e['inactive'] and e['hp'] == 0: continue
        put(e['position'][0], e['position'][2], 'z' if e['inactive'] else 'Z', None)
    fx, fz = facing(d['yaw'])
    put(px, pz, 'P', None); put(px + fx * cell * 1.5, pz + fz * cell * 1.5, '^', None)
    print(f"Karte {d['room']} Zelle {cell}: x {x0:.0f}..{x0+W*cell:.0f}, z {z0:.0f}..{z0+H*cell:.0f} (oben = kleines z). "
          f"# Wand, o Zylinder, . Boden, D Tuer, T Text, I Item/Objekt, Ziffer Interaktion%10, Z Gegner, z inaktiv, P Spieler, ^ Blick")
    head = [' '] * W
    for i in range(0, W, 10):
        for k, ch in enumerate(f"{int(x0 + i * cell)}"):
            if i + k < W: head[i + k] = ch
    print("      " + "".join(head))
    for j in range(H): print(f"{int(z0 + j * cell):5d} " + "".join(grid[j]))

def face_ticks(d, tx, tz):
    px, _, pz = d['actor']; yaw = d['yaw']
    target = yaw_to(px, pz, tx, tz); dl = delta_yaw(yaw, target)
    ticks = int(round(abs(dl) / TURN_PER_TICK)); key = 'left' if dl > 0 else 'right'
    return target, dl, ticks, key, math.hypot(tx - px, tz - pz)

def main():
    d = load(sys.argv[1]); cmd = sys.argv[2] if len(sys.argv) > 2 else ''
    if cmd == 'map': ascii_map(d, float(sys.argv[3]) if len(sys.argv) > 3 else 4.0, [float(v) for v in sys.argv[4:8]] if len(sys.argv) >= 8 else None)
    elif cmd == 'face':
        tx, tz = float(sys.argv[3]), float(sys.argv[4]); target, dl, ticks, key, dist = face_ticks(d, tx, tz)
        print(f"yaw={d['yaw']} target={target} delta={dl} ({deg(dl):+.0f}deg) -> keys {key} {ticks} Takte, dist={dist:.1f}")
    elif cmd == 'goto':
        tx, tz = float(sys.argv[3]), float(sys.argv[4]); run = len(sys.argv) > 5 and sys.argv[5] == 'run'
        stop = 4.0; target, dl, ticks, key, dist = face_ticks(d, tx, tz)
        t = 2; out = []
        if ticks: out.append(f"{t} keys {key} {ticks}"); t += ticks + 2
        walk = max(0, int((dist - stop) / (RUN_PER_TICK if run else WALK_PER_TICK)))
        if walk: out.append(f"{t} keys forward{' run' if run else ''} {walk}"); t += walk + 2
        out.append(f"{t} dump goto"); print("\n".join(out))
    elif cmd == 'wait':
        # wait <auto.json> <live.txt> <timeout_s> [stop] [room] [danger] [goto tx tz [run]]: blockiert, bis Kino/Dialog/Tuer UND
        # eine laufende route/goto vorbei sind (oder Game Over); danger: frueher bei aktivem Gegner < 14;
        # 'stop' haengt sofort 'run off' an (Spiel haelt, Modell entscheidet). Braucht aktiven autodump.
        # (autodump-Datei), haengt dann optional sofort goto-Zeilen an und druckt den Zustand. Allgemeines Werkzeug,
        # Ziel kommt vom Modell.
        import time
        auto, live, timeout = sys.argv[3], sys.argv[4], float(sys.argv[5]); t0 = time.time(); last = None; ack = 0.0; start_room = load(sys.argv[1])['room']
        while time.time() - t0 < timeout:
            try:
                import os
                if os.path.getmtime(auto) < t0: time.sleep(0.1); continue  # nur frische Autodumps zaehlen
                d = load(auto); key = (d['cinematic'], d['dialog_open'], d['hp'], d['room'])
                ctl = d.get('control', {})
                free = (not d['cinematic'] and not d['dialog_open'] and not d['door_active'] and not d.get('player_scripted')
                        and not ctl.get('steer') and not ctl.get('pending')) or d.get('gameover_active')
                # Gefahr: aktiver Gegner naeher als 14 -> sofort melden (Modell entscheidet: schiessen/ausweichen)
                px, _, pz = d['actor']
                near = [e for e in d['enemies'] if not e['inactive'] and e['hp'] > 0 and e['work']['mode'][0] in (1, 2) and math.hypot(e['position'][0]-px, e['position'][2]-pz) < 14]
                if near and 'danger' in sys.argv[6:] and not d['cinematic']:
                    if 'stop' in sys.argv[6:]: open(live, 'a').write("1 run off\n")
                    print("GEFAHR", [(e['index'], round(math.hypot(e['position'][0]-px, e['position'][2]-pz))) for e in near]); summary(d); return
                # 'room': erst frei melden, wenn der Raum gewechselt hat (Tuer-Legs mit laufendem goto davor)
                if 'room' in sys.argv[6:] and d['room'] == start_room and not d.get('gameover_active'): free = False
                if free:
                    if 'stop' in sys.argv[6:]: open(live, 'a').write("1 run off\n")  # sofort anhalten, Modell entscheidet
                    if len(sys.argv) > 8 and 'goto' in sys.argv[6:8]:
                        g = sys.argv.index('goto'); tx, tz = float(sys.argv[g+1]), float(sys.argv[g+2]); run = 'run' in sys.argv[g+3:g+4]
                        target, dl, ticks, kk, dist = face_ticks(d, tx, tz); t = 1; out = []
                        if ticks: out.append(f"{t} keys {kk} {ticks}"); t += ticks + 1
                        n = max(1, int((dist - 4) / (RUN_PER_TICK if run else WALK_PER_TICK))); out.append(f"{t} keys forward{' run' if run else ''} {n}"); t += n + 1
                        out.append(f"{t} dump goto"); open(live, 'a').write("\n".join(out) + "\n"); print("queued:", "; ".join(out))
                    print(f"frei nach {time.time()-t0:.0f}s"); summary(d); return
                # Dialogseite fertig (closable/turnable, keine Wahl): einmal bestaetigen, Text protokollieren. Wahlfragen bleiben stehen.
                if d['dialog_open'] and (d.get('dialog_closable') or d.get('dialog_turnable')) and not d.get('dialog_choosing') and time.time() - ack > 1.5:
                    open(live, 'a').write("1 confirm\n"); ack = time.time(); print(f"{time.time()-t0:4.0f}s ACK: " + d['dialog_text'].replace('\n', ' | ').strip())
                if d['dialog_open'] and d.get('dialog_choosing'): print("WAHL offen:", d['dialog_text'].replace('\n', ' | ')); summary(d); return
                if key != last: last = key; print(f"{time.time()-t0:4.0f}s cin={d['cinematic']} dlg={d['dialog_open']} hp={d['hp']} {d['room'][:7]}" + (" DLG: " + d['dialog_text'].replace('\n',' | ') if d.get('dialog_text') else ""))
            except (json.JSONDecodeError, FileNotFoundError, KeyError): pass
            time.sleep(0.1)
        print("timeout"); summary(load(auto))
    elif cmd == 'path':
        # path tx tz [run] [face fx fz] [then <aktion ...>]  -> eine route-Zeile (Wegsuche ueber walls)
        from pathfind import Grid
        # mehrere Ziele: path x1 z1 [via x2 z2 ...] -> Teilwege werden verkettet
        args = sys.argv[3:]; targets = [(float(args[0]), float(args[1]))]; k = 2
        while k + 2 < len(args) + 1 and k < len(args) and args[k] == 'via': targets.append((float(args[k+1]), float(args[k+2]))); k += 3
        rest = args[k:]; g = Grid(d); sx, sz = d['actor'][0], d['actor'][2]; pts = []; info = 0.0
        for tx, tz in targets:
            part, ln = g.path(sx, sz, tx, tz)
            if part is None: print('KEIN WEG zu', tx, tz, ln, file=sys.stderr); sys.exit(2)
            pts += part; info += ln; sx, sz = part[-1] if part else (sx, sz)
        line = 'route ' + ' '.join(f'{x:.1f} {z:.1f}' for x, z in pts) + (' ' + ' '.join(rest) if rest else '')
        print(f'# {len(pts)} Wegpunkte, Laenge {info:.0f}', file=sys.stderr); print('1 ' + line)
    elif cmd == 'fire':
        # fire <n> [low] [face x z]: Steuerzeilen fuer n Handgun-Schuesse (Zielen 10, je Schuss attack 3 + aim 17), low = Ziel senken
        n = int(sys.argv[3]); low = 'low' in sys.argv[4:]; t = 1; out = []
        if 'face' in sys.argv[4:]:
            k = sys.argv.index('face'); out.append(f"{t} face {sys.argv[k+1]} {sys.argv[k+2]}"); t += 20
        a = 'keys aim back' if low else 'keys aim'
        out.append(f"{t} {a}"); t += 14 if low else 10
        for _ in range(n): out.append(f"{t} {a} attack"); out.append(f"{t+3} {a}"); t += 20
        out.append(f"{t} keys none"); print("\n".join(out))
    elif cmd == 'walls':
        box = [float(v) for v in sys.argv[3:7]] if len(sys.argv) >= 7 else None
        for w in d.get('walls', []):
            x, z = w['p'][0], w['p'][2]
            if box and not (box[0] <= x <= box[2] and box[1] <= z <= box[3]): continue
            print(f"  w{w['i']:3d} fl{w['fl']} ty{w['ty']} id{w['id']} floor{w['floor']} attr {w['attr']:#x} p=({w['p'][0]:.1f},{w['p'][1]:.1f},{w['p'][2]:.1f}) e=({w['e'][0]:.1f},{w['e'][1]:.1f},{w['e'][2]:.1f})")
    else: summary(d)

if __name__ == '__main__': main()
