#!/usr/bin/env python3
"""Real RM_2070 push through runtime input; diagnostic placement only.
The room init script chooses the original box location (70,.1,33).
No event/progress flags or push phases are injected.
"""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
replay=a.output/'push.txt';replay.write_text('10 position 70 0.1 41.3 0\n11 keys forward\n100 keys none\n')
states=[]
for frames in (16,22,30,50,80,105,130,180):
 state=a.output/f'push-{frames}.json'
 with (a.output/f'push-{frames}.log').open('w') as log:
  subprocess.run([str(a.runtime.resolve()),'--data',str(a.data.resolve()),'--in-game','--room','RM_2070.RDX','--headless','--frames',str(frames),'--replay',str(replay.resolve()),'--state',str(state.resolve()),'--validate-poses','--screenshot',str((a.output/f'push-{frames}.bmp').resolve()),'--snapshots',str((a.output/'snapshots').resolve())],stdout=log,stderr=log,check=True)
 s=json.loads(state.read_text());assert s['blocked_commands']==0 and not s['pending_commands'],s
 assert s['push']['unsupported']==0,s
 states.append(s);print(frames,s['motion'],s['push'],s['push_objects'][0]['position'])
assert any(s['motion']==49 and s['push']['phase']==4 for s in states)
assert any(s['motion']==50 and s['push']['phase']==5 for s in states)
assert any(s['motion']==49 and s['push']['phase']==7 for s in states)
assert not states[-1]['push']['active'] and not states[-1]['cinematic']
z=lambda s:s['push_objects'][0]['position'][2]
assert z(states[0])==33 and z(states[4])<33
assert z(states[5])==z(states[-1]),'crate moved after release'
assert any('/2/0015' in sound for sound in states[4]['audio_played'])
print('Original RM_2070: contact / 49 / 50 + SE527 / reverse49 / control release passed.')
