#!/usr/bin/env python3
"""Einmaliger Uebersetzer der Original-Effektprozeduren (effsub0..6).

Liest die Originalquellen als Semantikreferenz (nie kompiliert, nie gelinkt) und
schreibt die Prozeduren als nativen Quelltext nach native/effect_procedures.cpp.
Der Aufruf geschieht von Hand, genau einmal je Erweiterung; das Ergebnis wird
danach als nativer Quelltext gepflegt. Das Build ruft den Uebersetzer NICHT auf
(tools/check_no_original_core.py verbietet Funktionskoerper-Generatoren im Bau).

Abdeckungstor: eine Prozedur wird nur ausgegeben, wenn nach der Uebersetzung
kein `sys->`, kein `op->`, kein unbekannter Aufruf und keine unbekannte
Typbezeichnung mehr uebrig bleibt. Alles andere landet im Bericht und wird
NICHT ausgegeben — eine halb uebersetzte Prozedur entsteht nie.

Aufruf:  python3 tools/generate_effect_procedures.py [--report PFAD]
"""
import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROG = ROOT / 'src/ps2/veronica/prog'
OUTPUT = ROOT / 'native/effect_procedures.cpp'

# Reihenfolge ist bedeutsam: spezielle Ausdruecke vor den allgemeinen Praefixen.
REWRITES = [
    (r'sys->ef_trs\[sys->ef_trsn\+\+\]\s*=\s*op;', 'effect_queue_translucent(w);'),
    (r'bhSetEffectTb\(&sys->ef,\s*([^,]+),\s*([^,]+),\s*([^)]+)\)',
     r'effect_request(effect_request_template(),\1,\2,\3)'),
    (r'bhSetEffectTb\(pEffect,\s*([^,]+),\s*([^,]+),\s*([^)]+)\)',
     r'effect_request(*pEffect,\1,\2,\3)'),
    (r'bhSetExtraEffectWork\(\)', 'effect_extra_work()'),
    (r'sys->ef_tn\[([^\]]+)\]\s*\+\s*([A-Za-z_][\w>.]*)', r'effect_category_index(\1,\2)'),
    (r'sys->bl_ct', 'effect_blood_counter()'),
    (r'\brand\(\)', 'effect_random()'),
    (r'\bsetentry\(', 'effect_set_entry('),
    (r'\beffinit\(op\)', 'effect_init(w)'),
    (r'&sys->ef\b', '&effect_request_template()'),
    (r'\bsys->ef\b', 'effect_request_template()'),
    (r'\bEF_WORK\b', 'EffectRequest'),
    (r'\bUV_WORK\b', 'EffectUv'),
    (r'\bNJS_TEXTURE_VTX\b', 'EffectVertex'),
    (r'\bO_WRK\b', 'EffectSprite'),
    (r'\bop->', 'w.'),
    (r'\bop\b(?!\s*->)', 'w'),
]

# Hilfsroutinen desselben Originals, die die Prozeduren aufrufen
# (effsub0.c:63 effset, :101 getuv). Wie die Prozeduren woertlich uebernommen.
HELPERS = [
    'void effset(O_WRK* op, UV_WORK* uvp, unsigned short num)',
    'void getuv(unsigned char type, O_WRK* op)',
]
HELPER_NAMES = {'effset', 'getuv'}

# Aufrufe und Typen, die der Shim (native/effect_core.h) bereitstellt.
ALLOWED_CALLS = {
    'effect_queue_translucent', 'effect_request', 'effect_extra_work',
    'effect_category_index', 'effect_blood_counter', 'effect_random',
    'effect_set_entry', 'effect_init', 'effect_request_template', 'effset', 'getuv',
    'njSin', 'njCos', 'njSqrt', 'njFraction', 'njAddVector', 'njSubVector',
    'njInnerProduct', 'njUnitVector', 'njCalcPoint', 'njCalcVector',
    'njUnitMatrix', 'njPushMatrixEx', 'njPopMatrixEx', 'njGetMatrix', 'njSetMatrix',
    'njMultiMatrix', 'njTranslate', 'njTranslateEx', 'njRotateEx', 'njRotateX',
    'njRotateY', 'njRotateZ', 'njRotateXYZ', 'njScale', 'njScaleEx', 'njUnitRotPortion',
    'njSetTextureNum', 'njTextureFilterMode', 'njSetTextureNumG',
    'abs', 'fabs', 'fabsf', 'sqrt', 'sqrtf', 'sin', 'cos', 'atan2', 'floor', 'floorf',
    'njUnitTransPortion',
}
SHIM_NAMES = {
    'effect_queue_translucent', 'effect_request', 'effect_extra_work',
    'effect_category_index', 'effect_blood_counter', 'effect_random', 'effect_set_entry',
    'effect_request_template',
}
ALLOWED_TYPES = {'EffectSprite', 'EffectRequest', 'EffectUv', 'EffectVertex'}
# Kontrollfluß und Sprachschluesselwoerter, die kein Aufruf sind.
KEYWORDS = {
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'case', 'do', 'else', 'break',
    'continue', 'static', 'const', 'int', 'unsigned', 'float', 'short', 'char', 'void',
}


