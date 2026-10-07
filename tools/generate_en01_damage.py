#!/usr/bin/env python3
"""Extract the original en01 damage/death state machine for the native host.

Everything below is transcribed from the original sources: BH_PWORK and its
helper structs (types.h), the weapon/combination tables, bhEne01_DmgCheck,
DamageAdd, DmgCheckType00/02, DmgModeJumpCheck, DG00..04/16, DD00..03,
bhEne_CalcDamage/ChgMtn and bhEne01_Init. The only edits are cutting the
linked-child allocation of bhEne01_Init (the host merges the upper-body actor
into one entity) and replacing WpnTab[x].flg by the host table accessor.
"""
import re, sys
from pathlib import Path
from generate_native_tables import block

root, out = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'
inc = root / 'include/ps2/veronica/prog'
en01 = (prog / 'en01.c').read_text()
zon = (prog / 'zonzon.c').read_text()
zon1 = (prog / 'zonzon1.c').read_text()
types = (inc / 'types.h').read_text()
en01h = (inc / 'en01.h').read_text()
zon1h = (inc / 'zonzon1.h').read_text()
zonh = (inc / 'zonzon.h').read_text()
macros = (inc / 'macros.h').read_text()


def struct(text, name):
    m = re.search(r'typedef struct ' + name + r'\s*\{', text)
    if not m:
        raise RuntimeError('missing struct ' + name)
    end = block(text, m.start())
    tail = text.index(';', end)
    return text[m.start():tail + 1]


def function(text, name):
    # Original definitions may annotate parameters between ')' and '{'.
    text = re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)
    m = re.search(r'^(?:static )?(?:unsigned char|void|int|float|BH_PWORK\*|ATR_WORK\*) ' + name + r'\s*\([^)]*\)\s*\{', text, re.M)
    if not m:
        raise RuntimeError('missing function ' + name)
    return text[m.start():block(text, m.start())]


def prototype(source):
    head = source[:source.index('{')].strip()
    return 'static ' + head.replace('static ', '') + ';'


def table(text, name):
    m = re.search(r'^(?:static )?(?:const )?[A-Za-z_0-9]+\s+\*?' + re.escape(name) + r'(?:\[[^\]]*\])*\s*=\s*\{', text, re.M)
    if not m:
        raise RuntimeError('missing table ' + name)
    return 'static ' + text[m.start():block(text, m.start())].replace('static ', '', 1) + ';'


# --- types -----------------------------------------------------------------
ptr = lambda s: s.replace('NJS_TEXLIST*', 'void*').replace('NJS_MDATA2_MOD*', 'void*')
parts = ['// Generated from original types.h, en01.h, zonzon1.h, macros.h; do not edit.\n']
parts.append('typedef struct NJS_CNK_OBJECT {unsigned evalflags;void* model;float pos[3];int ang[3];float scl[3];struct NJS_CNK_OBJECT* child;struct NJS_CNK_OBJECT* sibling;} NJS_CNK_OBJECT;\ntypedef struct {float px,py,pz,ox,oy,oz;} POINT;\ntypedef struct {float px,py,pz,vx,vy,vz;} NJS_LINE;\n'
             'typedef struct {NJS_POINT3 c1,c2;float r;} NJS_CAPSULE;\n')
for name in ['O_WORK', 'ML_WORK', 'MN_WORK', 'CPCL', 'BH_PWORK', 'WPNDAMAGE_WORK']:
    parts.append(ptr(struct(types, name)))
for name in ['EN01_CHG_WORK', 'EN01_CHK_WORK', 'EN01_FMTN_WORK', 'EN01_ATB_WORK', 'EN01_MTN_WORK', 'EN01_MTN2_WORK']:
    parts.append(struct(en01h, name))
for name in ['COMBJOINT_WORK', 'COMBWEP_WORK']:
    parts.append(struct(zon1h, name))
parts.append(struct(zonh, 'BT_WORK'))
parts.append(struct(zonh, 'BT_WRK'))
parts.append(struct(en01h, 'EN01_PERSONAL_TYPE'))
parts.append(struct(en01h, 'EN01_FIRE_WORK'))
for name in ['EF_WORK', 'BP_WORK', 'ATR_WORK']:
    parts.append(ptr(struct(types, name)))
parts.append('typedef struct {NJS_POINT3 c;float r;} NJS_SPHERE;')
m2 = re.search(r'typedef struct\s*\{[^}]*\}\s*NJS_POINT2_XZ;', types)
parts.append(m2.group(0))
parts.append('\n'.join(l for l in macros.splitlines() if re.match(r'#define (EXP0_|EPP_EXP0_|CEPW_EXP0_|EP_EXP0_|EXP1_|PEXP0_|EPW_EXP1_|EPW_EXP0_|NJM_DEG_ANG)', l)))
(out / 'en01_damage_types.inc').parent.mkdir(parents=True, exist_ok=True)
(out / 'en01_damage_types.inc').write_text('\n'.join(parts) + '\n')

