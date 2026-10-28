#!/usr/bin/env python3
"""Hints for an agent that does not know the next step: read the room script and the code, targeted and read-only.

Escalation (cheap -> expensive): read zone text -> `hint.py zone` -> `blocked` -> targeted `event/op`.
Agent play uses telemetry/stream only, never screenshots or desktop/vision observations.
Reads only data/rooms/*.RDT, native/ and the semantic reference src/ps2/veronica/prog/ (never archived/).

  hint.py room   [auto.json]          what is in this room: per zone its message text, item, door target or script
  hint.py zone i [auto.json]          what zone i does: message, script block with conditions (current flags), toggles
  hint.py blocked x z [auto.json]     why there is no path: which wall blocks, which script events open it
  hint.py event n [room]              room event n decoded (original opcode names, texts, items, flags)
  hint.py msg n [room]                room message n
  hint.py rules [auto.json]           item gates of this room: what happens with / without carried items
  hint.py why n [auto.json]           why room event n runs: its start conditions, what sets those flags
                                      (items you carry), and what the event does (texts, voice, items)
  hint.py guide [auto.json]           next steps in this room from its scripts: gates, items, zones that act,
                                      zones that need something first (and how), closed doors, item uses
  hint.py need F[m:b][==v] [auto.json] how the scripts of this room set that flag (conditions as steps)
  hint.py near [RM_xxxx|auto.json]    rooms within three doors (RDT door tables) and the items their files list
  hint.py uses [auto.json]            where the carried items are used (bhUseItemCheck sites of all rooms)
  hint.py op <hex|bhName>             where a script opcode is implemented (port and original reference)
  hint.py grep <regex>                bounded search in native/ and src/ps2/veronica/prog/ (max 40 hits)
"""
import functools, json, os, re, struct, subprocess, sys
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
        if n >= cnt: return f'<message {n} out of range>'
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
        m, b, mode = u8(1), u16(2) & 0x3ff, u8(5)
        if m == 10 and (b & 0xff) == 23:  # events.cpp bhCk: matrix 10 bit 23 compares the examined zone (values[20])
            note = f"if examined zone {'==' if mode == 0 else '!='} i{u8(4)}"
        elif m == 10 and (b & 0xff) == 22:  # values[16] = the floor the player stands on (events.cpp)
            note = f"if standing on floor {'==' if mode == 0 else '!='} {u8(4)}"
        else:
            note = f'if F[{m}:{b}]=={1 - mode} (now {flag_now(state, m, b)})'
    elif op == 0x05:
        m, b, mode = u8(1), u16(2) & 0x3ff, u8(5); note = f'F[{m}:{b}] := {1 - mode} (now {flag_now(state, m, b)})'
    elif op == 0x06:
        key, mode, val = u8(1), u8(2), u8(3); cmp = ['==', '>', '>=', '<', '<=', '!='][mode] if mode < 6 else '?'
        note = f"value[{key}{' zone' if key == 20 else ''}] {cmp} {val}"
    elif op == 0x07:  # bhCmpW (events.cpp case 7): field 5 hp, 7 dialogue answer (0 = Yes), 9 play time, 10 enemy hp
        field, mode, operand = u8(2), u8(3), u16(4); cmp = ['==', '>', '>=', '<', '<=', '!='][mode] if mode < 6 else '?'
        what = {5: 'hp', 7: 'answer', 9: 'play time', 10: f'enemy {u8(1)} hp'}.get(field, f'word field {field}')
        note = f"if {what} {cmp} {operand}" + (' (0 = Yes, 1 = No)' if field == 7 else '')
    elif op == 0x0a: note = f"wall w{u8(1)} {'off (open)' if u8(2) else 'on (closed)'}"
    elif op == 0x0b: note = f"zone i{u8(1)} {'off' if u8(2) else 'on'}"
    elif op == 0x0c: note = f"floor {u8(1)} {'off' if u8(2) else 'on'}"
    elif op == 0x25: note = f"zone i{u8(1)} redefined (attr {u16(2):#x}, prm {list(c[4:8])})"
    elif op == 0x0e: note = f"zone i{u8(4)} pickup bit {u16(2) & 0x3ff}"
    elif op in (0x10, 0x11): note = f"Item {u8(1)} {ITEMS.get(u8(1), '?')}" + (' used' if op == 0x10 else ' in inventory')
    elif op == 0x14: note = f"starts event {u8(3)} on level {u8(2)}"
    elif op == 0x1f: note = f'"{room.message(u8(2))}"' if not u8(1) else 'close message'
    elif op in (0xb2, 0xc2): note = f"gives item {u8(1)} {ITEMS.get(u8(1), '?')}"
    elif op == 0x01: note = f'IF (length {u8(1)})'
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


def informational_zones(room, state):
    """Text zones no room script reacts to (no value[20]==i test, no pickup check): reading them only
    shows their message. Zones another script redefines or toggles count too; their text can change,
    which the caller sees as a changed zone attribute/parameters."""
    out = set()
    for a in state.get('interactions', []):
        # Only plain message zones: type 3 with a direct message (attribute 0x8000), no script hook
        # (attribute & 9: the script reacts via value[20]) and no Yes/No question in the message.
        if a.get('type') != 3 or not a.get('attribute', 0) & 0x8000 or a.get('attribute', 0) & 9:
            continue
        message = room.message(a['parameters'][1]) if len(a.get('parameters', [])) > 1 else ''
        if not message.strip() or 'Yes' in message and 'No' in message:
            continue
        i = a['index']
        if zone_reactions(room, i):
            continue
        out.add(i)
    return out


def reacts_to_zone(c, i):
    """A script command that tests whether zone i was examined: bhCmpB value[20]==i, bhItmCk on zone i, or
    bhCk matrix 10 bit 23 (events.cpp compares values[20] with the extra byte)."""
    return (c[0] == 0x06 and c[1] == 20 and c[3] == i) or (c[0] == 0x0e and len(c) > 4 and c[4] == i) or \
        (c[0] == 0x04 and c[1] == 10 and (c[2] & 0xff) == 23 and len(c) > 5 and c[4] == i and c[5] == 0)


def zone_reactions(room, i):
    """[(script, offset)] of every command that reacts to examining zone i."""
    return [(s, o) for s, o, _ in find_refs(room, lambda c, i=i: reacts_to_zone(c, i))]


EFFECT_EVENT_LIMIT = 3


