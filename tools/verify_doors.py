#!/usr/bin/env python3
"""Verify real opening, walked door round trips, confirmation skip and inventory persistence."""
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
    parser.add_argument('--emulator',help='Optional executable such as wine64')
    args=parser.parse_args()
    prefix=[args.emulator] if args.emulator else []
    states={}
    with tempfile.TemporaryDirectory(prefix='recvx-door-') as temporary:
        for name,replay,frames in [('opening','lighter.txt',3200),('natural','door-roundtrip.txt',4100),('skip','door-roundtrip-skip.txt',3500)]:
            state=Path(temporary)/(name+'.json')
            subprocess.run(prefix+[str(args.runtime.resolve()),'--data',str(args.data.resolve()),'--headless','--replay',str(ROOT/'tests/native/replays'/replay),'--frames',str(frames),'--state',str(state)],check=True,cwd=ROOT)
            states[name]=json.loads(state.read_text())
        walk_replay=Path(temporary)/'hall-walk.txt'
        walk_replay.write_text('0 keys forward\n60 keys back\n120 keys forward run\n180 keys none\n')
        walk_state=Path(temporary)/'hall-walk.json'
        subprocess.run(prefix+[str(args.runtime.resolve()),'--data',str(args.data.resolve()),'--headless','--in-game','--room','RM_0010.RDX','--spawn','1','--replay',str(walk_replay),'--frames','240','--state',str(walk_state)],check=True,cwd=ROOT)
        walked=json.loads(walk_state.read_text())
        assert not walked['blocked_commands'] and any(sound.startswith('MULTSPQ1/0001/2/') for sound in walked['audio_played']), 'hall movement has no original footsteps'
        stairs=Path(temporary)/'stairs.json'
        subprocess.run(prefix+[str(args.runtime.resolve()),'--data',str(args.data.resolve()),'--headless','--in-game','--room','RM_0010.RDX','--spawn','1','--replay',str(ROOT/'tests/native/replays/first-stairs.txt'),'--frames','180','--state',str(stairs)],check=True,cwd=ROOT)
        stairs_state=json.loads(stairs.read_text())
        assert stairs_state['room']=='RM_0020.RDX' and stairs_state['room_changes']==1 and not stairs_state['door_active']
        assert not stairs_state['blocked_commands'] and not stairs_state['player_hidden']

    opening=states['opening']
    for name in ['natural','skip']:
        state=states[name]
        assert state['room']=='RM_0000.RDX' and state['room_changes']==2 and not state['door_active'], (name,'roundtrip',state)
        assert state['door_skipped']==(name=='skip'), (name,'skip')
        assert state['items']==opening['items'] and state['weapon']==opening['weapon'] and state['hp']==opening['hp'], (name,'inventory')
        assert not state['cinematic'] and not state['player_hidden'] and not state['player_scripted'] and not state['dialog_open'] and not state['movie_active'], (name,'opening replayed')
        assert state['blocked_commands']==0, (name,'event errors')
        assert any(sound.startswith('MULTSPQ1/0000/2/') for sound in state['audio_played']), (name,'cell movement has no original footsteps')
        assert not any(sound.startswith('MULTSPQ1/0001/2/') for sound in state['audio_played']), (name,'turning in place plays footsteps')
    assert states['natural']['actor']==states['skip']['actor'],'skip changes spawn'
    print('Walked cell/hall round trips, natural door completion, OK skip, identical spawn and persistent inventory and first-stairs room loading verified.')
if __name__=='__main__':main()