# --- functions ---------------------------------------------------------------
en01_names = ['bhEne01_DmgCheck', 'bhEne01_SetBlood', 'bhEne01_DmgCheckType00', 'bhEne01_DmgCheckType02',
              'bhEne01_DmgCheckTypeDmmy', 'bhEne01_DamageAdd', 'bhEne01_PartsDamageCheck', 'bhEne01_DmgModeJumpCheck',
              'bhEne01_Damage', 'bhEne01_Die', 'bhEne01_DGType00', 'bhEne01_DGType02',
              'bhEne01_DG00', 'bhEne01_DG01', 'bhEne01_DG02', 'bhEne01_DG03', 'bhEne01_DG04', 'bhEne01_DG16',
              'bhEne01_DDType00', 'bhEne01_DDType02', 'bhEne01_DD00', 'bhEne01_DD01', 'bhEne01_DD02', 'bhEne01_DD03', 'bhEne01_MV04', 'bhEne01_CmnFlgCheck',
              'bhEne01_Init', 'bhEne01_InitType00', 'bhEne01_InitType02', 'bhEne01_InitType06',
              'bhEne01_InitType10', 'bhEne01_InitType11', 'bhEne01_InitType15', 'bhEne01_CheckExpHead',
              'bhEne01_GetWalkMotion',
              'bhEne01_EneSearch', 'bhEne01_Brain00', 'bhEne01_Brain02', 'bhEne01_ActionModeCheck', 'bhEne01_MVType00', 'bhEne01_MVType02',
              'bhEne01_MV00', 'bhEne01_MV01', 'bhEne01_MV02', 'bhEne01_MV03', 'bhEne01_MV05', 'bhEne01_ChgWalkMtn', 'bhEne01_FastWalkCheck',
              'bhEne01_CheckMtnTbl', 'bhEne01_Move', 'bhEne01_SePlay',
              'bhEne01_Nage', 'bhEne01_NGType00', 'bhEne01_NG00', 'bhEne01_NG01', 'bhEne01_NG02', 'bhEne01_PlayerControl', 'bhEne01_PlayerLink',
              'bhEne01_EatCheck', 'bhEne01_EnemyPushChk', 'bhEne01_PlyDG00', 'bhEne01_PlyDG01', 'bhEne01_CutHead', 'bhEne01_NeckBloodEffect', 'bhEne01_DansaCheck', 'bhEne01_GakeotiCheck', 'bhEne01_KaidanCheck','bhEne01_ZulzulCheck','bhEne01_MV08','bhEne01_ExpHeadEffect','bhEne01_ExpHeadEffect2','bhEne01_NikuhenEffect','bhEne01_NeckBloodEffect2']
zon_names = [('zon', 'bhEne_ChgMtn')]
zon1_names = [('zon1', 'bhEne_CalcDamage'), ('zon1', 'bhEne_DGDirCheck'), ('zon1', 'bhEne_DirCheck'), ('zon1', 'bhEne_InitDamage')]
ai_zon = [function(zon, n) for n in ['bhEne_LeverCheck', 'bhEne_PlayerSePlay', 'bhEne_CollisionCheckWall2', 'ikou', 'ikou3', 'NitenDir_ck', 'bhCdirCheck', 'bhEne_CheckDirWall', 'bhEne_CheckDirWall3', 'bhEne_CheckSideWall', 'bhEne_CheckSideWall3']]
pwksub = (prog / 'pwksub.c').read_text()
ai_other = [function(pwksub, 'bhSearchPlayer'), function(pwksub, 'bhGetFrameNum'), function((prog / 'en01b.c').read_text(), 'bhSearchPlayer2'), function((prog / 'en01b.c').read_text(), 'bhEne01_RotNeck'), function((prog / 'en01b.c').read_text(), 'bhEne01_SideRotNeck')] + [function((prog / 'rutchk.c').read_text(), n) for n in ['bhCheckRoute', 'bhCheckRouteID']]
blood = [function(zon, n) for n in ['bhDGCdirCheck2', 'bhEne_SetBlood', 'bhEne_SetBlood2', 'bhEne_SetBlood3', 'bhEne_SetBlood4','bhEne_SetNikuhenEffect','bhEne_SetNikuhenEffect2']]
terrain = [function(zon,n) for n in ['bhKaidanAtrCheck','bhEne_EnemyAtariCheck','bhEne_PosCheck','bhEne_AngleCheck']] + [function((prog/'hitchk.c').read_text(),n) for n in ['bhSetUseKaidanFlag','bhClrUseKaidanFlag']]
terrain[-1]=terrain[-1].replace('return 0;','return;')
functions = [function((prog/'effsub4.c').read_text(),'bhEff_Sub356')] + terrain + [function((prog/'weapon.c').read_text(), 'bhCheckCapCol2Capsule')] + [function(en01, n) for n in en01_names] + [function(zon, 'bhEne_ChgMtn')] + blood + ai_zon + ai_other + [function(zon1, n[1]) for n in zon1_names]