def zone_effects(room, i, state, limit=420):
    """What examining zone i does, from the room script: texts it shows, walls and zones it switches,
    flags it sets, events it starts (with what those do) and items it gives, under the conditions that
    guard them. Conditions with a known current value are evaluated: branches that cannot run now are
    dropped, the rest keeps only the undecidable conditions (dialogue answer, floor). '' when no
    script reacts. Generic decoder, no room knowledge."""
    groups = []  # [(conditions [(text, truth)], negated, [effects])]
    for script, off in zone_reactions(room, i)[:3]:
        cmds = list(room.commands(script))
        k = next(j for j, (o, _, _) in enumerate(cmds) if o == off)
        base = cmds[k][1]
        conds, branch, current = {}, {}, None
        for o, depth, c in cmds[k + 1:]:
            if depth < base:
                break
            op = c[0]
            if depth == base and op in (0x04, 0x06, 0x0e) and not reacts_to_zone(c, i) and \
                    (op != 0x04 or (c[1] == 10 and (c[2] & 0xff) == 23)):
                break  # the next zone's section of the same dispatcher
            if op in CONDITION_OPS:
                truth = condition_holds(c, state)
                if op == 0x04 and c[1] == 10 and (c[2] & 0xff) in (22, 23):
                    truth = None
                if op == 0x11:
                    txt = f"carrying {ITEMS.get(c[1], c[1])}"
                elif op == 0x10:
                    txt = f"using {ITEMS.get(c[1], c[1])} here"
                else:
                    txt = annotate(room, c, state).split('-> ', 1)[-1].removeprefix('if ')
                    txt = re.sub(r' \(now [^)]*\)', '', txt)
                conds.setdefault(depth, []).append((txt, truth))
                continue
            if op == 0x01:
                continue
            if op == 0x02:
                branch[depth] = True
                continue
            if op in (0x03, 0xfb):
                conds.pop(depth + 1, None)
                branch.pop(depth + 1, None)
                continue
            effect = None
            if op == 0x1f and not c[1]:
                text = ' '.join(room.message(c[2]).replace('|', ' ').split())
                effect = f'shows "{text[:90]}"' if text else 'a voice-only line'
            elif op == 0x0a:
                effect = f"{'opens' if c[2] else 'closes'} wall w{c[1]}"
            elif op == 0x0b:
                effect = f"zone i{c[1]} {'off' if c[2] else 'on'}"
            elif op == 0x25:
                effect = f'zone i{c[1]} redefined'
            elif op == 0x05:
                m, b = c[1], (c[2] | (c[3] << 8)) & 0x3ff
                effect = f'sets F[{m}:{b}]:={1 - c[5]}'
            elif op == 0x14:
                n = c[3]
                try:
                    does = event_summary(room, n)
                except Exception:
                    does = '?'
                effect = f'starts event {n} ({does})' if not does.startswith('nothing visible') else f'starts event {n}'
            elif op in (0xb2, 0xc2):
                effect = f'gives {ITEMS.get(c[1], c[1])}'
            if effect is None:
                continue
            # The IF conditions sit one level above their block; an ELSE at that level negates them.
            levels = sorted(d2 for d2 in conds if d2 <= depth)
            key = tuple((d2, tuple(conds[d2]), bool(branch.get(d2))) for d2 in levels)
            if current is None or current[0] != key:
                current = (key, [])
                groups.append(current)
            if effect not in current[1]:
                current[1].append(effect)
    out, needs = [], []
    gate_flags = {g['flag'] for g in item_gates(room, state)} if groups else set()
    for key, effects in groups:
        open_conds, possible = [], True
        for _, cs, negated in key:
            truths = [t for _, t in cs]
            if not negated:
                if False in truths:
                    possible = False
                    for txt, t in cs:
                        if t is False:
                            flag = txt.split('==')[0]
                            txt += ' (an item check sets it)' if flag in gate_flags else ''
                            if txt not in needs:
                                needs.append(txt)
                    break
                open_conds += [txt for txt, t in cs if t is None]
            else:
                if False in truths:
                    continue  # the IF part cannot run: the ELSE part does
                if all(t is True for t in truths):
                    possible = False
                    break
                open_conds.append('not(' + '; '.join(txt for txt, t in cs if t is not True) + ')')
        if not possible:
            continue
        cond = '; '.join(open_conds)
        out.append((f'if {cond}: ' if cond else '') + ', '.join(effects))
    text = ' | '.join(out)
    if not text and needs:
        text = 'nothing now; would need ' + ' or '.join(needs[:3])
    return text[:limit] + ('...' if len(text) > limit else '')


def item_uses(room):
    """Where items are used in this room, from the scripts: [(item id, name, zone or None, floor or None)]
    for every bhUseItemCheck, with the zone test and the floor test (standing on floor N) whose IF
    blocks enclose it. One linear pass: Room.commands yields an IF's conditions and body one level
    deeper than the IF, so a test at depth k guards everything deeper or equal until a command at a
    lower depth (bhEndif, the next IF) or a bhElseCk at its own depth. Generic decoder."""
    out, seen = [], set()
    for script in room.scripts():
        zone_at, floor_at = {}, {}  # depth -> value of the test still in force
        testing = {}                # depth -> still in the condition list of the IF that opened it
        for o, depth, c in room.commands(script):
            for table in (zone_at, floor_at):
                for k in [k for k in table if k > depth or (k == depth and c[0] == 0x02)]:
                    table.pop(k, None)
            if c[0] == 0x01:
                testing[depth + 1] = True
                continue
            if c[0] not in CONDITION_OPS:
                testing[depth] = False  # a condition op after the body began guards nothing (bhItmCk at top level)
            if not testing.get(depth):
                if c[0] == 0x10:
                    zone = zone_at[max(zone_at)] if zone_at else None
                    floor = floor_at[max(floor_at)] if floor_at else None
                    key = (c[1], zone, floor)
                    if key not in seen:
                        seen.add(key)
                        out.append((c[1], ITEMS.get(c[1], str(c[1])), zone, floor))
                continue
            if c[0] == 0x04 and c[1] == 10 and len(c) > 5 and c[5] == 0:
                if (c[2] & 0xff) == 23:
                    zone_at[depth] = c[4]
                elif (c[2] & 0xff) == 22:
                    floor_at[depth] = c[4]
            elif c[0] == 0x06 and c[1] == 20 and c[2] == 0:
                zone_at[depth] = c[3]
            elif c[0] == 0x0e and len(c) > 4:
                zone_at[depth] = c[4]
            elif c[0] == 0x10:  # a use test inside an IF's condition list: the use site itself
                zone = zone_at[max(zone_at)] if zone_at else None
                floor = floor_at[max(floor_at)] if floor_at else None
                key = (c[1], zone, floor)
                if key not in seen:
                    seen.add(key)
                    out.append((c[1], ITEMS.get(c[1], str(c[1])), zone, floor))
    return out


_ITEM_TYPES = None


