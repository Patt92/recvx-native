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


def status_pulse(root, sub):
    """Original StatusMain condition monitor: PulseMain and its animation/draw routines.

    njDrawTextureH is captured as quads for the host renderer. DrawPoly2D only
    paints the black panel behind the strips, which the host sprite layer owns.
    """
    header = (root / 'include/ps2/veronica/prog/sub1.h').read_text()
    start = header.index('typedef struct PULSE_WORK')
    end = header.index('} PULSE_WORK;', start) + len('} PULSE_WORK;')
    m = re.search(r'^float cen_pos\[12\]\[6\]\s*=\s*\{', sub, re.M)
    out = ['// Generated from original sub1.c (PulseInit..Pulse00); do not edit.\n',
           'namespace pulse {\nstruct NJS_POINT2 {float x,y;};\nunion NJS_COLOR {std::uint32_t color;struct {std::int16_t u,v;} tex;};\n',
           header[start:end] + '\n',
           'struct S_WORK {unsigned hp;unsigned statusflg;};\n',
           'struct NJS_TEXTUREH_VTX {float x,y,z,u,v;std::uint32_t bcol,ocol;};\n',
           'struct PulsePart {int anim;};\n',
           'static thread_local S_WORK swork;\nstatic thread_local PULSE_WORK pulsework;\nstatic thread_local PulsePart parts_10b[11];\n',
           'static thread_local std::vector<std::array<NJS_TEXTUREH_VTX,4>>* captured=nullptr;\n',
           'static void njDrawTextureH(NJS_TEXTUREH_VTX* polygon,int count,int,int){if(captured&&count==4)captured->push_back({polygon[0],polygon[1],polygon[2],polygon[3]});}\n',
           'static void DrawPoly2D(NJS_POINT2*,NJS_COLOR*,NJS_COLOR*,float,unsigned,int){}\n',
           'static thread_local ' + sub[m.start():block(sub, m.start())] + ';\n',
           'void PulseInit();void PulseMain();void PulseAnim();void PulseFadeAnim();\n',
           'void PulseHealAnim(NJS_POINT2* pos,NJS_COLOR* texuv);void PulsePoisonHealAnim(NJS_POINT2* pos,NJS_COLOR* texuv);void Pulse00(NJS_POINT2* pos,NJS_COLOR* texuv);\n']
    for name in ('PulseInit', 'PulseMain', 'PulseAnim', 'PulseFadeAnim', 'PulseHealAnim', 'PulsePoisonHealAnim', 'Pulse00'):
        m = re.search(r'^void ' + name + r'\([^)]*\)[^\n]*\n\{', sub, re.M)
        routine = sub[m.start():block(sub, m.start())]
        routine = routine.replace('PulseAnim(st);', 'PulseAnim();')  # K&R-style call in PulseMain
        out.append(routine + '\n')
    out.append('}\n')
    return ''.join(out)


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
    m = re.search(r'^float cen_pos99\[12\]\[6\]\s*=\s*\{', sub, re.M)
    ui.append(sub[m.start():block(sub, m.start())] + ';\n')
    message = (a.root / 'src/ps2/veronica/prog/message.c').read_text()
    m = re.search(r'const unsigned char FontSz\[126\]\s*=\s*\{', message)
    ui.append(message[m.start():block(message, m.start())] + ';\n')
    (a.output / 'status_pulse.inc').write_text(status_pulse(a.root, sub))
    adx = (a.root / 'src/ps2/veronica/prog/adxwrap.c').read_text()
    m = re.search(r'^short AdxVolTbl\[128\]\s*=\s*\{', adx, re.M)
    (a.output / 'adx_tables.inc').write_text('// Generated from original adxwrap.c.\n' + adx[m.start():block(adx, m.start())] + ';\n')
    m = re.search(r'^PARTS\* BulletSet\([^\n]+\)[^\n]*\n\{', sub, re.M)
    ui.append(sub[m.start():block(sub, m.start())] + '\n')
    # Original ItemCommand's type-dependent sprite initialization and TrigerSet.
    start=sub.index('void TrigerSet(S_WORK* st)')
    ui.append(sub[start:block(sub,start)]+'\n')
    start=sub.index('void ItemCommand(S_WORK* st)')
    command=sub[start:block(sub,start)]
    init=command[command.index('        for (x = 0; x < 18; x++)'):command.index('        swork.statusflg &= ~0x100000;')]
    selection=command[command.index('    for (num1 = 0; num1 < (max + 1); num1++)'):command.rfind('}')]
    ui.append('std::array<PARTS,18> original_command_parts(unsigned item,unsigned cursor,bool burst){\n'
              'static const std::array<PARTS,24> initial=[](){std::array<PARTS,24> r;std::copy_n(parts_06b,24,r.begin());return r;}();\n'
              'std::copy(initial.begin(),initial.end(),parts_06b);unsigned flags[3]{};if(burst)flags[2]=0x80000000u>>(74%32);UiSystem system{flags};sys=&system;\n'
              'S_WORK work{item,cursor};S_WORK* st=&work;unsigned char x;unsigned short num1;\n'+init+
              'st->maincsr=cursor;const unsigned max=(itemdata[item].type&1)?(item==10?4:3):2;\n'+selection+
              'std::array<PARTS,18> result;std::copy_n(parts_06b,18,result.begin());sys=nullptr;return result;}\n')
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
