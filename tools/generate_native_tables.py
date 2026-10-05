#!/usr/bin/env python3
"""Extract original immutable UI tables and combination routines for the host build."""
import argparse
import re
from pathlib import Path


def block(text, start):
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return end


def main():
    p = argparse.ArgumentParser()
    p.add_argument('root', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    sub = (a.root / 'src/ps2/veronica/prog/sub1.c').read_text()
    ui = ['// Generated from original sub1.c; edit the original tables, not this file.\n']
    for m in re.finditer(r'^(?:NJS_TEXANIM|PARTS) parts_\d\d(?:b)?\[\d+\]\s*=\s*\{', sub, re.M):
        ui.append(sub[m.start():block(sub, m.start())] + ';\n')
    message = (a.root / 'src/ps2/veronica/prog/message.c').read_text()
    m = re.search(r'const unsigned char FontSz\[126\]\s*=\s*\{', message)
    ui.append(message[m.start():block(message, m.start())] + ';\n')
    # Keep the original pulse timing and fade state machine in the host build.
    header = (a.root / 'include/ps2/veronica/prog/sub1.h').read_text()
    start = header.index('typedef struct PULSE_WORK')
    end = header.index('} PULSE_WORK;', start) + len('} PULSE_WORK;')
    ui.append('namespace pulse {\nstruct NJS_POINT2 {float x,y;};\nunion NJS_COLOR {std::uint32_t color;struct {std::int16_t u,v;} tex;};\n' + header[start:end] + '\nstruct S_WORK {unsigned hp;};\nstatic thread_local S_WORK swork;\nstatic thread_local PULSE_WORK pulsework;\nvoid PulseFadeAnim();\n')
    for name in ('PulseInit', 'PulseAnim', 'PulseFadeAnim'):
        m = re.search(r'^void ' + name + r'\(\)\s*\n\{', sub, re.M)
        ui.append(sub[m.start():block(sub, m.start())] + '\n')
    ui.append('}\n')
    (a.output / 'ui_tables.inc').write_text(''.join(ui))
    routines = ['// Generated from original Combi_00 through Combi_05 in sub1.c.\n']
    for i in range(6):
        m = re.search(r'^unsigned char Combi_0' + str(i) + r'\([^\n]+\)\s*(?://[^\n]*)?\n\{', sub, re.M)
        routine = sub[m.start():block(sub, m.start())]
        routine = re.sub(r'(itemid\d|bullet2) << 16', r'std::uint32_t(\1) << 16', routine)
        routines.append(routine + '\n')
    (a.output / 'combinations.inc').write_text(''.join(routines))

if __name__ == '__main__':
    main()
