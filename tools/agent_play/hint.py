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
  hint.py op <hex|bhName>             where a script opcode is implemented (port and original reference)
  hint.py grep <regex>                bounded search in native/ and src/ps2/veronica/prog/ (max 40 hits)
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
        m, b, mode = u8(1), u16(2) & 0x3ff, u8(5); note = f'if F[{m}:{b}]=={1 - mode} (now {flag_now(state, m, b)})'
    elif op == 0x05:
        m, b, mode = u8(1), u16(2) & 0x3ff, u8(5); note = f'F[{m}:{b}] := {1 - mode} (now {flag_now(state, m, b)})'
    elif op == 0x06:
        key, mode, val = u8(1), u8(2), u8(3); cmp = ['==', '>', '>=', '<', '<=', '!='][mode] if mode < 6 else '?'
        note = f"value[{key}{' zone' if key == 20 else ''}] {cmp} {val}"
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
        if any(True for _ in find_refs(room, lambda c, i=i: (c[0] == 0x06 and c[1] == 20 and c[3] == i) or
                                        (c[0] == 0x0e and len(c) > 4 and c[4] == i))):
            continue
        out.add(i)
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
    refs = list(find_refs(room, lambda c: (c[0] == 0x06 and c[1] == 20 and c[3] == i) or (c[0] == 0x0e and len(c) > 4 and c[4] == i)))
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
    elif c == 'op': cmd_op(a[1])
    elif c == 'grep': cmd_grep(a[1])
    else: print(__doc__)


if __name__ == '__main__': main()
