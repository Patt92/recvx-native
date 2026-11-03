#!/usr/bin/env python3
"""Static reach scan of all room scripts: which script commands and controller sub-commands exist at all.

The headless room scan (tools/scan_event_rooms.py) runs 600 ticks without input, so it only reaches the opening
branches of each script. Cutscene, lift and grab paths run after player actions and stay unseen there. This
tool instead walks every script of every room statically: both branches of every IF (bhIfelCk/bhElseCk),
every event started by bhEvtOn (0x14), and every work selected by bhLoadWork/bhLoadWork2. It decodes with the
same instruction lengths as native/events.cpp (EventVm::instruction) and the original dispatch tables of
event.c: Player_controll (0x64), Sub_controll (0x67), Obj_controll (0x66) and Common_controll (0x69).

Output: per command (and per controller sub-command) the rooms and script offsets where it occurs, and the
commands that the native EventVm/world binding does not know (these stall a task for the rest of the room,
because EventVm::run marks the task blocked on an unimplemented command). It is a decode of the script bytes,
not a gameplay proof: a reached command may still be dead in play.

Byte lengths of Player_controll/Sub_controll are read off event.c (bhScePtr++ / += n per case) and listed in
PL_LEN/SUB_LEN below with the line of the case label."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

# Original Player_controll case lengths (whole instruction incl. the 0x64 byte): event.c line of "case N:".
PL_LEN = {0: 2,      # :9702
          7: 8,      # :9653 ++ then 3x +=2
          12: 6, 13: 6,  # :9475/:9491 5x ++
          52: 2,     # :9697
          128: 2,    # :9632 falls into 139 (++)
          129: 12,   # :9290
          130: 2,    # :9353
          131: 6,    # :9391
          132: 6,    # :9507 5x ++
          137: 4,    # :9602 ++, +=2
          139: 2,    # :9646
          140: 8,    # :9425 3x ++, 2x +=2
          141: 6,    # :9358 5x ++
          142: 2,    # :9387
          143: 4,    # :9544 3x ++
          145: 6,    # :9563 5x ++
          150: 4}    # :9270 ++, +=2
# Original Sub_controll case lengths: :9762 128 (falls into 139 ++), 139 ++, 144 ++, 143 (++ 4x ++ +=2 = 8? see below)
SUB_LEN = {0: 2,      # :9945
           128: 2,    # :9762 falls into 139
           139: 2,    # :9782
           143: 6,    # :9804 ++ mode0 ++ mode1 ++ mode2 ++ type ++ = 6 (native events.cpp 0x8f: 6)
           144: 2,    # :9788
           146: 8,    # :9846 (native 0x92: 8)
           147: 8,    # :9889 (native 0x93: 8)
           148: 4,    # :9912
           149: 4}    # :9925
# Native-known controller sub-commands (events.cpp case 0x64/0x67 switches); the rest stall a task.
NATIVE_PL = {7, 12, 13, 0x80, 0x34, 0x8b, 0, 0x83, 0x89, 150, 129, 130, 142}
NATIVE_SUB = {0, 0x80, 0x8b, 0x90, 0x8f, 0x92, 0x93, 0x94, 0x95}


def common_len(cmd, b2, b3):
    """EventVm::common_controll_length (native/events.cpp), 0 = unknown."""
    if cmd in (0, 36, 47): return 2
    if cmd in (2, 3, 4, 17, 18, 19, 28, 29, 30, 31, 32, 33): return 3
    if cmd in (14, 21, 23, 40, 41, 42, 46, 48, 49, 50, 52, 53): return 4
    if cmd in (43, 44, 45): return 5
    if cmd in (5, 6, 9, 11, 12, 13, 15, 16, 51, 132, 141): return 6
    if cmd in (10, 22): return 6 if b2 <= 2 else 2
    if cmd in (7, 8, 27, 140): return 8
    if cmd in (26, 38): return 10
    if cmd == 24: return 8 if b3 <= 3 else 3
    if cmd == 25: return 8 if b3 <= 1 else 3
    if cmd == 34: return 10 if b3 <= 3 else 3
    if cmd == 35: return 8 if b3 == 0 else 9 if b3 == 1 else 3
    return 0


# Fixed lengths per opcode as native/events.cpp advances them (advance(n) / world(..., n)); None = handled
# structurally below. Derived by reading the decoder; opcodes absent here are reported as "unknown".
FIXED = {
    0xf8: 4,  # bhSleep
    0x3: 2, 0x4: 6, 0x5: 6, 0x6: 4, 0x7: 6, 0x8: 4, 0x9: 4, 0xa: 4, 0xb: 4, 0xc: 4,
    0xd: 4, 0xe: 6, 0xf: 2, 0x10: 2, 0x11: 2, 0x12: 2, 0x13: 4, 0x14: 4, 0x15: 4, 0x16: 2,
    0x17: 8, 0x18: 2, 0x19: 8, 0x1a: 2, 0x1b: 6, 0x1c: 6, 0x1d: 4, 0x1e: 6, 0x1f: 4, 0x20: 4,
    0x21: 6, 0x22: 4, 0x23: 6, 0x24: 4, 0x25: 10, 0x26: 2, 0x27: 2, 0x28: 2, 0x29: 2, 0x2a: 4,
    0x2b: 2, 0x2c: 4, 0x2d: 2, 0x2e: 2, 0x2f: 4, 0x30: 4, 0x31: 2, 0x32: 12, 0x33: 8, 0x34: 12,
    0x35: 4, 0x36: 6, 0x37: 2, 0x38: 6, 0x39: 4, 0x3a: 2, 0x3b: 6, 0x3c: 4, 0x3d: 4, 0x3e: 4,
    0x3f: 4, 0x40: 8, 0x41: 2, 0x42: 4, 0x43: 4, 0x44: 6, 0x45: 2, 0x46: 8, 0x47: 10, 0x48: 6,
    0x49: 10, 0x4a: 8, 0x4b: 4, 0x4c: 6, 0x4d: 4, 0x4e: 14, 0x4f: 6, 0x50: 2, 0x51: 12, 0x52: 12,
    0x53: 12, 0x54: 12, 0x55: 2, 0x56: 6, 0x57: 2, 0x58: 14, 0x59: 12, 0x5a: 4, 0x5b: 2, 0x5c: 2,
    0x5d: 6, 0x5e: 4, 0x5f: 2, 0x60: 6, 0x61: 2, 0x62: 6, 0x63: 2, 0x65: 4, 0x68: 6, 0x6a: 2,
    0x6b: 2, 0x6c: 8, 0x6d: 22, 0x6e: 22, 0x6f: 12, 0x70: 2, 0x71: 2, 0x72: 14, 0x73: 22, 0x74: 4,
    0x75: 4, 0x76: 4, 0x77: 6, 0x78: 14, 0x79: 6, 0x7a: 2, 0x7b: 10, 0x7c: 10, 0x7d: 4, 0x7e: 4,
    0x7f: 6, 0x80: 4, 0x81: 2, 0x82: 4, 0x83: 2, 0x84: 8, 0x85: 4, 0x86: 16, 0x87: 2, 0x88: 10,
    0x89: 2, 0x8a: 2, 0x8b: 12, 0x8c: 6, 0x8d: 2, 0x8e: 4, 0x8f: 4, 0x90: 2, 0x91: 4, 0x92: 2,
    0x93: 2, 0x94: 4, 0x95: 4, 0x96: 2, 0x97: 8, 0x98: 12, 0x99: 4, 0x9a: 4, 0x9b: 6, 0x9c: 2,
    0x9d: 4, 0x9e: 2, 0x9f: 2, 0xa0: 6, 0xa1: 2, 0xa2: 4, 0xa3: 12, 0xa4: 4, 0xa5: 4, 0xa6: 4,
    0xa7: 4, 0xa8: 10, 0xa9: 2, 0xaa: 10, 0xab: 6, 0xac: 6, 0xad: 4, 0xae: 2, 0xaf: 4, 0xb0: 22,
    0xb1: 6, 0xb2: 4, 0xb3: 8, 0xb4: 4, 0xb5: 4, 0xb6: 2, 0xb7: 2, 0xb8: 2, 0xb9: 10, 0xba: 16,
    0xbb: 2, 0xbc: 2, 0xbd: 2, 0xbe: 6, 0xbf: 4, 0xc0: 28, 0xc1: 2, 0xc2: 6, 0xc3: 4, 0xc4: 8,
    0xc5: 2, 0xc6: 2, 0xc7: 4, 0xc8: 8, 0xc9: 4, 0xca: 2, 0xcb: 2, 0xcc: 4, 0xcd: 2, 0xce: 2,
    0xcf: 2, 0xd0: 4, 0xd1: 10, 0xd2: 10, 0xd3: 2, 0xd4: 4, 0xf3: 1, 0xf4: 1, 0xf9: 3, 0xfa: 4,
    0xfb: 2, 0xfd: 1, 0xfe: 1, 0xff: 2,
}
# dm0 slots: a task stays here forever in the original too.
DM0 = set(range(0xd5, 0xf3)) | {0xf5, 0xf6, 0xf7}
TABLES = {}


def room_tables(b):
    """Event section base, scd0, scd1 and the Event_init script offsets (EventVm constructor layout)."""
    u32 = lambda p: int.from_bytes(b[p:p+4], 'little')
    base = u32(28)  # native/room.cpp inspect_room: event section offset
    header = u32(base)
    scripts = [base + u32(base + p) for p in range(8, header, 4)]
    return base, base + header, base + u32(base + 4), scripts


def tables(b):
    """Table sizes the native port validates script operands against (native/world.cpp, native/events.cpp)."""
    u32 = lambda p: int.from_bytes(b[p:p+4], 'little')
    h = u32(16)  # room header (inspect_room room_offset)
    return {'interactions': u32(h + 0x9c), 'floors': u32(h + 0xa0), 'positions': u32(h + 0xa4), 'walls': u32(h + 0x98),
            'effects': u32(h + 0x94), 'objects': u32(h + 0x8c), 'items': u32(h + 0x90), 'enemies': u32(h + 0x88),
            'lights': u32(h + 0x1c) and 0}  # lights: counted by the room-light loader, not needed here


# Opcode -> (operand byte offset within the instruction, table, native throw message). A real script operand
# beyond the table makes EventVm/RoomWorld throw and blocks the task (docs: "task blocked ... for the room").
INDEXED = {
    0x0b: (1, 'interactions', 'script interaction outside room (events.cpp bhEtcAtariSet)'),
    0x25: (1, 'interactions', 'script interaction outside room (events.cpp bhEtcAtariChange)'),
    0x0c: (1, 'floors', 'script floor outside room (events.cpp bhFlrAtariSet)'),
    0x7c: (1, 'floors', 'script floor outside room (events.cpp bhFlrAtariChange)'),
    0x0a: (1, 'walls', 'script wall outside table (world.cpp bhWalAtariSet)'),
    0xb9: (1, 'walls', 'script ground position outside wall table (world.cpp)'),
    0x43: (1, 'effects', 'script effect outside original room table (world.cpp bhEffDispSet)'),
    0x88: (1, 'effects', 'script effect outside original room table (world.cpp bhEffUVSet)'),
    0x91: (1, 'effects', 'script effect outside original room table (world.cpp)'),
    0xaa: (1, 'effects', 'script effect outside original room table (world.cpp bhEffUVSet2)'),
    0xad: (1, 'effects', 'script effect outside original room table (world.cpp)'),
    0xd1: (1, 'effects', 'script effect outside original room table (world.cpp)'),
    0xd2: (1, 'effects', 'script effect outside original room table (world.cpp)'),
}


# Value domains the native VM rejects with an exception (native/events.cpp); the original switch has the same
# cases, so a script byte outside them would be a decode error here, not a gameplay path. Checked per opcode:
# (byte offset, predicate over (byte, TABLES), description).
VALUES = {
    0x04: [(1, lambda v, t: 1 <= v <= 16, 'bhCk matrix (event.c:443-540, flag() matrices 1..16)')],
    0x05: [(1, lambda v, t: 1 <= v <= 16, 'bhSet matrix'), (5, lambda v, t: v <= 5, 'bhSet mode (set_flag mode <= 5)')],
    0x06: [(1, lambda v, t: v in (0, 1, 2, 15, 8, 17, 21, 23, 24, 25), 'bhCmpB key (event.c:680-715)'),
           (2, lambda v, t: v <= 5, 'bhCmpB comparison (event.c:717-740)')],
    0x07: [(2, lambda v, t: v in (5, 7, 9, 10), 'bhCmpW field (event.c:759-770, native 5/7/9/10)'),
           (3, lambda v, t: v <= 5, 'bhCmpW comparison')],
    0x0e: [(4, lambda v, t: v < t.get('interactions', 1 << 30), 'bhItmCk interaction < interactions')],
    0x12: [(1, lambda v, t: v <= 5, 'bhCineSet mode')],
    0x5d: [(2, lambda v, t: v <= 6, 'bhPadCheck mode (event.c:4766)')],
    0x63: [(1, lambda v, t: v != 0, 'bhRandamSet modulus != 0')],
    0xb3: [(2, lambda v, t: v < t.get('interactions', 1 << 30), 'bhEtcAtariEnePosSet interaction'),
           (3, lambda v, t: v < t.get('positions', 1 << 30), 'bhEtcAtariEnePosSet position'),
           (5, lambda v, t: v < 4, 'bhEtcAtariEnePosSet evt_posno slot < 4')],
    0xb4: [(1, lambda v, t: v < 4, 'bhEtcAtariEvtPosSet evt_posno slot < 4'),
           (2, lambda v, t: v < t.get('interactions', 1 << 30), 'bhEtcAtariEvtPosSet interaction')],
    0xb7: [(1, lambda v, t: v < 4, 'bhSecurityBox player < 4')],
    0xbc: [(1, lambda v, t: v < 16, 'bhEvtKill level < 16')],
}
def walk(b, base, start, seen, found, room, unknown, kind_hint, kinds):
    """Depth-first over the byte stream; seen = visited (offset, work kind)."""
    stack = [(start, kind_hint)]
    u8 = lambda p: b[p]
    u16 = lambda p: int.from_bytes(b[p:p+2], 'little')
    while stack:
        p, kind = stack.pop()
        while True:
            if (p, kind) in seen or p >= len(b): break
            seen.add((p, kind))
            op = u8(p)
            where = f'{room}@{p-base:04x}'
            if op in DM0:
                found[('dm0', op)].append(where); break
            if op == 0x00:  # bhEnd
                break
            if op == 0x01:  # bhIfelCk: both branches
                stack.append((p + 2 + u8(p + 1), kind)); p += 2; continue
            if op == 0x02:  # bhElseCk: jump
                p = p + u8(p + 1); continue
            if op == 0x14:  # bhEvtOn: start event (level, event)
                packed = u16(p + 2); event = packed >> 8
                found[('evton', event)].append(where)
                kinds.setdefault(event, set()).add(kind)
                p += 4; continue
            if op in (0x65, 0x68):  # bhLoadWork/bhLoadWork2: work kind for the following controller commands
                kind = u8(p + 1); p += 4 if op == 0x65 else 6; continue
            if op == 0x64:
                sub = u8(p + 1); found[('pl', sub)].append(where)
                if sub == 137 and u8(p + 2) >= TABLES.get('positions', 1 << 30):
                    unknown[('operand', f'Player_controll 137 POS {u8(p + 2)} >= positions[{TABLES["positions"]}]')].append(where)
                if sub not in NATIVE_PL: unknown[('Player_controll', sub)].append(where)
                length = PL_LEN.get(sub)
                if length is None: unknown[('Player_controll length', sub)].append(where); break
                p += length; continue
            if op == 0x67:
                sub = u8(p + 1); found[('sub', sub)].append(where)
                if sub not in NATIVE_SUB: unknown[('Sub_controll', sub)].append(where)
                length = SUB_LEN.get(sub)
                if length is None: unknown[('Sub_controll length', sub)].append(where); break
                p += length; continue
            if op == 0x66:
                sub = u8(p + 1); found[('obj', sub)].append(where)
                p += {1: 2, 20: 4}.get(sub, 1); continue
            if op == 0x69:
                sub = u8(p + 1); found[('common', sub)].append(where)
                length = common_len(sub, u8(p + 2), u8(p + 3))
                if not length: unknown[('Common_controll', sub)].append(where); break
                p += length; continue
            if op == 0xfc:  # bhWhile (event.c:9132): condition at p+2, body at p+rel (bhScePtr = &bhScePtr[bhScePtr[1]]); walk both
                stack.append((p + 2, kind)); p = p + u8(p + 1); continue
            if op == 0xff:  # bhEvtEnd
                break
            if op in INDEXED:
                at, table, why = INDEXED[op]
                size = TABLES.get(table, 1 << 30)
                if u8(p + at) >= size:
                    unknown[('operand', f'{hex(op)} index {u8(p + at)} >= {table}[{size}] -> {why}')].append(where)
            for at, ok, why in VALUES.get(op, ()):
                if not ok(u8(p + at), TABLES):
                    unknown[('operand', f'{hex(op)} byte {at} = {u8(p + at)} outside {why}')].append(where)
            if op == 0x64 and u8(p + 1) == 137 and u8(p + 2) >= TABLES.get('positions', 1 << 30):
                unknown[('operand', f'Player_controll 137 POS {u8(p + 2)} >= positions[{TABLES["positions"]}]')].append(where)
            length = FIXED.get(op)
            if length is None:
                unknown[('opcode', op)].append(where); break
            found[('op', op)].append(where)
            p += length


def scan(data):
    """All rooms under data/rooms: (room count, found sites per command, unknown sites per gap)."""
    found, unknown = defaultdict(list), defaultdict(list)
    rooms = 0
    for f in sorted((Path(data) / 'rooms').glob('RM_*.RDT')):
        b = f.read_bytes(); room = f.stem
        try:
            base, scd0, scd1, scripts = room_tables(b)
        except Exception as e:  # noqa: BLE001
            unknown[('room', room)].append(str(e)); continue
        rooms += 1; seen = set(); kinds = {}
        global TABLES
        TABLES = tables(b)
        walk(b, base, scd0, seen, found, room, unknown, None, kinds)
        walk(b, base, scd1, seen, found, room, unknown, None, kinds)
        for index, start in enumerate(scripts):
            walk(b, base, start, seen, found, room, unknown, None, kinds)
    return rooms, found, unknown


def main():
    a = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    a.add_argument('--data', type=Path, default=ROOT / 'data')
    a.add_argument('--json', type=Path)
    a.add_argument('--show', help='command key to list all sites, e.g. pl:132 sub:146 op:0x7d')
    args = a.parse_args()
    rooms, found, unknown = scan(args.data)
    print(f'{rooms} rooms')
    def rooms_of(sites): return len({s.split("@")[0] for s in sites})
    for group, label in (('pl', 'Player_controll'), ('sub', 'Sub_controll'), ('obj', 'Obj_controll'), ('common', 'Common_controll')):
        keys = sorted(k for k in found if k[0] == group)
        print(f'\n{label}: ' + ', '.join(f'{k[1]}({rooms_of(found[k])}r/{len(found[k])})' for k in keys))
    print('\nNOT KNOWN TO THE NATIVE PORT (a task reaching these stalls for the rest of the room):')
    for k in sorted(unknown, key=lambda k: (k[0], str(k[1]))):
        sites = unknown[k]
        print(f'  {k[0]} {k[1] if k[0]=="room" else (hex(k[1]) if k[0]=="opcode" else k[1])}: {rooms_of(sites)} rooms, {len(sites)} sites, e.g. {", ".join(sites[:6])}')
    if args.show:
        g, n = args.show.split(':'); n = int(n, 0)
        print(f'\n{args.show}: ' + ', '.join(found[(g, n)]))
    if args.json:
        args.json.write_text(json.dumps({f'{k[0]}:{k[1]}': v for k, v in {**found, **{('unknown ' + k[0], k[1]): v for k, v in unknown.items()}}.items()}, indent=1))
    if not rooms:
        print('no rooms under', args.data / 'rooms'); return 77
    return 1 if unknown else 0


if __name__ == '__main__':
    sys.exit(main())
