#!/usr/bin/env python3
"""player.c:6972-7005 Blinkfolge: plp->mlwP->texP = plp->txp[PlyEyeTab[EXP1_I(4)]], am Ende txp[0].

txp[k] ist die Texturliste von mdl[k] des Spielers (dread.c:64); fuer SYSTEM 10 unterscheiden sich txp[0..2]
nur im Augenbild (128x128-Seite). Der Zeichenpass nimmt seit 2026-11-04 die Liste des aktuellen Eintrags
(RoomWorld::player_texture_set); vorher zeichnete der Spieler immer txp[0] und blinzelte nie.
Lauf RM_0020 ohne Eingabe: Bild 350 steht der Satz auf 2 (geschlossen), 352 wieder auf 0; das Bild unterscheidet
sich zwischen beiden (die Augen). Gegenprobe: gleiche Lage, gleicher Satz -> gleiches Bild (351/352 Satz 1/0,
353 Satz 0 muss gleich 352 bleiben, solange sich die Pose nicht aendert, wird hier nicht verlangt)."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def run(runtime, data, root, frames):
    state = root / f's{frames}.json'
    shot = root / f's{frames}.bmp'
    subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()), '--mods', str(root / 'no-mods'),
                    '--user-dir', str(root / 'user'), '--in-game', '--headless', '--room', 'RM_0020.RDX',
                    '--frames', str(frames), '--state', str(state), '--screenshot', str(shot)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return json.loads(state.read_text()), shot.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-eye-') as folder:
        root = Path(folder)
        closed, closed_img = run(args.runtime, args.data, root, 350)
        opened, opened_img = run(args.runtime, args.data, root, 352)
        assert closed['head']['eye'] == 2 and closed['head']['texture_set'] == 2, closed['head']
        assert opened['head']['eye'] == 0 and opened['head']['texture_set'] == 0, opened['head']
        assert closed_img != opened_img, 'geschlossene Augen (txp[2]) aendern das Bild nicht'
    print('player_eye_texture PASS')


if __name__ == '__main__':
    main()
