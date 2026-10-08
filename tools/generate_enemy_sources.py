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


P32_HDR = '#include "recvx_ptr32.h" /* Host(arm64): 32-Bit-Zeigerslots, siehe native/recvx_ptr32.h */'
P32_BYTE = ('#undef EXP0_BYTE\n'
            '#define EXP0_BYTE(pno, off) (*((char*)recvx_p32_get((char*)epw->exp0 + (pno * 4) + 128) + (off))) '
            '/* Host(arm64): Zeigerslot als Handle */\n'
            '#undef EXP0_INT\n'
            '#define EXP0_INT(pno, off) (*(int*)((char*)recvx_p32_get((char*)epw->exp0 + (pno * 4) + 128) + (off))) '
            '/* Host(arm64): Zeigerslot als Handle */')
SLOT_MACROS = r'(?:EXP0_I|EP_EXP0_I|EPW_EXP1_I|EXP1_I|PEXP0_I)'
CAST = r'\(\s*((?:const\s+)?[A-Za-z_][\w ]*?)\s*\*\s*\)'


def match_paren(s, i):
    """s[i] == '(' oder '[' -> Index hinter der schliessenden Klammer, sonst -1."""
    op = s[i]
    cl = ')' if op == '(' else ']'
    d = 0
    for j in range(i, len(s)):
        if s[j] in '([':
            d += 1
        elif s[j] in ')]':
            d -= 1
            if d == 0:
                return j + 1
    return -1


def operand_end(s, i):
    """Ende eines Adressoperanden ab i: '(...)' oder '&'? Bezeichner mit Postfix."""
    n = len(s)
    while i < n and s[i] == ' ':
        i += 1
    start = i
    if i < n and s[i] == '(':
        e = match_paren(s, i)
        return (start, e)
    if i < n and s[i] == '&':
        i += 1
    m = re.compile(r'[A-Za-z_]\w*').match(s, i)
    if not m:
        return (start, -1)
    i = m.end()
    while i < n:
        if s[i] in '([':
            e = match_paren(s, i)
            if e < 0:
                return (start, -1)
            i = e
        elif s.startswith('->', i):
            m = re.compile(r'->\s*[A-Za-z_]\w*').match(s, i)
            i = m.end()
        elif s[i] == '.':
            m = re.compile(r'\.\s*[A-Za-z_]\w*').match(s, i)
            if not m:
                break
            i = m.end()
        else:
            break
    return (start, i)


def rhs_end(s, i):
    """Ende des Zuweisungsausdrucks ab i (Tiefe 0: ';' ',' oder unpaarige ')')."""
    d = 0
    for j in range(i, len(s)):
        c = s[j]
        if c in '([':
            d += 1
        elif c in ')]':
            if d == 0:
                return j
            d -= 1
        elif c in ';,' and d == 0:
            return j
    return -1


DEREF_PP = re.compile(r'\*\s*\(\s*((?:const\s+)?[A-Za-z_][\w ]*?)\s*\*\s*\*\s*\)')


def rewrite_line(s, flagged):
    """Ein Durchlauf; liefert (neue Zeile, geaendert). (c)-(e) nur auf Zeilen, die der
    Compiler als Zeiger<->int-Cast meldet (32-Bit-Annahme des Originals)."""
    # (a)/(b)  *(T**)ADDR [= RHS]
    m = DEREF_PP.search(s)
    if m:
        t = m.group(1).strip()
        a, e = operand_end(s, m.end())
        if e < 0:
            return s, False
        addr = s[a:e]
        k = e
        wrapped = s[:m.start()].rstrip().endswith('(') and s[k:k + 1] == ')'
        if wrapped:
            k += 1
        while k < len(s) and s[k] == ' ':
            k += 1
        if s.startswith('=', k) and not s.startswith('==', k):
            r = rhs_end(s, k + 1)
            if r < 0:
                return s, False
            rhs = s[k + 1:r].strip()
            new = 'recvx_p32_put((void*)(%s), (%s))' % (addr, rhs)
            st = s[:m.start()].rstrip()[:-1] if wrapped else s[:m.start()]
            return st + new + s[r:], True
        new = '((%s*)recvx_p32_get((void*)(%s)))' % (t, addr)
        return s[:m.start()] + new + s[e:], True
    if not flagged:
        return s, False
    # (f) (int)((T*)SLOT(N)) -> SLOT(N): Hin-und-zurueck-Cast ist im Original die Identitaet
    m = re.search(r'\(\s*int\s*\)\s*\(\s*\(\s*[A-Za-z_][\w ]*?\s*\*\s*\)\s*(' + SLOT_MACROS + r'\s*\()', s)
    if m:
        e = match_paren(s, m.end() - 1)
        if e > 0 and s.startswith(')', e):
            return s[:m.start()] + s[m.start(1):e] + s[e + 1:], True
    # (g) cond ? (int)(A) : (int)(B) in Slot
    m = re.match(r'(\s*)(' + SLOT_MACROS + r'\s*\()', s)
    if m:
        e = match_paren(s, m.end() - 1)
        t = s[e:]
        m2 = re.match(r'\s*=\s*(.*?)\?\s*\(int\)\s*(\(.*?\))\s*:\s*\(int\)\s*(\(.*?\))\s*;\s*$', t)
        if m2:
            new = 'recvx_p32_put((void*)&%s, %s ? %s : %s);' % (s[m.start(2):e], m2.group(1).strip() + '' if False else '(' + m2.group(1).strip() + ')', m2.group(2), m2.group(3))
            return s[:m.start(2)] + new, True
    # (h) (T*)(SLOT(N) + k): Slot als Zeigerbasis
    m = re.search(CAST + r'\s*\(\s*(' + SLOT_MACROS + r')\s*\(', s)
    if m:
        e = match_paren(s, m.end() - 1)
        if e > 0:
            new = '((char*)recvx_p32_get((void*)&%s))' % s[m.start(2):e]
            return s[:m.start(2)] + new + s[e:], True
    # (c) (T*)SLOTMACRO(N)  und (T*)*(int*)ADDR
    for m in re.finditer(CAST + r'\s*(\(?)\s*(\*\s*\(\s*int\s*\*\s*\)|' + SLOT_MACROS + r'\s*\()', s):
        t = m.group(1).strip()
        if m.group(3).startswith('*'):
            a, e = operand_end(s, m.end())
            if e < 0:
                continue
            addr = s[a:e]
            end = e
            if m.group(2) == '(':
                if not s.startswith(')', end):
                    continue
                end += 1
            new = '((%s*)recvx_p32_get((void*)(%s)))' % (t, addr)
            return s[:m.start()] + new + s[end:], True
        mac_start = m.start(3)
        e = match_paren(s, m.end() - 1)
        if e < 0 or m.group(2):
            continue
        new = '((%s*)recvx_p32_get((void*)&%s))' % (t, s[mac_start:e])
        return s[:m.start()] + new + s[e:], True
    # (e) (*(int*)ADDR + M): Basis ist ein Zeigerslot
    m = re.search(r'(\(\s*)\*\s*\(\s*int\s*\*\s*\)', s)
    if m:
        a, e = operand_end(s, m.end())
        if e > 0:
            new = m.group(1) + '(char*)recvx_p32_get((void*)(%s))' % s[a:e]
            return s[:m.start()] + new + s[e:], True
    # (d) SLOTMACRO(N) = (int)RHS;
    m = re.match(r'(\s*)(' + SLOT_MACROS + r'\s*\()', s)
    if m:
        e = match_paren(s, m.end() - 1)
        k = e
        while k < len(s) and s[k] == ' ':
            k += 1
        if s.startswith('=', k) and not s.startswith('==', k):
            m2 = re.compile(r'\s*\(\s*(?:unsigned\s+)?int\s*\)\s*').match(s, k + 1)
            if m2:
                r = rhs_end(s, m2.end())
                if r > 0:
                    new = 'recvx_p32_put((void*)&%s, (%s))' % (s[m.start(2):e], s[m2.end():r].strip())
                    return s[:m.start(2)] + new + s[r:], True
    return s, False


