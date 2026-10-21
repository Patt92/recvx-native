#!/usr/bin/env python3
"""Static walk of every room's event scripts (scd0, scd1, events).

Lengths follow the native dispatcher (native/events.cpp) and, for the
Player/Sub/Common/Obj controllers, event.c. Branches and loops are inline in
the byte stream, so a linear walk from each script start to the next script
offset visits every command without executing anything. The result counts
commands per opcode/subcommand and lists the rooms, events and offsets of the
requested ones. No gameplay proof; a command may still be unreachable.
"""
import argparse
import collections
import json
import struct
from pathlib import Path

LEN = {
    0x00: 1, 0x01: 2, 0x02: 2, 0x03: 2, 0x04: 6, 0x05: 6, 0x06: 4, 0x07: 6, 0x08: 4, 0x09: 4,
    0x0a: 4, 0x0b: 4, 0x0c: 4, 0x0d: 4, 0x0e: 6, 0x0f: 2, 0x10: 2, 0x11: 2, 0x12: 2, 0x13: 4,
    0x14: 4, 0x15: 4, 0x16: 2, 0x17: 8, 0x18: 2, 0x19: 8, 0x1a: 2, 0x1b: 6, 0x1c: 6, 0x1d: 4,
    0x1e: 6, 0x1f: 4, 0x20: 4, 0x21: 6, 0x22: 4, 0x23: 6, 0x24: 4, 0x25: 10, 0x26: 2, 0x27: 2,
    0x28: 2, 0x29: 2, 0x2a: 4, 0x2b: 2, 0x2c: 4, 0x2d: 2, 0x2e: 2, 0x2f: 4, 0x30: 4, 0x31: 2,
    0x32: 12, 0x33: 8, 0x34: 12, 0x35: 4, 0x36: 6, 0x37: 2, 0x38: 6, 0x39: 4, 0x3a: 2, 0x3b: 6,
    0x3c: 4, 0x3d: 4, 0x3e: 4, 0x3f: 4, 0x40: 8, 0x41: 2, 0x42: 4, 0x43: 4, 0x44: 6, 0x45: 2,
    0x46: 8, 0x47: 10, 0x48: 6, 0x49: 10, 0x4a: 8, 0x4b: 4, 0x4c: 6, 0x4d: 4, 0x4e: 14, 0x4f: 6,
    0x50: 2, 0x51: 12, 0x52: 12, 0x53: 12, 0x54: 12, 0x55: 2, 0x56: 6, 0x57: 2, 0x58: 14, 0x59: 12,
    0x5a: 4, 0x5b: 2, 0x5c: 2, 0x5d: 6, 0x5e: 4, 0x5f: 2, 0x60: 6, 0x61: 2, 0x62: 6, 0x63: 2,
    0x65: 4, 0x66: 1, 0x68: 6, 0x6a: 2, 0x6b: 2, 0x6c: 8, 0x6d: 22, 0x6e: 22, 0x6f: 12, 0x70: 2,
    0x71: 2, 0x72: 14, 0x73: 22, 0x74: 4, 0x75: 4, 0x76: 4, 0x77: 6, 0x78: 14, 0x79: 6, 0x7a: 2,
    0x7b: 10, 0x7c: 10, 0x7d: 4, 0x7e: 4, 0x7f: 6, 0x80: 4, 0x81: 2, 0x82: 4, 0x83: 2, 0x84: 8,
    0x85: 4, 0x86: 16, 0x87: 2, 0x88: 10, 0x89: 2, 0x8a: 2, 0x8b: 12, 0x8c: 6, 0x8d: 2, 0x8e: 4,
    0x8f: 4, 0x90: 2, 0x91: 4, 0x92: 2, 0x93: 2, 0x94: 4, 0x95: 4, 0x96: 2, 0x97: 8, 0x98: 12,
    0x99: 4, 0x9a: 4, 0x9b: 6, 0x9c: 2, 0x9d: 4, 0x9e: 2, 0x9f: 2, 0xa0: 6, 0xa1: 2, 0xa2: 4,
    0xa3: 12, 0xa4: 4, 0xa5: 4, 0xa6: 4, 0xa7: 4, 0xa8: 10, 0xa9: 2, 0xaa: 10, 0xab: 6, 0xac: 6,
    0xad: 4, 0xae: 2, 0xaf: 4, 0xb0: 22, 0xb1: 6, 0xb2: 4, 0xb3: 8, 0xb4: 4, 0xb5: 4, 0xb6: 2,
    0xb7: 2, 0xb8: 2, 0xb9: 10, 0xba: 16, 0xbb: 2, 0xbc: 2, 0xbd: 2, 0xbe: 6, 0xbf: 4, 0xc0: 28,
    0xc1: 2, 0xc2: 6, 0xc3: 4, 0xc4: 8, 0xc5: 2, 0xc6: 2, 0xc7: 4, 0xc8: 8, 0xc9: 4, 0xca: 2,
    0xcb: 2, 0xcc: 4, 0xcd: 2, 0xce: 2, 0xcf: 2, 0xd0: 4, 0xd1: 10, 0xd2: 10, 0xd3: 2, 0xd4: 4,
    0xf3: 1, 0xf4: 1, 0xf8: 4, 0xf9: 3, 0xfa: 4, 0xfb: 2, 0xfc: 2, 0xfd: 1, 0xfe: 1, 0xff: 2,
}
PLAYER = {0: 2, 7: 8, 12: 6, 13: 6, 0x80: 2, 0x34: 2, 0x8b: 2, 0: 2, 0x83: 4, 0x89: 4, 150: 4, 129: 12, 130: 2,
          142: 2, 141: 6, 140: 8, 132: 6, 143: 4, 145: 6}
