#!/usr/bin/env python3
"""Extract original Cerberus trap placement and shared enemy area lookup."""
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
parts = ['// Generated original enemy spawn/area procedures; do not edit.\n']
for file, names in [('zonzon.c', ('bhEne_PosCheck','bhEne_EnemyAtariCheck')), ('en04.c', ('bhEne04_EnemyAtariCheck2','bhEne04_AtariCheck','bhEne04_AtariCheck2'))]:
    source = (root / 'src/ps2/veronica/prog' / file).read_text()
    for name in names:
        start = source.index(('ATR_WORK* ' if 'EnemyAtariCheck' in name else 'int ') + name + '(')
        code = source[start:block(source, start)].replace('rand()', 'spawn_random()')
        # Original accepts 32 but subsequently shifts a 32-bit word by that
        # amount. The host rejects this invalid table parameter rather than UB.
        code = code.replace('32 < hp->prm3', '32 <= hp->prm3')
        parts.append(code)
output.mkdir(parents=True, exist_ok=True)
(output / 'enemy_spawn.inc').write_text('\n'.join(parts) + '\n')
