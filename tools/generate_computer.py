#!/usr/bin/env python3
"""Extract the original special-event computer terminal (sp_evt.c).

Tables, script interpreter, soft keyboard, window state machine and the quad
builders are copied verbatim. Only constructs that depend on the PS2 ILP32
layout or on hardware services are rewritten:
  * `*(int*)&sys->com_md0 = N` writes com_md0 and the adjacent com_md1,
  * the message table walk uses pointer-sized entries,
  * the 3D character viewport (bhDrawSpEvtComCharacter) is not bound.
"""
import re
import sys
from pathlib import Path
from generate_native_tables import block

root, output = map(Path, sys.argv[1:])
text = (root / 'src/ps2/veronica/prog/sp_evt.c').read_text()
start = text.index('typedef void (*bhCtrSpEvtCom_mode0_proc)();')
body = text[start:]

# The viewport function needs the Ninja skin pipeline and the enemy work area.
character = body.index('void bhDrawSpEvtComCharacter()')
character_end = block(body, character)
body = (body[:character] +
        'void bhDrawSpEvtComCharacter()\n{\n    ++recvx_com_unsupported_character;\n}\n' +
        body[character_end:])

body, replaced = re.subn(r'\*\(int\*\)&sys->com_md0 = (\d+);', r'sys->com_md0 = \1; sys->com_md1 = 0;', body)
assert replaced == 1, replaced
assert 'unsigned int* mstb;' in body
body = body.replace('unsigned int* mstb;', 'char** mstb;')

types = (root / 'include/ps2/veronica/prog/types.h').read_text()
structs = ''
for name in ('QUAD', 'COM_EVT_WORK'):
    first = types.index('typedef struct %s \n{' % name)
    structs += types[first:block(types, first)] + ' %s;\n' % name

header = (root / 'include/ps2/veronica/prog/sp_evt.h').read_text()
prototypes = ''.join(line + '\n' for line in header.splitlines() if line.startswith('void '))

output.mkdir(parents=True, exist_ok=True)
(output / 'computer.inc').write_text('// Generated from original sp_evt.c, sp_evt.h and types.h; do not edit.\n' + structs + prototypes + body)
