"""Extract the original three-folder selection state machine."""
import re, sys
from pathlib import Path
from generate_native_tables import block
root, out = map(Path, sys.argv[1:])
files = (root/'src/ps2/veronica/prog/fileview.c').read_text()
items = (root/'src/ps2/veronica/prog/itemview.c').read_text()
parts = ['// Generated from fileview.c/itemview.c; platform bindings only.\n']
for source, names in [(files, ['SearchTag', 'SelectFile', 'SelectTag', 'PlayPageCheck']), (items, ['FileSyu'])]:
    for name in names:
        m = re.search(r'^(?:void|int) '+name+r'\([^\n]*\)[^\n]*\n\{', source, re.M)
        if not m: raise RuntimeError('missing '+name)
        parts.append(source[m.start():block(source,m.start())]+'\n')
out.mkdir(parents=True,exist_ok=True)
(out/'file_folder.inc').write_text(''.join(parts))