def procedure_body(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def translate(body, signature, name):
    # Die Signatur zuerst und genau: der Arbeitszeiger wird zur nativen Referenz.
    native_signature = signature.replace('O_WRK* op', 'EffectSprite& w') \
                                .replace('UV_WORK*', 'EffectUv*')
    text = body.replace(signature, native_signature, 1)
    for pattern, replacement in REWRITES:
        text = re.sub(pattern, replacement, text)
    return text


# Felder des nativen Effektarbeitssatzes (native/effect_sprites.h) und des
# Eckpunkts. Ein Zugriff, den der Shim nicht kennt, haelt die Prozedur zurueck.
OP_FIELDS = {
    'id', 'type', 'mode0', 'mode1', 'mode2', 'mode3', 'flg', 'stflg', 'ani_ct',
    'polygon_queued', 'frame_queued', 'mdflg', 'ayp', 'yn', 'func', 'ax', 'ay', 'az',
    'lkono', 'ct0', 'ct1', 'ct2', 'ct3', 'tex_id', 'bl_src', 'bl_dst', 'uv', 'mdlver',
    'flr_no', 'px', 'py', 'pz', 'gpx', 'gpy', 'gpz', 'lox', 'loy', 'loz', 'xn', 'zn',
    'sx', 'sy', 'sz', 'sxb', 'syb', 'szb', 'spd', 'lkwkp', 'mlwP', 'jno', 'tv',
    'quads', 'extra', 'exp0', 'flame_compact_uv',
}
VTX_FIELDS = {'x', 'y', 'z', 'u', 'v', 'col'}
# Originalglobale, die der Shim bewusst nicht fuehrt.
OPAQUE_GLOBALS = {'eff', 'cam', 'rom', 'ene', 'plp', 'sys', 'effp'}


def uncovered(text, name):
    """Was nach der Uebersetzung noch unversorgt ist. Kommentare tragen nichts bei."""
    code = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', text, flags=re.S)
    if 'sys->' in code or 'op->' in code:
        return 'sys->/op->'
    fields = set(re.findall(r'\bw\.(\w+)', code))
    unknown_fields = sorted(f for f in fields if f not in OP_FIELDS)
    if unknown_fields:
        return 'unbekannte Arbeitsfelder: ' + ', '.join(unknown_fields)
    bad_globals = sorted(g for g in OPAQUE_GLOBALS if re.search(r'\b' + g + r'\b', code))
    if bad_globals:
        return 'unbekannte Originalglobale: ' + ', '.join(bad_globals)
    calls = set(re.findall(r'\b([A-Za-z_]\w*)\s*\(', code))
    unknown = sorted(c for c in calls if c not in ALLOWED_CALLS and c not in KEYWORDS
                     and c != name and c not in HELPER_NAMES and c not in SHIM_NAMES)
    if unknown:
        return 'unbekannte Aufrufe: ' + ', '.join(unknown)
    types = set(re.findall(r'\b([A-Z]\w+)\b', code))
    unknown_types = sorted(t for t in types if t not in ALLOWED_TYPES
                           and t not in {'NJS_POINT3', 'NJS_VECTOR', 'NJS_MATRIX', 'NJS_ANGLE',
                                         'EffectSprite', 'NULL', 'U', 'F'})
    if unknown_types:
        return 'unbekannte Typen: ' + ', '.join(unknown_types)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, default=ROOT / 'work/effect_procedures.generator.md')
    args = parser.parse_args()

    # Prozeduren und die Hilfsroutinen desselben Originals, mit ihrer
    # vollstaendigen Originalsignatur.
    entries = []
    for path in sorted(PROG.glob('effsub*.c')):
        source = path.read_text(encoding='utf-8', errors='replace')
        entries += [(m.group(0), m.group(2), path.name)
                    for m in re.finditer(r'(void (bhEff\d+)\(O_WRK\* op\))', source)]
        for helper in HELPERS:
            if helper in source:
                entries.append((helper, re.match(r'void (\w+)', helper).group(1), path.name))

    emitted, skipped = [], []
    for signature, name, filename in entries:
        source = (PROG / filename).read_text(encoding='utf-8', errors='replace')
        text = translate(procedure_body(source, signature), signature, name)
        reason = uncovered(text, name)
        if reason:
            skipped.append((name, filename, reason))
        else:
            emitted.append((name, filename, text))

    ordered = sorted(emitted, key=lambda e: 0 if e[0] in HELPER_NAMES else 1)
    parts = ['// Erzeugt von tools/generate_effect_procedures.py aus effsub0..6; danach als',
             '// nativer Quelltext gepflegt. Aufruf von Hand, nicht aus dem Build.',
             '#include "effect_core.h"',
             'namespace recvx {',
             'namespace {']
    for name, _, text in ordered:
        parts.append(text)
    parts.append('}')
    parts.append('')
    parts.append('// effect.c:869 bhControlEffect: die Prozedur je Effektkennung.')
    parts.append('const EffectProcedure original_effect_procedures[]={')
    for name, _, _ in emitted:
        if name in HELPER_NAMES:
            continue
        parts.append('    {%s,%s},' % (int(name[5:]), name))
    parts.append('};')
    parts.append('const std::size_t original_effect_procedure_count='
                 'sizeof(original_effect_procedures)/sizeof(original_effect_procedures[0]);')
    parts.append('}')
    OUTPUT.write_text('\n'.join(parts) + '\n')

    report = ['# Effektprozedur-Generator, Laufbericht', '',
              'Prozeduren in effsub0..6 gefunden: %d' % len(entries),
              'ausgegeben: %d' % len(emitted), 'zurueckgestellt: %d' % len(skipped), '',
              '## Zurueckgestellt (Shim deckt die Dienstflaeche noch nicht)', '',
              '| Prozedur | Datei | Grund |', '|---|---|---|']
    for name, filename, reason in skipped:
        report.append('| %s | %s | %s |' % (name, filename, reason))
    args.report.write_text('\n'.join(report) + '\n')
    print('ausgegeben %d, zurueckgestellt %d (Bericht: %s)' % (len(emitted), len(skipped), args.report))


if __name__ == '__main__':
    main()
