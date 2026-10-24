#!/usr/bin/env python3
"""RM_3220 Aufzug: die Spielerin bleibt in der Kabine, bis die Tuersequenz uebernimmt.

Ereignis 50 (scd0-Start ab Eingang 0) faehrt die Kabine per Spline bis y 79.9 und endet mit
Common 23/1 (flg |= 0x118), DoorCall nach RM_3120 und 12 01 (sp_flg |= ~0, event.c:1646). Im Original
laeuft danach Task 7 mit bhStartDoorDemo Fall 0 (game.c:24-26, room.c:965-976, sp_flg = 0x48) vor
bhControlPlayer, dessen Koerper-/Bodenteil unter sp_flg 0x1 steht (player.c:1365).

Vorher: der native Spielerpass lief im Anforderungsbild noch mit sp_flg -1, die Bodenfuehrung zog y von
79.9 auf grand[3] = 44 und Claire verschwand unter dem Kabinenboden, bevor die Tuer kam."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

def run(runtime, data, root, frames):
    state = root / f'state-{frames}.json'
    subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()), '--mods', str(root / 'no-mods'),
                    '--user-dir', str(root / f'user-{frames}'), '--headless', '--in-game', '--room', 'RM_3220.RDX',
                    '--spawn', '0', '--frames', str(frames), '--state', str(state)],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return json.loads(state.read_text())

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-rm3220-') as folder:
        root = Path(folder)
        ride = run(args.runtime, args.data, root, 230)
        assert not ride['door_active'] and ride['actor'][1] > 79.0, ('Fahrt nicht oben', ride['actor'], ride['door_active'])
        for frames in (236, 240):
            door = run(args.runtime, args.data, root, frames)
            assert door['door_active'], ('Tuersequenz nicht gestartet', frames)
            assert abs(door['actor'][1] - ride['actor'][1]) < 0.5, ('Spielerin faellt aus der Kabine', frames, door['actor'])
            assert door['work_flags'][6] & 0x48 == 0x48 and not door['work_flags'][6] & 1, ('sp_flg', frames, hex(door['work_flags'][6]))
    print('RM_3220 elevator: player stays in the cabin until the door sequence.')

if __name__ == '__main__':
    main()
