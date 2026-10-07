#!/usr/bin/env python3
"""Extract original hitchk.c point queries and wall reflection for effect debris."""
import re,sys
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);src=(root/'src/ps2/veronica/prog/hitchk.c').read_text();parts=[]
for name in ['bhCheckInnerP4','bhCheckInnerTriangle','bhCheckBox','bhCheckWallType','bhSetWallRefAngle']:
 m=re.search(r'^(?:int|void|ATR_WORK\*) '+name+r'\(',src,re.M);parts.append(src[m.start():block(src,m.start())])
line=(root/'src/ps2/veronica/prog/hitchkl.c').read_text()
extra=[]
for name in ['bhCollisionCheckLine','bhCollisionCheckLine2','bhCollisionCheckLineMain','bhCollisionCheckL2PL','bhInOutCheck','bhCollisionCheckL2XZPL','bhCollisionCheckL2XYPL','bhCollisionCheckL2YZPL','bhGetHitCollisionNormal']:
 m=re.search(r'^(?:int|void|ATR_WORK\*) '+name+r'\(',line,re.M);extra.append(line[m.start():block(line,m.start())])
parts += [f[:f.index('{')].strip()+';' for f in extra] + extra
out.mkdir(parents=True,exist_ok=True);(out/'effect_collision.inc').write_text('// Generated from original hitchk.c; do not edit.\n'+'\n'.join(parts))
