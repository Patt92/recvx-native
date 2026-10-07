#!/usr/bin/env python3
"""Reuse Claire's original ponytail object (objitm.c bhObjClpn, player.c bhCalcHair, HAIR_WORK)
verbatim for the native C bridge (native/player_hair.c)."""
import re
import sys
from pathlib import Path
from generate_native_tables import block

root, output = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'
header = (root / 'include/ps2/veronica/prog/objitm.h').read_text()
parts = ['// Generated from objitm.c/player.c/objitm.h; do not edit.\n']
start = header.index('typedef struct HAIR_WORK')
parts.append(header[start:header.index('} HAIR_WORK;', start)] + '} HAIR_WORK;\n')
for file, name in (('player.c', 'bhCalcHair'), ('objitm.c', 'bhObjClpn')):
    source = (prog / file).read_text()
    m = re.search(r'void ' + name + r'\(O_WRK\* op[^)]*\)\s*\{', source)
    parts.append('static ' + source[m.start():block(source, m.start())] + '\n')
output.mkdir(parents=True, exist_ok=True)
(output / 'player_hair.inc').write_text('\n'.join(parts))
