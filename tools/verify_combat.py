#!/usr/bin/env python3
"""Verify real opening, original knife pickup/equipment and shared combat replay."""
import argparse,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--data',type=Path,required=True)
    parser.add_argument('--emulator')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-combat-') as folder:
        state=Path(folder)/'knife.json'
        def path(p):
            absolute=str(p.resolve())
            return 'Z:'+absolute.replace('/','\\') if args.emulator else absolute
        prefix=[args.emulator] if args.emulator else []
        subprocess.run(prefix+[str(args.runtime.resolve()),'--data',path(args.data),'--headless','--replay',path(ROOT/'tests/native/replays/knife-combat.txt'),'--frames','4380','--state',path(state)],check=True,cwd=ROOT)
        saved=json.loads(state.read_text())
        assert saved['weapon']==2 and saved['combat_attacks']>=1 and not saved['combat_shots']
        assert any((item>>16)&255==8 for item in saved['items'])
        assert not saved['inventory_open'] and not saved['dialog_open'] and not saved['blocked_commands']
        assert saved['motion']==42 and saved['audio_played'].count('ARMS/0012/0020')>=1
    print('Original opening → knife pickup → inventory equipment → knife attack → idle passed.')
if __name__=='__main__':main()
