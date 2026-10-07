#!/usr/bin/env python3
"""Exercise FILE through the runtime input owner with original room assets."""
import argparse, json, subprocess
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('runtime',type=Path)
parser.add_argument('data',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
subprocess.run([str(args.runtime.resolve()),'--write-controls',str(args.output/'controls.ini')],check=True,stdout=subprocess.DEVNULL)
replay='''30 cancel
35 left
40 left
45 confirm
65 confirm
90 confirm
140 right
175 cancel
210 cancel
'''
script=args.output/'folder.txt';script.write_text(replay)
for name,frames in [('registers',86),('reader',170),('returned',200),('closed',240),('manual-item',130),('manual-item-return',190)]:
    script.write_text('30 right\n35 confirm\n45 confirm\n150 cancel\n' if name.startswith('manual-item') else replay)
    state=args.output/(name+'.json')
    command=[str(args.runtime.resolve()),'--data',str(args.data.resolve()),'--mods',str(args.output/'no-mods'),'--room','RM_0060.RDX','--inventory','--headless','--frames',str(frames),'--replay',str(script),'--state',str(state),'--screenshot',str(args.output/(name+'.bmp')),'--controls',str(args.output/'controls.ini'),'--snapshots',str(args.output/'snapshots')]
    with (args.output/(name+'.log')).open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    result=json.loads(state.read_text());assert result['blocked_commands']==0,(name,result['event_tasks'])
    if name=='registers':assert result['file_folder_open'] and result['file_registers'] and result['file_number']==0,result
    elif name=='reader':assert result['files_open'] and not result['file_folder_open'] and result['file_page']==1 and not result['file_transition'] and result['file_scroll']==0,result
    elif name=='returned':assert result['inventory_open'] and not result['files_open'] and result['menu_navigation'] and result['main_cursor']==1 and result['manual_read'],result
    elif name=='manual-item':assert result['files_open'] and not result['file_folder_open'] and result['file_number']==0 and result['file_page']==0,result
    elif name=='manual-item-return':assert result['inventory_open'] and result['menu_navigation'] and result['main_cursor']==1 and result['manual_read'] and not any((item>>16)==145 for item in result['items']),result
    else:assert not result['inventory_open'] and not result['files_open'],result
print('runtime FILE folder, register, page, manual return and status close passed')
