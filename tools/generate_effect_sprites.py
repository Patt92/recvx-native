#!/usr/bin/env python3
"""Extract original atlas sprite procedures without rewriting their state logic,
plus the player-linked lighter data (player.c WpnEffTab[1][0], light.c lgttab[1])."""
import re
import sys
from pathlib import Path
root, output = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'
parts = ['// Generated from effsub1.c/effsub5.c, player.c and light.c; do not edit.\n']


def function(source, name):
    start = source.index('void ' + name + '(')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end].replace('rand()', 'effect_random()')


for file, names in (('effsub1.c', ('bhEff005', 'bhEff009', 'bhEff016', 'bhEff023', 'bhEff024', 'bhEff025', 'bhDraw024', 'bhDraw025', 'bhEff122')), ('effsub5.c', ('bhEff200', 'bhEff201', 'bhEff218', 'bhEff230'))):
    source = (prog / file).read_text()
    parts += [function(source, name) for name in names]

# weapon.c bhSetExplosion: barrel/grenade explosion (sound, flash light, effects 16/23/24).
parts.append(function((prog / 'weapon.c').read_text(), 'bhSetExplosion'))
parts.append(function((prog / 'weapon.c').read_text(), 'bhSetExplosionEffect'))

# bhControlPlayer: bhSetEffectTb(WpnEffTab[1], &WpnTab[1].wp_fps1, plp, 9) for the lighter.
player = (prog / 'player.c').read_text()
table = player[player.index('EF_WORK WpnEffTab[23][4]'):]
rows = re.findall(r'\{\s*(0x[0-9A-Fa-f]+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(\d+),\s*([-\d.]+)f,\s*([-\d.]+)f,\s*([-\d.]+)f,'
                  r'\s*([-\d.]+)f,\s*([-\d.]+)f,\s*([-\d.]+)f,\s*(-?\d+),\s*(-?\d+)\s*\}', table)
lighter = rows[4]  # WpnEffTab[1][0]
parts.append('const EF_WORK lighter_effect={%s,%s,%s,%s,%s,%sf,%sf,%sf,%sf,%sf,%sf,%s,%s};' % lighter)

# bhSetLightTab(&lgttab[1], 1): the lighter's room light 1.
light = (prog / 'light.c').read_text()
block = light[light.index('// lgttab[1]'):light.index('// lgttab[2]')]
values = re.findall(r'(-?0x[0-9A-Fa-f]+|-?[\d.]+)f?,\s*//', block)
if len(values) != 29:
    raise SystemExit('lgttab[1] layout changed')
parts.append('const LGT_WK lighter_light={' + ','.join(v + ('f' if '.' in v else '') for v in values) + '};')
effect = (prog / 'effect.c').read_text()
info = re.search(r'const EFFECT_INFO ef_info\[\d+\] = \{(.*?)\};', effect, re.S).group(1)
entries = re.findall(r'\{\s*(-?\d+),\s*(\d+)\s*\}', info)
parts.append('const int effect_info_flags[]={' + ','.join(f for f, _ in entries) + '};')
parts.append('const int effect_info_anims[]={' + ','.join(n for _, n in entries) + '};')
output.mkdir(parents=True, exist_ok=True)
(output / 'effect_sprites.inc').write_text('\n'.join(parts) + '\n')
