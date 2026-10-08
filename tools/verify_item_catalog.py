#!/usr/bin/env python3
"""Host catalog and original manual -> EraseItem -> ammunition regression.
Inventory/position in the diagnostic snapshot are arranged to expose the old
missing equipment-index decrement. Original scripts and normal menu inputs run.
"""
import argparse,json,shutil,struct,subprocess,zlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--data',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
r=str(a.runtime.resolve())
subprocess.run([r,'--write-controls',str(out/'controls.ini')],check=True,stdout=subprocess.DEVNULL)
shutil.copyfile(Path(__file__).resolve().parents[1]/'debug.ini',out/'debug.ini')
def run(name,replay,frames):
 script=out/(name+'.txt');script.write_text(replay);state=out/(name+'.json')
 with (out/(name+'.log')).open('w') as log:
  subprocess.run([r,'--data',str(a.data.resolve()),'--mods',str(out/'no-mods'),'--controls',str(out/'controls.ini'),'--debug-config',str(out/'debug.ini'),'--snapshots',str(out/'snapshots'),'--room','RM_0070.RDX','--in-game','--headless','--frames',str(frames),'--replay',str(script),'--state',str(state),'--screenshot',str(out/(name+'.bmp'))],check=True,stdout=log,stderr=subprocess.STDOUT)
 v=json.loads(state.read_text());assert not v['blocked_commands'],v['event_tasks'];return v
seed=run('seed','0 debug_key F2\n1 debug_select snapshot_save 0\n2 confirm\n',4)
# No gameplay flag injection: only diagnostic inventory arrangement/pose.
b=bytearray((out/'snapshots/snapshot-1.rcvxsnap').read_bytes());assert b[:8]==b'RCVXSNAP'
struct.pack_into('<12I',b,16+0x1f4,3,50<<16|1,145<<16|1,1<<16|1,*([0]*8))
struct.pack_into('<fffI',b,16+0x804,53,0,11,32768)
struct.pack_into('<I',b,len(b)-4,zlib.crc32(b[:-4]));(out/'snapshots/snapshot-2.rcvxsnap').write_bytes(b)
load='0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n'
manual=load+'30 inventory\n60 right\n65 confirm\n75 confirm\n150 cancel\n210 cancel\n'
command=run('manual-command',manual,73);assert command['inventory_open']
closed=run('manual-closed',manual,240)
assert closed['manual_read'] and not closed['files_open']
assert closed['items'][:4]==[2,50<<16|1,1<<16|1,0],closed['items']
assert closed['weapon']==20,'knife identity lost after manual removal'
taken=run('manual-ammo',manual+'260 position 53 0 11 32768\n270 confirm\n320 confirm\n350 confirm\n',600)
assert taken['items'][:4]==[2,50<<16|1,1<<16|1,12<<16|15],taken['items']
assert taken['weapon']==20 and not taken['dialog_open'] and not taken['inventory_open']
given=run('file-give','0 debug_key F2\n1 debug_select give 264\n2 confirm\n3 debug_key F2\n',20)
assert given['items']==seed['items'] and given['file_flags']&(1<<8),'file catalog grant consumed a slot'
# Legacy double acquisition: owned file 8 plus its model in pip[2].
struct.pack_into('<12I',b,16+0x1f4,3,50<<16|1,48<<16|1,1<<16|1,*([0]*8))
struct.pack_into('<I',b,16+0x1f4+383*4,257)
struct.pack_into('<I',b,len(b)-4,zlib.crc32(b[:-4]));(out/'snapshots/snapshot-3.rcvxsnap').write_bytes(b)
repaired=run('legacy-file','0 debug_key F2\n1 debug_select snapshot_load 2\n2 confirm\n',20)
assert repaired['items'][:4]==[2,50<<16|1,1<<16|1,0] and repaired['weapon']==20
assert repaired['file_flags']==257
print('Original manual removal, equipped knife after ammunition, typed file catalog and owned legacy duplicate repair passed.')
