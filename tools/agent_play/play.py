#!/usr/bin/env python3
"""Two-stage agent interface: observe -> choose ONE action -> act -> compact observation.

No game strategy, automatic acknowledgements, screenshots, desktop APIs, Git, builds or launch.
Standalone: play.py state | play.py act '{"op":"move","to":[x,z]}'
Optional persistent cache: play.py serve; use --socket build/play/play.sock on client calls.
The server performs work only on requests; it does not play autonomously.
"""
import argparse
from contextlib import contextmanager
import fcntl
import json
import math
import os
from pathlib import Path
import socket
import socketserver
import signal
import threading
import time

from nav import enemy_state, owner
from pathfind import GridCache, _blocked_fn, active_walls, segment_free

KINDS = {0: 'door', 1: 'stairs', 2: 'stairs', 3: 'text', 4: 'item'}
# events.cpp EventVm::interact (hitchk.c bhCheckExmAtari): probe distance ahead of a standing
# player (fpx == px, arn 0) per zone type, and the attribute bit that refuses each facing quadrant.
REACH = {0: 4.5, 3: 6.0, 4: 2.0}
# Stair search (runtime begin_stairs, hitchk.c type 1/2): pxn[1] 0.75 and pxn[2] 4.5 ahead; it runs
# before EventVm::interact, so a stair hit answers before any type 0/3/4 zone.
STAIR_REACH = {1: .75, 2: 4.5}
PROBE = {**REACH, **STAIR_REACH}
FACING_BLOCK = {32768: 0x400, 16384: 0x800, 0: 0x1000, 49152: 0x2000}
BODY = 3.5
# wait/act return 'danger' when an active enemy newly comes this close (host decision aid).
DANGER = 15.0
# Observed grab starts at d 5.8..6.3 (live runs); inside this the enemy can grab, passing is not free.
GRAB_REACH = 7.0
FINAL_APPROACH = 8.0
BUTTONS = {'confirm', 'cancel', 'up', 'down', 'left', 'right', 'inventory', 'select', 'skip'}
KEYS = {'forward', 'back', 'left', 'right', 'run', 'aim', 'attack', 'next_target', 'none'}


class PlayError(Exception):
    pass


def busy(d):
    c = d.get('control', {})
    return bool(c.get('steer') or c.get('pending'))


def finite_pair(value):
    if not isinstance(value, list) or len(value) != 2:
        raise PlayError('expected [x,z]')
    result = tuple(float(v) for v in value)
    if not all(math.isfinite(v) for v in result):
        raise PlayError('non-finite coordinates')
    return result


def threats(d):
    """Visible, living enemies in work mode 1 (active) or 2 (grabbing); scripted ones excluded."""
    out = []
    for e in d.get('enemies', []):
        if e.get('inactive') or e.get('hidden') or e.get('scripted') or e.get('hp', 0) <= 0 or 'work' not in e \
                or enemy_state(e) not in ('active', 'grabbing'):
            continue
        if abs(e['position'][0]) < .05 and abs(e['position'][2]) < .05:
            continue
        out.append(e)
    return out


def danger(d, radius=DANGER):
    """An active or grabbing enemy within radius, only while the player owns the input (FREE/GRAB)."""
    if owner(d) not in ('FREE', 'GRAB'):
        return False
    px, _, pz = d['actor']
    return any(math.hypot(e['position'][0] - px, e['position'][2] - pz) < radius for e in threats(d))


def equipped(d):
    """pip[0] (items[0]) is the equipped slot; returns (slot, item id, rounds, name) or None."""
    items = d.get('items', [])
    slot = items[0] if items else 0
    if not 0 < slot < len(items) or not (items[slot] >> 16) & 255:
        return None
    names = d.get('item_names', [])
    return slot, (items[slot] >> 16) & 255, items[slot] & 0xffff, names[slot] if slot < len(names) else ''


def inventory(d):
    """[slot, id, count, name]; names are the status-list names exported by the runtime."""
    names = d.get('item_names', [])
    out = []
    for i, v in enumerate(d.get('items', [])[1:], 1):
        if v:
            row = [i, (v >> 16) & 255, v & 0xffff]
            if i < len(names) and names[i]:
                row.append(names[i])
            out.append(row)
    return out


