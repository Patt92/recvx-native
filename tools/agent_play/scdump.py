#!/usr/bin/env python3
"""Linearer Befehls-Dump eines Raumskripts: scdump.py RM_0000 scd0 [von_hex bis_hex] | RM_0050 event1"""
import sys, struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import walk_event_scripts as w
room, script = sys.argv[1], sys.argv[2]
lo = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0; hi = int(sys.argv[4], 16) if len(sys.argv) > 4 else 1 << 30
d = open(f'data/rooms/{room}.RDT', 'rb').read(); u32 = lambda o: struct.unpack_from('<I', d, o)[0]
ev = u32(28); header = u32(ev)
starts = [('scd0', ev + header), ('scd1', ev + u32(ev + 4))] + [(f'event{i}', ev + u32(ev + p)) for i, p in enumerate(range(8, header, 4))]
bounds = sorted(set(o for _, o in starts)) + [len(d)]
start = dict(starts)[script]; end = bounds[bounds.index(start) + 1]
p = start; depth = 0
while p < end:
    n = w.length(d, p)
    if n is None: print(f'off=0x{p-ev:x} unknown op {d[p]:02x}'); break
    if lo <= p - ev <= hi: print(f'off=0x{p-ev:x} {"  "*max(depth,0)}' + ' '.join(f'{b:02x}' for b in d[p:p+n]))
    op = d[p]
    if op == 0x01: depth += 1
    if op == 0x03: depth -= 1
    if op in (0xfa, 0xfc): depth += 1
    if op in (0xfb, 0xfd, 0xf3): depth -= 1
    p += n
