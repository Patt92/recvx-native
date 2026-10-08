#!/usr/bin/env python3
"""Erzeugt eine plattformfreie Fassung der Original-types.h.

Quelle: include/ps2/veronica/prog/types.h (bleibt unveraendert).
Ausgabe: build/native/generated/types/types.h (Build-Artefakt, nicht im Git).

Es wird nichts umgeschrieben oder ergaenzt. Entfernt werden ausschliesslich
  * die PS2-SDK-/CRI-Includes (ersetzt durch build-tools/ninja/),
  * die PDS_PERIPHERAL-Felder von SYS_WORK (Pad-Treiber),
  * reine PS2-Hardware-/Video-Strukturen, die kein Gegner/Spielobjekt nutzt.
Zeilenweise Verarbeitung, kein Regex ueber Blockgrenzen.
"""
import argparse, re, subprocess, sys
from pathlib import Path

REMOVE_STRUCTS = [
    'NODE', 'SCISSOR_NODE', 'SCISSOR', 'PS2_PLANE',
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
            out.append('typedef unsigned int u_int; typedef unsigned long long u_long;')
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
