#!/usr/bin/env python3
"""Exercise original GameOverSet and Retry Yes/No against the native runtime.
The isolated event table is diagnostic; room/player/audio/texture resources are original.
"""
import argparse,json,struct,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='recvx-gameover-') as d:
 mods=Path(d);(mods/'rooms').mkdir()
 b=bytearray((a.data/'rooms/RM_0020.RDT').read_bytes());off=len(b)
 # Init ends. Scan waits, invokes original bhGameOverSet and ends. Retry checkpoint precedes the scan.
 b+=struct.pack('<III',12,17,18)+bytes([0x14,0,0,0,0,0,0xf8,0xf9,20,0,0xc6,0,0])
 struct.pack_into('<I',b,28,off);(mods/'rooms/RM_0020.RDT').write_bytes(b)
 def run(name,frames,replay=''):
  state=a.output/(name+'.json');log=a.output/(name+'.log');r=a.output/(name+'.txt');r.write_text(replay)
  with log.open('w') as stream:
   result=subprocess.run([str(a.runtime.resolve()),'--data',str(a.data.resolve()),'--mods',str(mods),'--room','RM_0020.RDX','--headless','--in-game','--frames',str(frames),'--ss','1','--replay',str(r.resolve()),'--state',str(state.resolve())],stdout=stream,stderr=subprocess.STDOUT)
  if result.returncode:raise RuntimeError(log.read_text())
  value=json.loads(state.read_text());assert value['blocked_commands']==0 and not value['pending_commands'],value
  return value
 prompt=run('prompt',170);assert prompt['gameover_active'] and prompt['gameover_phase']==2,prompt
 retry=run('retry',190,'175 confirm\n');assert retry['retry_count']==1 and not retry['gameover_active'] and retry['hp']==160,retry
 title=run('decline',400,'175 right\n176 confirm\n');assert title['title_active'] and not title['gameover_active'],title
 assert 'ADV/0008' in title['audio_played'],title['audio_played']
 print('Original GameOverSet: prompt, Yes restores retry checkpoint, No plays ADV death voice and returns to title.')
