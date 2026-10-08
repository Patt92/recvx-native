#!/usr/bin/env python3
"""Extract objitm.c bhObjItmBox, the room object routine behind the item-box lid.

SLUS_201.84 bhJumpObject (0x0032D4E0) holds bhObjItmBox at index 100; the
decompiled initializer in objitm.c lists it early because the intermediate
bhObjDmy entries are omitted. bhCheckExmAtari (hitchk.c, type 4 with attr 0x2)
switches a box object to type 100.
"""
import re
import sys
from pathlib import Path
from generate_native_tables import block
root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/objitm.c').read_text()
match = re.search(r'^void bhObjItmBox\(O_WRK\* op\)\s*\{', source, re.M)
if not match:
    raise RuntimeError('missing original bhObjItmBox')
routine = source[match.start():block(source, match.start())]
output.mkdir(parents=True, exist_ok=True)
(output / 'object_itmbox.inc').write_text('// Generated from original objitm.c; do not edit.\n' + routine + '\n')
