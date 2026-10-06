#!/usr/bin/env python3
"""Extract original ItemBoxChange, including restrictions and stack exchanges."""
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
sub = (root / 'src/ps2/veronica/prog/sub1.c').read_text()
table = sub.index('short standard[3][2]')
start = sub.index('void ItemBoxChange(')
output.mkdir(parents=True, exist_ok=True)
(output / 'box_engine.inc').write_text('// Generated from original sub1.c; do not edit.\n' + sub[table:block(sub, table)] + ';\n' + sub[start:block(sub, start)] + '\n')
