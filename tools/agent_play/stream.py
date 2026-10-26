#!/usr/bin/env python3
"""Debug stream for agents: follows the runtime's autodump file (`autodump 5 auto`) and reports only changes.

  stream.py [auto.json] [--log build/play/stream.log] [--radar build/play/radar.txt] [--idle 120]
            [--until near,grab,gameover,choice,room,dead,stalled] [--once]

Writes
  - event lines to stdout and stream.log: ROOM, OWNER, DIALOG, CHOICE, HP, GRAB, GAMEOVER, ENEMY (appears,
    state, posture), NEAR, SIGHT, ODD (ground contact), INVENTORY, ZONE, WALL, ITEM, ROUTE;
  - radar.txt: a ~15-line situation report kept current, the cheap substitute for a screenshot.
--until: exit with the first matching event (exit code 0) so a background task wakes the agent exactly then.
One instance per log (lock); exits after --idle seconds without new data.
Enemy posture comes from the engine's node matrices against the floor (body.posture standing/low/lying,
body.contact ground/air/under).
"""
import json, math, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pathfind import Grid
from nav import load, owner, enemy_state

args = [a for a in sys.argv[1:] if not a.startswith('--')]
opts = {sys.argv[i]: sys.argv[i + 1] for i in range(1, len(sys.argv) - 1) if sys.argv[i].startswith('--') and sys.argv[i] != '--once'}
AUTO = args[0] if args else 'build/play/auto.json'
LOG = opts.get('--log', os.path.join(os.path.dirname(AUTO), 'stream.log'))
RADAR = opts.get('--radar', os.path.join(os.path.dirname(AUTO), 'radar.txt'))
ONCE = '--once' in sys.argv
UNTIL = set(filter(None, opts.get('--until', '').split(',')))


def bearing(d, x, z):
    px, _, pz = d['actor']
    target = math.degrees(math.atan2(-(x - px), -(z - pz))) % 360; cur = d['yaw'] * 360 / 65536
    delta = (target - cur + 180) % 360 - 180  # + = left (keys left increases yaw)
    if abs(delta) < 20: return 'ahead'
    if abs(delta) > 150: return 'behind'
    return f"{'left' if delta > 0 else 'right'} {abs(delta):.0f}°"


class Sight:
    def __init__(self): self.key = None; self.grid = None
    def los(self, d, x, z):
        key = (d['room'], tuple(w['fl'] for w in d.get('walls', [])), d.get('player_floor'))
        if key != self.key: self.key = key; self.grid = Grid(d, cell=1.0, radius=0.6)
        g = self.grid; px, _, pz = d['actor']
        return g.los(g.idx(px, pz), g.idx(x, z))


sight = Sight()


def enemies(d):
    px, _, pz = d['actor']; out = {}
    for e in d['enemies']:
        if e['inactive'] or e['hidden'] or e['identity'] == 0 or 'work' not in e: continue
        x, z = e['position'][0], e['position'][2]
        if abs(x) < 0.05 and abs(z) < 0.05: continue  # script enemy not placed yet
        dist = math.hypot(x - px, z - pz); b = e.get('body') or {}
        out[e['index']] = dict(id=e['identity'], x=x, z=z, dist=dist, hp=e['hp'], state=enemy_state(e),
                               posture=b.get('posture', '?'), contact=b.get('contact', '?'), top=b.get('top'),
                               sight=sight.los(d, x, z) if dist < 60 else False)
    return out


def inv(d): return {v >> 16: v & 0xffff for v in d['items'][1:] if v}  # items[0] = equipment slot
def band(dist): return 0 if dist < 10 else 1 if dist < 20 else 2 if dist < 35 else 3


