#!/usr/bin/env python3
"""Check original security-room progression, stairs and return doors.

Diagnostic placements and debug room selection exercise real input/script paths;
these are bounded checks, not a complete walked campaign or animation-fidelity proof.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
SECURITY = '''60 position 12 0 70 0
3200 confirm
4000 debug_key F2
4001 debug_select room 6
4002 confirm
4003 debug_key F2
4060 position 23 0 8 0
'''
STAIRS = '''60 position -186 0 86 0
61 confirm
260 position -175.5 8 85.8489 0
261 confirm
'''
RETURN = '''60 position -33 0 110 16384
61 confirm
180 confirm
500 skip
700 position 32.5 0 70 49152
701 confirm
750 skip
'''
AMBIENCE = '60 position -166 8 72 32768\n60 confirm\n400 position 40 0 0 0\n400 confirm\n850 position 40 0 74 32768\n850 confirm\n1300 position 53.5 0 25 49152\n1300 confirm\n'
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--data',type=Path,required=True)
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-continuity-') as temporary:
        folder=a.output.resolve() if a.output else Path(temporary)
        folder.mkdir(parents=True,exist_ok=True)
        results={}
        for name,room,script,frames in [('security','RM_0030.RDX',SECURITY,7200),('up-motion','RM_0050.RDX',STAIRS,90),('up','RM_0050.RDX',STAIRS,240),('down-motion','RM_0050.RDX',STAIRS,280),('down','RM_0050.RDX',STAIRS,520),('return','RM_0050.RDX',RETURN,1100),('ambient-inside6','RM_0050.RDX',AMBIENCE,350),('ambient-inside7','RM_0050.RDX',AMBIENCE,800),('ambient-back6','RM_0050.RDX',AMBIENCE,1250),('ambient-outside5','RM_0050.RDX',AMBIENCE,1650)]:
            replay=folder/(name+'.txt');replay.write_text(script)
            state=folder/(name+'.json')
            with (folder/(name+'.log')).open('w') as log:
                subprocess.run([str(a.runtime.resolve()),'--data',str(a.data.resolve()),'--in-game','--headless','--room',room,'--replay',str(replay),'--frames',str(frames),'--state',str(state)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
            r=json.loads(state.read_text());results[name]=r
            assert not r['blocked_commands'],(name,r['event_tasks'])
            if name.endswith('-motion'):
                assert r['cinematic'] and r['motion']==(35 if name=='up-motion' else 36),(name,'original stair motion not running')
            else:
                assert all(not r[k] for k in ['cinematic','player_hidden','player_scripted','dialog_open','inventory_open','movie_active']),(name,'control not released')
        s=results['security'];assert s['room']=='RM_0040.RDX'
        assert s['weapon']==4 and sum(1 for packed in s['items'][2:12] if packed==(9<<16)|12)==1,'Steve gift lost/duplicated across room change'
        assert 'VOICE1/0056' in s['audio_played'] and 'VOICE1/0075' in s['audio_played'],'security-room original dialogue not completed'
        assert abs(results['up']['actor'][1]-8)<.01 and abs(results['down']['actor'][1])<.01,'original stairs did not traverse both floors'
        assert results['return']['room']=='RM_0020.RDX' and results['return']['room_changes']==2,'return doors not completed'
        for name,room,changes in [('ambient-inside6','RM_0060.RDX',1),('ambient-inside7','RM_0070.RDX',2),('ambient-back6','RM_0060.RDX',3),('ambient-outside5','RM_0050.RDX',4)]:
            r=results[name];assert r['room']==room and r['room_changes']==changes,(name,'ambient room traversal failed')
            assert r['background_active']==[1,0,0],(name,'unrequested ambient stop added to original bank/request path')
            assert r['audio_played'].count('DOOR/0002/0001')>0 and r['audio_played'].count('DOOR/0002/0000')>0,(name,'door opening/closing sequence absent')
        print('Original Steve security-room dialogue/control release, both stairs directions and dog-area/cemetery return doors passed; original ambient retention without an explicit stop passed (indoor rain still unresolved) (diagnostic placements).')
if __name__=='__main__':main()
