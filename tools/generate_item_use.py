#!/usr/bin/env python3
"""Extract original ItemUse, its Use_01..Use_05 paths and the herb routine Use_00."""
import re
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/sub1.c').read_text()


def routine(name):
    match = re.search(r'^unsigned char ' + name + r'\([^\n]*\)\s*\n\{', source, re.M)
    if not match:
        raise RuntimeError('missing original routine ' + name)
    return source[match.start():block(source, match.start())]


parts = ['// Generated from original sub1.c; do not edit.\n']
for name in ['Use_01', 'Use_02', 'Use_04', 'Use_05', 'ItemUse']:
    code = routine(name)
    # Metrowerks accepts ItemUse's extra argument to these no-argument
    # definitions; give that unused argument an explicit C++ signature.
    if name in ['Use_02', 'Use_04']:
        code = code.replace(name + '()', name + '(S_WORK*)', 1)
    parts.append(code + '\n')

# Use_00 leaves `herbuse` unassigned when a poison cure is refused (stflg
# 0x200000) and when an item has neither heal nor cure bits. SLUS_201.84 keeps
# herbuse in callee-saved $s3 (Use_00 0x002A31B0: only `daddiu $s3,$zero,1`).
# ItemUse (0x0029C9E0) and ItemCommand (0x002A05A0) never write $s3; StatusMain
# (0x0029DD60) clears it at 0x0029DDC8 (setnum = 0) before calling
# ItemCommand at 0x0029E22C. The value returned on those paths is therefore 0.
# GetItem calls Use_00 without using its result.
use00 = routine('Use_00')
use00, count = re.subn(r'unsigned char herbuse;', 'unsigned char herbuse = 0;', use00)
assert count == 1, count

output.mkdir(parents=True, exist_ok=True)
(output / 'item_use.inc').write_text(''.join(parts))
(output / 'item_use00.inc').write_text('// Generated from original sub1.c; do not edit.\n' + use00 + '\n')
