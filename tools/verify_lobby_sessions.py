#!/usr/bin/env python3
"""Diagnostic SDL lobby cancel/bind-failure paths must not leak the selected NPC into main NEW GAME."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--data',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
for occupied in (False,True):
    name='occupied' if occupied else 'cancel'
    reservation=socket.socket();reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    if occupied:reservation.listen()
    else:reservation.close()
    script=f'''40 confirm
80 down
90 down
100 confirm
130 down
140 confirm
150 down
160 down
170 confirm
171 sdl_key down Delete
172 sdl_key up Delete
173 sdl_text 127.0.0.1:{port}
180 confirm
190 down
200 confirm
230 cancel
240 cancel
260 new_game
300 sdl_key down F10
'''
    replay=out/(name+'.txt');replay.write_text(script)
    state=out/(name+'.json')
    cmd=[str(a.runtime.resolve()),'--data',str(a.data.resolve()),'--mods',str(out/'no-mods'),
         '--user-dir',str(out/('user-'+name)),'--player2','npc','--renderer','software','--ss','1',
         '--replay',str(replay),'--state',str(state)]
    try:
        with (out/(name+'.log')).open('w') as log:
            subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=35)
        s=json.loads(state.read_text())
        assert s['player2_selected_mode']=='off' and not s.get('player2',{}).get('active',False),s.get('player2')
        assert not s['netplay'] and not s['lobby']['busy'],s['lobby']
        expected='cannot listen' if occupied else 'Waiting for guest'
        assert expected in s['lobby']['status'],s['lobby']
    finally:
        reservation.close()
    with socket.socket() as freed:freed.bind(('127.0.0.1',port))
    print(f'{name}: main NEW GAME clears companion mode; listener released PASS',flush=True)