SUB = {0: 2, 0x80: 2, 0x8b: 2, 0x90: 2, 0x8f: 6, 0x92: 8, 0x93: 8, 0x94: 4, 0x95: 4}


def common(cmd, b2, b3):
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
    return None


def length(d, p):
    op = d[p]
    if op == 0x64: return PLAYER.get(d[p + 1])
    if op == 0x67: return SUB.get(d[p + 1])
    if op == 0x69: return common(d[p + 1], d[p + 2], d[p + 3])
    if op == 0x66: return {1: 2, 20: 4}.get(d[p + 1], 1)
    return LEN.get(op)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--data', type=Path, default=Path('data'))
    ap.add_argument('--output', type=Path)
    ap.add_argument('--find', nargs='*', default=[], help='opcode[:sub] in hex, e.g. 64:84 82 cf d3')
    args = ap.parse_args()
    wanted = set()
    for f in args.find:
        a, _, b = f.partition(':')
        wanted.add((int(a, 16), int(b, 16) if b else None))
    counts, hits, unknown, misaligned = collections.Counter(), [], [], []
    for room in sorted((args.data / 'rooms').glob('*.RDT')):
        d = room.read_bytes()
        u32 = lambda o: struct.unpack_from('<I', d, o)[0]
        ev = u32(28)
        header = u32(ev)
        starts = [('scd0', ev + header), ('scd1', ev + u32(ev + 4))]
        for i, p in enumerate(range(8, header, 4)):
            starts.append((f'event{i}', ev + u32(ev + p)))
        bounds = sorted(set(o for _, o in starts)) + [len(d)]
        for name, start in starts:
            end = bounds[bounds.index(start) + 1]
            last = end == len(d)
            p, ended, depth = start, False, 0
            while p < end:
                if last and ended and d[p] not in (0x01, 0x02, 0x03, 0xfb, 0xfd, 0xf3, 0xf4) and depth <= 0:
                    break
                op = d[p]
                n = length(d, p)
                if n is None:
                    if not ended:
                        unknown.append((room.stem, name, p - ev, op, d[p + 1] if p + 1 < len(d) else None))
                    break
                sub = d[p + 1] if op in (0x64, 0x67, 0x69, 0x66) else None
                counts[(op, sub)] += 1
                if (op, sub) in wanted or (op, None) in wanted:
                    hits.append({'room': room.stem, 'script': name, 'offset': p - ev, 'op': op, 'bytes': list(d[p:p + n])})
                if op == 0x01: depth += 1
                if op == 0x03: depth -= 1
                if op in (0xfa, 0xfc): depth += 1
                if op in (0xfb, 0xfd, 0xf3): depth -= 1
                if op in (0x00, 0xff):
                    ended = True
                p += n
            if not last and p != end:
                misaligned.append((room.stem, name, p - ev, end - ev))
    report = {'counts': {f'{op:02x}' + (f':{sub:02x}' if sub is not None else ''): c for (op, sub), c in sorted(counts.items())},
              'hits': hits, 'misaligned': misaligned, 'unknown': [{'room': r, 'script': s, 'offset': o, 'op': op, 'sub': sub} for r, s, o, op, sub in unknown]}
    if args.output:
        args.output.write_text(json.dumps(report, indent=1) + '\n')
    print(f'{sum(counts.values())} commands, {len(unknown)} walks stopped at an unknown length, {len(misaligned)} scripts not ending on the next script offset')
    for m in misaligned[:20]:
        print('misaligned', m)
    for h in hits:
        print(h['room'], h['script'], f"off=0x{h['offset']:x}", ' '.join(f'{b:02x}' for b in h['bytes']))
    for u in unknown[:40]:
        print('unknown', u)


if __name__ == '__main__':
    main()
