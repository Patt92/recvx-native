#!/usr/bin/env python3
"""Compile original inventory inspection state transitions, never a selected-item list."""
import re,sys
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
view=(root/'src/ps2/veronica/prog/itemview.c').read_text();sub=(root/'src/ps2/veronica/prog/sub1.c').read_text();files=(root/'src/ps2/veronica/prog/fileview.c').read_text()
parts=['// Generated original inventory engine routines; host only replaces platform bindings.\n']
for source,names in [(sub,['ItemSearch','NameChangeSet']),(view,['MdlDirChk','ItemModelCheck','ItemModelMessageWait','MdlAction00','MdlAction01','MdlAction02','MdlHideCheck','FlagErase','ItemModelActionSet','ItemModelChangeZoomOut','ItemModelChangeZoomIn'])]:
 for name in names:
  m=re.search(r'^(?:void|unsigned char|unsigned int\*) '+name+r'\([^\n]*\)[^\n]*\n\{',source,re.M)
  if not m:raise RuntimeError('missing original routine '+name)
  code=source[m.start():block(source,m.start())]
  code=code.replace('DSP_WORK* dw;', 'const DSP_WORK* dw;').replace('(DSP_WORK*)&dsptbl', '&dsptbl')
  code=code.replace('(actionprg - 1)[itemflg[dw->hide][1]]','actionprg[itemflg[dw->hide][1] - 1]')
  parts.append(code+'\n')
# Copy the actual name-state switch, rather than maintaining another native list.
a=sub.index('        itemid = st->itemid;',sub.index('void SpriteOnOff'));a=sub.index('        switch (itemid)',a);b=block(sub,a)
code=sub[a:b].replace('st->pip[st->listcsr_0]','packed');parts.append('unsigned item_name_id(unsigned packed){unsigned itemid=(packed>>16)&255;\n'+code+'\nreturn itemid;}\n')
for name in ['fstbl','fsheader','wallpaper']:
 m=re.search(r'^unsigned int '+name+r'\[24\]\s*=\s*\{',files,re.M);parts.append('const '+files[m.start():block(files,m.start())]+';\n')
m=re.search(r'^unsigned int FileNumberSwitch\([^\n]*\)\s*\n\{',files,re.M);parts.append(files[m.start():block(files,m.start())]+'\n')
# Room-bound file models are metadata from the original switch, not a new
# itemdata type. The subsequent TG-01 override is intentionally outside it:
# TG-01 remains an inventory object after ItemModelChangeZoomIn opens file 2.
fn=files[m.start():block(files,m.start())]
switch_at=fn.index('switch (sys->rom_no')
switch=fn[switch_at:block(fn,switch_at)]
models={}
for match in re.finditer(r'if\s*\(num == (\d+)\)\s*\{\s*filenum = (\d+);\s*\}',switch):
 item,file=map(int,match.groups());models[item]=models.get(item,0)|(1<<file)
if not models:raise RuntimeError('original room file mappings not found')
parts.append('unsigned file_model_mask(unsigned item){switch(item){\n'+''.join(f'case {item}:return 0x{mask:08x}u;\n' for item,mask in sorted(models.items()))+'default:return 0;}}\n')
(out/'inventory_engine.inc').write_text(''.join(parts))