def item_types():
    """itemdata[].type from native/item_tables.c (a data table, no original code): bit 0x2 = ammunition."""
    global _ITEM_TYPES
    if _ITEM_TYPES is None:
        src = open(os.path.join(ROOT, 'native/item_tables.c')).read()
        i = src.index('const ITEM_WORK itemdata[156]')
        rows = re.findall(r'\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}', src[i:src.index('};', i)])
        _ITEM_TYPES = [int(r[2]) for r in rows]
    return _ITEM_TYPES


@functools.lru_cache(maxsize=1)
def guns():
    """Item ids that ammunition (itemdata type 0x2) loads into by the native combine table: the guns."""
    from play import item_tables
    types = item_types()
    combidata, cmb = item_tables()
    out = set()
    for source, pos in enumerate(cmb):
        if pos is None or pos < 0 or pos >= len(combidata) or not (source < len(types) and types[source] & 2):
            continue
        for k in range(combidata[pos]):
            at = pos + 1 + 3 * k
            target = combidata[at] if at < len(combidata) else -1
            if at + 2 < len(combidata) and combidata[at + 2] & 255 <= 5 and 0 < target < 150 \
                    and not types[target] & 2 and ITEMS.get(target):  # 150+: loaded-ammo variants
                out.add(target)
    return frozenset(out)


@functools.lru_cache(maxsize=1)
def all_item_uses():
    """Use sites of every item over all room scripts: {item id: [(room, zone or None)]}. Read once from the
    game data (data/rooms/*.RDT); a cache for the driver, no room knowledge in the code."""
    out = {}
    for name in sorted(os.listdir(os.path.join(ROOT, 'data/rooms'))):
        if not name.endswith('.RDT'):
            continue
        try:
            room = Room(name)
            uses = item_uses(room)
        except Exception:
            continue
        types = item_types()
        for item, _, zone, floor in uses:
            if item < len(types) and types[item] & 0x2:
                continue  # ammunition: its use checks are reload/combine paths, not puzzle sites
            out.setdefault(item, []).append((room.name, zone, floor))
    return out


@functools.lru_cache(maxsize=None)
def room_static(name):
    """Zones and items of a room from its RDT alone (no runtime state): the same tables EventVm reads
    (room offset +0x1c/+0x9c zones of 36 bytes, +0x10/+0x90 items). For rooms the player has not
    entered: doors with targets, items lying there, text zones with their messages."""
    room = Room(name)
    d, u32 = room.d, room.u32
    ro = u32(16)
    count, offset = u32(ro + 0x9c), u32(ro + 0x1c)
    zones = []
    for i in range(min(count, 4096)):
        p = offset + i * 36
        zones.append(dict(index=i, flags=d[p], type=d[p + 1], floor=struct.unpack_from('<b', d, p + 3)[0],
                          attribute=u32(p + 4), position=list(struct.unpack_from('<3f', d, p + 8)),
                          extent=list(struct.unpack_from('<3f', d, p + 20)), parameters=list(d[p + 32:p + 36])))
    icount, ioffset = u32(ro + 0x90), u32(ro + 16)
    items = [struct.unpack_from('<H', d, ioffset + i * 36 + 4)[0] for i in range(min(icount, 4096))]
    fcount, foffset = u32(ro + 0xa0), u32(ro + 0x20)  # floor areas (EventVm floors_, bhCheckFloorP)
    floors = []
    for i in range(min(fcount, 4096)):
        p = foffset + i * 36
        floors.append(dict(index=i, flags=d[p], type=d[p + 1], position=list(struct.unpack_from('<3f', d, p + 8)),
                           extent=list(struct.unpack_from('<3f', d, p + 20))))
    return dict(room=room.name, zones=zones, items=items, floors=floors)


def door_graph():
    """Every door of every room from the RDT files: {room: [(zone, stage, target room number)]}. The
    initial zone flags decide whether a door is active at room start; scripts may switch others on."""
    out = {}
    for name in sorted(os.listdir(os.path.join(ROOT, 'data/rooms'))):
        if not name.endswith('.RDT'):
            continue
        try:
            st = room_static(name)
        except Exception:
            continue
        out[st['room']] = [(z['index'], z['parameters'][0], z['parameters'][1], bool(z['flags'] & 1))
                           for z in st['zones'] if z['type'] == 0]
    return out


def neighbours(graph, room, radius=3):
    """Rooms within `radius` doors of `room` (BFS over the door graph, active doors only):
    [(room, distance, via zone of the first hop)]."""
    room = room[:7]
    if room not in graph:
        return []
    stage = room[3]
    name = lambda stage_, n: f'RM_{stage_}{n:02d}0'
    seen, queue, out = {room}, [(room, 0, None)], []
    while queue:
        here, dist, first = queue.pop(0)
        if dist >= radius:
            continue
        for zone, stage_no, target, active in graph.get(here, []):
            if not active:
                continue
            nxt = name(str(stage_no) if here == room or stage_no != int(stage) else stage, target)
            if nxt in seen or nxt not in graph:
                continue
            seen.add(nxt)
            hop = first if first is not None else zone
            out.append((nxt, dist + 1, hop))
            queue.append((nxt, dist + 1, hop))
    return out


# sub1.c:1606-1615 (ItemBox open): cb_flg 0x80000 selects the storage itm[192], 0x100000 itm[224], neither
# the shared box itm[64]; cb_flg 0x40000 opens a box. As flags: F[10:12], F[10:11], F[10:13]
# (flag word bit b = 0x80000000 >> b). Zones of type 4 select the storage by attribute 4 / 8
# (events.cpp EventVm::interact, hitchk.c item box branch).
BOX_NAMES = {64: 'item box (one storage shared by every normal item box)',
             192: 'security box A (its own storage, itm[192])',
             224: 'security box B (its own storage, itm[224])'}


def _box_flags(room, cmds, seen):
    found = set()
    for o, depth, c in cmds:
        if c[0] == 0x05 and c[1] == 10 and len(c) > 5 and c[5] == 0:
            b = (c[2] | (c[3] << 8)) & 0x3ff
            if b in (11, 12, 13):
                found.add(b)
        elif c[0] == 0x14 and c[3] not in seen and f'event{c[3]}' in room.scripts():
            seen.add(c[3])
            found |= _box_flags(room, room.commands(f'event{c[3]}'), seen)
    return found


