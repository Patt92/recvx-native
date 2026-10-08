#!/usr/bin/env python3
"""Disassembliert Funktionen aus dem Original-PS2-ELF (R5900, inkl. VU0-Makromodus).

Die Decomp-Quellen enthalten an den Hardwaregrenzen Inline-Assembler
(`asm volatile` in ps2_NinjaCnk.c, ps2_Vu1Strip.c, ps2_dummy.c ...). Diese
Bloecke sind von Hand rekonstruiert und damit nicht automatisch belastbar. Wer
sie nach C portiert, muss sie gegen das echte Programm pruefen; dieses Werkzeug
liefert die dafuer noetige Disassembly mit Adressen, Rohwort und Symbolnamen.

Beispiele:

    tools/disasm_ps2.py --elf <SLUS_201.84> Ps2AddPrim3DEx
    tools/disasm_ps2.py --elf <SLUS_201.84> 0x2cc1cc+0xf0
    tools/disasm_ps2.py --elf <SLUS_201.84> --cop2-only njCnkCvVnPs2

Symbolnamen und -groessen kommen aus der ELF-Symboltabelle (das Original ist
nicht gestrippt); `--symbols` ergaenzt eine splat-`symbol_addrs.txt`.
rabbitizer liefert die R5900-Dekodierung und wird, falls nicht importierbar,
aus dem angegebenen venv nachgeladen (`--rabbitizer`).
"""
import argparse
import os
import re
import struct
import sys

SYMLINE = re.compile(r'^\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;?(.*)$')
SYMSIZE = re.compile(r'size:\s*(0x[0-9A-Fa-f]+)')
# VU0-Makromodus und die uebrigen COP2-Transfers liegen im Opcode-Feld 0x12.
COP2 = 0x12


def load_rabbitizer(extra):
    """rabbitizer importieren, notfalls aus einem vorhandenen venv."""
    for path in extra:
        if path and path not in sys.path:
            sys.path.insert(0, path)
    try:
        import rabbitizer
    except ImportError:
        sys.exit('rabbitizer fehlt. Mit --rabbitizer <site-packages> den Pfad angeben, '
                 'z. B. aus einem venv mit "pip install rabbitizer".')
    return rabbitizer


class Elf:
    """Minimaler ELF32-LSB-Leser: vaddr -> Bytes und die Symboltabelle."""

    def __init__(self, path):
        self.data = open(path, 'rb').read()
        if self.data[:4] != b'\x7fELF' or self.data[4] != 1 or self.data[5] != 1:
            sys.exit('%s ist kein ELF32-LSB (erwartet: PS2-Executable).' % path)
        (self.entry, phoff, shoff, _flags, _ehsize, phentsize, phnum,
         shentsize, shnum, shstrndx) = struct.unpack_from('<IIIIHHHHHH', self.data, 24)
        self.segments = []
        for i in range(phnum):
            p_type, p_offset, p_vaddr, _p_paddr, p_filesz = struct.unpack_from(
                '<IIIII', self.data, phoff + i * phentsize)
            if p_type == 1:  # PT_LOAD
                self.segments.append((p_vaddr, p_offset, p_filesz))
        self.sections = []
        for i in range(shnum):
            name, s_type, _flags, addr, offset, size, link, _info, _align, entsize = \
                struct.unpack_from('<IIIIIIIIII', self.data, shoff + i * shentsize)
            self.sections.append(dict(name=name, type=s_type, addr=addr, offset=offset,
                                      size=size, link=link, entsize=entsize))
        shstr = self.sections[shstrndx]
        for s in self.sections:
            s['name'] = self._cstr(shstr['offset'] + s['name'])
        self.symbols = {}
        self.sizes = {}
        for s in self.sections:
            if s['type'] != 2:  # SHT_SYMTAB
                continue
            strtab = self.sections[s['link']]
            for off in range(s['offset'], s['offset'] + s['size'], s['entsize']):
                st_name, st_value, st_size, st_info, _other, _shndx = \
                    struct.unpack_from('<IIIBBH', self.data, off)
                if not st_name or not st_value:
                    continue
                name = self._cstr(strtab['offset'] + st_name)
                self.symbols.setdefault(name, st_value)
                if st_size:
                    self.sizes.setdefault(name, st_size)
                # STT_FUNC/STT_OBJECT fuer die Adressaufloesung sammeln.
                if (st_info & 0xF) in (1, 2):
                    self.sizes.setdefault(name, st_size)

    def _cstr(self, off):
        end = self.data.index(b'\0', off)
        return self.data[off:end].decode('utf-8', 'replace')

    def read(self, vaddr, size):
        for base, offset, filesz in self.segments:
            if base <= vaddr and vaddr + size <= base + filesz:
                start = offset + (vaddr - base)
                return self.data[start:start + size]
        return None


