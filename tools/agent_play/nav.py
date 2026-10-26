#!/usr/bin/env python3
"""Agent reader and planner for --control runs (see work/live-play/PLAYBOOK.md).

  nav.py A                                   compact state: player, input owner, dialogue, zones, enemies, items
  nav.py A path x z [via x z ...] [run] [stop d] [face x z] [then <line>]
                                             pathfinding over the room's walls -> one `route` line on stdout
  nav.py A wait A L secs [stop] [room] [danger]
                                             block until cinematic/dialogue/door/route are over; acknowledges
                                             finished dialogue pages, stops on Yes/No
  nav.py A fire n [low] [face x z]           control lines for n handgun shots (`low` = lowered aim)
  nav.py A map [cell [x0 z0 x1 z1]]          ASCII top view (only when `path` fails)
  nav.py A walls [x0 z0 x1 z1]               raw wall list

A = state file (build/play/auto.json), L = control file (build/play/live.txt).
Coordinates: x right, z forward; facing (actor.cpp:312) = (-sin(yaw), -cos(yaw)), yaw 0..65535:
yaw 0 -> -z, 16384 -> -x, 32768 -> +z, 49152 -> +x. Turning 1310/tick, walking 0.49/tick, running ~1.0/tick.
"""
import json, math, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

TURN_PER_TICK = 1310
WALK_PER_TICK = 0.49
RUN_PER_TICK = 1.0


def load(path):
    s = open(path).read()
    try: return json.loads(s)
    except json.JSONDecodeError: return json.loads(re.sub(r'[\x00-\x1f]', ' ', s))


def facing(yaw):
    a = yaw / 65536.0 * 2 * math.pi
    return -math.sin(a), -math.cos(a)


def yaw_to(px, pz, tx, tz):
    return int(round(math.degrees(math.atan2(-(tx - px), -(tz - pz))) / 360.0 * 65536.0)) % 65536


def owner(d):
    on = [k for k, v in [('DEBUGMENU', d.get('debug_open')), ('GAMEOVER', d.get('gameover_active')), ('CINEMATIC', d['cinematic']),
                         ('DIALOG', d['dialog_open']), ('INVENTORY', d['inventory_open']), ('BOX', d.get('box_open')),
                         ('DOOR', d['door_active'])] if v]
    return ' '.join(on) if on else 'FREE'


def enemy_state(e):
    m0 = e.get('work', {}).get('mode', [0])[0]
    if m0 == 4 or (e['hp'] <= 0 and m0 == 3): return 'dead'
    return {1: 'active', 2: 'grabbing', 3: 'hit', 5: 'scripted'}.get(m0, f'm{m0}')


