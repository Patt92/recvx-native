#!/usr/bin/env python3
"""Extract the original push action, object, collision and motion routines.
Only host ABI adaptations (mode bytes and sequenced pointer increments) occur.
"""
import re,sys
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);prog=root/'src/ps2/veronica/prog'
parts=['// Generated original push engine. Do not edit.']
def function(file,name):
 s=(prog/file).read_text();m=re.search(r'^(?:static\s+)?(?:void|int|Int|Float|ATR_WORK\*)\s+'+name+r'\([^;]*?\)\s*\{',s,re.M)
 if not m: raise RuntimeError(name)
 f=s[m.start():block(s,m.start())]
 f=re.sub(r'\*\(int\*\)\&plp->mode0 = (0x[0-9a-fA-F]+|[0-9]+);',r'PushModeAssign(plp, \1);',f)
 f=f.replace('static int tmpS[3], tmpD[3];', 'static _Thread_local int tmpS[3], tmpD[3];')
 f=re.sub(r'\*srcP = 32768 \+ \*srcP\+\+;\s*\*srcP = 32768 - \*srcP;\s*\*srcP = 32768 \+ \*\+\+srcP;', 'srcP[0] += 32768; srcP[1] = 32768 - srcP[1]; srcP[2] += 32768;',f)
 f=f.replace('datP = NormalTbl;', 'datP = (char*)NormalTbl;').replace('mkaP = &NullMka;', 'mkaP = (NJS_MKEY_A_MOD*)&NullMka;')
 return f if f.startswith("static ") else "static "+f
p=(prog/'player.c').read_text();h=(root/'include/ps2/veronica/prog/player.h').read_text();i=h.index('typedef struct EXP_WORK');parts.append(h[i:h.index('} EXP_WORK;',i)]+ '} EXP_WORK;')
for table in ('PlMtnAct','PlyInfo','PlyTrsZ'):
 m=re.search(r'^(?:const )?(?:unsigned short|float|char) '+table+r'\[.*?=\s*\{',p,re.M);parts.append('static '+p[m.start():block(p,m.start())]+';')
for name in ('njDistanceP2P','njDistanceP2L','njCollisionCheckSC','njCollisionCheckBS'):parts.append(function('ps2_NaColi.c',name))
for name in ('bhCheckDansaAtari','bhSetDansaLimitAtari','bhCheckFloorP','bhCheckInnerP4','bhCheckInnerTriangle','bhCheckInnerTriangle2','bhCheckBox','bhCheckBox2Box','bhCheckWallType','bhCheckWallType2','bhCheckWall2Box','bhCheckWallEx'):parts.append(function('hitchk.c',name))
for name in ('bhCheckFloorNum','bhAddSpeed'):parts.append(function('pwksub.c',name))
# Player collision block, unchanged from bhControlPlayer. The surrounding host
# driver provides saved body/front positions and does not duplicate the physics.
a=p.index('        if (expw->arn != expw->arp)',p.index('void bhControlPlayer()'))
b=p.index('        if ((plp->flg & 0x8))',a)
c=p.index('        fsnd = plp->flg;',b)
d=p.index('        if ((plp->flg & 0x200000))',c)
parts.append('static void PushPlayerCollision(void) {\n EXP_WORK* expw=plp->exp0; NJS_POINT3 bps; int fsnd,bhit; unsigned stf_bk;\n'+p[a:b]+p[c:d]+'}\n')
parts.append(function('objitm.c','bhObj001'));parts.append(function('player.c','bhCPM2_act_psh'))
a=p.index('    if ((!(sys->gm_flg & 0x40))',p.index('void bhCPM1_act_bas()'))
b=p.index('    p = plp;',a)
parts.append('static void PushRotationSpeed(void) {\n'+p[a:b]+'}\n')
s=(prog/'Motion.c').read_text();parts.append(s[s.index('static int mka_ang'):s.index('// 99.94%')].replace('static int mka_ang', 'static _Thread_local int mka_ang'))
for name in ('AngZyxToYzx','AngYzxToZyx','SetMtnNormal','SetMtnNormalHokan','SetMtnFast','SetMtnFastHokan','SetMtnSlow','SetMtnSlowHokan','bhSetMotion'):parts.append(function('Motion.c',name))
out.mkdir(parents=True,exist_ok=True);(out/'player_push.inc').write_text('\n\n'.join(parts)+'\n')