def zone_box(room, i, state):
    """Storage base (64, 192, 224) of the item box zone i opens, or None. From the zone record (type 4,
    attribute 4/8) or from the room script: the examine block of zone i (without branches that test
    another zone) and the events it starts, setting the box flags. Generic decoder."""
    a = next((z for z in state.get('interactions', []) if z['index'] == i), None)
    if a is None:  # not in the live table (other floor, switched off): the room file's record
        a = next((z for z in room_static(room.name + '.RDT')['zones'] if z['index'] == i), None)
    if a is None:
        return None
    if a.get('type') == 4 and a.get('attribute', 0) & 0xc:
        return 192 if a['attribute'] & 4 else 224
    bits = set()
    for script, off in zone_reactions(room, i)[:3]:
        cmds = list(room.commands(script))
        k = next(j for j, (o, _, _) in enumerate(cmds) if o == off)
        base = cmds[k][1]
        other = {}  # depth -> an enclosing condition there tests another zone
        body = []
        for o, depth, c in cmds[k + 1:]:
            if depth < base:
                break
            if depth == base and c[0] in (0x04, 0x06, 0x0e) and not reacts_to_zone(c, i) and \
                    (c[0] != 0x04 or (c[1] == 10 and (c[2] & 0xff) == 23)):
                break  # the next zone's section of the same dispatcher
            for d2 in [d2 for d2 in other if d2 > depth]:
                other.pop(d2, None)
            if c[0] == 0x02:
                other.pop(depth, None)  # ELSE: the other-zone test of this level is negated from here
                continue
            if c[0] in (0x04, 0x06, 0x0e) and any(reacts_to_zone(c, j) for j in range(64) if j != i):
                other[depth] = True
                continue
            if not any(other.values()):
                body.append((o, depth, c))
        bits |= _box_flags(room, body, set())
    if 12 in bits:
        return 192
    if 11 in bits:
        return 224
    if 13 in bits:
        return 64
    return None


@functools.lru_cache(maxsize=1)
def all_box_places():
    """Every zone of the game that opens a security box, from the room files: {base: [(room, zone)]}.
    Normal item boxes are objects bound at runtime; their places are learnt while playing."""
    out = {}
    for name in sorted(os.listdir(os.path.join(ROOT, 'data/rooms'))):
        if not name.endswith('.RDT'):
            continue
        try:
            room = Room(name)
            zones = room_static(name)['zones']
        except Exception:
            continue
        state = dict(interactions=zones)
        for z in zones:
            if (z['type'] == 3 and zone_reactions(room, z['index'])) or (z['type'] == 4 and z['attribute'] & 0xc):
                base = zone_box(room, z['index'], state)
                if base in (192, 224):
                    out.setdefault(base, []).append((room.name, z['index']))
    return out


def zone_labels(room, state):
    """Short label per active zone from the room data: its message, item, door target or script hook.
    Same sources as `hint.py room`, one line each, for the driver's situation text."""
    out = {}
    for a in state.get('interactions', []):
        if not a.get('flags', 0) & 1:
            continue
        i, t, prm = a['index'], a.get('type'), a.get('parameters', [0, 0])
        if t == 3 and a.get('attribute', 0) & 0x8000 and len(prm) > 1:
            text = ' '.join(room.message(prm[1]).replace('|', ' ').split())
            out[i] = 'text: "' + (text[:110] + '...' if len(text) > 110 else text) + '"'
        elif t == 3:
            refs = zone_reactions(room, i)
            box = zone_box(room, i, state) if refs else None
            out[i] = BOX_NAMES[box] if box else ('object the room script reacts to when examined'
                                                 if refs or a.get('attribute', 0) & 9 else
                                                 'object without text (examine it to see what it is)')
        elif t == 4 and a.get('attribute', 0) & 0xc:
            out[i] = BOX_NAMES[zone_box(room, i, state)]
        elif t == 4:
            it = next((r for r in state.get('room_items', []) if r.get('index') == prm[0]), None)
            out[i] = (f"item: {ITEMS.get(it['identity'], '?')}" + (' (taken)' if it.get('inactive') else '') +
                      (' (hidden)' if it.get('hidden') else '')) if it else 'object to examine'
        elif t == 0 and len(prm) > 1:
            out[i] = f'door to room {prm[1]}'
        elif t in (1, 2):
            out[i] = 'stairs/ladder/step'
    return out


def cmd_zone(i, state):
    room = Room(state['room']); a = next((z for z in state['interactions'] if z['index'] == i), None)
    if a is None: print('no zone', i); return
    kind = {0: 'DOOR', 1: 'STAIRS', 2: 'STAIRS2', 3: 'TEXT', 4: 'ITEM/OBJECT'}.get(a['type'], a['type'])
    locks = [n for bit, n in ((0x400, 'facing +z blocked'), (0x800, '-x blocked'), (0x1000, '-z blocked'), (0x2000, '+x blocked')) if a['attribute'] & bit]
    print(f"Zone i{i} {kind} {'on' if a['flags'] & 1 else 'OFF'} attr {a['attribute']:#x} prm {a['parameters']} {' '.join(locks)}")
    if a['type'] == 3 and a['attribute'] & 0x8000: print(f"  direct message {a['parameters'][1]}: \"{room.message(a['parameters'][1])}\"")
    if a['type'] == 3 and a['attribute'] & 9: print(f"  camera cut {a['parameters'][0]}; the script reacts via value[20]=={i}")
    if a['type'] == 4: print(f"  room item/object slot {a['parameters'][0]}")
    if a['type'] == 0: print(f"  door to room number {a['parameters'][1]} (stage {a['parameters'][0]}), spawn {a['parameters'][2]}")
    refs = list(find_refs(room, lambda c: reacts_to_zone(c, i)))
    tog = list(find_refs(room, lambda c: c[0] in (0x0b, 0x25) and c[1] == i))
    if not refs and not tog: print('  no script checks this zone')
    for s, o, _ in refs[:4]:
        print(f'  script reacts ({s} 0x{o:x}):'); print('\n'.join(block_after(room, s, o, state, 24)))
    for s, o, ctx in tog[:6]:
        print(f'  zone switched in {s} 0x{o:x}; preceded by:')
        for oo, dd, cc in ctx: print(f'    0x{oo:x} {annotate(room, cc, state)}')


