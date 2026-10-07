#!/usr/bin/env python3
"""Original muzzle state and type-0 draw path; no replacement artwork."""
import sys,re
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
s=(root/'src/ps2/veronica/prog/effsub1.c').read_text()
a=s.index('void bhEff003(');procedure=s[a:block(s,a)].replace('rand()', 'effect_random()')
a=s.index('static UV_WORK uvinfo_t00');table=s[a:block(s,a)]+';'
a=s.index('    case 0:\n    case 5:',s.index('void bhDrawWeaponEffect('));b=s.index('\n    case 1:',a)
# The original type 0/5 branch does not call any platform service except these draw adapters.
branch=s[a:b]
(out/'weapon_flash.inc').write_text('// Generated from effsub1.c.\n'+procedure+'\nstatic void draw_muzzle(O_WRK* op){\n'+table+'\nfloat scl,off;UV_WORK* uvp;int ani=op->ct0-1;NJS_TEXTURE_VTX tv[4],tvb[4];\nfor(auto& v:tv)v.col=op->tvp->col;\nswitch(op->type){\n'+branch+'\n}\n}\n')
