#!/usr/bin/env python3
"""Erzeugt eine plattformfreie Fassung der Original-types.h.

Quelle: include/ps2/veronica/prog/types.h (bleibt unveraendert).
Ausgabe: build/native/generated/types/types.h (Build-Artefakt, nicht im Git).

Es wird nichts umgeschrieben oder ergaenzt. Entfernt werden ausschliesslich
  * die PS2-SDK-/CRI-Includes (ersetzt durch build-tools/ninja/),
  * die PDS_PERIPHERAL-Felder von SYS_WORK (Pad-Treiber); mit --exact-layout
    (ILP32-Build der Original-Engine) bleiben die Zeigerfelder als Zeiger auf
    den unvollstaendigen Typ stehen und das Wertfeld ms_per wird zu einem
    gleich grossen Bytefeld, sodass alle SYS_WORK-Offsets original bleiben,
  * reine PS2-Hardware-/Video-Strukturen, die kein Gegner/Spielobjekt nutzt.
Zeilenweise Verarbeitung, kein Regex ueber Blockgrenzen.
"""
import argparse, re, subprocess, sys
from pathlib import Path

REMOVE_STRUCTS = [
    'NODE', 'SCISSOR_NODE', 'SCISSOR',
    'TIM2_PICTUREHEADER', 'TIM2_PICTUREHEADER_EX',
    'StrFile', 'VoData', 'VoTag', 'QWORD', 'ViBuf', 'VideoDec', 'AudioDec',
]
REMOVE_FIELD_TYPE = 'PDS_PERIPHERAL'
INCLUDE_DROP = re.compile(
    r'#include\s*[<"](ninja\.h|sg_|eetypes|eestruct|libvu0|libcdvd|libipu|libmpeg|'
    r'libgraph|libdma|libpad|libmc|libsdr|csl\.h|libssyn|sif|sdrcmd|sdmacro|'
    r'PREFIX_PS2|override_katana|\.\./\.\./\.\./)')
NINJA = ['sg_xpt.h', 'ninjadef.h', 'ninjastr.h', 'ninjacnk.h', 'ninjaapi.h']


def main():
    ap = argparse.ArgumentParser()
    root = Path(__file__).resolve().parent.parent
    ap.add_argument('--types', default=str(root / 'include/ps2/veronica/prog/types.h'))
    ap.add_argument('--out', default=str(root / 'build/native/generated/types/types.h'))
    ap.add_argument('--ninja', default=str(root / 'build-tools/ninja'))
    ap.add_argument('--no-check', action='store_true')
    ap.add_argument('--exact-layout', action='store_true')
    a = ap.parse_args()

    lines = Path(a.types).read_text().split('\n')

    # 1) Struct-Bereiche (typedef struct ... } NAME;) zeilenweise suchen
    drop = set()
    closers = {}
    for i, l in enumerate(lines):
        m = re.match(r'^\}\s*(\w+)\s*;', l)
        if m:
            closers[m.group(1)] = i
    for name in REMOVE_STRUCTS:
        if name not in closers:
            sys.exit('struct nicht gefunden: ' + name)
        end = closers[name]
        start = end
        while start >= 0 and not lines[start].startswith('typedef struct'):
            start -= 1
        drop.update(range(start, end + 1))

    out = []
    inserted = False
    for i, l in enumerate(lines):
        if i in drop:
            continue
        if INCLUDE_DROP.match(l.strip()):
            continue
        if REMOVE_FIELD_TYPE in l:
            if a.exact_layout:
                m = re.match(r'^(\s*)(?:const\s+)?PDS_PERIPHERAL\s+(\w+)\s*;\s*//\s*offset\s*0x[0-9A-Fa-f]+,\s*size\s*(0x[0-9A-Fa-f]+)', l)
                if m:
                    out.append('%sunsigned char %s[%s]; /* PDS_PERIPHERAL (Pad-Treiber), Originalgroesse */' % m.groups())
                else:
                    out.append(l)
            continue
        out.append(l)
        if not inserted and l.startswith('#define _TYPES_H_'):
            out.append('/* generiert von tools/generate_types.py - nicht editieren */')
            # Das Original bekam diese libc-Header ueber ninja.h (float, math, stdio, stdlib)
            out.extend('#include <%s>' % h for h in ('limits.h', 'float.h', 'math.h', 'stdio.h', 'stdlib.h', 'string.h'))
            out.extend('#include "%s"' % n for n in NINJA)
            # PS2-/BSD-Typen, die der Original-Compiler ueber das SCE-SDK bekam.
            # Nur Namen: Enemy-Code benutzt sie als Deklaration bzw. Bytezeiger
            # (en04.c u_char/u_int, ps2_dummy.h sceGsDBuffDc, ps2_sg_pad.h
            # PDS_PERIPHERAL). Keine PS2-Header, keine Strukturinhalte.
            out.append('typedef unsigned char u_char; typedef unsigned short u_short;')
            out.append('typedef unsigned int u_int;')
            # LP64 libc declares u_long as unsigned long. It is the same
            # original 64-bit quantity, while spelling it long long conflicts
            # with sys/types.h when native C++ asset owners include the STL.
            # LLP64/ILP32 still need long long to retain the original width.
            out.append('#if defined(__LP64__)')
            out.append('typedef unsigned long u_long;')
            out.append('#else')
            out.append('typedef unsigned long long u_long;')
            out.append('#endif')
            # libvu0.h: reine Datentypen (16-Byte-ausgerichtete Vektoren/Matrizen), ohne SDK-Funktionen
            out.append('typedef float sceVu0FVECTOR[4] __attribute__((aligned(16)));')
            out.append('typedef int sceVu0IVECTOR[4] __attribute__((aligned(16)));')
            out.append('typedef float sceVu0FMATRIX[4][4] __attribute__((aligned(16)));')
            out.append('typedef struct sceGsDBuffDc sceGsDBuffDc;')
            out.append('typedef struct PDS_PERIPHERAL PDS_PERIPHERAL;')
            out.append('typedef struct PDS_PERIPHERALINFO PDS_PERIPHERALINFO;')
            # write() in en15.c kam aus der Metrowerks-libc
            out.append('#include <unistd.h>')
            inserted = True
    if not inserted:
        sys.exit('Include-Guard nicht gefunden')

    outp = Path(a.out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    outp.write_text('\n'.join(out))
    print('geschrieben:', outp, len(out), 'Zeilen')

    if not a.no_check:
        r = subprocess.run(['clang', '-fsyntax-only', '-w', '-x', 'c', '-I' + a.ninja,
                            str(outp)], capture_output=True, text=True, timeout=120)
        if r.returncode:
            print(r.stderr[:2000])
            sys.exit('bereinigte types.h kompiliert nicht')
        print('Syntaxpruefung ok')


if __name__ == '__main__':
    main()
