#!/usr/bin/env python3
"""Uebersetzt src/ps2/veronica/prog/en*.c fuer den Host (Original bleibt unveraendert).

Ausgabe: build/native/generated/enemy/<name>.c

Jede Aenderung ist eine reine Schreibweisen-Uebersetzung und wird hier mit
Beleg dokumentiert. Keine Logik wird veraendert.

1) Metrowerks "*mkaP->key++" / "*mkfP->key++"  (81 Stellen in en03.c, en23.c)
   `key` ist ein Array-Feld (NJS_MKEY_A_MOD.key[3] bzw. NJS_MKEY_F_MOD.key[3]).
   Metrowerks akzeptiert `array++`, Clang nicht. Die Disassembly (SLUS_201.84,
   gelesen mit rabbitizer) zeigt, was mwcc daraus macht: Der Zeiger wird nicht
   veraendert, die Zugriffe nach einer Zuweisung von mkaP/mkfP lesen feste
   Offsets der Basis. Belegt fuer:
     bhEne03_DG12 (if/else-Zweig): lhu 0x0/0x2/0x4 -> njRotateXYZ(key[0], -key[1], -key[2])
     bhEne23_DG06: lhu 0x0/0x2/0x4 -> ang[0..2] = key[0..2]
     bhEne03_DG02: lwc1 0x0/0x4/0x8 -> trans.x/y/z = key[0..2]
   Daraus folgt: k-ter Zugriff nach einer Zuweisung = key[k mod 3] (der Rest,
   if/else-Zweige mit je drei Zugriffen, folgt demselben Muster). Die uebrigen
   Funktionen sind nicht einzeln gegen die Disassembly geprueft.

2) Implizite Funktionsdeklarationen
   Metrowerks (C89) akzeptiert Aufrufe ohne Prototyp als `int f()`. Auf arm64
   waeren Zeigerrueckgaben dadurch 32 Bit breit (Abschneiden). Es wird nur der
   Original-Header eingebunden, der den Prototyp bereits enthaelt (include/ps2/
   veronica/prog). Keine neuen Deklarationen, keine Logikaenderung.
"""
import argparse, re, subprocess, sys
from pathlib import Path

KEY = re.compile(r'\*\s*(mk[af]P)->key\+\+')
ASSIGN = re.compile(r'\b(mk[af]P)\s*(?:=|\+=)')
FUNC = re.compile(r'^[A-Za-z_].*\bbhEne\w*\s*\(')


def fix_key(lines, report):
    counts = {}
    n = 0
    for i, l in enumerate(lines):
        if FUNC.match(l):
            counts = {}
        # Zuweisung (nicht die key-Zugriffe selbst) setzt den Zaehler zurueck
        stripped = KEY.sub('', l)
        for m in ASSIGN.finditer(stripped):
            counts[m.group(1)] = 0

        def rep(m):
            nonlocal n
            v = m.group(1)
            k = counts.get(v, 0)
            counts[v] = k + 1
            n += 1
            return '%s->key[%d]' % (v, k % 3)
        lines[i] = KEY.sub(rep, l)
    report['key++'] = n
    return lines


UNDECL = re.compile(r"call to undeclared function '(\w+)'")


def prototype_headers(inc):
    """Funktionsname -> Header mit dem Original-Prototyp."""
    table = {}
    pat = re.compile(r'^[A-Za-z_][\w \*]*?[ \*](\w+)\s*\(')
    for h in sorted(inc.glob('*.h')):
        for line in h.read_text(errors='surrogateescape').split('\n'):
            m = pat.match(line)
            if m and not line.startswith(('static', 'typedef', 'return', 'extern "')):
                table.setdefault(m.group(1), h.name)
    return table


def add_includes(path, flags, table, report):
    """Fuegt fuer implizit deklarierte Funktionen den Original-Header hinzu."""
    cmd = ['clang', '-fsyntax-only', '-w', '-ferror-limit=0'] + flags + [str(path)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    need = []
    for m in UNDECL.finditer(r.stderr):
        h = table.get(m.group(1))
        if h and h not in need:
            need.append(h)
    if not need:
        return
    text = path.read_text(errors='surrogateescape').split('\n')
    last = max((i for i, l in enumerate(text) if l.startswith('#include')), default=-1)
    ins = ['#include "%s" /* Host: Prototyp statt impliziter int-Deklaration */' % h for h in need]
    text[last + 1:last + 1] = ins
    path.write_text('\n'.join(text), errors='surrogateescape')
    report['includes'] = need


def main():
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=str(root / 'src/ps2/veronica/prog'))
    ap.add_argument('--out', default=str(root / 'build/native/generated/enemy'))
    ap.add_argument('--no-includes', action='store_true')
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    R = root
    flags = ['-DRECVX_NATIVE=1', '-include', str(R / 'build/native/generated/types/types.h'),
             '-I' + str(R / 'build-tools/ninja'), '-I' + str(R / 'native'),
             '-I' + str(R / 'include/ps2/veronica/prog'),
             '-I' + str(R / 'build/native/generated'), '-I' + str(Path(a.src))]
    table = prototype_headers(R / 'include/ps2/veronica/prog')
    srcs = [Path(a.src) / f for f in a.files] if a.files else sorted(Path(a.src).glob('en*.c'))
    for p in srcs:
        rep = {}
        lines = p.read_text(errors='surrogateescape').split('\n')
        lines = fix_key(lines, rep)
        (out / p.name).write_text('\n'.join(lines), errors='surrogateescape')
        if not a.no_includes:
            add_includes(out / p.name, flags, table, rep)
            if rep.get('includes'):
                print('%s: + %s' % (p.name, ' '.join(rep['includes'])))
        if rep['key++']:
            print('%s: %d key++ uebersetzt' % (p.name, rep['key++']))


if __name__ == '__main__':
    main()

# Hinweis: Clang meldet 3 Funktionen ohne return (bhEne01_ActionModeCheck,
# bhEne09_DmgCheck, bhEne17_CameraControl) als Fehler (-Wreturn-mismatch);
# mwcc nicht. Aufrufer verwerfen den Wert. Kompilieren mit -Wno-return-mismatch,
# der Quelltext bleibt unveraendert.
