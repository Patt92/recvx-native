#!/usr/bin/env python3
"""RM_5010 Intro: nach Bestaetigen der letzten Meldungsseite muss der Spieler freikommen.

game.c:264-270: ist die Meldung fertig (st_flg 0x200/0x4000/0x1000) und wird bestaetigt oder abgebrochen
(pad_ps 0x800/0x1000 bzw. cb_flg 0x1000 ohne 0x1), setzt das Original st_flg 0x400000 und cb_flg 0x2000.
Das Introskript wartet darauf mit bhCk Matrix 5 Bit 9 (MSB-zuerst = 0x400000): Ereignis 0 @0x41a und
Ereignis 3 @0x57c. Vorher fordern Ereignis 0 @0x408 (bhEasySESet, op 0x86, SeNo 0x255) und Ereignis 3
@0x54c (SeNo 0x209) ihre Effektklaenge an.

Vorher: der Port setzte die Bits beim Bestaetigen nicht, beide Ereignisse warteten endlos, cinematic und
player_scripted blieben aktiv und der Spieler bekam vor dem Tyrant-Kampf nie die Kontrolle.

Natuerlicher Ablauf ab Raumstart, nur Eingaben (Bestaetigen ab Frame 1800, danach Vorwaerts laufen)."""
import argparse
import json
import math
import subprocess
import tempfile
from pathlib import Path

START = (5.0, 0.0, 7.0)
REPLAY = ''.join(f'{f} confirm\n' for f in range(1800, 5000, 120)).replace(
    '1800 confirm\n', '1800 confirm\n1810 keys forward run\n1900 keys none\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())
    with tempfile.TemporaryDirectory(prefix='recvx-rm5010-') as folder:
        root = Path(folder)
        script = root / 'intro.txt'
        script.write_text(REPLAY)

        def run(frames):
            state = root / f'state-{frames}.json'
            done = subprocess.run([runtime, '--data', data, '--mods', str(root / 'no-mods'),
                                   '--snapshots', str(root / 'snapshots'), '--user-dir', str(root / f'user-{frames}'),
                                   '--headless', '--in-game', '--enemy-ai', 'on', '--room', 'RM_5010.RDX',
                                   '--frames', str(frames), '--replay', str(script), '--state', str(state)],
                                  check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            s = json.loads(state.read_text())
            assert 'event index outside table' not in done.stdout, done.stdout[-1500:]
            assert s['blocked_commands'] == 0, ('blockierte Eventbefehle', s['blocked_commands'])
            return s

        free = run(1805)
        played = free['audio_played']
        for request in ('MULTSPQ1/0082/2/seq85/prog85', 'MULTSPQ1/0082/2/seq9/prog9'):
            assert request in played, ('Intro-SE fehlt', request, played)
        assert not free['cinematic'], 'Cinematic laeuft nach dem Bestaetigen weiter'
        assert not free['player_scripted'], 'Spieler bleibt skriptgesteuert'
        assert not free['dialog_open'], 'Meldung bleibt offen'

        moved = run(1900)
        distance = math.dist(START, moved['actor'])
        assert distance > 20.0, ('Spieler bewegt sich nicht', moved['actor'], distance)
    print(f'RM_5010 intro: player released after final message confirm, moved {distance:.1f} units')


if __name__ == '__main__':
    main()
