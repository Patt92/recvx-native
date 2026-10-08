#!/usr/bin/env python3
"""Prueft die handgeschriebenen Hardwareteile der Decomp gegen das Original-ELF.

An den Hardwaregrenzen steht in den Decomp-Quellen Inline-Assembler
(`asm volatile`, u. a. ps2_NinjaCnk.c, ps2_Vu1Strip.c, ps2_dummy.c) und daneben
liegen die VU-Mikroprogramme als `.vsm`. Beides ist von Hand entstanden und
damit nicht automatisch belastbar. Wer diese Teile nach C portiert, braucht
vorher die Antwort auf zwei Fragen:

  --vsm     Entspricht der Mikrocode-Dump byteweise dem ELF?
            (Die `.vsm` fuehren je Instruktion das Rohwort mit.)
  --inline  Entspricht die Befehlsfolge eines Inline-Blocks dem ELF?
            Verglichen werden die COP2-Befehle (VU0-Makromodus, lqc2/sqc2)
            mit Feldsuffix und VU-Registeroperanden; EE-Register und
            Platzhalter wie %0 lassen sich nicht vergleichen und bleiben aussen
            vor. Abweichungen werden als Diff gemeldet, nicht als Urteil.

Beispiele:

    tools/check_asm_blocks.py --elf <SLUS_201.84> --vsm archived/ps2/vsm/ps2_vu0.vsm
    tools/check_asm_blocks.py --elf <SLUS_201.84> --symbols <symbol_addrs.txt> \\
        --inline src/ps2/veronica/prog/ps2_NinjaCnk.c
"""
import argparse
import difflib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from disasm_ps2 import Elf, load_rabbitizer, load_symbol_file  # noqa: E402

# Zeilen der .vsm: /* OFFSET ROHWORT */ Mnemonik.  "----" und reine
# Kommentarzeilen (z. B. die aufgeloesten DMA-/VIF-Tags) tragen kein Rohwort.
VSM_LINE = re.compile(r'/\*\s*([0-9A-Fa-f]{4})\s+([0-9A-Fa-f]{8,16})\s*\*/')
VSM_LABEL = re.compile(r'^\s*(?:\.global\s+(\w+)|(\w+):)')
# COP2: VU0-Makromodus (0x12) plus die Quadword-Transfers lqc2/sqc2.
COP2_OPCODES = (0x12, 0x36, 0x3E)
VU_OPERAND = re.compile(r'^\$?(vf\d+[xyzw]*|vi\d+|ACC|Q|I|P)$', re.IGNORECASE)
C_FUNC = re.compile(r'^([A-Za-z_][\w \*]*?[ \*])([A-Za-z_]\w*)\s*\([^;]*$')
ASM_START = re.compile(r'\basm\s+volatile\b')
# Inline-Befehle: Mnemonik am Zeilenanfang, Operanden bis Zeilenende/Kommentar.
C_INSTR = re.compile(r'^\s*([a-z][a-z0-9_]*(?:\.[a-z0-9]+)?)\s*(.*?)\s*$', re.IGNORECASE)
C_SKIP = {'set', 'word', 'global', 'data', 'text', 'align', 'end'}
# Dieselbe Auswahl wie COP2_OPCODES, nur ueber die Mnemonik: VU0-Makromodus
# (v...), die Quadword-Transfers und die Steuerregister-Transfers.
COP2_MNEMONIC = re.compile(r'^(v[a-z0-9]+|lqc2|sqc2|qmfc2|qmtc2|cfc2|ctc2)(\.[a-z0-9]+)?$',
                           re.IGNORECASE)


def vsm_words(path):
    """[(Offset, Rohbytes, Label, Mnemonik)] aus einer .vsm-Datei."""
    out = []
    label = ''
    for line in open(path, encoding='utf-8', errors='replace'):
        m = VSM_LABEL.match(line)
        if m and (m.group(2) or ''):
            label = m.group(2)
        m = VSM_LINE.search(line)
        if not m:
            continue
        raw = m.group(2)
        if len(raw) % 2:
            continue
        text = line[m.end():].strip()
        out.append((int(m.group(1), 16), bytes.fromhex(raw), label, text))
    return out


def check_vsm(elf, path, symbol):
    """Vergleicht die Rohworte der .vsm byteweise mit dem ELF-Inhalt."""
    if symbol not in elf.symbols:
        sys.exit('Symbol %s fehlt im ELF; mit --at den Namen angeben.' % symbol)
    base = elf.symbols[symbol]
    words = vsm_words(path)
    if not words:
        sys.exit('%s enthaelt keine Rohworte.' % path)
    bad = []
    for offset, raw, label, text in words:
        have = elf.read(base + offset, len(raw))
        if have != raw:
            bad.append((offset, raw, have, label, text))
    print('%s gegen %s (0x%08X): %d Rohworte, %d Abweichungen'
          % (os.path.basename(path), symbol, base, len(words), len(bad)))
    for offset, raw, have, label, text in bad[:40]:
        print('  +0x%04X  %s: vsm %s  elf %s  (%s)'
              % (offset, label, raw.hex().upper(),
                 have.hex().upper() if have else '<nicht geladen>', text))
    if len(bad) > 40:
        print('  ... %d weitere' % (len(bad) - 40))
    return not bad