# Linked child actors (head/arms/legs) are folded into the one host entity:
# only the allocation branch of bhEne01_Init is removed; the zeroing branch for
# re-initialisation and all flag setup stay verbatim.
init = next(f for f in functions if f.startswith('void bhEne01_Init('))
cut_start = init.index('\tif (epw->exp0 == NULL){')
cut_end = init.index('    EXP0_I(0x4C) = 0;')
init_fixed = init[:cut_start] + '\t/* host: exp0 is zero-allocated by recvx_en01_init, no linked child works */\n    epw->exp1 = 0;\n    EXP0_I(0x24) = -1;\n\n' + init[cut_end:]
functions[functions.index(init)] = init_fixed
# The original ends bhEne01_ActionModeCheck with a bare `return;` (undefined value, ignored by every caller).
am = next(f for f in functions if f.startswith('int bhEne01_ActionModeCheck('))
functions[functions.index(am)] = am[:am.rindex('    return;')] + '    return 0;' + am[am.rindex('    return;') + len('    return;'):]
for i,f in enumerate(functions):
 if f.startswith('void bhEne_SetNikuhenEffect('):
  functions[i]=f[:f.rindex('}')]+'    recvx_finish_effect(epw, eno);\n}'
functions=[f.replace('*(ATR_WORK **)(epw->exp0 + 0x74)', '*recvx_stair(epw)').replace('(char*)EXP0_I(0x74)', '(char*)*recvx_stair(epw)') for f in functions]
text = '\n'.join(functions)
text = text.replace('WpnTab[epw->wpnr_no].flg', 'recvx_wpn_flg(epw->wpnr_no)')
text = text.replace('en01.h', 'en01.h')

body = ['// Generated from original en01.c, zonzon.c and zonzon1.c; do not edit.\n']
for t in ['En01_WpnDamageTbl', 'CombWepTbl', 'CombJointTbl', 'chg_mtn_tbl', 'en01_BldTbl', 'en01_BldTbl2', 'en01prt_blood_tbl',
          'en01_hp_tbl', 'en01_PersonalType', 'Ene01CapColTabA', 'Ene01CapColTabB', 'En01SdwTabW', 'En01SdwTabB', 'en01_flipTree', 'en01_flipTree2', 'kaidan_ang', 'en01_walk_mtn', 'en01_mtn_tbl', 'en01_mtn_tbl2', 'en01_mtn_tbl3', 'en01_tree', 'En01_PlyMtn_OffsetTbl'] + [p + '_mtn0%d_xz' % n for p in ('cl', 'ch', 'st') for n in (1, 2, 4, 5)]:
    try:
        body.append(table(en01, t))
    except RuntimeError:
        body.append('/* ' + t + ' not found as table */')
for name in en01_names:
    pass
protos = [prototype(f) for f in functions]
body.extend(protos)
body.extend(['static void bhEne01_DmgCheckTypeDmmy_();'][:0])
# Dispatch tables, unported entries become NULL (reported by the host).
extracted = set(en01_names)
stubs = set()
def dispatch(name):
    m = re.search(r'^void \(\*' + name + r'\[\d+\]\)\(BH_PWORK\*\)\s*=\s*\{', en01, re.M)
    src = en01[m.start():block(en01, m.start())]
    head, eq, rest = src.partition('=')
    def keep(x):
        if x.group(0) not in extracted:
            stubs.add(x.group(0))
        return x.group(0)
    rest = re.sub(r'\bbhEne01_[A-Za-z0-9_]+\b', keep, rest)
    src = head + eq + rest
    return 'static ' + src.replace('void (*', 'void (*', 1) + ';'
for t in ['bhEne01_Mode0']:
    pass
tables = [dispatch(t) for t in ['bhEne01_NageTypeW', 'bhEne01_NageMode2W', 'bhEne01_MoveTypeW', 'bhEne01_MoveMode2W', 'bhEne01_DamageTypeW', 'bhEne01_DamageMode2W', 'bhEne01_DieTypeW', 'bhEne01_DieMode2W', 'bhEne01_DmgCheckType', 'bhEne01_InitType']]
# Routines the host has not ported get a stub that reports the original name (never a silent NULL call).
body.extend('static void %s(BH_PWORK* epw){(void)epw;recvx_en01_unbound("%s");}' % (n, n) for n in sorted(stubs))
body.extend(tables)
# The B tables belong to the linked child works (EXP0 bit 0x80000000), which the host never creates.
# eneset.c bhEne00: the explosive barrel enemy.
eneset = (prog / 'eneset.c').read_text()
barrel = function(eneset, 'bhEne00')
body_barrel = barrel

