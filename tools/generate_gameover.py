#!/usr/bin/env python3
"""Copy the original gameover.c state machine, mesh and header into the native adapter."""
import sys,re
from generate_native_tables import block
from pathlib import Path
root,out=map(Path,sys.argv[1:]);src=(root/'src/ps2/veronica/prog/gameover.c').read_text();h=(root/'include/ps2/veronica/prog/gameover.h').read_text()
h=h[h.index('typedef struct GV_WORK'):h.index('#endif')]
body=src[src.index('typedef void (*bhCtrGov_mode0_proc)();'):]
# Aligned local volatile variable remains only to preserve the original matched loop.
body=body.replace('__attribute__((aligned(16)))','')
# C++ does not permit void* assignment to the typed work pointer.
body=body.replace('gv = sys->gov_exp;', 'gv = static_cast<GV_WORK*>(sys->gov_exp);')
player=(root/'src/ps2/veronica/prog/player.c').read_text();m=re.search(r'const unsigned short PlMtnAct\[[^=]+=',player); table=player[m.start():block(player,m.start())]+';\n'
screen=(root/'src/ps2/veronica/prog/screen.c').read_text();fade=''
for name in ['bhSetScreenFade','bhControlScreenFade']:
 start=screen.index('void '+name+'(');fade+=screen[start:block(screen,start)]+'\n'
out.mkdir(parents=True,exist_ok=True);(out/'gameover.inc').write_text('// Generated from original gameover.c/gameover.h; do not edit.\n'+h+table+fade+body)