def compact(d, age, grid_cache=None):
    """Bound geometry/enemy output, preserve full current dialogue for the decision."""
    px, _, pz = d['actor']
    c = d.get('control', {})
    result = dict(room=d['room'][:7], owner=owner(d), at=[round(px, 1), round(pz, 1)],
                  yaw=d['yaw'], hp=d['hp'], age_s=round(age, 2),
                  route=c.get('result'), busy=busy(d),
                  weapon=d.get('weapon', 0),
                  inv=inventory(d))
    eq = equipped(d)
    if eq:
        result['equipped'] = [eq[3] or eq[1], eq[2]]
    if age > 2:
        result['stale'] = True
    if c.get('held'):
        # run auto: the runtime holds (no ticks) until the next action; metrics are heartbeats.
        result['held'] = True
    if d.get('dialog_open'):
        result['dialog'] = dict(text=d.get('dialog_text', ''), choice=d.get('dialog_choosing', False),
                                selection=d.get('dialog_selection', 0),
                                ready=bool(d.get('dialog_closable') or d.get('dialog_turnable') or d.get('dialog_choosing')))
        if not d.get('dialog_text', '').strip():
            # Page without printable text (voice-only scene page): confirm advances it as written.
            result['dialog'].update(voice=True, index=d.get('dialog_index'))
    if d.get('menu_message_text'):
        result['menu_text'] = d['menu_message_text']
    if d.get('inventory_open'):
        result['menu'] = menu = dict(cursor=d.get('main_cursor'), navigation=d.get('menu_navigation'),
                                     slot=d.get('menu_selected'), item=d.get('menu_item_name') or d.get('menu_item'))
        if eq and d.get('menu_selected') == eq[0]:
            # Equip toggles: Equip on the equipped slot unequips it (Inventory::equip / WeaponSet).
            menu['equipped'] = True
        if d.get('menu_commands_open'):
            commands = d.get('menu_commands', [])
            k = d.get('menu_command', 0)
            menu['commands'] = commands
            menu['command'] = commands[k] if 0 <= k < len(commands) else k
        if d.get('menu_locked'):
            menu['locked'] = True
        if d.get('menu_examining'):
            menu['examining'] = True
        if d.get('menu_combining'):
            menu['combine_from'] = [d.get('menu_combine_from'), d.get('menu_combine_name')]
    if d.get('files_open'):
        result['file'] = [d.get('file_number'), d.get('file_page'), d.get('file_pages')]
    if d.get('box_open'):
        result['box_base'] = d.get('box_base')
    if d.get('title_active'):
        result['title'] = dict(mode=d.get('title_mode'), stage=d.get('title_stage'), cursor=d.get('title_cursor'),
                               entry=d.get('title_entry'), entries=d.get('title_entries'),
                               load_available=d.get('title_load_available'))
    if (d.get('typewriter') or {}).get('active'):
        t = d['typewriter']
        # Save/load screen: name the highlighted choice. Typewriter::choose: selection 0 = Yes, 1 = No;
        # saving over a used slot opens on No; up/down toggles (pad 0x1/0x2), confirm takes it.
        tw = dict(mode=t.get('mode'), phase=t.get('phase_name'), text=t.get('text', ''),
                  slot=(t.get('slot') or {}).get('n', t.get('cursor')),
                  slot_used=bool((t.get('slot') or {}).get('exists')), used_slots=t.get('used_slots'))
        if t.get('phase_name') in ('confirm', 'special_save'):
            tw['highlighted'] = 'No' if t.get('selection') else 'Yes'
            tw['toggle'] = 'up or down'
        result['typewriter'] = tw
    if d.get('movie_active'):
        result['movie_frame'] = d.get('movie_frame')
    zones = []
    room_items = {e['index']: e for e in d.get('room_items', [])}
    for a in d.get('interactions', []):
        if not a['flags'] & 1 or a.get('floor', 0) != d.get('player_floor', 0):
            continue
        x, z = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        zinfo = dict(i=a['index'], kind=KINDS.get(a['type'], str(a['type'])),
                     at=[round(x, 1), round(z, 1)], d=round(math.hypot(x-px, z-pz), 1))
        if a['type'] == 0:
            zinfo['destination'] = a['parameters'][1]
        if a['type'] == 4:
            # Linked room item (itwp[prm0]) as exported in room_items; attribute 2 = object branch.
            e = None if a.get('attribute', 0) & 2 else room_items.get(a['parameters'][0])
            zinfo['item'] = 'object' if a.get('attribute', 0) & 2 else 'none' if e is None else \
                'inactive' if e.get('inactive') else 'hidden' if e.get('hidden') else 'present'
        zones.append(zinfo)
    zones.sort(key=lambda z: z['d'])
    result['zones'] = zones[:5]
    result['other_zones'] = max(0, len(zones) - 5)
    # Item zones beyond the nearest five stay visible: [zone, at, d, linked room item state].
    far_items = [[z['i'], z['at'], z['d'], z['item']] for z in zones[5:] if z['kind'] == 'item']
    if far_items:
        result['far_items'] = far_items
    result['doors'] = [z['i'] for z in zones if z['kind'] == 'door']
    enemies = []
    for e in d.get('enemies', []):
        if e.get('inactive') or e.get('hidden') or 'work' not in e or enemy_state(e) == 'dead':
            continue
        x, z = e['position'][0], e['position'][2]
        if abs(x) < .05 and abs(z) < .05:
            continue
        dist = math.hypot(x-px, z-pz)
        if dist > 40:
            continue
        body = e.get('body') or {}
        info = dict(i=e['index'], hp=e['hp'], d=round(dist, 1), at=[round(x, 1), round(z, 1)],
                    state=enemy_state(e), posture=body.get('posture'), contact=body.get('contact'))
        info['mode'] = e['work'].get('mode')
        if dist < GRAB_REACH and info['state'] in ('active', 'grabbing'):
            info['reach'] = True
        if grid_cache is not None:
            g = grid_cache.get(d, radius=.6)
            info['sight'] = g.los(g.idx(px, pz), g.idx(x, z))
        enemies.append(info)
    result['enemies'] = sorted(enemies, key=lambda e: e['d'])[:4]
    reach = [e['i'] for e in result['enemies'] if e.get('reach')]
    if reach:
        result['in_reach'] = reach
    if d.get('gameover_active'):
        # Retry restores the retry point (bhPopGameData); after a fresh start that is the game start.
        r = d.get('gameover_retry') or {}
        result['gameover'] = dict(retry_room=(r.get('room') or '')[:7] or None, retry_play_time=r.get('play_time'))
    return result


