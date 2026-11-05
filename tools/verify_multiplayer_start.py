#!/usr/bin/env python3
"""Diagnostic real-menu navigation: submenu appearance and direct NPC start."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
base='40 confirm\n80 down\n90 down\n100 confirm\n'
for name,script in [('menu',base+'120 sdl_key down F10\n'),('solo',base+'130 confirm\n450 sdl_key down F10\n')]:
    replay=out/(name+'.txt');replay.write_text(script)
    for ext in ('.bmp','.json'):(out/(name+ext)).unlink(missing_ok=True)
    with (out/(name+'.log')).open('w') as log:
        subprocess.run([str(a.runtime.resolve()),'--data',str(a.data.resolve()),'--mods',str(out/'no-mods'),'--user-dir',str(out/('user-'+name)),'--renderer','software','--ss','1','--replay',str(replay),'--state',str(out/(name+'.json')),'--screenshot',str(out/(name+'.bmp'))],env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=40)
    s=json.loads((out/(name+'.json')).read_text())
    if name=='solo':
        assert s['player2_selected_mode']=='npc' and s['player2']['mode']=='npc',s.get('player2')
        assert not s['title_active'],s['title_active']
print('Multiplayer submenu and direct Solo NPC start PASS')
