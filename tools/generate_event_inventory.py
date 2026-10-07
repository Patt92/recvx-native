#!/usr/bin/env python3
"""Extract original event inventory mutations with their PS2 operand layout."""
import re
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/event.c').read_text()
names = ['bhItemGetGet', 'bhItemGetGetEx', 'bhArmsItemSet', 'bhPlItemLost',
         'bhPlItemLostEx', 'bhPlItemChangeM', 'bhPlItemTamaSet']
parts = ['// Generated from original event.c; do not edit.\n']
for name in names:
    match = re.search(r'^unsigned int ' + name + r'\(\)[^\n]*\n\{', source, re.M)
    if not match:
        raise RuntimeError('missing original routine ' + name)
    code = source[match.start():block(source, match.start())]
    # PS2 scripts are little endian and may be unaligned on the native host.
    code = code.replace('*(unsigned short*)bhScePtr', 'read_u16(bhScePtr)')
    parts.append(code + '\n')
output.mkdir(parents=True, exist_ok=True)
(output / 'event_inventory.inc').write_text(''.join(parts))
