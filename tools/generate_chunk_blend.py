#!/usr/bin/env python3
"""Extract Ninja chunk modes and the PS2 ALPHA register table, without remapping."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
chunk = (root / 'src/ps2/veronica/prog/ps2_NinjaCnk.c').read_text()
system = (root / 'src/ps2/veronica/prog/ps2_NaSystem.c').read_text()
lines = ['// Generated from ps2_NinjaCnk.c / njColorBlendingModeSys; do not edit.']
for name in ['lNaCnkSrcAlphaMode', 'lNaCnkDstAlphaMode']:
    match = re.search(r'int\s+' + name + r'\[8\]\s*=\s*\{([^}]+)\}', chunk)
    values = re.findall(r'\d+', match.group(1))
    if len(values) != 8:
        raise SystemExit(name + ': unexpected table size')
    lines.append('static constexpr unsigned ' + name + '[8]={' + ','.join(values) + '};')
match = re.search(r'alpha_tbl\[13\]\[12\]\s*=\s*\{(.*?)\};', system, re.S)
values = re.findall(r'0x[0-9A-Fa-f]+', match.group(1))
if len(values) != 13 * 12:
    raise SystemExit('alpha_tbl: unexpected table size')
lines.append('static constexpr std::uint64_t alpha_tbl[13][12]={')
for start in range(0, len(values), 12):
    lines.append('{' + ','.join(value + 'ull' for value in values[start:start+12]) + '},')
lines.append('};')
output.mkdir(parents=True, exist_ok=True)
(output / 'chunk_blend.inc').write_text('\n'.join(lines) + '\n')
