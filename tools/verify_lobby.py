#!/usr/bin/env python3
"""Diagnostic SDL loopback: title > multiplayer > lobby, edited IP and swapped seats."""
import argparse,json,os,subprocess,socket
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
base='40 confirm\n80 down\n90 down\n100 confirm\n130 down\n140 confirm\n'
with socket.socket() as reservation:
    reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
processes=[]
for role in ('host','guest'):
    script=base+('150 down\n160 right\n' if role=='host' else '150 right\n160 down\n')+f'170 down\n180 confirm\n181 sdl_key down Delete\n182 sdl_key up Delete\n183 sdl_text 127.0.0.1:{port}\n190 confirm\n200 down\n210 confirm\n'
    replay=out/(role+'.txt');replay.write_text(script)
    state=out/(role+'.json');log=(out/(role+'.log')).open('w')
    for ext in ('.bmp','.json'):(out/(role+ext)).unlink(missing_ok=True)
    cmd=[str(a.runtime.resolve()),'--data',str(a.data.resolve()),'--mods',str(out/'no-mods'),'--user-dir',str(out/('user-'+role)),'--player2','npc','--renderer','software','--ss','1','--replay',str(replay),'--frames','900','--state',str(state),'--screenshot',str(out/(role+'.bmp'))]
    processes.append((subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT),state,log))
try:
    for proc,state,log in processes:
        proc.wait(timeout=60);log.close();assert proc.returncode==0,(proc.returncode,state)
    h,g=[json.loads(state.read_text()) for _,state,_ in processes]
    assert 'Player 2' in h['netplay'] and 'Player 1' in g['netplay'],(h['netplay'],g['netplay'])
    assert h['player2_selected_mode']==g['player2_selected_mode']=='local'
    assert not h['title_active'] and not g['title_active'], 'connected lobby did not start directly'
    assert 'DESYNC' not in h['netplay']+g['netplay'],(h['netplay'],g['netplay'])
    assert h['input_tick']==g['input_tick']==900,(h['input_tick'],g['input_tick'])
    assert h['title_mode']==g['title_mode'] and h['title_cursor']==g['title_cursor']
    print('Lobby: title navigation, SDL text address entry, handshake, host Player 2 / guest Player 1 and 900 shared ticks PASS')
finally:
    for proc,_,log in processes:
        if proc.poll() is None:proc.kill();proc.wait()
        log.close()
