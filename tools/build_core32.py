#!/usr/bin/env python3
"""Baut die Original-Engine als ILP32-Modul (wasm32) und uebersetzt es mit wasm2c zu C.

Warum: Die Originalquellen setzen 32-Bit-Zeiger voraus (bhSetRoom reloziert
RDT-Zeiger mit `(unsigned int)sys->rdtp + off`, Gegner legen Zeiger in 4-Byte-
Slots, BH_PWORK ist 0x580 Byte). Auf arm64/x64 verschieben 8-Byte-Zeiger alle
Felder. wasm32 ist ILP32 und Little Endian wie der EE; wasm2c macht daraus
gewoehnlichen C-Code, den jede Hostplattform nativ kompiliert. Es gibt keinen
Interpreter: das Ergebnis ist C mit einem linearen 32-Bit-Speicher.

Eingaben (unveraendert): die von tools/generate_enemy_sources.py erzeugten
Quellen, die nativen Core-Ports (native/core_*.c) und types.h in exakter
Originalanordnung (tools/generate_types.py --exact-layout). Die Umbenennungen
nj*/np*/bh* -> core_* werden aus CMakeLists.txt (recvx_original_engine)
uebernommen, damit beide Builds dieselbe Bindung haben.

Ausgabe: <out>/core32.c, <out>/core32.h (wasm2c) und <out>/imports.txt
(Funktionen, die der Host mit Originalsemantik bereitstellen muss).

Aufruf: build_core32.py <repo> <generated-dir> <out-dir>
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

root, gen, out = (Path(a).resolve() for a in sys.argv[1:4])
out.mkdir(parents=True, exist_ok=True)
obj = out / 'obj'
obj.mkdir(exist_ok=True)

cmake = (root / 'CMakeLists.txt').read_text()
m = re.search(r'target_compile_definitions\(recvx_original_engine PRIVATE(.*?)\)', cmake, re.S)
defines = re.findall(r'(\w+=\w+|RECVX_NATIVE=1)', m.group(1))
# bhCalcModel/bhCalcTree sind in MdlPut.c reines C; core_model.c ist nur deren
# 64-Bit-Fassung, ebenso njMemCopy4 (ps2_NaMem.c). Im ILP32-Modul gilt die
# Originalquelle selbst.
ORIGINAL_C = ('bhCalcModel', 'bhCalcTree', 'njMemCopy4')
defines = [d for d in defines if d.split('=')[0] not in ORIGINAL_C]

clang = shutil.which('clang')
wasm_ld = shutil.which('wasm-ld') or '/opt/homebrew/opt/lld/bin/wasm-ld'
wasm2c = shutil.which('wasm2c')
if not (clang and Path(wasm_ld).exists() and wasm2c):
    sys.exit('benoetigt clang (wasm32), wasm-ld (lld) und wasm2c (wabt)')

types32 = gen / 'types32' / 'types.h'
subprocess.run([sys.executable, str(root / 'tools/generate_types.py'), '--exact-layout', '--no-check',
                '--out', str(types32)], check=True, stdout=subprocess.DEVNULL)

sources = sorted((gen / 'enemy').glob('*.c'))
sources += [root / 'native' / n for n in (
    'main_globals.c', 'core_njplus_collision.c', 'core_njplus_memory.c', 'core_njplus_modelstate.c',
    'core_njplus_skin.c', 'core_model.c', 'core_effect.c', 'core_object.c', 'core32_bridge.c')]
# Ninja-Mathematik/Matrixstapel: eigene Header (native/ninja_matrix.h), ohne types.h.
plain = [root / 'src/ps2/veronica/prog/ps2_NaMath.c', root / 'native/ninja_matrix.c',
         root / 'native/core_ninja_ex.c', root / 'native/core32_libc.c']

flags = ['--target=wasm32-unknown-unknown', '-ffreestanding', '-O2', '-w', '-Wno-return-mismatch',
         '-fno-builtin', '-ffp-contract=off', '-isystem', str(root / 'tools/wasm32/include'),
         '-include', str(types32), '-I' + str(root / 'build-tools/ninja'), '-I' + str(root / 'native'),
         '-I' + str(root / 'include/ps2/veronica/prog'), '-I' + str(gen), '-I' + str(root / 'src/ps2/veronica/prog')]
flags += ['-D' + d for d in defines]

plain_flags = ['--target=wasm32-unknown-unknown', '-ffreestanding', '-O2', '-w', '-fno-builtin', '-ffp-contract=off',
               '-isystem', str(root / 'tools/wasm32/include'), '-I' + str(root / 'native'),
               '-I' + str(root / 'build-tools/ninja'), '-I' + str(root / 'include/ps2/veronica/prog'), '-DRECVX_NATIVE=1']
objects = []
for src in sources + plain:
    o = obj / (src.stem + '.o')
    r = subprocess.run([clang] + (plain_flags if src in plain else flags) + ['-c', str(src), '-o', str(o)],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit('%s:\n%s' % (src, r.stderr[-3000:]))
    objects.append(str(o))

wasm = out / 'core32.wasm'
r = subprocess.run([wasm_ld, '--no-entry', '--export-all', '--allow-undefined', '--import-memory',
                    '--initial-memory=268435456', '--max-memory=268435456', '-z', 'stack-size=1048576', '-o', str(wasm)] + objects,
                   capture_output=True, text=True)
if r.returncode:
    sys.exit(r.stderr[-3000:])

subprocess.run([wasm2c, str(wasm), '-n', 'core32', '-o', str(out / 'core32.c')], check=True)

# Indirekte Aufrufe: Das Original ruft K&R-deklarierte Funktionen (`void f()`)
# ueber typisierte Zeigertabellen auf (z. B. bhEne01_MoveMode2B -> bhEne01_Dummy).
# Auf dem EE und in nativem C ist das wohldefiniert genug: ueberzaehlige
# Argumente bleiben unbenutzt. WebAssembly wuerde den Typ pruefen und abbrechen.
# Geprueft bleiben Tabellengrenze und Null-Eintrag; der Aufruf selbst folgt der
# C-Aufrufkonvention des Hosts wie der MIPS-Konvention des Originals.
code = (out / 'core32.c').read_text()
old = '''#define CHECK_CALL_INDIRECT(table, ft, x)                       \\
  (LIKELY((x) < table.size && ft == table.data[x].func_type) || \\
   func_types_eq_slowpath(&(table), ft, x))'''
new = '''#define CHECK_CALL_INDIRECT(table, ft, x)                       \\
  (LIKELY((x) < table.size && table.data[x].func) ||           \\
   (TRAP(CALL_INDIRECT), 0))'''
if old not in code:
    sys.exit('wasm2c: CHECK_CALL_INDIRECT nicht gefunden (wabt-Version?)')
(out / 'core32.c').write_text(code.replace(old, new))

header = (out / 'core32.h').read_text()
decls = {}
for m in re.finditer(r'^(void|u32|u64|f32|f64) w2c_env_(\w+)\(struct w2c_env\*((?:, (?:u32|u64|f32|f64))*)\);$', header, re.M):
    decls[m.group(2)] = (m.group(1), [t.strip() for t in m.group(3).split(',') if t.strip()])
imports = sorted(decls)
(out / 'imports.txt').write_text('\n'.join(imports) + '\n')

# Importe, die native/core32_host.c (noch) nicht bindet, melden sich als
# Portierungsluecke beim Host (recvx_core32_gap) und liefern 0. Nichts wird
# stillschweigend ersetzt; die Liste der Luecken ist zur Laufzeit abfragbar.
host = (root / 'native/core32_host.c').read_text()
bound = set(re.findall(r'^\w+\s+w2c_env_(\w+)\s*\(', host, re.M))
lines = ['/* Generiert von tools/build_core32.py: ungebundene Importe des ILP32-Moduls. */',
         '#include "core32.h"', 'void recvx_core32_gap(const char* name);']
for name in imports:
    if name in bound:
        continue
    ret, args = decls[name]
    params = ''.join(', %s a%d' % (t, i) for i, t in enumerate(args))
    body = 'recvx_core32_gap("%s");' % name + ('' if ret == 'void' else ' return 0;')
    lines.append('%s w2c_env_%s(struct w2c_env* env%s) { (void)env; %s }' % (ret, name, params, body))
(out / 'core32_gaps.c').write_text('\n'.join(lines) + '\n')
print('gebunden: %d, Luecken: %d' % (len(bound & set(imports)), len(set(imports) - bound)))
print('%d Quellen, %d Importe -> %s' % (len(sources), len(imports), out))