def cmd_blocked(x, z, state):
    from pathfind import Grid
    pts, info = Grid(state).path(state['actor'][0], state['actor'][2], x, z)
    if pts: print(f'path exists ({info:.0f}) - not blocked'); return
    room = Room(state['room']); px, pz = state['actor'][0], state['actor'][2]
    cands = []
    for w in state['walls']:
        if not (w['fl'] & 1) or w['ty'] > 5: continue
        test = dict(state); test['walls'] = [dict(v, fl=0) if v['i'] == w['i'] else v for v in state['walls']]
        p2, _ = Grid(test).path(px, pz, x, z)
        if p2: cands.append(w)
    if not cands: print('no single wall blocks - check several walls, floor level or stairs (hint.py room)'); return
    scripted = [w for w in cands if any(True for _ in find_refs(room, lambda c, wi=w['i']: c[0] == 0x0a and c[1] == wi))]
    fixed = [w for w in cands if w not in scripted]
    if fixed: print('fixed walls on the way (detour or other access needed): ' + ' '.join(f"w{w['i']}({w['p'][0]:.0f},{w['p'][2]:.0f})" for w in fixed))
    for w in scripted:
        print(f"blocking wall w{w['i']} ty{w['ty']} p=({w['p'][0]:.0f},{w['p'][2]:.0f}) e=({w['e'][0]:.0f},{w['e'][2]:.0f}) attr {w['attr']:#x}")
        refs = list(find_refs(room, lambda c, wi=w['i']: c[0] == 0x0a and c[1] == wi))
        if not refs: print('  no script switches it (fixed, or an object/enemy wall)')
        for s, o, ctx in refs:
            print(f"  {s} 0x{o:x} {annotate(room, room.d[room.ev + o:room.ev + o + 4], state)}; preceded by:")
            for oo, dd, cc in ctx: print(f'    0x{oo:x} {annotate(room, cc, state)}')
        trig = [s for s, _, _ in refs]
        for s in sorted(set(trig)):
            if s.startswith('event'):
                n = int(s[5:]); starters = list(find_refs(room, lambda c, n=n: c[0] == 0x14 and c[3] == n))
                for ss, oo, ctx in starters[:2]:
                    print(f'  {s} is started in {ss} 0x{oo:x}; conditions before:')
                    for o3, d3, c3 in ctx: print(f'    0x{o3:x} {annotate(room, c3, state)}')


def cmd_room(state):
    room = Room(state['room']); print(f"{room.name}: {len(state['interactions'])} zones, {room.u32(room.msg)} messages")
    for a in state['interactions']:
        if not (a['flags'] & 1): continue
        k = {0: 'DOOR', 1: 'STAIRS', 2: 'STAIRS2', 3: 'TEXT', 4: 'ITEM'}.get(a['type'], a['type'])
        cx, cz = a['position'][0] + a['extent'][0] / 2, a['position'][2] + a['extent'][2] / 2
        what = ''
        if a['type'] == 3 and a['attribute'] & 0x8000: what = '"' + room.message(a['parameters'][1])[:70] + '"'
        elif a['type'] == 3:
            refs = list(find_refs(room, lambda c, i=a['index']: c[0] == 0x06 and c[1] == 20 and c[3] == i))
            what = f"script {refs[0][0]} 0x{refs[0][1]:x}" if refs else 'no script'
        elif a['type'] == 4:
            it = next((r for r in state['room_items'] if r['index'] == a['parameters'][0]), None)
            what = (f"Item {it['identity']} {ITEMS.get(it['identity'], '?')}" + (' (taken)' if it['inactive'] else '') + (' (hidden)' if it['hidden'] else '')) if it else f"object {a['parameters'][0]}"
        elif a['type'] == 0: what = f"door -> room {a['parameters'][1]}"
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
    if len(r) > 40: print(f'... {len(r) - 40} more')


CONDITION_OPS = {0x04, 0x06, 0x07, 0x0d, 0x0e, 0x10, 0x11}  # bhCk bhCmpB bhCmpW bhDieCk bhItmCk bhUseItemCheck bhPlItemCheck


def enclosing_conditions(cmds, k):
    """Conditions of the IF blocks around command k: [(negated, [condition commands])], innermost first.

    By byte range as event.c runs it: bhIfelCk at o with length L sends a failed test to o+2+L, so its
    true part is [o+2, o+L) with a bhEndif or bhElseCk at o+L; a bhElseCk at e with length Le skips
    [e+2, e+Le), its false part."""
    at = cmds[k][0]
    by_offset = {o: c for o, _, c in cmds}
    out = []
    for j in range(k - 1, -1, -1):
        o, _, c = cmds[j]
        if c[0] != 0x01:
            continue
        end = o + c[1]
        negated = None
        if o + 2 <= at < end:
            negated = False
        else:
            tail = by_offset.get(end)
            if tail is not None and tail[0] == 0x02 and end + 2 <= at < end + tail[1]:
                negated = True
        if negated is None:
            continue
        conds = []
        for o2, _, c2 in cmds[j + 1:]:
            if c2[0] not in CONDITION_OPS:
                break
            conds.append(c2)
        out.append((negated, conds))
    return out


def inventory_ids(state):
    return {(v >> 16) & 255 for v in (state or {}).get('items', [])[1:] if v}


def condition_holds(c, state):
    """True/False for flag tests with a known current value, None otherwise."""
    if c[0] == 0x04:
        now = flag_now(state, c[1], (c[2] | (c[3] << 8)) & 0x3ff)
        return None if now == '?' else now == 1 - c[5]
    if c[0] == 0x11:
        return c[1] in inventory_ids(state)
    return None


# --- guide: next steps derived from the scripts ----------------------------------------------------------
def floor_area(room, n):
    """'floor area n (x a..b, z c..d)' from the room file; the area bhCheckFloorP reports as values[16]."""
    try:
        f = room_static(room.name + '.RDT')['floors'][n]
        x0, z0 = f['position'][0], f['position'][2]
        return f"floor area {n} (x {min(x0, x0 + f['extent'][0]):.0f}..{max(x0, x0 + f['extent'][0]):.0f}, " \
               f"z {min(z0, z0 + f['extent'][2]):.0f}..{max(z0, z0 + f['extent'][2]):.0f})"
    except (IndexError, KeyError, OSError, struct.error):
        return f'floor area {n}'


def condition_text(room, c, state):
    """A script condition as something to do or to have; (text, holds now: True/False/None)."""
    op = c[0]
    if op == 0x04 and c[1] == 10 and (c[2] & 0xff) == 23 and len(c) > 5:
        return (f'examine zone {c[4]}' if c[5] == 0 else f'examine another zone than {c[4]}'), None
    if op == 0x04 and c[1] == 10 and (c[2] & 0xff) == 22 and len(c) > 5:
        return (('stand in ' if c[5] == 0 else 'stand outside ') + floor_area(room, c[4])), None
    if op == 0x06 and c[1] == 20 and c[2] == 0:
        return f'examine zone {c[3]}', None
    if op == 0x07 and c[2] == 7:
        return ('answer Yes' if c[4] == 0 else 'answer No'), None
    if op == 0x10:
        return f'use {ITEMS.get(c[1], c[1])} there (from the inventory)', None
    if op == 0x11:
        return f'carry {ITEMS.get(c[1], c[1])}', c[1] in inventory_ids(state)
    if op == 0x04:
        m, b = c[1], (c[2] | (c[3] << 8)) & 0x3ff
        return f'F[{m}:{b}]=={1 - c[5]}', condition_holds(c, state)
    return annotate(room, c, state).split('-> ', 1)[-1], condition_holds(c, state)


