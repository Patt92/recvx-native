#!/usr/bin/env python3
"""Extract original ItemUse plus the floor-bound Use_01/Use_05 paths."""
import re
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/sub1.c').read_text()
parts = ['// Generated from original sub1.c; do not edit.\n']
for name in ['Use_01', 'Use_02', 'Use_04', 'Use_05', 'ItemUse']:
    match = re.search(r'^unsigned char ' + name + r'\([^\n]*\)\s*\n\{', source, re.M)
    if not match:
        raise RuntimeError('missing original routine ' + name)
    code = source[match.start():block(source, match.start())]
    # Metrowerks accepts ItemUse's extra argument to these no-argument
    # definitions; give that unused argument an explicit C++ signature.
    if name in ['Use_02', 'Use_04']:
        code = code.replace(name + '()', name + '(S_WORK*)', 1)
    parts.append(code + '\n')
output.mkdir(parents=True, exist_ok=True)
(output / 'item_use.inc').write_text(''.join(parts))
