#!/usr/bin/env python3
"""Reuse the original stair/ladder player actions (player.c bhCPM2_act_kdu/kdd/hsu/hsd),
their tables and EXP_WORK verbatim for the native C bridge (native/player_stairs.c)."""
import re
import sys
from pathlib import Path
from generate_native_tables import block

root, output = map(Path, sys.argv[1:])
prog = root / 'src/ps2/veronica/prog'
player = (prog / 'player.c').read_text()
header = (root / 'include/ps2/veronica/prog/player.h').read_text()
parts = ['// Generated from player.c/player.h; do not edit.\n']
start = header.index('typedef struct EXP_WORK')
parts.append(header[start:header.index('} EXP_WORK;', start)] + '} EXP_WORK;\n')
# The same original tables are also compiled from the complete player.c into
# recvx_original_engine. This extract keeps the verbatim definitions but gives
# them internal linkage, like the extracted bhCPM2_act_* functions below, so
# both translation units can coexist in one link without a second copy
# claiming external linkage.
for table in ('PlMtnAct', 'PlFootSnd', 'PlKDU'):
    m = re.search(r'const [a-z ]+ ' + table + r'(\[\d+\])+ =\s*\{', player)
    parts.append('static ' + player[m.start():block(player, m.start())] + ';\n')
for name in ('bhCPM2_act_kdu', 'bhCPM2_act_kdd', 'bhCPM2_act_hsu', 'bhCPM2_act_hsd', 'bhCPM2_act_dnu', 'bhCPM2_act_dnd'):
    m = re.search(r'void ' + name + r'\(\)\s*\{', player)
    parts.append('static ' + player[m.start():block(player, m.start())] + '\n')
output.mkdir(parents=True, exist_ok=True)
(output / 'player_stairs.inc').write_text('\n'.join(parts))
