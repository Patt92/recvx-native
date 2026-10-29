#!/usr/bin/env python3
"""Exercise real debug-menu callbacks, hotkey remapping and modal pause with cooked data."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--data',type=Path,required=True)
    parser.add_argument('--emulator')
    parser.add_argument('--screenshot',type=Path)
    args=parser.parse_args()
    prefix=[args.emulator] if args.emulator else []
    with tempfile.TemporaryDirectory(prefix='recvx-debug-runtime-') as folder:
        folder=Path(folder)
        def run(name,script,frames=30,key='F2',extra=()):
            replay=folder/(name+'.txt');replay.write_text(script)
            config=folder/(name+'.ini');config.write_text('[debug]\nhotkey = '+key+'\n')
            state=folder/(name+'.json')
            command=prefix+[str(args.runtime.resolve()),'--data',str(args.data.resolve()),'--headless','--in-game','--room','RM_0010.RDX','--debug-config',str(config),'--replay',str(replay),'--frames',str(frames),'--state',str(state)]+list(extra)
            subprocess.run(command,check=True,cwd=ROOT)
            return json.loads(state.read_text())
        short=run('pause-short','0 debug_key F2\n',1)
        long=run('pause-long','0 debug_key F2\n',180)
        assert short['debug_open'] and long['debug_open']
        for field in ['actor','animation_frame','events','movie_frame','audio_played']:
            assert short[field]==long[field],('debug pause advances '+field)
        script='''0 debug_key F2
1 debug_select equip 8
2 confirm
3 debug_select give_count 30
4 confirm
5 debug_select give 12
6 confirm
7 debug_select hp 80
8 confirm
9 debug_select side_pack 1
10 confirm
11 debug_select wireframe 1
12 confirm
13 debug_select room 2
14 confirm
15 debug_select spawn 0
16 confirm
'''
        extra=['--screenshot',str(args.screenshot.resolve())] if args.screenshot else []
        state=run('actions',script,extra=extra)
        assert state['debug_open'] and state['weapon']==2 and state['hp']==80 and state['wireframe']
        assert state['room']=='RM_0020.RDX' and any((item>>16)&255==12 and item&65535==30 for item in state['items'])
        assert not state['blocked_commands'] and not state['debug_status'].startswith('Error:')
        for character in range(10,15):
            state=run('character-'+str(character),'0 debug_key F2\n1 debug_select character '+str(character)+'\n2 confirm\n')
            assert state['debug_open'] and state['player_entry']==character and not state['debug_status'].startswith('Error:'),state
        # Failed grants retain both inventory and the underlying status screen.
        setup='0 inventory\n1 confirm\n2 down\n3 confirm\n4 debug_key F2\n5 debug_select give_count 999\n6 confirm\n'
        before=run('full-before',setup)
        failed=run('full-failure',setup+'7 debug_select give 95\n8 confirm\n')
        assert failed['debug_status'].startswith('Error:') and failed['items']==before['items'] and failed['inventory_open']==before['inventory_open']
        granted=run('status-grant',setup+'7 debug_select give_count 1\n8 confirm\n9 debug_select give 21\n10 confirm\n')
        assert not granted['inventory_open'] and not granted['debug_status'].startswith('Error:')
        mods=folder/'bad-mods';(mods/'rooms').mkdir(parents=True);(mods/'rooms/RM_0020.RDT').write_bytes(b'bad')
        failed=run('bad-room','0 debug_key F2\n1 debug_select room 2\n2 confirm\n',extra=['--mods',str(mods)])
        assert failed['room']=='RM_0010.RDX' and failed['debug_status'].startswith('Error:'),'failed resource preflight replaced the live room'
        closed=run('close','0 debug_key F2\n1 debug_key F2\n',3)
        assert not closed['debug_open'] and not closed['wireframe'],'hotkey toggles wireframe or cannot close'
        for name,script,expected in [('old-key','0 debug_key F2\n',False),('new-key','0 debug_key F6\n',True)]:
            assert run(name,script,key='F6')['debug_open']==expected,'hotkey remapping ignored'
    print('Debug pause, all five character bodies, knife equip, ammo grant, HP/bag/wireframe, room/spawn changes, close and remapped hotkey verified with original data.')
if __name__=='__main__':main()
