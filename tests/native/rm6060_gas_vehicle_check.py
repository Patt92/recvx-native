#!/usr/bin/env python3
"""RM_6060 mit Giftgas: das Fahrzeug (Raumobjekt 6, id 608) steht ueber seinem Schattenbild (Objekt 20, id 611).

Original: scd0 startet im Gaszweig (ev_flg 292) Ereignis 16. Es setzt work->px/py/pz = (-5, -4.8, -5),
Common_controll 36 nullt mlwP->objP[0].pos (event.c), danach Common_controll 34 mit v0 0: bhSetMotion
laeuft sofort (event.c switch(v0)) und schreibt je Motionkanal objP[n].pos (Motion.c:131-145). Raummotion 5
legt Knoten 0 auf (43.27, 23.70, 41.99), das Fahrzeug steht damit auf Ebene 2 (y 18.1) ueber Objekt 611.

Vorher: der Port hielt den Common-36-Wert als dauerhafte Knotenvorgabe, die jede spaetere Motion
ueberschrieb; das Fahrzeug blieb bei (-5, -4.8, -5) unter dem Boden, sichtbar war nur der Schatten.

Aufbau (Diagnose): Snapshot in RM_6060 ueber das Debugmenue, dann ev_flg 292/296/297 im SAVEFILE gesetzt
(Gas aktiv, Strom an) und der Snapshot nach Neues Spiel geladen."""
import argparse
import json
import math
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

GAS_FLAGS = (292, 296, 297)
KEY0 = (43.27, 23.70, 41.99)


def run(runtime, data, root, name, frames, replay, extra):
    script = root / f'{name}.txt'
    script.write_text(replay)
    state = root / f'{name}-{frames}.json'
    subprocess.run([runtime, '--data', data, '--mods', str(root / 'no-mods'), '--snapshots', str(root / 'snapshots'),
                    '--user-dir', str(root / f'user-{name}'), '--headless', '--enemy-ai', 'on', '--frames', str(frames),
                    '--replay', str(script), '--state', str(state)] + extra,
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return json.loads(state.read_text())


def set_flags(path, flags):
    data = bytearray(path.read_bytes()[:-4])
    assert data[:8] == b'RCVXSNAP', 'kein Snapshot'
    ev = 16 + 20  # Kopf (Magic, Format, SAVEFILE-Groesse), dann version/save_ct/Bytes/gm_mode, ev_flg
    for bit in flags:
        at = ev + (bit // 32) * 4
        word = struct.unpack_from('<I', data, at)[0] | (0x80000000 >> (bit % 32))
        struct.pack_into('<I', data, at, word)
    path.write_bytes(bytes(data) + struct.pack('<I', zlib.crc32(bytes(data)) & 0xffffffff))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())
    with tempfile.TemporaryDirectory(prefix='recvx-rm6060-gas-') as folder:
        root = Path(folder)
        run(runtime, data, root, 'save', 60, '40 debug_key F2\n41 debug_select snapshot_save 0\n42 confirm\n43 debug_key F2\n',
            ['--in-game', '--room', 'RM_6060.RDX'])
        snapshot = root / 'snapshots' / 'snapshot-1.rcvxsnap'
        assert snapshot.is_file(), 'Snapshot nicht geschrieben'
        set_flags(snapshot, GAS_FLAGS)
        s = run(runtime, data, root, 'load', 120,
                '1 new_game\n20 debug_key F2\n21 debug_select snapshot_load 0\n22 confirm\n23 debug_key F2\n', [])
        assert s['room'] == 'RM_6060.RDX', s['room']
        ev = s['event_flags']
        assert all(ev[b // 32] & (0x80000000 >> (b % 32)) for b in GAS_FLAGS), 'Gasflags nicht geladen'
        objects = {o['identity']: o for o in s['room_objects']}
        vehicle, shadow = objects[608], objects[611]
        assert not vehicle['hidden'] and not vehicle['inactive'], vehicle
        assert not shadow['inactive'], ('Schattenbild 611 im Gaszweig nicht aktiv', shadow)
        assert math.dist(vehicle['root'], KEY0) < 0.05, ('objP[0].pos nicht von Motion 5 geschrieben', vehicle)
        world = [p + r for p, r in zip(vehicle['position'], vehicle['root'])]
        horizontal = math.dist((world[0], world[2]), (shadow['position'][0], shadow['position'][2]))
        assert horizontal < 3.0 and abs(world[1] - shadow['position'][1]) < 1.0, ('Fahrzeug nicht ueber seinem Schatten', world, shadow)
    print(f'RM_6060 gas: vehicle root {[round(v, 2) for v in world]} over shadow {shadow["position"]}')


if __name__ == '__main__':
    main()
