#!/usr/bin/env python3
"""RM0100 original crate edge collision and action descent.
Diagnostic placement on real crate surfaces; no progress/script flags injected.
"""
import argparse,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--data',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
r=str(a.runtime.resolve())
subprocess.run([r,'--write-controls',str(out/'controls.ini')],check=True,stdout=subprocess.DEVNULL)
shutil.copyfile(Path(__file__).resolve().parents[1]/'debug.ini',out/'debug.ini')
def run(name,script,frames):
 replay=out/(name+'.txt');replay.write_text(script);state=out/(name+'.json')
 with (out/(name+'.log')).open('w')as log:
  subprocess.run([r,'--data',str(a.data.resolve()),'--mods',str(out/'no-mods'),'--controls',str(out/'controls.ini'),'--debug-config',str(out/'debug.ini'),'--snapshots',str(out/'snapshots'),'--in-game','--room','RM_0100.RDX','--headless','--frames',str(frames),'--replay',str(replay),'--state',str(state),'--validate-poses','--screenshot',str(out/(name+'.bmp'))],stdout=log,stderr=log,check=True)
 s=json.loads(state.read_text());assert not s['blocked_commands'],s['event_tasks'];assert s['pending_commands']==[{'opcode':139,'event':0,'offset':34,'bytes':[139,0,0,5,34,11,144,1,52,33,0,2]}],s['pending_commands'];assert not s['push']['unsupported']&~1,s['push'];return s # 0x1: HOKAN gap only
# The room's original event places crate 7 at (-12,4,-105.6).
for mode in ('forward','forward run'):
 for yaw in (0,16384,32768,49152):
  name=f'edge-{mode.replace(" ","-")}-{yaw}'
  s=run(name,f'30 position -12 13 -105.6 {yaw}\n31 keys {mode}\n130 keys none\n',150)
  x,y,z=s['actor'];assert y==13 and abs(x+12)<=4.5 and abs(z+105.6)<=4.5,s['actor']
# Free north side: confirm starts the original DND instead of walking off.
down='30 position -12 13 -105.6 0\n31 keys forward\n100 keys none\n110 confirm\n'
mid=run('descent-action',down,120);assert mid['motion'] ==22,mid['motion']
end=run('descent-end',down,250);assert end['actor'][1]==4 and not end['cinematic'],end['actor']
print('RM0100: all four walk/run edges stay at 13; action descent returns to 4.')
