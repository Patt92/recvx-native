#!/usr/bin/env python3
"""Extract the original enemy state machines of en10/en20/en25/en54/en55/en71.

Everything below is transcribed verbatim from the original sources:

  * en71.c  Ant C      bhEne71 + Init/Move/Nage/Damage/Die and the Mode0 table
  * en20.c  Ant B      bhEne20 + Init/Move/MV00..MV07/Nage/Damage/Die
  * en10.c  Ant A      bhEne10 + Init/CheckPos/Brain/Move/MV00..MV02/Nage/...
  * en25.c  Ceiling trap   bhEne25 + Init/Brain/Move/MV00..MV03/SetEffect/
                           PlayerControl and its BloodParam
  * en54.c  Armored knight A (sword/medium shield)
  * en55.c  Armored knight B (lance/large shield)

plus the shared helpers these call: bhEne_CallSE/bhEne_DirTarget/bhArcTan2/
bhEne_SetBloodEffect4/bhEne_BloodPool (zonzon1.c), bhAddSpeed (pwksub.c) and
bhEne_CallocWork (eneset.c).

The only edits are the ones the host ABI forces and each is annotated at the
replacement site in `enemy_ai.c`:

  * bhEne_CallocWork allocates from the original sys->memp arena. The host owns
    one fixed EXP0 block per work instead, so the call becomes `epw->exp0`
    exactly as the already ported en04 Init does.
  * The function-pointer dispatch tables keep their original order and are
    emitted as statics after the forward declarations.

No behaviour, ordering, table value, counter or flag is changed.
"""
import re
import sys
from pathlib import Path

from generate_native_tables import block

root, out = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'


def source(name):
    return (prog / name).read_text(encoding='utf-8', errors='strict')


def strip(text):
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)


def function(text, name):
    """Return the original definition of `name`, comments removed."""
    text = strip(text)
    pattern = (r'^(?:static\s+)?(?:void|int|float|unsigned\s+char|unsigned\s+int|'
               r'BH_PWORK\s*\*|ATR_WORK\s*\*|O_WRK\s*\*|void\s*\*)\s*\*?\s*'
               + re.escape(name) + r'\s*\([^;{)]*\)\s*\{')
    m = re.search(pattern, text, re.M)
    if not m:
        raise RuntimeError('missing function ' + name)
    return text[m.start():block(text, m.start())]


def prototype(body):
    head = body[:body.index('{')].strip()
    return 'static ' + head.replace('static ', '') + ';'


def table(text, name):
    """Return a function-pointer / data table definition verbatim."""
    text = strip(text)
    # Covers plain arrays (`T name[n] = {`), function-pointer arrays written as
    # `T (*name[n])(args) = {` and the `T* name[n] = {` spelling.
    pattern = (r'^(?:static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?[A-Za-z_0-9]+'
               r'[^;={]*?\b' + re.escape(name) + r'\s*\[[^\]]*\][^;={]*=\s*\{')
    m = re.search(pattern, text, re.M)
    if not m:
        raise RuntimeError('missing table ' + name)
    body = text[m.start():block(text, m.start())]
    return 'static ' + body.replace('static ', '', 1) + ';'


# Per enemy: source file, the original functions in definition order and the
# original dispatch/data tables that reference them.
ENEMIES = [
    ('en71.c',
     ['bhEne71', 'bhEne71_Init', 'bhEne71_Move', 'bhEne71_Nage',
      'bhEne71_Damage', 'bhEne71_Die'],
     ['bhEne71_Mode0']),
    ('en20.c',
     ['bhEne20', 'bhEne20_Init', 'bhEne20_Move',
      'bhEne20_MV00', 'bhEne20_MV01', 'bhEne20_MV02', 'bhEne20_MV03',
      'bhEne20_MV04', 'bhEne20_MV05', 'bhEne20_MV06', 'bhEne20_MV07',
      'bhEne20_Nage', 'bhEne20_Damage', 'bhEne20_Die'],
     ['bhEne20_Mode0', 'bhEne20_MoveMode2']),
    ('en10.c',
     ['bhEne10', 'bhEne10_Init', 'bhEne10_CheckPos', 'bhEne10_Brain',
      'bhEne10_Move', 'bhEne10_MV00', 'bhEne10_MV01', 'bhEne10_MV02',
      'bhEne10_Nage', 'bhEne10_Damage', 'bhEne10_Die'],
     ['bhEne10_Mode0', 'bhEne10_MoveMode2']),
    ('en25.c',
     ['bhEne25', 'bhEne25_Init', 'bhEne25_Brain', 'bhEne25_Move',
      'bhEne25_MV00', 'bhEne25_MV01', 'bhEne25_MV02', 'bhEne25_MV03',
      'bhEne25_Nage', 'bhEne25_Damage', 'bhEne25_Die',
      'bhEne25_SetEffect', 'bhEne25_PlayerControl'],
     ['bhEne25_Mode0', 'bhEne25_MoveMode2']),
    ('en54.c',
     ['bhEne54', 'bhEne54_MainLoop', 'bhEne54_SetMtn', 'bhEne54_CollCheck',
      'bhEne54_CalcEnemy', 'bhEne54_Init', 'bhEne54_Move', 'bhEne54_Damage',
      'bhEne54_MVType00', 'bhEne54_MV00', 'bhEne54_DGType00', 'bhEne54_DG00'],
     ['bhEne54_Mode0', 'bhEne54_MoveType', 'bhEne54_MoveMode2',
      'bhEne54_DamageType', 'bhEne54_DamageMode2']),
    ('en55.c',
     ['bhEne55', 'bhEne55_MainLoop', 'bhEne55_SetMtn', 'bhEne55_CollCheck',
      'bhEne55_CalcEnemy', 'bhEne55_Init', 'bhEne55_Move', 'bhEne55_Damage',
      'bhEne55_MVType00', 'bhEne55_MV00', 'bhEne55_DGType00', 'bhEne55_DG00'],
     ['bhEne55_Mode0', 'bhEne55_MoveType', 'bhEne55_MoveMode2',
      'bhEne55_DamageType', 'bhEne55_DamageMode2']),
]

