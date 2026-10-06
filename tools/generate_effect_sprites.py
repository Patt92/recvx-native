#!/usr/bin/env python3
"""Extract original atlas sprite procedures without rewriting their state logic."""
import sys
from pathlib import Path
root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/effsub5.c').read_text()
parts = ['// Generated from effsub5.c; do not edit.\n']
for name in ('bhEff200', 'bhEff201', 'bhEff218'):
    start = source.index('void ' + name + '(')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    parts.append(source[start:end].replace('rand()', 'effect_random()'))
output.mkdir(parents=True, exist_ok=True)
(output / 'effect_sprites.inc').write_text('\n'.join(parts) + '\n')
