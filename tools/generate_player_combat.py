#!/usr/bin/env python3
"""Extract original player/playpch attack states, targeting and Motion routines.
Only host ABI adaptations are permitted; no replacement transitions or timers.
"""
import re, sys
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);prog=root/'src/ps2/veronica/prog';inc=root/'include/ps2/veronica/prog'
def function(file,name):
 s=(prog/file).read_text();s=re.sub(r'//[^\n]*|/\*.*?\*/','',s,flags=re.S)
 m=re.search(r'^(?:static\s+)?(?:void|int|short|float|GA_WORK\*|search\*)\s+'+name+r'\([^;]*?\)\s*\{',s,re.M)
 if not m:raise RuntimeError(name)
 f=s[m.start():block(s,m.start())]
 f=f.replace('return (int)topP;', 'return topP != NULL;')
 # SYS_WORK.itm starts at 0x270; itm[95] is offset 0x3ec = BH_PWORK.at_flg.
 # Preserve this original ILP32 alias through the named field on the 64-bit host.
 f=f.replace('((SYS_WORK*)plp)->itm[95]', 'plp->at_flg')
 f=f.replace('ewP->mnwP[ewP->mtn_no].md2P->p[0]', '((NJS_MDATA2_MOD*)ewP->mnwP[ewP->mtn_no].md2P)->p[0]')
 f=f.replace('ewP->mnwP[ewP->mtn_no].md2P[rutP[obj_no]]', '((NJS_MDATA2_MOD*)ewP->mnwP[ewP->mtn_no].md2P)[rutP[obj_no]]')
 if name=='bhCountBullet':f=f.replace('int bhCountBullet()', 'int bhCountBulletOriginal()')
 f=f.replace('bhCPM2_SearchPch(plp, plp->wpnr_no);','bhCPM2_SearchPch();') # original declaration has no parameters; ignored extra arguments
 f=re.sub(r'\*srcP = 32768 \+ \*srcP\+\+;\s*\*srcP = 32768 - \*srcP;\s*\*srcP = 32768 \+ \*\+\+srcP;','srcP[0] += 32768; srcP[1] = 32768 - srcP[1]; srcP[2] += 32768;',f)
 return 'static '+f.removeprefix('static ')
def struct(file,name):
 s=(inc/file).read_text();m=re.search(r'typedef struct (?:_search|'+name+r')\s*\{',s) if name=='search' else re.search(r'typedef struct '+name+r'\s*\{',s)
 return s[m.start():s.index(';',block(s,m.start()))+1]
parts=['// Generated original attack engine. Do not edit.']
# Host-layout types preserve the fields consumed by the original functions.
for name in ['EXP_WORK','WPN_TAB']:parts.append(struct('player.h',name))
parts.append(struct('playpch.h','search'));parts.append(struct('playpch.h','PP_WORK'))
parts.append(struct('types.h','GA_WORK'))
parts.append(struct('types.h','LGT_WORK').replace('NJS_LIGHT_PTR* light','void* light'))
# effect.c drives O_WRK, a separate original work type. Ninja's low-level vertex
# records (ninjastr.h) and the texture list it embeds are declared with the
# original field order; only pointers widen for the 64-bit host.
parts.append('typedef struct {float x,y,z;unsigned col;} NJS_POLYGON_VTX;')
parts.append('typedef struct {float x,y,z,u,v;unsigned col;} NJS_TEXTURE_VTX;')
parts.append('typedef struct {void* textures;unsigned nbTexture;} NJS_TEXLIST;')
parts.append(struct('types.h','O_WRK'))
parts.append('static EXP_WORK* combat_exp;')
functions=[]
for file,names in [('pwksub.c',['bhCalcLockEneYR','bhSearchNearEnemy','bhSearchNextEnemy','bhSetGunFire','bhSetYakkyou','bhSetMagazine','bhSetWaterSplash','bhSetWaterSplash3']),('playpch.c',['MixSetToJointRot','GetOneObjectMotion','SetOneObjectMotion','bhSearchEnemy','SetLockOnDirection','PlyPchMain','bhCPM2_act_suw_pch','bhCPM2_act_wsc_pch','bhCPM2_act_atk_pch','CheckGunHit','bhCPM2_SearchPch']),('player.c',['bhCPM1_act_atk','bhCPM2_act_suw','bhCPM2_act_wpn','bhCPM2_act_wre','bhCPM2_act_atk','bhCPM2_act_rld','bhCPM2_act_knf']),('playpch2.c',['bhCPM2_act_scp']),('weapon.c',['bhCheckBullet','bhCountBullet']),('effect.c',['bhSetEffect','bhSetEffectTb']),('Motion.c',['AngZyxToYzx','AngYzxToZyx','SetMtnNormal','SetMtnNormalHokan','SetMtnFast','SetMtnFastHokan','SetMtnSlow','SetMtnSlowHokan','bhSetMotion'])]:
 for name in names:functions.append(function(file,name))
# Private original weapon tables used by the C state machine.
for name in ('WpnTab','WpnEffTab','PlMtnWpn','KnfAtrTab'):
 t=(prog/'player.c').read_text();m=re.search(r'^(?:const )?[^\n=]+\b'+name+r'\[[^\n=]+\s*=\s*\{',t,re.M);parts.append('static '+t[m.start():block(t,m.start())]+';')
parts.extend(f[:f.index('{')].strip()+';' for f in functions)
# These are original sub-blocks of bhCPM0_action and bhControlPlayer, extracted rather than rewritten.
p=(prog/'player.c').read_text();a=p.index('    if (((sys->gm_flg & 0x1000000))',p.index('void bhCPM0_action()'));b=p.index('    if (!(plp->flg & 0x10000))',a)
parts.append('static void CombatEntry(void) {\n'+p[a:b]+'}\n')
a=p.index('        if ((plp->flg & 0x100000))',p.index('void bhControlPlayer()'));b=p.index('        if ((plp->flg & 0x40000))',a)
# Extract the complete original yaw block, including its side effects.
a=p.index('        if ((plp->flg & 0x100000))',p.index('void bhControlPlayer()'))
parts.append('static void CombatYaw(void) { O_WORK* owP; float px,py,pz;\n'+p[a:block(p,a)]+'\n}')
a=p.index('        if (((plp->mode0 == 1) && (plp->mode1 == 1) && (plp->mode2 == 0x45)',p.index('void bhControlPlayer()'))
parts.append('static void CombatKnife(void) { GA_WORK gap;\n'+p[a:block(p,a)]+'\n}')
s=(prog/'Motion.c').read_text();parts.append(s[s.index('static int mka_ang'):s.index('// 99.94%')])
# Types/prototypes precede host services; function definitions follow them.
out.mkdir(parents=True,exist_ok=True)
(out/'player_combat_types.inc').write_text('\n\n'.join(parts[:parts.index('static EXP_WORK* combat_exp;')])+ '\n')
# The yaw/entry blocks follow the main procedures so host pointers are declared by the core.
rest=parts[parts.index('static EXP_WORK* combat_exp;')+1:]
(out/'player_combat.inc').write_text('\n\n'.join(rest+functions)+'\n')
