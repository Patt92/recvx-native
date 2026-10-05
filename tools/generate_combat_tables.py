#!/usr/bin/env python3
"""Extract original weapon, damage and zombie health constants for native combat."""
import sys,re
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
result=['// Generated from original player.c, weapon.c and en01.c.\n']
for file,names in [('player.c',['WpnTab','PlMtnWpn','KnfAtrTab']),('weapon.c',['EneDamNear','EneDamMid','EneDamFar']),('en01.c',['en01_hp_tbl'])]:
 text=(root/'src/ps2/veronica/prog'/file).read_text()
 for name in names:
  match=re.search(r'^const [^\n=]+\b'+name+r'\[[^\n=]+\s*=\s*\{',text,re.M)
  if not match:raise RuntimeError('missing original table '+name)
  result.append(text[match.start():block(text,match.start())]+';\n')
(out/'combat_tables.inc').write_text(''.join(result))
