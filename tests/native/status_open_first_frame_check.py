#!/usr/bin/env python3
"""Statusbildschirm: kein fertiges Bild vor dem Einschieben (Aufnahme und Inventartaste).

bhSysCallMonitor Modus 5 (system.c:1987-2013) gibt Task 9 frei, StatusMain laeuft aber erst im
Folgedurchlauf; dessen erster Pass setzt mit StatusInit/CenterPositionInit (sub1.c:1389/3308) die
Startlage ausserhalb des Bildes. Davor zeichnet niemand: Task 7 ruht, SpriteOnOff und bhDispMessage
laufen nicht. Vorher zeigte der Port in diesem Zwischenbild den fertig aufgebauten Bildschirm (rund
70 % helle Pixel) samt erstem Zeichen der GetItem-Frage, danach schob er ihn neu ein ("baut sich
zweimal auf"). Geprueft: nach der Ausblende ist das erste nicht schwarze Bild ein Einschiebebild
(unter 30 % helle Pixel), und das Einschieben wird danach heller. Keine feste Bildnummer.
"""
import argparse
import struct
import subprocess
import tempfile
from pathlib import Path


def lit_fraction(path):
    b = path.read_bytes()
    off = struct.unpack_from('<I', b, 10)[0]
    w, h = struct.unpack_from('<ii', b, 18)
    bpp = struct.unpack_from('<H', b, 28)[0] // 8
    row = (w * bpp + 3) & ~3
    lit = n = 0
    for y in range(0, abs(h), 4):
        for x in range(0, w, 4):
            p = off + y * row + x * bpp
            lit += (b[p] + b[p + 1] + b[p + 2]) > 30
            n += 1
    return lit / n


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-status-open-') as temporary:
        root = Path(temporary)
        controls = root / 'controls.ini'
        subprocess.run([str(runtime), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)

        def series(name, inputs, frames):
            replay = root / (name + '.txt')
            replay.write_text('\n'.join(inputs) + '\n')
            values = []
            for frame in frames:
                shot, state = root / f'{name}-{frame}.bmp', root / f'{name}-{frame}.json'
                subprocess.run([str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'),
                                '--user-dir', str(root / 'user'), '--controls', str(controls),
                                '--debug-config', str(root / 'debug.ini'), '--in-game', '--room', 'RM_0010.RDX',
                                '--renderer', 'software', '--enemy-ai', 'on', '--headless', '--frames', str(frame),
                                '--replay', str(replay), '--state', str(state), '--screenshot', str(shot)],
                               check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
                values.append(lit_fraction(shot))
            black = [i for i, v in enumerate(values) if v == 0.0]
            assert black, (name, 'Ausblende erreicht kein schwarzes Bild', values)
            after = values[black[-1] + 1:]
            assert len(after) >= 3, (name, 'zu wenige Bilder nach der Ausblende', values)
            assert after[0] < 0.3, (name, 'erstes Statusbild ist schon der fertige Bildschirm', values)
            assert after[0] < after[1] < after[2], (name, 'Einschieben wird nicht heller', values)
            print(name, ' '.join(f'{v:.2f}' for v in values))

        series('pickup', ['20 position 2.5 0 -19 0', '21 confirm'], range(46, 58))
        series('inventory', ['20 inventory'], range(22, 34))
    print('status screen opens without a finished first frame')


if __name__ == '__main__':
    main()
