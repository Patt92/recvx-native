#!/usr/bin/env python3
"""Extract original lock-on searches and arm rotation from pwksub/playpch."""
import re,sys
from pathlib import Path
from generate_native_tables import block
root,out=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
parts=['// Generated from pwksub.c/playpch.c; host adapters supply actors and wall query.\n']
for file,names in [('pwksub.c',['bhCalcLockEneYR','bhSearchNearEnemy','bhSearchNextEnemy']),('playpch.c',['bhSearchEnemy','SetLockOnDirection','MixSetToJointRot'])]:
 source=(root/'src/ps2/veronica/prog'/file).read_text()
 for name in names:
  match=re.search(r'^(?:static )?(?:int|void|search\*) '+name+r'\([^;]*?\)\s*\{',source,re.M)
  if not match:raise RuntimeError(name)
  body=source[match.start():block(source,match.start())]
  body=body.replace('return (int)topP;', 'return topP != NULL;') # pointer presence, not PS2 pointer truncation
  parts.append(body+'\n')
(out/'targeting.inc').write_text(''.join(parts))