def summary(d):
    px, py, pz = d['actor']; yaw = d['yaw']; fx, fz = facing(yaw); pw = d.get('player_work', {}).get('mode')
    print(f"{d['room'][:7]} ({px:.1f},{pz:.1f}) yaw {yaw} facing({fx:+.1f},{fz:+.1f}) hp {d['hp']} {owner(d)} pw{pw}"
          f" route={d.get('control', {}).get('result')}")
    if d.get('dialog_text'):
        print("DLG:", d['dialog_text'].replace('\n', ' | ').strip(),
              f"[choices {d.get('dialog_choices')} sel {d.get('dialog_selection')}{' closable' if d.get('dialog_closable') else ''}{' turnable' if d.get('dialog_turnable') else ''}]")
    if d.get('menu_message_text'): print("MENU:", d['menu_message_text'].replace('\n', ' | ').strip())
    if d.get('debug_status'): print("DBG:", d['debug_status'])
    for a in d['interactions']:
        if not (a['flags'] & 1) or a['type'] in (1, 2): continue
        cx, cz = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        kind = {0: 'DOOR', 3: 'TEXT', 4: 'ITEM'}.get(a['type'], f"ty{a['type']}")
        print(f" i{a['index']} {kind} ({cx:.0f},{cz:.0f}) d{math.hypot(cx-px,cz-pz):.0f} x{a['position'][0]:.0f}..{a['position'][0]+a['extent'][0]:.0f}"
              f" z{a['position'][2]:.0f}..{a['position'][2]+a['extent'][2]:.0f} attr{a['attribute']:#x} prm{a['parameters'][:2]}")
    los = None
    try:
        from pathfind import Grid
        g = Grid(d, cell=1.0, radius=0.6); los = lambda x, z: g.los(g.idx(px, pz), g.idx(x, z))
    except Exception: pass
    for e in d['enemies']:
        if (e['inactive'] and e['hp'] == 0) or 'work' not in e: continue
        ex, ez = e['position'][0], e['position'][2]
        if abs(ex) < 0.05 and abs(ez) < 0.05: continue  # script enemy not placed yet
        b = e.get('body') or {}
        post = f" {b['posture']}{'' if b.get('contact') in (None, 'ground') else '/' + b['contact']}" if b else ''
        sight = '' if los is None else (' SIGHT' if los(ex, ez) else ' wall')
        print(f" EN{e['index']} id{e['identity']} hp{e['hp']} {'off' if e['inactive'] else enemy_state(e)} ({ex:.0f},{ez:.0f}) d{math.hypot(ex-px,ez-pz):.0f}{post}{sight}")
    print(" items", [(i['index'], i['identity'], ('H' if i['hidden'] else '') + ('X' if i['inactive'] else ''), tuple(round(v) for v in i['position']))
                     for i in d['room_items'] if 'position' in i],
          "| inv", [(v >> 16, v & 0xffff) for v in d['items'][1:] if v], "weapon", d.get('weapon'))


def ascii_map(d, cell=4.0, box=None):
    from pathfind import active_walls
    walls = active_walls(d)  # same selection as pathfinding / move_walls (floor, height, attr 4)
    floors = d.get('floors', [])
    px, _, pz = d['actor']
    if box: x0, z0, x1, z1 = box
    else:
        xs = [px]; zs = [pz]
        for w in walls + floors: xs += [w['p'][0], w['p'][0] + w['e'][0]]; zs += [w['p'][2], w['p'][2] + w['e'][2]]
        for a in d['interactions']: xs += [a['position'][0], a['position'][0] + a['extent'][0]]; zs += [a['position'][2], a['position'][2] + a['extent'][2]]
        x0, x1, z0, z1 = min(xs), max(xs), min(zs), max(zs)
    x0, z0 = math.floor(x0 / cell) * cell, math.floor(z0 / cell) * cell
    W, H = int(math.ceil((x1 - x0) / cell)) + 1, int(math.ceil((z1 - z0) / cell)) + 1
    grid = [[' '] * W for _ in range(H)]
    def idx(x, z): return int(math.floor((x - x0) / cell)), int(math.floor((z - z0) / cell))
    def put(x, z, ch):
        i, j = idx(x, z)
        if 0 <= i < W and 0 <= j < H: grid[j][i] = ch
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
        if a['flags'] & 1: put(a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2, str(a['index'] % 10))
    for e in d['enemies']:
        if e['inactive'] and e['hp'] == 0: continue
        put(e['position'][0], e['position'][2], 'z' if e['inactive'] else 'Z')
    fx, fz = facing(d['yaw']); put(px, pz, 'P'); put(px + fx * cell * 1.5, pz + fz * cell * 1.5, '^')
    print(f"map {d['room']} cell {cell}: x {x0:.0f}..{x0+W*cell:.0f}, z {z0:.0f}..{z0+H*cell:.0f} (top = small z). "
          "# wall, o cylinder, . floor, D door, T text, I item, digit zone%10, Z enemy, z inactive, P player, ^ facing")
    head = [' '] * W
    for i in range(0, W, 10):
        for k, ch in enumerate(f"{int(x0 + i * cell)}"):
            if i + k < W: head[i + k] = ch
    print("      " + "".join(head))
    for j in range(H): print(f"{int(z0 + j * cell):5d} " + "".join(grid[j]))