def diff(prev, d, ev):
    if prev is None or prev['room'] != d['room']:
        doors = sum(1 for a in d['interactions'] if a['type'] == 0 and a['flags'] & 1)
        ev('room', f"ROOM {d['room'][:7]} at ({d['actor'][0]:.0f},{d['actor'][2]:.0f}) doors {doors} zones {sum(1 for a in d['interactions'] if a['flags'] & 1)}")
        prev = None
    if prev is None or owner(prev) != owner(d): ev('owner', f"OWNER {owner(d)}")
    if d.get('dialog_text') and (prev is None or prev.get('dialog_text') != d['dialog_text']):
        text = d['dialog_text'].strip().replace('\n', ' | ')
        ev('choice' if d.get('dialog_choosing') else 'dialog', ('CHOICE ' if d.get('dialog_choosing') else 'DIALOG ') + text)
    if prev is not None and prev['hp'] != d['hp']: ev('hp', f"HP {prev['hp']}->{d['hp']}")
    pw, ppw = d.get('player_work', {}).get('mode', [0])[0], (prev or {}).get('player_work', {}).get('mode', [0])[0]
    if pw == 4 and ppw != 4: ev('grab', "GRAB starts (alternate keys left/right)")
    if ppw == 4 and pw != 4: ev('grab_end', "GRAB over")
    if d.get('gameover_active') and not (prev or {}).get('gameover_active'): ev('gameover', "GAMEOVER (load snapshot 2)")
    E, P = enemies(d), enemies(prev) if prev else {}
    for i, e in E.items():
        p = P.get(i)
        tag = f"EN{i} id{e['id']} ({e['x']:.0f},{e['z']:.0f}) d{e['dist']:.0f} {bearing(d, e['x'], e['z'])}"
        if p is None:
            if e['state'] != 'dead': ev('enemy', f"ENEMY {tag} {e['posture']} {e['state']}{' SIGHT' if e['sight'] else ''}")
            continue
        if p['state'] != e['state']: ev('dead' if e['state'] == 'dead' else 'enemy', f"ENEMY {tag} {p['state']}->{e['state']} hp{e['hp']}")
        if p['posture'] != e['posture'] and e['state'] != 'dead':
            ev('posture', f"ENEMY {tag} {p['posture']}->{e['posture']}" + (' (lower aim: fire n low)' if e['posture'] == 'lying' else ''))
        if p['contact'] != e['contact'] and e['contact'] != 'ground': ev('odd', f"ODD {tag} ground contact {e['contact']} (top {e['top']:.1f})")
        if e['state'] in ('active', 'grabbing') and band(e['dist']) < band(p['dist']) and band(e['dist']) <= 1:
            ev('near', f"NEAR {tag} {e['posture']}{' SIGHT' if e['sight'] else ' wall'}")
        if e['sight'] != p['sight'] and e['state'] != 'dead' and e['dist'] < 35: ev('sight', f"SIGHT {tag} {'clear' if e['sight'] else 'blocked'}")
    if prev is None: return
    I, PI = inv(d), inv(prev)
    for k in set(I) | set(PI):
        if I.get(k) != PI.get(k): ev('inventory', f"INVENTORY item{k} {PI.get(k, 0)}->{I.get(k, 0)}")
    Z = {a['index']: a['flags'] & 1 for a in d['interactions']}; PZ = {a['index']: a['flags'] & 1 for a in prev['interactions']}
    for k in Z:
        if PZ.get(k) is not None and PZ[k] != Z[k]: ev('zone', f"ZONE i{k} {'on' if Z[k] else 'off'}")
    W = {w['i']: w['fl'] & 1 for w in d.get('walls', [])}; PW = {w['i']: w['fl'] & 1 for w in prev.get('walls', [])}
    changed = [k for k in W if PW.get(k) is not None and PW[k] != W[k]]
    if changed: ev('wall', "WALL " + ' '.join(f"w{k} {'closed' if W[k] else 'open'}" for k in changed))
    RI = {i['index']: i['hidden'] for i in d['room_items']}; PRI = {i['index']: i['hidden'] for i in prev['room_items']}
    for k, hidden in RI.items():
        if PRI.get(k) and not hidden: ev('item', f"ITEM visible room item {k}")
    c, pc = d.get('control', {}).get('result'), prev.get('control', {}).get('result')
    if c != pc and c in ('arrived', 'stalled', 'cancelled'): ev(c, f"ROUTE {c} at ({d['actor'][0]:.0f},{d['actor'][2]:.0f})")


def radar(d, recent):
    px, _, pz = d['actor']
    lines = [f"{time.strftime('%H:%M:%S')} {d['room'][:7]} ({px:.1f},{pz:.1f}) yaw {d['yaw']} hp {d['hp']} {owner(d)} route={d.get('control', {}).get('result')}"]
    if d.get('dialog_text'): lines.append(('CHOICE ' if d.get('dialog_choosing') else 'DIALOG ') + d['dialog_text'].strip().replace('\n', ' | '))
    for i, e in sorted(enemies(d).items(), key=lambda kv: kv[1]['dist'])[:5]:
        lines.append(f" EN{i} id{e['id']} d{e['dist']:.0f} {bearing(d, e['x'], e['z'])} {e['posture']}/{e['contact']} {e['state']} hp{e['hp']}"
                     f"{' SIGHT' if e['sight'] else ' wall'} ({e['x']:.0f},{e['z']:.0f})")
    zs = []
    for a in d['interactions']:
        if not (a['flags'] & 1) or a['type'] in (1, 2): continue
        cx, cz = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        zs.append((math.hypot(cx - px, cz - pz), a['index'], {0: 'DOOR', 3: 'TEXT', 4: 'ITEM'}.get(a['type'], '?'), cx, cz))
    for dist, i, k, cx, cz in sorted(zs)[:5]: lines.append(f" i{i} {k} d{dist:.0f} {bearing(d, cx, cz)} ({cx:.0f},{cz:.0f})")
    lines.append(" inv " + ' '.join(f"{k}x{v}" for k, v in inv(d).items()))
    lines += [' > ' + r for r in recent[-4:]]
    return '\n'.join(lines) + '\n'


def main():
    import fcntl
    lock = open(LOG + '.lock', 'w')
    try: fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        if not ONCE: print('stream already running (lock ' + LOG + '.lock)'); return
    idle = float(opts.get('--idle', 120)); last_update = time.time(); started = time.time()
    prev, mtime, recent, hit = None, 0, [], []
    def ev(kind, msg):
        line = f"{time.strftime('%H:%M:%S')} {msg}"; recent.append(line); print(line, flush=True)
        with open(LOG, 'a') as f: f.write(line + '\n')
        if kind in UNTIL and prev_ready[0]: hit.append(line)
    prev_ready = [False]
    while True:
        try:
            m = os.path.getmtime(AUTO)
            if m != mtime:
                mtime = m; last_update = time.time(); d = load(AUTO)
                prev_ready[0] = m >= started  # a stale file found at start never triggers --until
                diff(prev, d, ev); prev = d
                tmp = RADAR + '.tmp'; open(tmp, 'w').write(radar(d, recent)); os.replace(tmp, RADAR)
                if ONCE: print(radar(d, recent), end=''); return
                if hit: print('UNTIL ' + hit[0]); print(radar(d, recent), end=''); return
        except (FileNotFoundError, json.JSONDecodeError, KeyError): pass
        if not ONCE and time.time() - last_update > idle: print(f'stream stopped: no new data for {idle:.0f} s'); sys.exit(3)
        time.sleep(0.05)


if __name__ == '__main__': main()
