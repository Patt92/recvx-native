#!/usr/bin/env python3
"""plp->stflg 0x100/0x200 sind Ein-Pass-Impulse (player.c:1365/1390).

bhControlPlayer loescht im Passtor (sp_flg 0x1, stflg 0x1000000 frei) vor der
Moduswahl stflg 0x300; der Pass setzt 0x200 bei jedem Fusskontakt
(player.c:1435/2195-3681, pl_evt.c:281-468) und 0x100 beim Schuss
(player.c:4985/5690, playpch*.c). en07.c:301/328/393 lesen beide als Weckreiz.
Ohne das Loeschen blieb 0x200 nach dem ersten Schritt dauerhaft stehen.

Pruefung: RM_1000, ab Bild 20 vorwaerts gehen (keine Positions-/Flagaenderung).
Im Kontaktbild 62 steht 0x200, im Bild davor und danach nicht.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        replay = root / 'walk.txt'
        replay.write_text(''.join(f'{f} keys forward\n' for f in range(20, 140)))

        def bits(frames):
            state = root / f'state-{frames}.json'
            subprocess.run([str(args.runtime), '--data', str(args.data), '--mods', str(root / 'mods'),
                            '--snapshots', str(root / 'snapshots'), '--headless', '--in-game',
                            '--room', 'RM_1000.RDX', '--frames', str(frames), '--replay', str(replay),
                            '--state', str(state)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return json.loads(state.read_text())['work_flags'][14] & 0x300

        seen = {n: bits(n) for n in (61, 62, 63, 79, 80, 81)}
        assert seen[62] == 0x200 and seen[80] == 0x200, seen
        assert seen[61] == 0 and seen[63] == 0 and seen[79] == 0 and seen[81] == 0, seen
    print('stflg 0x200 pulst je Fusskontakt (Bild 62/80) und faellt im naechsten Spielerpass: PASS')


if __name__ == '__main__':
    main()