def setters(room, m, b, value):
    """Where flag F[m:b] gets `value`: [(script, offset, [(negated, [conditions])])], with the start
    conditions of an event appended (the bhEvtOn that starts it)."""
    out = []
    for script in room.scripts():
        cmds = list(room.commands(script))
        for k, (o, d, c) in enumerate(cmds):
            if c[0] == 0x05 and c[1] == m and ((c[2] | (c[3] << 8)) & 0x3ff) == b and 1 - c[5] == value:
                conds = enclosing_conditions(cmds, k)
                if script.startswith('event'):
                    n = int(script[5:])
                    for s2 in room.scripts():
                        cmds2 = list(room.commands(s2))
                        for k2, (o2, d2, c2) in enumerate(cmds2):
                            if c2[0] == 0x14 and c2[3] == n:
                                out.append((script, o, conds + enclosing_conditions(cmds2, k2)))
                                break
                        else:
                            continue
                        break
                    else:
                        out.append((script, o, conds))
                else:
                    out.append((script, o, conds))
    return out


def how_to_set(room, m, b, value, state, depth=0, seen=None):
    """Plain steps that make F[m:b]==value, from the scripts (one line per way, at most three), the
    conditions already true left out, flags in them resolved one level further."""
    seen = seen if seen is not None else set()
    if (m, b, value) in seen or depth > 2:
        return []
    seen.add((m, b, value))
    found = setters(room, m, b, value)
    # A flag the room's item gate sets while the player carries any of its items (bhPlItemCheck).
    for g in item_gates(room, state):
        if g['flag'] == f'F[{m}:{b}]':
            carried = g.get('carried') or []
            return [f"carry any of {len(g['items'])} checked items (such as {', '.join(g['items'][:4])})" +
                    (f"; you carry {', '.join(carried)}" if carried else '; you carry none of them') if value
                    else f"carry none of the {len(g['items'])} checked items" +
                    (f"; you carry {', '.join(carried)}" if carried else '')]
    # An item check (bhPlItemCheck around bhSet, one IF per item): "carry any of these items".
    items = set()
    for script, off, conds in found:
        cs = [c for negated, group in conds for c in group if not negated]
        if len(cs) == 1 and cs[0][0] == 0x11:
            items.add(cs[0][1])
    if items and len(items) == len(found):
        have = [ITEMS.get(i, str(i)) for i in sorted(items & inventory_ids(state))]
        names = [ITEMS.get(i, str(i)) for i in sorted(items)]
        return [f"carry any of {len(names)} checked items (such as {', '.join(names[:4])})" +
                (f"; you carry {', '.join(have)}" if have else '; you carry none of them')]
    if not found and m == 10:
        return [f'F[10:{b}] is a state the game sets itself (an action such as examining or using an item), '
                'not a script of this room']
    ways = []
    for script, off, conds in found:
        steps, impossible = [], False
        for negated, cs in conds:
            texts = [condition_text(room, c, state) for c in cs]
            if negated:
                if all(t is True for _, t in texts) and texts:
                    impossible = True
                    break
                open_ = [x for x, t in texts if t is not True]
                if open_:
                    steps.append('not (' + ' and '.join(open_) + ')')
                continue
            for c, (x, t) in zip(cs, texts):
                if t is True:
                    continue
                if c[0] == 0x04 and not (c[1] == 10 and (c[2] & 0xff) in (22, 23)) and t is False and depth < 2:
                    m2, b2 = c[1], (c[2] | (c[3] << 8)) & 0x3ff
                    sub = how_to_set(room, m2, b2, 1 - c[5], state, depth + 1, seen)
                    steps.append(x + (' (' + sub[0] + ')' if sub else ''))
                else:
                    steps.append(x)
        if impossible:
            continue
        line = ', '.join(dict.fromkeys(steps)) or 'happens by itself in this room'
        if line not in ways:
            ways.append(line)
        if len(ways) == 3:
            break
    return ways


def door_openers(room, i, state):
    """How a switched-off door zone i gets switched on (bhEtcAtariSet on / bhEtcAtariSet2), as steps."""
    ways = []
    for script in room.scripts():
        cmds = list(room.commands(script))
        for k, (o, d, c) in enumerate(cmds):
            if (c[0] == 0x0b and c[1] == i and c[2] == 0) or (c[0] == 0x25 and c[1] == i):
                steps = []
                for negated, cs in enclosing_conditions(cmds, k):
                    for c2 in cs:
                        x, t = condition_text(room, c2, state)
                        if t is not True and not negated:
                            steps.append(x)
                line = ', '.join(dict.fromkeys(steps)) or 'a room event'
                if script.startswith('event'):
                    line += f' (event {script[5:]})'
                if line not in ways:
                    ways.append(line)
    return ways[:3]


