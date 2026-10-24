#!/usr/bin/env python3
"""Das erste Bild einer Ereigniskamera zeigt die rohe bhSetEventCamera-Lage.

main.c:195-201: Task 7 (bhSysCallGame) vor Task 8 (bhSysCallEvent). In Task 7 rechnet
bhControlCamera (game.c:58, camera.c:23-59) cam.mtx, dann wird gezeichnet (game.c:61-112) und erst
danach laeuft bhCheckCut (game.c:289-296) mit bhControlEventCamera (cut.c:1986). bhCamSet 0
(event.c:958-966) in Task 8 setzt ueber bhSetEventCamera (cut.c:1644-1723) die Schluessellage direkt.
Das naechste Bild zeichnet deshalb diese Lage ohne Kontrollpass (cam.mode0 0), das Bild danach
nach genau einem, dann zwei usw.

Aufstellung (Diagnose): RM_1140-Hebelreplay, Bild 110 "Move the lever?" -> Yes, Ereignis 0 Ebene 15
ruft bhCamSet(0, evc 3) im Durchlauf nach der Antwort (game.c:147 reicht die Antwort weiter). Gemessen wird die Lage, aus der gezeichnet wird ("camera_view": Stand von
bhControlCamera, mit cam.mode0/ct0 der Ereigniskamera zu diesem Zeitpunkt). Der Port fuhr den ersten
bhControlEventCamera-Pass noch vor dem Zeichnen desselben Bildes."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / 'tests/native/replays/rm1140-lever.txt'


def run(runtime, data, root, frames):
    state = root / f'f{frames}.json'
    subprocess.run([str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'), '--user-dir', str(root / 'user'),
                    '--in-game', '--headless', '--room', 'RM_1140.RDX', '--frames', str(frames),
                    '--replay', str(REPLAY), '--state', str(state)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.loads(state.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-evcam-first-') as folder:
        root = Path(folder)
        (root / 'user').mkdir()
        before = run(runtime, data, root, 111)
        assert before['event_camera'] == 1 and before['camera_view'] and not before['camera_view']['event_init'], before['camera_view']
        # Die beantwortete Interaktionsmeldung meldet cb_flg 0x2000 (hitchk.c:4836-4842) und damit
        # cb_flg 0x20000000; game.c:147 reicht das erst im naechsten Durchlauf als 0x1000 an
        # bhControlEvent weiter, bhCamSet laeuft daher ein Bild nach der Antwort.
        for passes, frames in enumerate((112, 113, 114, 115)):
            s = run(runtime, data, root, frames)
            view = s['camera_view']
            assert s['event_camera'] == 3 and s['room_changes'] == 0, (frames, s['event_camera'], s['room_changes'])
            if passes == 0:
                assert view['event_init'] and view['event_clock'] == 0, \
                    f'erstes evc-3-Bild nach einem bhControlEventCamera-Pass gezeichnet: {view}'
            else:
                assert not view['event_init'] and view['event_clock'] == passes, \
                    f'Bild {frames}: {passes} Paesse erwartet, gezeichnet mit {view}'
    print('bhCamSet 0: erstes Bild in der rohen Schluessellage, danach ein bhControlEventCamera-Pass je Bild.')


if __name__ == '__main__':
    main()
