#!/usr/bin/env python3
"""Extract the original en07 (Bat) enemy procedures for the native host.

Every function below is copied verbatim from src/ps2/veronica/prog/en07.c and
its dispatch tables. The only edit is the host ABI: bhEne_CallocWork (sys->memp
arena) is replaced by the fixed EXP0 block of the work, identical to the
already ported bhEne04_Init/bhEne01 pattern.
"""
import re, sys
from pathlib import Path
from generate_native_tables import block

root, out = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'
en07 = (prog / 'en07.c').read_text()

def strip(text):
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)

def function(text, name):
    text = strip(text)
    m = re.search(r'^(?:static )?(?:void|int|unsigned char|float)\s*\*?\s*' + re.escape(name) +
                  r'\s*\([^)]*\)\s*\{', text, re.M)
    if not m:
        raise RuntimeError('missing function ' + name)
    return text[m.start():block(text, m.start())]

def table(text, name):
    m = re.search(r'^void \(\*' + re.escape(name) + r'\[[^\]]*\]\)\(BH_PWORK\*\)\s*=\s*\{', strip(text), re.M)
    if not m:
        raise RuntimeError('missing table ' + name)
    end = strip(text).index('};', m.start()) + 2
    return strip(text)[m.start():end] + ';'

# Every bhEne07* routine and the file-scope tables, in source order.
names = sorted(set(re.findall(r'^(?:void|int)\s+(bhEne07\w*)\s*\(', strip(en07), re.M)))
tables = ['bhEne07_Mode0', 'bhEne07_BrainType', 'bhEne07_MoveMode2', 'bhEne07_NageMode2', 'bhEne07_DeadMode2']

inc = root / 'include/ps2/veronica/prog'
types = (inc / 'types.h').read_text()
macros = (inc / 'macros.h').read_text()

def struct(text, name):
    m = re.search(r'typedef struct ' + name + r'\s*\{', text)
    if not m:
        raise RuntimeError('missing struct ' + name)
    end = block(text, m.start())
    return text[m.start():text.index(';', end) + 1]

parts = ['// Generated from original types.h, macros.h and en07.c (Bat); do not edit.']
# Same struct set the en01 generator needs for BH_PWORK and its helpers.
ptr = lambda s: s.replace('NJS_TEXLIST*', 'void*').replace('NJS_MDATA2_MOD*', 'void*')
# Host header set of the en01 generator: the Ninja base types BH_PWORK needs.
parts.append('typedef struct NJS_CNK_OBJECT {unsigned evalflags;void* model;float pos[3];int ang[3];float scl[3];struct NJS_CNK_OBJECT* child;struct NJS_CNK_OBJECT* sibling;} NJS_CNK_OBJECT;\n'
             'typedef struct {NJS_POINT3 c1,c2;float r;} NJS_CAPSULE;\n')
for name in ['O_WORK', 'ML_WORK', 'MN_WORK', 'CPCL', 'BH_PWORK', 'ATR_WORK', 'BP_WORK']:
    parts.append(ptr(struct(types, name)))
parts.append('\n'.join(l for l in macros.splitlines() if re.match(r'#define (EXP0_|EXP1_|PEXP0_|NJM_DEG_ANG)', l)))
for name in ['ENE07_HITPOINT', 'junction_tree', 'SdwTab', 'AtkPos', 'AtkAngle', 'CapColTab']:
    m = re.search(r'^static [A-Za-z_0-9 ]+\s+' + name + r'(\[[^\]]*\])*\s*=\s*\{', strip(en07), re.M)
    if not m:
        raise RuntimeError('missing data ' + name)
    end = strip(en07).index('};', m.start()) + 2
    parts.append(strip(en07)[m.start():end] + ';')

# Forward declarations: the dispatch tables precede the routines they name.
for name in names:
    head = strip(en07)[re.search(r'^(?:void|int)\s+' + re.escape(name) + r'\s*\(', strip(en07), re.M).start():]
    head = head[:head.index('{')].strip()
    parts.append('static ' + head + ';')
parts.append('void bhEne_Event(BH_PWORK* epw);')
parts += [function(en07, n) for n in names]
for name in tables:
    parts.append(table(en07, name))

statics = '\n'.join(parts)
# Host ABI: one fixed EXP0 block per work instead of the sys->memp arena.
statics = re.sub(r'bhEne_CallocWork\(0x44, 0x8\)', 'epw->exp0', statics)
statics = re.sub(r'bhEne_CallocWork\(0x8, 0x8\)', 'epw->exp1', statics)
if 'bhEne_CallocWork' in statics:
    raise RuntimeError('unexpected bhEne_CallocWork remained')

out.mkdir(parents=True, exist_ok=True)
(out / 'en07_bat.inc').write_text(statics + '\n')
