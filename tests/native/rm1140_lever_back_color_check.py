#!/usr/bin/env python3
"""RM_1140 Hebel: die Rueckfarbe des Ereignisses gilt schon im ersten Bild der Ereigniskamera.

Ereignis 0, Ebene 15 (Skript @1166): nach "Move the lever?" -> Yes laufen in EINEM Eventpass
bhCamSet(0, evc 3) (event.c bhCamSet), bhBGColorSet(ff 00 00 00, bcl_ct 0) (event.c:7206-7239:
rom->bak_col, gm_flg 0x8000) und bhSetDoorCall (event.c:2333, Typ 255 = ohne Tuerdemo).

Original: Task 7 (bhSysCallGame, Zeichnen) laeuft vor Task 8 (bhSysCallEvent) (main.c:196,
bhSysTaskJumpTab), bhControlEOR folgt erst danach in Ps2SwapDBuff (ps2_NaSystem.c:67-86) und
installiert mit bcl_ct 0 sofort bhChangeBackColorEvt (sync.c:120-137). Die so gesetzte Farbe
loescht den naechsten Puffer, also das erste Bild mit evc 3. bhSetDoorDemo (room.c:883) setzt
kein ts_flg 0x80; das kommt erst in Modus 10 des Tuertasks (system.c:1027-1044), sodass das EOR
in diesem Bild nicht gesperrt ist. evc 3 zeigt keine Raumgeometrie: das Bild ist reine Rueckfarbe.

Der Port lief das EOR erst am Ende des Takts (nach dem Zeichnen) und sperrte es zusaetzlich,
sobald die DoorCall-Anforderung anstand: das erste evc-3-Bild loeschte mit der alten Raumfarbe
rom->bak_col 0x061628 - das kurze dunkelblaue Aufblitzen beim Ziehen des Hebels."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm1140-lever.txt'


def pixels(path):
    data = path.read_bytes()
    assert data[:2] == b'BM', path
    offset = int.from_bytes(data[10:14], 'little')
    width = int.from_bytes(data[18:22], 'little')
    height = int.from_bytes(data[22:26], 'little')
    assert int.from_bytes(data[28:30], 'little') == 24, path
    row = (width * 3 + 3) & ~3
    colours = {}
    for y in range(height):
        line = data[offset + y * row:offset + y * row + width * 3]
        for x in range(0, width * 3, 3):
            rgb = (line[x + 2], line[x + 1], line[x])
            colours[rgb] = colours.get(rgb, 0) + 1
    return colours


def run(runtime, data, root, frames):
    state, shot = root / f'f{frames}.json', root / f'f{frames}.bmp'
    subprocess.run([str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'), '--user-dir', str(root / 'user'),
                    '--in-game', '--headless', '--room', 'RM_1140.RDX', '--frames', str(frames),
                    '--replay', str(REPLAY), '--state', str(state), '--screenshot', str(shot)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.loads(state.read_text()), pixels(shot)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-rm1140-lever-') as folder:
        root = Path(folder)
        (root / 'user').mkdir()
        # Letztes Bild vor der Antwort: Hebelansicht evc 1, gezeichnete Geometrie.
        before, before_px = run(runtime, data, root, 110)
        assert before['event_camera'] == 1 and before['room_changes'] == 0, (before['event_camera'], before['room_changes'])
        assert len(before_px) > 100, f'Hebelansicht ohne Geometrie: {len(before_px)} Farben'
        # Erstes evc-3-Bild: die beantwortete Meldung wird erst im Durchlauf nach der Antwort
        # freigegeben (game.c:147), der DoorCall steht dann an, der Raum ist noch nicht gewechselt.
        after, after_px = run(runtime, data, root, 112)
        assert after['event_camera'] == 3 and after['room_changes'] == 0, (after['event_camera'], after['room_changes'])
        room_clear = (0x06, 0x16, 0x28)  # RM_1140 rom->bak_col (Raumkopf +0x104)
        assert room_clear not in after_px, f'erstes evc-3-Bild loescht mit der alten Raumfarbe 0x061628: {sorted(after_px.items(), key=lambda c: -c[1])[:3]}'
        assert after_px == {(0, 0, 0): sum(after_px.values())}, f'erstes evc-3-Bild nicht in bhBGColorSet-Farbe 0x000000: {sorted(after_px.items(), key=lambda c: -c[1])[:3]}'
    print('RM_1140 Hebel: bhBGColorSet (bcl_ct 0) loescht das erste evc-3-Bild schwarz, kein Aufblitzen der Raumfarbe.')


if __name__ == '__main__':
    main()