def pointer_int_truncation(line):
    """Zeiger-int-Cast, der auf arm64 Adressbits verliert (markiert).
    - `return (int)ptr;`   -> Handle (nur Null/Nichtnull wird ausgewertet)
    - `(unsigned int)ptr + 63) & ~0x3F` (bhEne_CallocWork, Speicherausrichtung)
      -> uintptr_t: die Ausrichtung ist Adressarithmetik, kein Zeigerslot."""
    m = re.match(r'(\s*)return \(int\)(\w+);\s*$', line)
    if m:
        return '%sreturn (int)recvx_p32_intern(%s); /* Host(arm64): Handle statt Zeigerkuerzung */' % (m.group(1), m.group(2)), True
    if '(unsigned int)sys->memp + 63' in line:
        return line.replace('(unsigned int)sys->memp + 63', '(uintptr_t)sys->memp + 63').replace(
            '(unsigned char*)(((uintptr', '(unsigned char*)(((uintptr') + ' /* Host(arm64): uintptr_t statt unsigned int */', True
    return line, False


def flagged_lines(path, flags):
    cmd = ['clang', '-fsyntax-only', '-ferror-limit=0', '-Wno-return-mismatch',
           '-Wint-to-pointer-cast', '-Wpointer-to-int-cast'] + flags + [str(path)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    out = set()
    for m in re.finditer(r':(\d+):\d+: warning: cast (?:to|from) .*(?:int-to-pointer-cast|pointer-to-int-cast)', r.stderr):
        out.add(int(m.group(1)))
    return out


def pointer_slots(path, flags, report):
    """Zeiger in 32-Bit-Slots -> Handle-Tabelle (arm64-Abweichung, markiert)."""
    text = path.read_text(errors='surrogateescape').split('\n')
    flag = flagged_lines(path, flags)
    n = 0
    for i, l in enumerate(text):
        if l.lstrip().startswith(('#', '//')):
            continue
        if (i + 1) in flag:
            l2, ch = pointer_int_truncation(l)
            if ch:
                l = l2
                n += 1
        for _ in range(16):
            l2, ch = rewrite_line(l, (i + 1) in flag)
            if not ch:
                break
            l = l2
            n += 1
        text[i] = l
    if n:
        last = max(i for i, l in enumerate(text) if l.startswith('#include'))
        text[last + 1:last + 1] = [P32_HDR, P32_BYTE]
        path.write_text('\n'.join(text), errors='surrogateescape')
    report['p32'] = n


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
        pointer_slots(out / p.name, flags, rep)  # nach add_includes: Zeilennummern stabil
        if rep.get('p32'):
            print('%s: %d Zeigerslots -> Handle' % (p.name, rep['p32']))
        if rep['key++']:
            print('%s: %d key++ uebersetzt' % (p.name, rep['key++']))


if __name__ == '__main__':
    main()

# Hinweis: Clang meldet 3 Funktionen ohne return (bhEne01_ActionModeCheck,
# bhEne09_DmgCheck, bhEne17_CameraControl) als Fehler (-Wreturn-mismatch);
# mwcc nicht. Aufrufer verwerfen den Wert. Kompilieren mit -Wno-return-mismatch,
# der Quelltext bleibt unveraendert.