def guide(state, done_zones=(), limit=12):
    """The next steps in the current room, derived from its scripts and the state: weapon, item gates,
    items to pick up, zones that act now, zones that need something first (with how to get it),
    closed doors (with what opens them), carried items and where they are used. No room knowledge."""
    room = Room(state['room'])
    out = []
    carried = inventory_ids(state)
    # 0. A gun left in a security box (guns: what ammunition loads into, native combine table).
    gun_ids = guns()
    if not carried & gun_ids:
        for base, key in ((192, 'security_items'), (224, 'security_items_2')):
            boxed = [(v >> 16) & 255 for v in state.get(key) or [] if v and ((v >> 16) & 255) in gun_ids]
            if boxed:
                places = [f'{r} zone {z}' for r, z in all_box_places().get(base, [])]
                out.append(f"You carry no gun: {', '.join(ITEMS.get(g, g) for g in boxed)} lies in security box "
                           f"{'A' if base == 192 else 'B'} (opened at {', '.join(places) or '?'}); take it out first.")
                break
    # 1. Item gates of this room (a passage open only without / with listed items).
    walls = {w['i']: w.get('fl', 0) & 1 for w in state.get('walls', [])}
    for r in room_rules(room, state):
        gate = sorted(int(w) for w in re.findall(r'w(\d+)', r['rule'].split(') opens')[0]))
        shut = [w for w in gate if walls.get(w) == 1]
        have = [ITEMS.get(i, str(i)) for i in carried if ITEMS.get(i) in set(r.get('all_items') or [])]
        if shut and have:
            out.append(f"Passage (walls {','.join('w%d' % w for w in gate)}) is closed while you carry "
                       f"{', '.join(have)}: put them into an item box here, then go through.")
        elif shut:
            out.append(f"Passage (walls {','.join('w%d' % w for w in gate)}) is closed although you carry none of "
                       "its items: something else keeps it shut (see the zones below).")
    # 2. Items lying here.
    zones = [a for a in state.get('interactions', []) if a.get('flags', 0) & 1]
    for a in zones:
        if a.get('type') == 4:
            it = next((x for x in state.get('room_items', []) if x.get('index') == a['parameters'][0]), None)
            if it and not it.get('inactive') and not it.get('hidden'):
                out.append(f"Pick up {ITEMS.get(it['identity'], it['identity'])} (zone {a['index']}).")
    # 3. Zones that act now / that need something first.
    for a in zones:
        i = a['index']
        if a.get('type') != 3 or i in done_zones or not zone_reactions(room, i):
            continue
        box = zone_box(room, i, state)
        eff = zone_effects(room, i, state, limit=260)
        eff = re.sub(r'standing on floor == (\d+)', lambda m_: 'standing in ' + floor_area(room, int(m_.group(1))), eff)
        if box:
            rest = ' | '.join(p for p in eff.split(' | ') if 'opens walls' in p or 'gives ' in p)
            if rest:
                out.append(f"Zone {i} ({BOX_NAMES[box].split(' (')[0]}) also: {rest}")
            continue
        if not eff:
            continue
        if eff.startswith('nothing now; would need '):
            needs = re.findall(r'F\[(\d+):(\d+)\]==(\d)', eff)
            hows = []
            for m, b, v in needs[:2]:
                ways = how_to_set(room, int(m), int(b), int(v), state)
                hows += [w for w in ways if w not in hows]
            if hows:
                out.append(f"Zone {i} does nothing yet; to make it work: " + ' OR '.join(hows[:2]) + '.')
            else:
                out.append(f"Zone {i} does nothing yet (needs {', '.join(f'F[{m}:{b}]=={v}' for m, b, v in needs)}; "
                           "set elsewhere or by an event).")
        elif any(k in eff for k in ('opens wall', 'gives ', 'starts event', 'zone i')):
            out.append(f"Examine zone {i}: {eff}")
    # 4. Doors that are switched off.
    static = {z['index']: z for z in room_static(room.name + '.RDT')['zones']}
    for i, z in sorted(static.items()):
        if z['type'] != 0:
            continue
        live = next((a for a in state.get('interactions', []) if a['index'] == i), None)
        if live and live.get('flags', 0) & 1:
            continue
        ways = door_openers(room, i, state)
        out.append(f"Door zone {i} (to room {z['parameters'][1]}) is closed" +
                   (': it opens when ' + ' OR '.join(ways) + '.' if ways else '.'))
    # 5. Carried items and where the game uses them.
    uses = all_item_uses()
    listed = set()
    for v in state.get('items', [])[1:]:
        item = (v >> 16) & 255
        if v and item in uses and item not in listed:
            listed.add(item)
            sites = []
            for r, z, f in uses[item][:3]:
                where = 'here' if r == room.name else r
                if z is not None:
                    where += f' zone {z}'
                if f is not None:
                    where += ' in ' + (floor_area(room, f) if r == room.name else f'floor area {f}')
                sites.append(where)
            out.append(f"{ITEMS.get(item, item)} is used at: {', '.join(sites)}.")
    return out[:limit]


def why(room, n, state, limit=1800):
    """Explain room event n from the script alone: what it does, the start that applies now, its cause."""
    carried = inventory_ids(state)
    starts = []
    for script in room.scripts():
        cmds = list(room.commands(script))
        for k, (o, d, c) in enumerate(cmds):
            if c[0] == 0x14 and c[3] == n:
                starts.append((script, o, enclosing_conditions(cmds, k)))
    # What the event does: texts, voice, items first; walls and zones summarised.
    said, other, walls_open, walls_close, zones = [], [], [], [], 0
    name = f'event{n}'
    if name in room.scripts():
        for o, d, c in room.commands(name):
            if c[0] == 0x1f and not c[1]:
                text = ' '.join(room.message(c[2]).replace('|', ' ').split())
                item = f'text "{text}"' if text else 'a voice-only line (no text in the game data)'
                if item not in said:
                    said.append(item)
            elif c[0] == 0x19 and 'voice' not in ' '.join(said):
                said.append('a voice line')
            elif c[0] in (0xb2, 0xc2):
                said.append(f'gives {ITEMS.get(c[1], c[1])}')
            elif c[0] == 0x10:
                said.append(f'uses {ITEMS.get(c[1], c[1])}')
            elif c[0] == 0x0a:
                (walls_close if not c[2] else walls_open).append(f'w{c[1]}')
            elif c[0] in (0x0b, 0x25):
                zones += 1
            elif c[0] == 0x14:
                other.append(f'starts event {c[3]}')
    if any('voice-only' in x for x in said):
        said = [x for x in said if x != 'a voice line']
    does = said + other
    if walls_open:
        does.append('opens walls ' + ','.join(walls_open))
    if walls_close:
        does.append('closes walls ' + ','.join(walls_close))
    if zones:
        does.append(f'switches {zones} zones')
    lines = [f'event {n} does: ' + ('; '.join(does) if does else 'nothing visible (camera/model/flags only)')]
    if not starts:
        lines.append('started by a scene, door or the system, not by a condition in this room\'s scripts')
    else:
        # The start whose flag tests hold now explains what just happened best.
        def score(st):
            tests = [condition_holds(c, state) for neg, conds in st[2] for c in conds if not neg]
            return sum(1 for t in tests if t is True) - sum(1 for t in tests if t is False)
        script, o, blocks = max(starts, key=score)
        conds = []
        flags = set()
        for negated, cs in blocks:
            for c in cs:
                conds.append(('not ' if negated else '') + annotate(room, c, state).split('-> ')[-1].replace('if ', ''))
                if c[0] == 0x04 and not negated:
                    flags.add((c[1], (c[2] | (c[3] << 8)) & 0x3ff, 1 - c[5]))
        lines.append(f'runs from {script} when: ' + ('; '.join(conds) if conds else 'always (no condition)'))
        for m, b, v in sorted(flags):
            items, setters = set(), set()
            for scr in room.scripts():
                cmds = list(room.commands(scr))
                for k, (o2, d2, c2) in enumerate(cmds):
                    if c2[0] == 0x05 and c2[1] == m and ((c2[2] | (c2[3] << 8)) & 0x3ff) == b and 1 - c2[5] == v:
                        setters.add(scr)
                        for _, cs in enclosing_conditions(cmds, k):
                            items |= {x[1] for x in cs if x[0] in (0x10, 0x11)}
            if items:
                have = sorted(ITEMS.get(i, str(i)) for i in items & carried)
                lines.append(f'cause F[{m}:{b}]={v}: set by {", ".join(sorted(setters))} while you carry any of {len(items)} '
                             f'items' + (f'; you carry {", ".join(have)}' if have else '; you carry none'))
    text = '\n'.join(lines)
    return text if len(text) <= limit else text[:limit] + ' ...'