def wait(d0, auto, live, timeout, flags):
    """Block until the player is free again (or game over, danger, Yes/No); needs a running autodump."""
    t0 = time.time(); last = None; ack = 0.0; start_room = d0['room']
    while time.time() - t0 < timeout:
        try:
            if os.path.getmtime(auto) < t0: time.sleep(0.05); continue  # only fresh autodumps count
            d = load(auto); ctl = d.get('control', {})
            free = (not d['cinematic'] and not d['dialog_open'] and not d['door_active'] and not d.get('player_scripted')
                    and not ctl.get('steer') and not ctl.get('pending')) or d.get('gameover_active')
            px, _, pz = d['actor']
            near = [e for e in d['enemies'] if not e['inactive'] and e['hp'] > 0 and e.get('work', {}).get('mode', [0])[0] in (1, 2)
                    and math.hypot(e['position'][0] - px, e['position'][2] - pz) < 14]
            if near and 'danger' in flags and not d['cinematic']:
                if 'stop' in flags: open(live, 'a').write("1 run off\n")
                print("DANGER", [(e['index'], round(math.hypot(e['position'][0] - px, e['position'][2] - pz))) for e in near]); summary(d); return
            if 'room' in flags and d['room'] == start_room and not d.get('gameover_active'): free = False
            if free:
                if 'stop' in flags: open(live, 'a').write("1 run off\n")
                print(f"free after {time.time()-t0:.0f}s"); summary(d); return
            # finished page without a choice: acknowledge once and log the text; Yes/No stays for the model
            if d['dialog_open'] and (d.get('dialog_closable') or d.get('dialog_turnable')) and not d.get('dialog_choosing') and time.time() - ack > 1.5:
                open(live, 'a').write("1 confirm\n"); ack = time.time(); print(f"{time.time()-t0:4.0f}s ACK: " + d['dialog_text'].replace('\n', ' | ').strip())
            if d['dialog_open'] and d.get('dialog_choosing'): print("CHOICE:", d['dialog_text'].replace('\n', ' | ')); summary(d); return
            key = (d['cinematic'], d['dialog_open'], d['hp'], d['room'])
            if key != last: last = key; print(f"{time.time()-t0:4.0f}s cin={d['cinematic']} dlg={d['dialog_open']} hp={d['hp']} {d['room'][:7]}")
        except (json.JSONDecodeError, FileNotFoundError, KeyError): pass
        time.sleep(0.05)
    print("timeout"); summary(load(auto))


def main():
    if len(sys.argv) < 2: print(__doc__); return
    d = load(sys.argv[1]); cmd = sys.argv[2] if len(sys.argv) > 2 else ''
    if cmd == 'map': ascii_map(d, float(sys.argv[3]) if len(sys.argv) > 3 else 4.0, [float(v) for v in sys.argv[4:8]] if len(sys.argv) >= 8 else None)
    elif cmd == 'wait': wait(d, sys.argv[3], sys.argv[4], float(sys.argv[5]), sys.argv[6:])
    elif cmd == 'path':
        from pathfind import Grid
        args = sys.argv[3:]; targets = [(float(args[0]), float(args[1]))]; k = 2
        while k < len(args) and args[k] == 'via': targets.append((float(args[k + 1]), float(args[k + 2]))); k += 3
        rest = args[k:]; g = Grid(d); sx, sz = d['actor'][0], d['actor'][2]; pts = []; length = 0.0
        for tx, tz in targets:
            part, ln = g.path(sx, sz, tx, tz)
            if part is None: print(f'NO PATH to {tx} {tz}: {ln} (try: hint.py blocked {tx} {tz})', file=sys.stderr); sys.exit(2)
            pts += part; length += ln; sx, sz = part[-1] if part else (sx, sz)
        print(f'# {len(pts)} waypoints, length {length:.0f}', file=sys.stderr)
        print('1 route ' + ' '.join(f'{x:.1f} {z:.1f}' for x, z in pts) + (' ' + ' '.join(rest) if rest else ''))
    elif cmd == 'fire':
        n = int(sys.argv[3]); low = 'low' in sys.argv[4:]; t = 1; out = []
        if 'face' in sys.argv[4:]:
            k = sys.argv.index('face'); out.append(f"{t} face {sys.argv[k+1]} {sys.argv[k+2]}"); t += 26  # 180 deg = 25 ticks
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