def c_functions(path):
    """[(Name, Startzeile, Endzeile)] der Funktionen einer Decomp-Quelle."""
    lines = open(path, encoding='utf-8', errors='surrogateescape').read().split('\n')
    out = []
    for i, line in enumerate(lines):
        m = C_FUNC.match(line)
        if not m or line.lstrip().startswith(('return', 'typedef', 'static const')):
            continue
        # Funktionskoerper: bis zur schliessenden Klammer in Spalte 0.
        for j in range(i + 1, len(lines)):
            if lines[j].startswith('}'):
                out.append((m.group(2), i + 1, j + 1))
                break
    return lines, out


def inline_signature(text):
    """Mnemonik + VU-Operanden eines Inline-Befehls, oder None."""
    m = C_INSTR.match(text)
    if not m:
        return None
    mnem = m.group(1).lower()
    if mnem.split('.')[0] in C_SKIP or mnem.startswith('.'):
        return None
    if not COP2_MNEMONIC.match(mnem) or text.rstrip().endswith(':'):
        return None
    ops = [o.strip() for o in m.group(2).split(',') if o.strip()]
    return normalise(mnem, ops)


def normalise(mnem, ops):
    """(Basismnemonik, Zielfeld oder None, VU-Registeroperanden).

    Getrennt, weil die Decomp zwei Schreibweisen mischt: Felder stehen teils am
    Operanden (`vf5xyz`, `vf4x`), teils als Mnemoniksuffix (`vsub.xyz`), teils
    fehlen sie (`vmulx` statt `vmulx.xyzw`). Registerfolge und Befehlsfolge sind
    damit vergleichbar; die Feldangaben werden anschliessend einzeln verglichen
    und nur gemeldet, wenn beide Seiten ein Feld nennen und es abweicht.
    Die Quellfelder von vdiv/vrsqrt/vsqrt nennt die Decomp nicht; sie stehen in
    der Disassembly (tools/disasm_ps2.py) und bleiben hier unverglichen.
    cfc2/ctc2/qmfc2/qmtc2 tragen teils die Pipelinesuffixe .i/.ni; sie sind
    fuer die Semantik ohne Belang."""
    mnem = mnem.lower()
    for base in ('cfc2', 'ctc2', 'qmfc2', 'qmtc2'):
        if mnem.startswith(base + '.'):
            mnem = base
    parts = mnem.split('.', 1)
    base, field = parts[0], (parts[1] if len(parts) > 1 else None)
    kept = []
    for op in ops:
        op = op.split('/*')[0].split('#')[0].strip().rstrip(',')
        op = re.sub(r'^\(|\)$', '', op)
        if VU_OPERAND.match(op):
            op = op.lower().lstrip('$')
            # Feldliste am Register entfernen: vf5xyz/vf4x -> vf5/vf4.
            op = re.sub(r'^(vf\d+)[xyzw]+$', r'\1', op)
            kept.append(op)
    return (base, field, tuple(kept))


def structure(sig):
    """Vergleichsschluessel ohne Feldangabe."""
    return (sig[0], sig[2])


def c_blocks(lines, start, end):
    """VU-Signaturen aller Inline-Bloecke einer Funktion, in Quelltextreihenfolge."""
    out = []
    i = start - 1
    while i < end:
        if ASM_START.search(lines[i]):
            depth = 0
            quoted = False
            j = i
            while j < end:
                for ch in lines[j]:
                    if ch == '"':
                        quoted = not quoted
                    elif ch in '({' and not quoted:
                        depth += 1
                    elif ch in ')}' and not quoted:
                        depth -= 1
                sig = inline_signature(lines[j])
                if sig and sig[0] not in ('asm',):
                    out.append((j + 1, sig))
                if depth == 0 and j > i:
                    break
                j += 1
            i = j
        i += 1
    return out


def elf_cop2(elf, rabbitizer, addr, size):
    """COP2-Befehle einer Funktion als Signaturen, in Programmreihenfolge."""
    code = elf.read(addr, size)
    if code is None:
        return None
    out = []
    for i in range(0, len(code) - 3, 4):
        word = struct.unpack_from('<I', code, i)[0]
        if (word >> 26) not in COP2_OPCODES:
            continue
        instr = rabbitizer.Instruction(word, vram=addr + i,
                                       category=rabbitizer.InstrCategory.R5900)
        text = instr.disassemble().strip()
        # Nicht dekodierte Worte gibt rabbitizer als ".word 0x... # mnemonik" aus.
        if text.startswith('.word'):
            comment = text.split('#', 1)
            text = comment[1].strip() if len(comment) > 1 else text
            text = text.split('#')[0].strip()
        parts = text.split(None, 1)
        ops = [o.strip() for o in parts[1].split(',')] if len(parts) > 1 else []
        out.append((addr + i, normalise(parts[0], ops)))
    return out


