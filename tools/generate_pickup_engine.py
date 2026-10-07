#!/usr/bin/env python3
"""Extract original pickup, sorting, automatic reload and bullet decrement."""
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/sub1.c').read_text()
start = source.index('void GetItem(S_WORK* st)')
output.mkdir(parents=True, exist_ok=True)
pickup = source[start:block(source, start)]
start = source.index('void ItemSort()')
sort = source[start:block(source, start)]
# NJS_TEXNAME::texaddr is the third 32-bit word in the PS2 ABI. Express the
# original pip[num1+2] access directly, avoiding type punning and host pointers.
import re
sort, count = re.subn(r'\(\(NJS_TEXNAME\s*\*\)\s*&st->pip\[num1\]\)->texaddr',
                      'st->pip[num1 + 2]', sort)
assert count == 6, count
weapon = (root / 'src/ps2/veronica/prog/weapon.c').read_text()
start = weapon.index('int bhCountBullet()')
count_bullet = weapon[start:block(weapon, start)]
reload_routines = []
for signature in ('unsigned char Combi_99(', 'int bhSearchBullet()'):
    start = source.index(signature)
    routine = source[start:block(source, start)]
    routine = re.sub(r'(itemid1|itemid2) << 16', r'std::uint32_t(\1) << 16', routine)
    reload_routines.append(routine)
(output / 'pickup_engine.inc').write_text('// Generated from original sub1.c and weapon.c; do not edit.\n' + pickup + '\n' + sort + '\n' + count_bullet + '\n' + '\n'.join(reload_routines) + '\n')