class Play:
    def __init__(self, directory='build/play', *, max_age=2.0):
        self.directory = Path(directory)
        self.auto = self.directory / 'auto.json'
        self.live = self.directory / 'live.txt'
        self.max_age = max_age
        self.grids = GridCache()
        self.margin_grids = GridCache()
        self.sight = GridCache()
        self._stamp = None
        self._state = None
        self._stream_offset = None
        self._action_lock = threading.Lock()
        self._read_lock = threading.Lock()
        self._observe_lock = threading.Lock()

    def read(self):
        with self._read_lock:
            stat = self.auto.stat()
            stamp = (stat.st_ino, stat.st_mtime_ns, stat.st_size)
            if stamp != self._stamp:
                with open(self.auto) as f:
                    # Runtime replaces this path atomically; bind metadata to the file read.
                    stat = os.fstat(f.fileno())
                    stamp = (stat.st_ino, stat.st_mtime_ns, stat.st_size)
                    self._state = json.load(f, strict=False)
                self._stamp = stamp
            return self._state, stamp, max(0., time.time() - stat.st_mtime)

    def observe(self):
        with self._observe_lock:
            return self._observe()

    def _observe(self):
        d, _, age = self.read()
        result = compact(d, age, self.sight)
        stream = self.directory / 'stream.log'
        if stream.exists():
            size = stream.stat().st_size
            offset = self._stream_offset
            if offset is None or offset > size:
                offset = max(0, size-4096)
                # Historical events are not a current decision context.
                if time.time()-stream.stat().st_mtime > self.max_age:
                    offset = size
            with open(stream, 'rb') as f:
                f.seek(max(offset, size-4096))
                text = f.read().decode('utf-8', errors='replace')
            self._stream_offset = size
            lines = text.splitlines()
            if lines:
                result['events'] = lines[-3:]
        return result

    def describe_zone(self, index):
        d, _, age = self.read()
        a = self.zone(d, index)
        result = dict(room=d['room'][:7], age_s=round(age, 2), zone=a, approach=self.approaches(d, a))
        if a['type'] in (0, 3, 4) and a.get('attribute', 0) & 0x3c00 == 0x3c00:
            result['approach_note'] = 'all facings blocked (attribute 0x3c00): this zone cannot be triggered by examine'
        elif a['type'] == 2 and a['parameters'][0] != 0:
            result['approach_note'] = 'step zone with prm0 != 0: not entered by the step branch'
        elif not result['approach']:
            result['approach_note'] = 'no reachable standing point found'
        return result

    @staticmethod
    def stair_direction(d, a):
        """Unit x/z vector from this stair zone toward its partner (mark_stair_etc: prm3 0xFF -> idx+-1 by prm0)."""
        zones = d.get('interactions', [])
        idx = a['index']
        partner = (idx + 1 if a['parameters'][0] == 0 else idx - 1) if a['parameters'][3] == 0xFF else a['parameters'][3]
        b = next((z for z in zones if z['index'] == partner), None)
        if b is None:
            return None
        dx = (b['position'][0] + b['extent'][0] / 2) - (a['position'][0] + a['extent'][0] / 2)
        dz = (b['position'][2] + b['extent'][2] / 2) - (a['position'][2] + a['extent'][2] / 2)
        n = math.hypot(dx, dz)
        return (dx / n, dz / n) if n > .5 else None

    @staticmethod
    def near_for(a, to, face):
        """How far short of `to` (toward face) the player may stop and still put the probe in the zone."""
        reach = PROBE.get(a['type'])
        dx, dz = face[0] - to[0], face[1] - to[1]
        n = math.hypot(dx, dz)
        if reach is None or n < 1e-6:
            return .5
        ux, uz = dx / n, dz / n
        xs = sorted((a['position'][0], a['position'][0] + a['extent'][0]))
        zs = sorted((a['position'][2], a['position'][2] + a['extent'][2]))
        s = 0.
        while s < 3 and xs[0] <= to[0] + (reach - s - .1) * ux <= xs[1] and zs[0] <= to[1] + (reach - s - .1) * uz <= zs[1]:
            s += .1
        return round(max(.5, s - .2), 2)

    def approaches(self, d, a, limit=3):
        """Standing points (free at the body radius, reachable) whose interaction probe lands in the zone.

        Geometry only, mirroring the probe offsets and facing-quadrant refusals of EventVm::interact;
        whether the zone then reacts is up to the room script. The first zone in index order whose
        probe (at its own reach) hits answers: points where a lower-index zone hits are dropped; points
        where a higher-index zone also hits rank last and name it in `overlap` (a probe that misses
        this zone by a little would trigger that one). Ranked by no overlap, probe depth in the zone
        (0.25 steps), closeness of the probe to the centre line across the long side, path length."""
        reach = PROBE.get(a['type'])
        if reach is None or (a['type'] == 2 and a['parameters'][0] != 0):
            return []
        def bounds(z):
            return (sorted((z['position'][0], z['position'][0] + z['extent'][0])),
                    sorted((z['position'][2], z['position'][2] + z['extent'][2])))
        xs, zs = bounds(a)
        def order(z):
            return (0 if z['type'] in STAIR_REACH else 1, z['index'])
        others = [(order(b), PROBE[b['type']], *bounds(b)) for b in d.get('interactions', [])
                  if b['index'] != a['index'] and b['flags'] & 1 and b['type'] in PROBE
                  and b.get('floor', 0) == d.get('player_floor', 0)]
        stair_dir = self.stair_direction(d, a) if a['type'] == 1 else None
        m = min(.25, (xs[1] - xs[0]) / 2, (zs[1] - zs[0]) / 2)
        cx, cz = (xs[0] + xs[1]) / 2, (zs[0] + zs[1]) / 2
        # Centre line runs along the long side: a door is entered through the middle of its face.
        along_x = xs[1] - xs[0] >= zs[1] - zs[0]
        tests = [_blocked_fn(w, BODY)[1] for w in active_walls(d)]
        g = self.grids.get(d, radius=BODY)
        dist = g.distances(d['actor'][0], d['actor'][2])
        dirs = [(math.sin(k * math.pi / 36), math.cos(k * math.pi / 36)) for k in range(72)]
        found = []
        step = .25
        steps = int((xs[1] - xs[0] + 2 * reach) / step) + 1, int((zs[1] - zs[0] + 2 * reach) / step) + 1
        for ix in range(steps[0]):
            for iz in range(steps[1]):
                x, z = xs[0] - reach + ix * step, zs[0] - reach + iz * step
                cost = dist.get(g.idx(x, z))
                if cost is None or any(t(x, z) for t in tests):
                    continue
                best = None
                for ux, uz in dirs:
                    px, pz = x + reach * ux, z + reach * uz
                    inner = min(px - xs[0], xs[1] - px, pz - zs[0], zs[1] - pz)
                    yaw = int(round(math.atan2(-ux, -uz) * 65536 / (2 * math.pi))) & 0xffff
                    if inner < m:
                        continue
                    if a['type'] in REACH and a.get('attribute', 0) & FACING_BLOCK[(yaw + 8192) & 0xc000]:
                        continue
                    if a['type'] == 2:
                        # Step branch: 6 ahead along the snapped quadrant inside the zone shrunk by 1.
                        q = ((yaw + 8192) & 0xc000) * 2 * math.pi / 65536
                        ix, iz = x - 6 * math.sin(q), z - 6 * math.cos(q)
                        if not (xs[0] + 1 <= ix <= xs[1] - 1 and zs[0] + 1 <= iz <= zs[1] - 1):
                            continue
                    if stair_dir and ux * stair_dir[0] + uz * stair_dir[1] < math.cos(math.pi / 4):
                        continue
                    hit_keys = [k for k, r, bx, bz in others if bx[0] <= x + r * ux <= bx[1] and bz[0] <= z + r * uz <= bz[1]]
                    if any(k < order(a) for k in hit_keys):
                        continue
                    hits = [k[1] for k in hit_keys]
                    off = (1 - (ux * stair_dir[0] + uz * stair_dir[1])) if stair_dir else abs(px - cx) if along_x else abs(pz - cz)
                    key = (bool(hits), -math.floor(inner / .25), off)
                    if best is None or key < best[0]:
                        best = (key, px, pz, hits)
                if best:
                    found.append((best[0], cost, x, z, best[1], best[2], best[3]))
        out = []
        for key, cost, x, z, px, pz, hits in sorted(found):
            if all(math.hypot(x - o['to'][0], z - o['to'][1]) >= 1.5 for o in out):
                out.append(dict(to=[round(x, 2), round(z, 2)], face=[round(px, 2), round(pz, 2)], path=round(cost, 1),
                                depth=round(-key[1] * .25, 2)))
                if hits:
                    out[-1]['overlap'] = hits
            if len(out) == limit:
                break
        return out

    @contextmanager
    def exclusive(self):
        # All facade clients share a nonblocking lock. No queued duplicate controller.
        if not self._action_lock.acquire(blocking=False):
            raise PlayError('another action is running')
        try:
            with open(self.directory / 'play.lock', 'a') as f:
                try:
                    fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError as exc:
                    raise PlayError('another action is running') from exc
                yield
        finally:
            self._action_lock.release()

    def append(self, commands):
        # One complete append; never truncate/replay someone else's control file.
        text = ''.join(f'{tick} {line}\n' for tick, line in commands)
        with open(self.live, 'a') as f:
            f.write(text)

    def zone(self, d, index):
        a = next((a for a in d.get('interactions', []) if a['index'] == index), None)
        if a is None or not a['flags'] & 1:
            raise PlayError('zone absent or inactive')
        if a.get('floor', 0) != d.get('player_floor', 0):
            raise PlayError('zone is on another floor')
        return a

    def route(self, d, target, *, run=True, face=None, confirm=False, radius=3.5, danger=True, near=0.0):
        x, z = finite_pair(list(target))
        # Do not silently substitute the nearest free cell for a blocked goal.
        if any(_blocked_fn(w, radius)[1](x, z) for w in active_walls(d)):
            raise PlayError('target blocked; choose a free approach, not a shifted grid goal (see zone approach)')
        # Waypoints keep a 0.5 clearance from corners when the room allows it, else the body radius.
        # The final segment to the exact target is tested against the wall geometry, not the raster
        # (0.05 below the body radius so a start in wall contact is not rejected).
        failure = None
        start = (d['actor'][0], d['actor'][2])
        # A free straight line (with the 0.5 margin, or the body radius under 3 units) needs no raster detour.
        near_target = math.hypot(x - start[0], z - start[1]) < 3
        if segment_free(d, start, (x, z), radius + .5) or (near_target and segment_free(d, start, (x, z), radius - .05)):
            caches = ()
            points = [(x, z)]
        else:
            caches = ((self.margin_grids, radius + .5), (self.grids, radius))
        for cache, r in caches:
            g = cache.get(d, radius=r)
            points, length = g.path(d['actor'][0], d['actor'][2], x, z)
            if points is None:
                failure = failure or 'no path: ' + str(length)
                continue
            start = points[-1] if points else (d['actor'][0], d['actor'][2])
            if not segment_free(d, start, (x, z), radius - .05):
                failure = 'target segment blocked'
                continue
            points = list(points) + [(x, z)]
            break
        else:
            if caches:
                raise PlayError(failure)
        line = 'route ' + ' '.join(f'{a:.4f} {b:.4f}' for a, b in points)
        if run:
            line += ' run'
        # Runtime intermediate tolerance is 3; final precise approach must be explicit.
        line += ' stop 0.35'
        length = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip([start] + points[:-1], points))
        if danger and length >= FINAL_APPROACH:
            # Runtime ends the route when an active/grabbing enemy newly enters DANGER near the rest of the path;
            # a final approach shorter than FINAL_APPROACH is never cut.
            line += f' danger {DANGER:g}'
        if near:
            # A stall this close to the exact target still faces and confirms (result arrived_near).
            line += f' near {near:g}'
        if face is not None:
            fx, fz = finite_pair(list(face))
            line += f' face {fx:.4f} {fz:.4f}'
        if confirm:
            line += ' then confirm'
        return line

    def compile(self, d, action):
        op = action.get('op')
        allowed = {'press': {'op', 'key'}, 'move': {'op', 'to', 'run', 'face', 'danger'},
                   'interact': {'op', 'zone', 'to', 'run', 'face', 'danger'},
                   'keys': {'op', 'keys', 'ticks'}, 'fire': {'op', 'shots', 'low', 'target'},
                   'shake': {'op', 'presses', 'then'}, 'run': {'op', 'mode', 'dist'}}
        if op not in allowed or set(action)-allowed[op]:
            raise PlayError('unknown action or arguments')
        if op == 'press':
            key = action.get('key')
            if key not in BUTTONS:
                raise PlayError('unknown button')
            if key == 'confirm' and d.get('dialog_open') and not (d.get('dialog_choosing') or d.get('dialog_closable') or d.get('dialog_turnable')):
                raise PlayError('dialogue is still printing; wait')
            return [(1, key)]
        if op in ('move', 'interact'):
            if owner(d) != 'FREE':
                raise PlayError('movement requires FREE input owner')
            if op == 'interact':
                a = self.zone(d, action.get('zone'))
                if 'to' not in action:
                    raise PlayError('interact needs an explicit free approach in to; inspect zone bounds')
            target = finite_pair(action.get('to'))
            face = action.get('face')
            if op == 'interact' and face is None:
                face = [a['position'][0] + a['extent'][0]/2, a['position'][2] + a['extent'][2]/2]
            if not isinstance(action.get('danger', True), bool):
                raise PlayError('danger must be true or false')
            return [(1, self.route(d, target, run=action.get('run', True), face=face,
                                   confirm=op == 'interact', danger=action.get('danger', True),
                                   near=self.near_for(a, target, face) if op == 'interact' else 0.0))]
        if op == 'run':
            # on = real time; auto = real time, but held while an active enemy is within dist and
            # nothing is queued (runtime run auto); each action then runs only for its own ticks.
            mode = action.get('mode')
            if mode == 'on':
                return [(1, 'run on')]
            dist = action.get('dist', 18)
            if mode != 'auto' or not isinstance(dist, (int, float)) or not 4 <= dist <= 60:
                raise PlayError('run needs mode on|auto, dist 4..60')
            return [(1, f'run auto {float(dist):g}')]
        if op == 'shake':
            # Grab: newly pressed directions shake the enemy off (bhEne_LeverCheck reads sys->pad_ps).
            if owner(d) != 'GRAB':
                raise PlayError('shake requires GRAB input owner')
            presses = action.get('presses', 8)
            if not isinstance(presses, int) or not 1 <= presses <= 30:
                raise PlayError('presses must be 1..30')
            commands = [(1 + 3 * k, 'keys ' + ('left' if k % 2 == 0 else 'right')) for k in range(presses)] + \
                [(1 + 3 * presses, 'keys none')]
            then = action.get('then')
            if then is not None:
                # Escape move without a decision gap: the route follows the last press; the runtime waits
                # out the grab scene that is running at its start and only then steers (no danger cut).
                if not isinstance(then, dict) or then.get('op') != 'move' or set(then) - {'op', 'to', 'run', 'face'}:
                    raise PlayError('shake then must be a move {"op":"move","to":[x,z]}')
                commands.append((2 + 3 * presses, self.route(d, finite_pair(then.get('to')), run=then.get('run', True),
                                                             face=then.get('face'), danger=False)))
            return commands
        if op == 'keys':
            if owner(d) not in ('FREE', 'GRAB'):
                raise PlayError('keys require FREE or GRAB input owner')
            keys = action.get('keys', [])
            ticks = action.get('ticks', 1)
            if not keys or any(k not in KEYS for k in keys) or not isinstance(ticks, int) or not 1 <= ticks <= 90:
                raise PlayError('keys require known keys and 1..90 ticks')
            return [(1, 'keys ' + ' '.join(keys) + f' {ticks}')]
        if op == 'fire':
            if owner(d) != 'FREE':
                raise PlayError('fire requires FREE input owner')
            eq = equipped(d)
            if not eq or eq[1] != 9:
                raise PlayError('fire cadence currently supports the equipped handgun (item 9) only')
            shots = action.get('shots', 1)
            if not isinstance(shots, int) or not 1 <= shots <= 8:
                raise PlayError('shots must be 1..8')
            # Runtime 'fire n [low]': aim held 10 ticks, then an attack edge every 20 ticks. Before it the
            # player turns to the chosen (default: nearest active/grabbing) enemy so the lock-on cone has it.
            fire = f'fire {shots}' + (' low' if action.get('low') else '')
            target = action.get('target')
            enemies = threats(d)
            if target is not None:
                enemies = [e for e in enemies if e['index'] == target]
                if not enemies:
                    raise PlayError('target is not an active enemy')
            if not enemies:
                return [(1, fire)]
            e = min(enemies, key=lambda e: math.hypot(e['position'][0] - d['actor'][0], e['position'][2] - d['actor'][2]))
            return [(1, f"face {e['position'][0]:.4f} {e['position'][2]:.4f} then {fire}")]
        raise PlayError('unknown action; use move/interact/press/keys/fire/shake/run/wait')

    @staticmethod
    def compile_wait(action):
        # wait without ticks writes nothing; with ticks it lets a held runtime run that long.
        if 'ticks' not in action:
            return []
        ticks = action['ticks']
        if not isinstance(ticks, int) or not 1 <= ticks <= 300:
            raise PlayError('wait ticks must be 1..300')
        return [(ticks, 'tick')]

    def act(self, action, seconds=4.0):
        started = time.monotonic()
        if not isinstance(action, dict) or not isinstance(action.get('op'), str):
            raise PlayError('action must be an object with op')
        if not 0 < seconds <= 8:
            raise PlayError('action wait budget must be >0 and <=8 seconds')
        with self.exclusive():
            d0, stamp0, age = self.read()
            if age > self.max_age:
                raise PlayError('stale metrics: runtime stopped or paused; no input sent')
            waiting = action.get('op') == 'wait'
            if waiting and set(action) - {'op', 'ticks'}:
                raise PlayError('wait takes only ticks')
            if (not waiting or 'ticks' in action) and busy(d0):
                raise PlayError('action already pending; use wait, do not resend')
            commands = self.compile_wait(action) if waiting else self.compile(d0, action)
            if commands:
                # A recently closed runtime can leave a fresh-looking final file. Require
                # one advancing sample before writing anything, without a desktop probe.
                limit = time.monotonic() + min(.5, seconds)
                while time.monotonic() < limit:
                    fresh, fresh_stamp, _ = self.read()
                    if fresh_stamp != stamp0:
                        if fresh['room'] != d0['room'] or owner(fresh) != owner(d0) or busy(fresh):
                            raise PlayError('context changed before dispatch; observe and decide again')
                        d0, stamp0 = fresh, fresh_stamp
                        # Coordinates and menu guards use the dispatch-time state.
                        commands = self.compile_wait(action) if waiting else self.compile(d0, action)
                        break
                    time.sleep(.025)
                else:
                    raise PlayError('metrics not advancing; no input sent')
                # Keep metrics ticking while this action/menu transition runs; a chosen
                # run mode (auto hold, or an explicit run action) is not overridden.
                keep = action.get('op') == 'run' or d0.get('control', {}).get('run') == 'auto'
                self.append(commands if keep else [(1, 'run on')] + commands)
            last_stamp = stamp0
            seen_fresh = False
            settled_at = None
            reason = 'waiting'
            while time.monotonic() - started < seconds:
                try:
                    d, stamp, age = self.read()
                except (FileNotFoundError, json.JSONDecodeError):
                    time.sleep(.025)
                    continue
                if stamp == last_stamp:
                    if age > self.max_age:
                        reason = 'stale'
                        break
                    time.sleep(.025)
                    continue
                last_stamp = stamp
                seen_fresh = True
                if waiting and 'ticks' in action and not busy(d) and \
                        d.get('control', {}).get('tick', 0) - d0.get('control', {}).get('tick', 0) >= action['ticks']:
                    # wait ticks n: the requested game time has run; report it instead of idling the budget.
                    reason = 'ticked'
                    break
                reason = self.reason(d0, d, route=action.get('op') in ('move', 'interact'), waiting=waiting,
                                     fighting=action.get('op') in ('fire', 'shake'))
                if reason:
                    if commands and time.monotonic()-started < .1:
                        continue
                    if reason == 'held' and commands and d.get('control', {}).get('tick') == d0.get('control', {}).get('tick'):
                        continue  # heartbeat from before the runtime took the new lines
                    if reason in ('settled', 'menu'):
                        # Avoid the one-frame free gap before pickup/file/door ownership.
                        if settled_at is None:
                            settled_at = time.monotonic()
                        if time.monotonic() - settled_at < .25:
                            continue
                    break
                settled_at = None
            else:
                reason = 'running' if seen_fresh else 'no_update'
            output = self.observe()
            output['result'] = reason or 'running'
            output['elapsed_ms'] = round((time.monotonic()-started)*1000)
            if commands and action.get('op') == 'fire':
                d1, _, _ = self.read()
                c0, c1 = d0, d1
                target = d1.get('combat_target', -1)
                hp = next((e['hp'] for e in d1.get('enemies', []) if e['index'] == target), None)
                output['fire'] = dict(shots=c1.get('combat_shots', 0) - c0.get('combat_shots', 0),
                                      hits=c1.get('combat_hits', 0) - c0.get('combat_hits', 0),
                                      target=None if target < 0 else target, target_hp=hp)
            if commands and action.get('op') == 'interact' and output['result'] == 'settled':
                output['result'] = 'no_interaction_observed'
            return output

    @staticmethod
    def reason(before, d, *, route=False, waiting=False, fighting=False):
        if d.get('gameover_active'):
            return 'gameover'
        if d['room'] != before['room']:
            return 'room'
        if d['hp'] < before['hp']:
            return 'damage'
        if d.get('player_work', {}).get('mode', [0])[0] == 4:
            # Let a dispatched shake sequence finish before reporting the grab again.
            return None if busy(d) else 'grab'
        if not route and not fighting and danger(d) and not danger(before):
            # Routes report 'danger' from the runtime corridor test (route option danger).
            return 'danger'
        if d.get('dialog_choosing'):
            return 'choice'
        if d.get('dialog_open'):
            return 'dialog' if d.get('dialog_closable') or d.get('dialog_turnable') else None
        if busy(d):
            return None
        if d.get('control', {}).get('result') in ('stalled', 'cancelled', 'interrupted', 'danger'):
            # Ignore an old route result when this request is a menu/button action.
            if route or before.get('control', {}).get('result') != d['control']['result']:
                return d['control']['result']
        if d.get('cinematic') or d.get('door_active') or d.get('player_scripted') or d.get('movie_active'):
            return None
        if d.get('title_active') and d.get('title_mode') not in (6, 9):
            return None
        if waiting and owner(d) == owner(before) and owner(d) != 'FREE':
            ui_keys = ('title_mode', 'title_cursor', 'typewriter', 'main_cursor', 'menu_navigation',
                       'menu_selected', 'menu_commands_open', 'menu_command', 'menu_examining', 'menu_combining',
                       'file_number', 'file_page', 'files_open', 'debug_open')
            if all(d.get(k) == before.get(k) for k in ui_keys):
                return None
        if d.get('inventory_open') and d.get('menu_locked'):
            return None  # status screen slide/command window still moving: pad ignored
        if d.get('inventory_open') or d.get('box_open') or d.get('files_open') or d.get('debug_open'):
            return 'menu'
        if d.get('control', {}).get('held'):
            return 'held'
        if owner(d) != 'FREE':
            return 'owner'
        return 'settled'