def check_inline(elf, rabbitizer, path):
    """Vergleicht jede Funktion mit Inline-Assembler gegen das ELF."""
    lines, funcs = c_functions(path)
    clean = True
    checked = 0
    for name, start, end in funcs:
        got = c_blocks(lines, start, end)
        if not got:
            continue
        if name not in elf.symbols or not elf.sizes.get(name):
            print('%s: %s - kein Symbol/keine Groesse im ELF, nicht geprueft'
                  % (os.path.basename(path), name))
            clean = False
            continue
        have = elf_cop2(elf, rabbitizer, elf.symbols[name], elf.sizes[name])
        if have is None:
            print('%s: %s - Adresse nicht im geladenen Segment' % (os.path.basename(path), name))
            clean = False
            continue
        checked += 1
        a = [sig for _, sig in got]
        b = [sig for _, sig in have]
        sa = [structure(s) for s in a]
        sb = [structure(s) for s in b]
        if sa != sb:
            clean = False
            print('%s: %s - ABWEICHUNG (Quelle %d, ELF %d COP2-Befehle)'
                  % (os.path.basename(path), name, len(a), len(b)))
            sm = difflib.SequenceMatcher(a=sa, b=sb, autojunk=False)
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag == 'equal':
                    continue
                for k in range(i1, i2):
                    print('    Zeile %-5d nur in Quelle: %s' % (got[k][0], fmt(a[k])))
                for k in range(j1, j2):
                    print('    0x%08X nur im ELF  : %s' % (have[k][0], fmt(b[k])))
            continue
        # Struktur gleich: jetzt die Felder vergleichen.
        conflicts = [(got[k][0], have[k][0], a[k], b[k]) for k in range(len(a))
                     if a[k][1] and b[k][1] and a[k][1] != b[k][1]]
        unstated = sum(1 for s in a if not s[1])
        if conflicts:
            clean = False
            print('%s: %s - FELDABWEICHUNG in %d von %d COP2-Befehlen'
                  % (os.path.basename(path), name, len(conflicts), len(a)))
            for cline, caddr, csrc, celf in conflicts:
                print('    Zeile %-5d %s  <->  0x%08X %s' % (cline, fmt(csrc), caddr, fmt(celf)))
            continue
        note = '' if not unstated else ', %d ohne Feldangabe in der Quelle' % unstated
        print('%s: %s - %d COP2-Befehle identisch%s'
              % (os.path.basename(path), name, len(a), note))
    return clean, checked


def fmt(sig):
    return '%s%s %s' % (sig[0], '.' + sig[1] if sig[1] else '', ', '.join(sig[2]))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--elf', required=True, help='Original-Executable (SLUS_201.84 / SLPM_650.22)')
    ap.add_argument('--symbols', action='append', default=[], help='splat-symbol_addrs.txt')
    ap.add_argument('--rabbitizer', action='append', default=[])
    ap.add_argument('--vsm', action='append', default=[], help='VU-Mikrocode-Dump (.vsm)')
    ap.add_argument('--at', help='Symbol des .vsm-Pakets (Standard: Dateiname ohne Endung)')
    ap.add_argument('--inline', action='append', default=[], help='Decomp-Quelle mit asm volatile')
    args = ap.parse_args()

    if not args.vsm and not args.inline:
        ap.error('mindestens --vsm oder --inline angeben')
    if args.at and len(args.vsm) != 1:
        ap.error('--at gilt fuer genau eine --vsm-Datei; ohne --at wird der Name '
                 'aus dem Dateinamen gebildet (ps2_vu1.vsm -> ps2_vu1sub0)')

    default_venv = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                                'archived', 'ps2', 'venv')
    extra = list(args.rabbitizer)
    for py in ('3.14', '3.13', '3.12'):
        extra.append(os.path.join(default_venv, 'lib', 'python%s' % py, 'site-packages'))
    rabbitizer = load_rabbitizer(extra)

    elf = Elf(args.elf)
    for p in args.symbols:
        load_symbol_file(p, elf)

    ok = True
    for path in args.vsm:
        symbol = args.at or os.path.splitext(os.path.basename(path))[0] + 'sub0'
        ok = check_vsm(elf, path, symbol) and ok
    total = 0
    for path in args.inline:
        clean, checked = check_inline(elf, rabbitizer, path)
        ok = clean and ok
        total += checked
    if args.inline:
        print('%d Funktionen mit Inline-Assembler geprueft.' % total)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