def event_summary(room, n):
    """First line of why() without the condition part: what event n does."""
    return why(room, n, {}, limit=400).splitlines()[0].split('does: ', 1)[-1]


def item_gates(room, state):
    """Generic item checks of a room: a flag that a script sets while the player carries any item of a list
    (bhPlItemCheck around bhSet), and the events whose start depends on that flag. Returns
    [dict(flag, items, carried, with_items, without_items)] with event summaries; read from the scripts only."""
    carried = inventory_ids(state)
    gates = {}
    for script in room.scripts():
        cmds = list(room.commands(script))
        for k, (o, d, c) in enumerate(cmds):
            if c[0] != 0x05:
                continue
            m, b, v = c[1], (c[2] | (c[3] << 8)) & 0x3ff, 1 - c[5]
            items = {x[1] for _, cs in enclosing_conditions(cmds, k) for x in cs if x[0] == 0x11}
            if items:
                g = gates.setdefault((m, b, v), set())
                g |= items
    out = []
    for (m, b, v), items in gates.items():
        on, off = set(), set()
        for script in room.scripts():
            cmds = list(room.commands(script))
            for k, (o, d, c) in enumerate(cmds):
                if c[0] != 0x14:
                    continue
                for negated, cs in enclosing_conditions(cmds, k):
                    for x in cs:
                        if x[0] == 0x04 and x[1] == m and ((x[2] | (x[3] << 8)) & 0x3ff) == b:
                            holds_when_set = (1 - x[5]) == v
                            (on if holds_when_set != negated else off).add(c[3])
        def describe(evs):
            res = []
            for n in sorted(evs):
                text = event_summary(room, n)
                if not text.startswith('nothing visible'):
                    res.append(f'event {n}: {text}')
            return res[:4]
        out.append(dict(flag=f'F[{m}:{b}]', items=sorted(ITEMS.get(i, str(i)) for i in items),
                        carried=sorted(ITEMS.get(i, str(i)) for i in items & carried),
                        with_items=describe(on), without_items=describe(off),
                        with_items_all=[f'event {n}:' for n in sorted(on)],
                        without_items_all=[f'event {n}:' for n in sorted(off)]))
    return out


def event_walls(room, n, seen=None):
    """Walls event n (and events it starts) opens and closes: (opened, closed) sets of wall indices."""
    seen = seen if seen is not None else set()
    if n in seen or f'event{n}' not in room.scripts():
        return set(), set()
    seen.add(n)
    opened, closed = set(), set()
    for o, d, c in room.commands(f'event{n}'):
        if c[0] == 0x0a:
            (opened if c[2] else closed).add(c[1])
            (closed if c[2] else opened).discard(c[1])
        elif c[0] == 0x14:
            o2, c2 = event_walls(room, c[3], seen)
            opened |= o2; closed |= c2
    return opened, closed


def room_rules(room, state):
    """Short, model-facing rules of a room derived from its scripts: item checks whose branches open or
    close walls (a passage that depends on what the player carries)."""
    rules = []
    for g in item_gates(room, state):
        def walls(evs):
            o, c = set(), set()
            for line in evs:
                n = int(line.split(':')[0].split()[1])
                o2, c2 = event_walls(room, n)
                o |= o2; c |= c2
            return o, c
        o_with, c_with = walls(g['with_items_all'])
        o_without, c_without = walls(g['without_items_all'])
        # A passage gated by the items: some event of the 'with items' branch closes it and some event of the
        # 'without items' branch opens it (the branches run in order, so both may also touch it otherwise).
        gate = c_with & o_without
        if not gate:
            continue
        n = len(g['items'])
        names = ', '.join(g['items'][:10]) + (', ...' if n > 10 else '')
        now = (f"You carry {', '.join(g['carried'])}, so it stays CLOSED." if g['carried']
               else 'You carry none of them, so it OPENS.')
        if g['carried']:
            names = ', '.join(g['carried']) + f' (and {n - len(g["carried"])} other listed items)'
        rules.append(dict(
            rule=(f"A passage here (walls {', '.join('w%d' % w for w in sorted(gate))}) opens only if you carry "
                  f"NONE of these {n} items: {names}. {now}"),
            how=("Put every listed item you carry into an item box (op box store), keep only what is not on "
                 "the list, then walk through. Take the items back from a box on the other side if the game "
                 "allows it (its texts say so)."),
            all_items=g['items']))
    return rules


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
    elif c == 'why':
        st = load_state(a[2] if len(a) > 2 else auto); print(why(Room(st['room']), int(a[1]), st))
    elif c == 'rules':
        st = load_state(a[1] if len(a) > 1 else auto); print(json.dumps(room_rules(Room(st['room']), st), indent=1))
    elif c == 'msg': room = Room(a[2] if len(a) > 2 else load_state(auto)['room']); print(room.message(int(a[1])))
    elif c == 'guide':
        st = load_state(a[1] if len(a) > 1 else auto)
        for line in guide(st):
            print('- ' + line)
    elif c == 'need':
        st = load_state(a[2] if len(a) > 2 else auto)
        m, b, v = re.match(r'F?\[?(\d+):(\d+)\]?(?:==(\d))?', a[1]).groups()
        for w in how_to_set(Room(st['room']), int(m), int(b), int(v or 1), st):
            print('- ' + w)
    elif c == 'near':
        st = load_state(a[2] if len(a) > 2 else auto) if not (len(a) > 1 and a[1].startswith('RM_')) else None
        room = a[1] if len(a) > 1 and a[1].startswith('RM_') else st['room']
        for name, dist, via in neighbours(door_graph(), room, 3):
            items = [ITEMS.get(i, str(i)) for i in room_static(name + '.RDT')['items'] if i]
            print(f"{name}: {dist} door(s) away via zone {via}; items in its file: {', '.join(items) or 'none'}")
    elif c == 'uses':
        st = load_state(a[1] if len(a) > 1 else auto)
        uses = all_item_uses()
        for v in st.get('items', [])[1:]:
            if v and ((v >> 16) & 255) in uses:
                print(ITEMS.get((v >> 16) & 255, '?') + ': ' + ', '.join(
                    f"{r}" + (f" zone {z}" if z is not None else '') + (f" floor area {f}" if f is not None else '')
                    for r, z, f in uses[(v >> 16) & 255]))
    elif c == 'op': cmd_op(a[1])
    elif c == 'grep': cmd_grep(a[1])
    else: print(__doc__)


if __name__ == '__main__': main()