# Shared original helpers the above call, with the file they are defined in.
HELPERS = [
    ('zonzon1.c', 'bhArcTan2'),
    ('zonzon1.c', 'bhEne_CallSE'),
    ('zonzon1.c', 'bhEne_DirTarget'),
    ('zonzon1.c', 'bhEne_SetBloodEffect4'),
    ('zonzon1.c', 'bhEne_BloodPool'),
    ('pwksub.c', 'bhAddSpeed'),
]

parts = ['// Generated from original en10/en20/en25/en54/en55/en71.c,'
         ' zonzon1.c and pwksub.c; do not edit.']

# en25 keeps its own file-scope blood parameter block.
en25 = strip(source('en25.c'))
m = re.search(r'^static BP_WORK BloodParam\s*=\s*\{', en25, re.M)
if not m:
    raise RuntimeError('missing en25 BloodParam')
parts.append(en25[m.start():en25.index(';', block(en25, m.start())) + 1])

# zonzon1.c BloodCol/BloodType, read by bhEne_SetBloodEffect4/bhEne_BloodPool.
zon1 = source('zonzon1.c')
parts.append(table(zon1, 'BloodCol'))
parts.append(table(zon1, 'BloodType'))

helper_bodies = [function(source(file), name) for file, name in HELPERS]
parts += [prototype(b) for b in helper_bodies]

bodies = []
for file, names, tables in ENEMIES:
    text = source(file)
    group = [function(text, name) for name in names]
    bodies += group
    # The original dispatch typedefs. en10.c and en20.c both spell them
    # Mode0_proc/MoveMode2_proc at file scope; in the single host translation
    # unit they are suffixed per enemy, and the tables below with them.
    suffix = '_' + file[:-2]
    local = {}
    for m in re.finditer(r'^typedef void \(\*([A-Za-z_0-9]+)\)\(BH_PWORK\*[^)]*\);',
                         strip(text), re.M):
        local[m.group(1)] = m.group(1) + suffix
        parts.append('typedef void (*%s)(BH_PWORK*);' % local[m.group(1)])
    parts += [prototype(b) for b in group]

    def rename(code):
        for old, new in local.items():
            code = re.sub(r'\b' + re.escape(old) + r'\b', new, code)
        return code

    # The tables reference the procedures, so they follow the prototypes.
    parts += [rename(table(text, name)) for name in tables]

statics = '\n'.join(helper_bodies + bodies)
# File-scope definitions become statics inside the single host translation unit.
statics = re.sub(r'^(void|int|float|unsigned char|unsigned int|void\s*\*|O_WRK\s*\*)\s',
                 r'static \1 ', statics, flags=re.M)
# Host ABI: one fixed EXP0 block per work instead of the sys->memp arena
# (identical to the already ported bhEne04_Init).
statics = statics.replace('bhEne_CallocWork(4, 8)', 'epw->exp0')
statics = statics.replace('bhEne_CallocWork(0x24, 8)', 'epw->exp0')
statics = statics.replace('bhEne_CallocWork(0x30, 8)', 'epw->exp0')
statics = statics.replace('bhEne_CallocWork(0xC, 0x8)', 'epw->exp0')
if 'bhEne_CallocWork' in statics:
    raise RuntimeError('unexpected bhEne_CallocWork size remained')
parts.append(statics)

out.mkdir(parents=True, exist_ok=True)
(out / 'enemy_ai.inc').write_text('\n'.join(parts) + '\n')