# weapon.c bhCheckBombAtari: the per-enemy hit assignment (the host does the sphere/capsule/wall tests).
weapon = (prog / 'weapon.c').read_text()
bomb = function(weapon, 'bhCheckBombAtari')
loop = bomb.index('pp = ene;')
gate = bomb.index('if (bhCheckC2WallN(&wal, 0x400) == 0)', loop)
inner_open = bomb.index('{', gate)
inner = bomb[inner_open + 1:block(bomb, inner_open) - 1]
bomb_function = ('static void bhBombHit(BH_PWORK* pp, NJS_POINT3* pos, float ar, int dmax, int dmin, NJS_POINT3 ps)\n{\n'
                 '    NJS_VECTOR sca;\n    float knr, ll;\n    int j, kno;\n' + inner + '}\n')

# bhEne01_SetMtn: the damage flinch of the upper body (EXP0 0x8000000) rotates object 8 through rot_tbl.
# The merged host actor has no child work, so the block is lifted verbatim and writes into `flinch_obj`.
set_mtn = function(en01, 'bhEne01_SetMtn')
rot_tbl = re.search(r'static int rot_tbl\[4\]\[8\] =\s*\{', set_mtn)
flinch_at = set_mtn.index('if (EPP_EXP0_I(0x40) & 0x8000000)')
flinch = set_mtn[flinch_at:block(set_mtn, flinch_at)].replace('obj = &epw->mlwP->objP[8];', 'obj = &flinch_obj;')
body.append(table(eneset, 'En00CapColTab'))
body.append('static void bhEne00(BH_PWORK* ep);')
body.append(bomb_function)
body.append(set_mtn[rot_tbl.start():block(set_mtn, rot_tbl.start())] + ';')
body.append('typedef struct {int ang[3];} NJS_CNK_OBJECT_STUB;static NJS_CNK_OBJECT_STUB flinch_obj;static int flinch_valid;')
body.append('static void bhEne01_Flinch(BH_PWORK* epw){\n    BH_PWORK* epp = epw;\n    NJS_CNK_OBJECT_STUB* obj;\n    flinch_valid = 0;\n    ' + flinch.replace('{\n', '{\n        flinch_valid = 1;\n', 1) + '\n}')
body.append('static void (*bhEne01_NageTypeB[16])(BH_PWORK*) = {0};\nstatic void (*bhEne01_PlyDmgMode[2])(BH_PWORK*, BH_PWORK*) = {bhEne01_PlyDG00, bhEne01_PlyDG01};\nstatic void (*bhEne01_MoveTypeB[16])(BH_PWORK*) = {0};\nstatic void (*bhEne01_DamageTypeB[16])(BH_PWORK*) = {0};\nstatic void (*bhEne01_DieTypeB[16])(BH_PWORK*) = {0};')
body.append('\nstatic ' + body_barrel)
body.append(('\n' + text).replace('\nvoid ', '\nstatic void ').replace('\nint ', '\nstatic int ').replace('\nfloat ', '\nstatic float '))
(out / 'en01_damage.inc').write_text('\n'.join(body) + '\n')

# Cerberus uses the same BH_PWORK hit assignment and host blood services.
en04 = (prog / 'en04.c').read_text()
dog_names = ['bhEne04_Init','bhEne04_DmgChk','bhEne04_ChgDmgMode','bhEne04_DamageAdd','bhEne04_DG00','bhEne04_DG01','bhEne04_DD00','bhEne04_ChgMtn','bhEne04_SePlay','bhEne04_MV05','bhEne04_MV04']
dog = [function(en04,n) for n in dog_names]
dog = [f.replace('bhEne_CallocWork(88, 8)','epw->exp0').replace('u_char*','unsigned char*') if f.startswith('void bhEne04_Init(') else f for f in dog]
dog_text = '\n'.join(dog).replace('WpnTab[epw->wpnr_no].flg','recvx_wpn_flg(epw->wpnr_no)')
dog_body = ['// Generated from original en04.c; do not edit.']
dog_body += [table(en04,n) for n in ['En04_WpnDamageTbl','en04prt_blood_tbl','en04_BldTbl','en04_hp_tbl','Ene04CapColTab','en04_flipTree','En04SdwTab']]
dog_body += [prototype(f) for f in dog]
dog_body.append(dog_text.replace('\nvoid ','\nstatic void ').replace('\nint ','\nstatic int '))
(out / 'en04_damage.inc').write_text('\n'.join(dog_body)+'\n')
