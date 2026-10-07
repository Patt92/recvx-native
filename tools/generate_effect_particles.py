#!/usr/bin/env python3
"""Extract the original particle emitter used by enemy blood (effsub3.c).

The structs, the PtclTbl/Eff302Prm tables, bhSetEffParticle and the bhEff302 mover are transcribed
verbatim. Only the allocator (AllocOwork over eff[512]) is replaced by the host pool.
"""
import re, sys
from pathlib import Path
from generate_native_tables import block

root, out = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'
header = (root / 'include/ps2/veronica/prog/effsub3.h').read_text()
src = (prog / 'effsub3.c').read_text()


def struct(name):
    m = re.search(r'typedef struct ' + name + r'\s*\{', header)
    end = block(header, m.start())
    return header[m.start():header.index(';', end) + 1]


def function(name):
    m = re.search(r'^(?:static )?(?:void|int|O_WRK\*) ' + name + r'\s*\([^)]*\)\s*\{', src, re.M)
    return src[m.start():block(src, m.start())]


def table(name):
    m = re.search(r'^(?:static )?(?:const )?[A-Za-z_0-9]+\s+' + re.escape(name) + r'(?:\[[^\]]*\])*\s*=\s*\{', src, re.M)
    return 'static ' + src[m.start():block(src, m.start())].replace('static ', '', 1) + ';'


parts = ['// Generated from original effsub3.c/effsub3.h; do not edit.\n']
types_h = (root / 'include/ps2/veronica/prog/types.h').read_text()
parts.append('typedef struct {float x,y,z,u,v;unsigned col;} NJS_TEXTURE_VTX;\ntypedef struct {void* p;void* col;void* tex;int num;} NJS_POINT3COL;\ntypedef union {unsigned color;struct {short u,v;} tex;} NJS_COLOR;')
for name in ['EFF302PRM_WORK', 'PD_WORK', 'PT_WORK', 'E02_WORK', 'E02_WRK', 'E17_WORK']:
    parts.append(struct(name))
m = re.search(r'typedef struct BP_WORK\s*\{', types_h)
parts.append(types_h[m.start():types_h.index(';', block(types_h, m.start())) + 1])
parts.append(table('Eff302Prm'))
for number in range(19):
    parts.append(table('PtclDat%02d' % number))
parts.append(table('PtclTbl'))
parts.append('static O_WRK* AllocOwork();\nstatic O_WRK* AllocOworkOne();')
parts.append(function('bhSetBloodPoolLnk'))
parts.append(function('bhEff300'))
parts.append(function('bhSetEffParticle'))
parts.append(function('bhEff302'))
parts.append(function('bhEff_PtclLineDraw'))
text = '\n'.join(parts).replace('njSetMatrix(NULL, cam.mtx);','njSetMatrix(NULL, &cam.mtx);')
# C++ const correctness for the const PtclDat tables (the C original relies on implicit conversion).
text = text.replace('PD_WORK* pdP;', 'const PD_WORK* pdP;')
# FuncTbl[drw_typ] picks the draw procedure; the host draws from the E02 state, keep the type instead.
text = text.replace('oP->func = (void*)FuncTbl[pdP->drw_typ];', 'oP->func = (void*)(intptr_t)pdP->drw_typ;')
text = text.replace('oP->tex_id = sys->ef_tn[4];', 'oP->tex_id = 4; /* effect texture category 4 (blood/spark atlas) */')
out.mkdir(parents=True, exist_ok=True)
(out / 'effect_particles.inc').write_text(text + '\n')
