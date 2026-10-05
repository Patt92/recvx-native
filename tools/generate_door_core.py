#!/usr/bin/env python3
"""Reuse the original door model/camera/fade procedures in the native C bridge."""
import argparse
from pathlib import Path
import re
from generate_native_tables import block

p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
header=(a.root/'include/ps2/veronica/prog/door.h').read_text()
names=['FADEPROC_WORK']+[f'VIEWPROC{i}_WORK' for i in range(1,8)]+[f'DOORPROC{i}_WORK' for i in (1,2,3,4,5,7)]
structs=['// Generated original parameter layouts from door.h.\n']
for name in names:
 m=re.search(r'typedef struct '+name+r'\b',header);end=block(header,m.start());structs.append(header[m.start():end]+f' {name};\n')
(a.output/'door_structs.inc').write_text(''.join(structs))
source=(a.root/'src/ps2/veronica/prog/door.c').read_text()
names=['CompareSint32','CompareFloat','FadeProc1']+[f'ViewProc{i}' for i in range(1,8)]+[f'DoorProc{i}' for i in range(1,8)]
out=['// Generated original door.c procedures. VectorMove uses the native matrix API.\n']
for name in names:
 m=re.search(r'static (?:void|int) '+name+r'\([^;]*?\)\s*\{',source);procedure=source[m.start():block(source,m.start())];procedure=procedure.replace('return ret;', 'return 0;') if name=='CompareSint32' else procedure;procedure=procedure[:-1]+'    return 0;\n}' if name=='CompareFloat' else procedure;out.append(procedure+'\n')
(a.output/'door_procedures.inc').write_text(''.join(out))