def request(play, message):
    try:
        if message['op'] == 'state':
            return play.observe()
        if message['op'] == 'zone':
            return play.describe_zone(int(message['zone']))
        if message['op'] == 'act':
            return play.act(message['action'], message.get('seconds', 4))
        raise PlayError('unknown request')
    except (PlayError, FileNotFoundError, ValueError, KeyError, TypeError) as exc:
        return {'error': str(exc)}


def serve(play, path):
    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            try:
                try:
                    message = json.loads(self.rfile.readline(16384))
                    result = request(play, message)
                except (ValueError, TypeError) as exc:
                    result = {'error': str(exc)}
                self.wfile.write((json.dumps(result, ensure_ascii=False, separators=(',', ':')) + '\n').encode())
            except (BrokenPipeError, ConnectionResetError):
                pass
    # Do not unlink another running server's socket.
    with socketserver.ThreadingUnixStreamServer(str(path), Handler) as server:
        os.chmod(path, 0o600)
        print(json.dumps({'ready': str(path)}), flush=True)
        def interrupt(signum, frame):
            raise KeyboardInterrupt
        previous = signal.signal(signal.SIGTERM, interrupt)
        try:
            server.serve_forever(poll_interval=.1)
        except KeyboardInterrupt:
            pass
        finally:
            Path(path).unlink(missing_ok=True)
            signal.signal(signal.SIGTERM, previous)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', default='build/play')
    parser.add_argument('--socket')
    parser.add_argument('--seconds', type=float, default=4)
    parser.add_argument('command', choices=['state', 'zone', 'act', 'serve'])
    parser.add_argument('action', nargs='?')
    args = parser.parse_args()
    play = Play(args.directory)
    if args.command == 'serve':
        serve(play, args.socket or str(play.directory / 'play.sock'))
        return
    message = dict(op=args.command)
    if args.command == 'act':
        message.update(action=json.loads(args.action or '{}'), seconds=args.seconds)
    if args.command == 'zone':
        message['zone'] = args.action
    if args.socket:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(args.socket)
            sock.sendall((json.dumps(message)+'\n').encode())
            with sock.makefile() as f:
                result = json.loads(f.readline())
    else:
        result = request(play, message)
    print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    if 'error' in result:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