def load_symbol_file(path, elf):
    """splat-`symbol_addrs.txt` als Ergaenzung zur ELF-Symboltabelle."""
    for line in open(path, encoding='utf-8', errors='replace'):
        m = SYMLINE.match(line)
        if not m:
            continue
        name, addr, rest = m.group(1), int(m.group(2), 16), m.group(3)
        elf.symbols.setdefault(name, addr)
        size = SYMSIZE.search(rest)
        if size:
            elf.sizes.setdefault(name, int(size.group(1), 16))


def resolve(target, elf):
    """`Symbol`, `0xADDR`, `0xADDR+LEN` oder `0xADDR-0xENDE` -> (Name, Adresse, Laenge)."""
    m = re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)(?:([+-])(0x[0-9A-Fa-f]+|\d+))?', target)
    if m:
        addr = int(m.group(1), 0)
        if m.group(2) == '+':
            size = int(m.group(3), 0)
        elif m.group(2) == '-':
            size = int(m.group(3), 0) - addr
        else:
            size = 0x80
        return ('0x%08X' % addr, addr, size)
    if target not in elf.symbols:
        sys.exit('Symbol %s steht weder im ELF noch in --symbols.' % target)
    addr = elf.symbols[target]
    size = elf.sizes.get(target, 0)
    if not size:
        sys.exit('Fuer %s ist keine Groesse bekannt; Bereich als 0x%08X+LEN angeben.'
                 % (target, addr))
    return (target, addr, size)


def branch_target(at, instr):
    """Sprungziel. getBranchOffsetGeneric() enthaelt die +4 des Delay-Slots schon."""
    if not instr.isBranch():
        return None
    try:
        return at + instr.getBranchOffsetGeneric()
    except Exception:
        return None


def label_map(elf):
    """Adresse -> Symbolname, fuer Sprung- und Aufrufziele."""
    out = {}
    for name, addr in elf.symbols.items():
        out.setdefault(addr, name)
    return out


def disassemble(elf, rabbitizer, name, addr, size, cop2_only, branches):
    code = elf.read(addr, size)
    if code is None:
        sys.exit('0x%08X+0x%X liegt in keinem geladenen Segment.' % (addr, size))
    labels = label_map(elf)
    # Erst alle Worte dekodieren, dann interne Sprungziele als Label ausgeben.
    instrs = []
    targets = set()
    for i in range(0, len(code) - 3, 4):
        word = struct.unpack_from('<I', code, i)[0]
        at = addr + i
        instr = rabbitizer.Instruction(word, vram=at, category=rabbitizer.InstrCategory.R5900)
        instrs.append((at, word, instr))
        dest = branch_target(at, instr)
        if dest is not None and addr <= dest < addr + size:
            targets.add(dest)
    print('/* %s: 0x%08X .. 0x%08X (0x%X Bytes) */' % (name, addr, addr + size, size))
    for at, word, instr in instrs:
        opcode = word >> 26
        if cop2_only and opcode != COP2:
            continue
        if at in targets:
            print('.L%08X:' % at)
        text = instr.disassemble()
        note = ''
        if instr.isBranch():
            dest = branch_target(at, instr)
            if dest in targets:
                note = '  /* -> .L%08X */' % dest
            elif dest in labels:
                note = '  /* -> %s */' % labels[dest]
            elif dest is not None:
                note = '  /* -> 0x%08X */' % dest
        elif instr.isJumpWithAddress():
            dest = instr.getInstrIndexAsVram()
            if dest in labels:
                note = '  /* -> %s */' % labels[dest]
        if branches and not (instr.isBranch() or instr.isJumpWithAddress()):
            continue
        print('    /* %08X %08X */  %s%s' % (at, word, text, note))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--elf', required=True, help='Original-Executable (SLUS_201.84 / SLPM_650.22)')
    ap.add_argument('--symbols', action='append', default=[],
                    help='zusaetzliche splat-symbol_addrs.txt')
    ap.add_argument('--rabbitizer', action='append', default=[],
                    help='site-packages-Pfad, falls rabbitizer nicht im Interpreter liegt')
    ap.add_argument('--cop2-only', action='store_true',
                    help='nur COP2/VU0-Makromodus-Befehle zeigen')
    ap.add_argument('--branches-only', action='store_true',
                    help='nur Spruenge/Aufrufe zeigen (Kontrollfluss-Uebersicht)')
    ap.add_argument('target', nargs='+',
                    help='Symbolname oder 0xADDR[+LEN|-0xENDE]')
    args = ap.parse_args()

    default_venv = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                                'archived', 'ps2', 'venv')
    extra = list(args.rabbitizer)
    for py in ('3.14', '3.13', '3.12'):
        extra.append(os.path.join(default_venv, 'lib', 'python%s' % py, 'site-packages'))
    rabbitizer = load_rabbitizer(extra)

    elf = Elf(args.elf)
    for path in args.symbols:
        load_symbol_file(path, elf)
    for target in args.target:
        name, addr, size = resolve(target, elf)
        disassemble(elf, rabbitizer, name, addr, size, args.cop2_only, args.branches_only)
        print()


if __name__ == '__main__':
    main()
