#!/usr/bin/env python3
"""Extract subpl motion/movement dispatch and original enemy dispatch membership."""
import re, sys
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]); prog=root/'src/ps2/veronica/prog'; src=(prog/'subpl.c').read_text()
names=['em60_init','mv00_subpl0','mv00_subpl0Ex','mv00_subpl1','mv00_subpl2','mv00_subpl2_0','mv00_subpl3','mv00_subpl3_0','mv00_subpl5','mv00_subpl6','mv00_subpl0_1','bhSub_DirTarget','mv00_subpl10']
parts=['// Generated from subpl.c and eneset.c.\n']
for name in names:
 m=re.search(r'(void|int) '+name+r'\([^)]*\)\s*\{',src); parts.append('static '+src[m.start():src.index('{',m.start())].strip()+';')
for table in ['subpl_mv00_tbl','subpl_mv01_tbl']:
 m=re.search(r'Mv0[01]Subpl_proc '+table+r'\[15\]\s*=\s*\{',src);parts.append('static '+src[m.start():block(src,m.start())]+';')
for name in names:
 m=re.search(r'(void|int) '+name+r'\([^)]*\)\s*\{',src); routine=src[m.start():block(src,m.start())]
 if name=='em60_init':
  # The host dispatch covers bhJumpEnemy[100]; the linked synthetic id 199
  # is handled by the existing segmented actor sampler, never this work.
  start=routine.index('    if (epw->id == 199)'); routine=routine[:start]+routine[block(routine,start):]
 routine=routine.replace('(int)*(unsigned short*)&epw->ct0','(int)(unsigned short)epw->ct0').replace('*(short*)&epw->ct0','(short)epw->ct0')
 parts.append('static '+routine)
t=(prog/'eneset.c').read_text();m=re.search(r'bhJumpEnemy\[100\]\s*=\s*\{',t); entries=re.findall(r'\bbh(?:Ene\d+|Subpl)\b',t[m.end():block(t,m.start())]);assert len(entries)==100
parts.append('static const unsigned char subplayer_ids[100] = {'+','.join(str(int(n=='bhSubpl')) for n in entries)+'};')
out.mkdir(parents=True,exist_ok=True);(out/'subplayer.inc').write_text('\n'.join(parts))
