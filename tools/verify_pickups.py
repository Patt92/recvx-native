#!/usr/bin/env python3
"""Bounded original-room map and wide-weapon pickup replays.

Diagnostic placements/debug inventory changes use ordinary dialogue input and
the actual room scripts. Screenshots do not prove a complete playthrough.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
# Die Karte (ATR 11, x 16..23) ist nur von Westen erreichbar: oestlich davon liegt die
# erhoehte Plattform, Wand 21 Typ 7 (x 19..63, y 7..8). bhCheckWallEx Typ 7 (hitchk.c:562)
# schiebt einen 16,5 hohen Koerper dort heraus; die fruehere Diagnoseposition (26,0,90)
# stand unter der Plattform und war im Original unerreichbar.
MAP = '60 position 14 0 90 49152\n61 confirm\n100 confirm\n180 confirm\n'
# Die Diagnoseposition der Calico-Aufnahme stand auf z=12 und erreichte den Regal-ETC
# nicht mehr (RM_0070 ETC 4, pos(49,0,7) ext(7.99,0,7.99), attr 0x3c00). Gemessen am
# 2026-10-13 mit drei Runtimes (pickupsel 13:0x, combat-aim 2026-10-12 23:45,
# doorskip3 2026-10-13 08:24): z=6 und z=10 oeffnen die Frage 157, z=12 und z=14 nicht —
# in allen drei Builds gleich, also keine neue Regression, sondern eine veraltete
# Platzierung. OFFEN und nicht hiermit geklaert: warum bhCheckExmAtari den Bereich ab
# z>=12 ablehnt (Probepunkt `fpx - sin*(3+(3-arn))`, hitchk.c); das gehoert in den
# Interaktions-/Weltpfad, nicht in den Dialog-/GetItem-Pfad.
CALICO = '''0 debug_key F2
1 debug_select snapshot_load 2
2 confirm
10 debug_key F2
11 debug_select give 9
12 confirm
13 debug_select give 12
14 confirm
15 debug_select equip 9
16 confirm
17 debug_key F2
60 position 47 0 10 0
61 confirm
100 confirm
180 confirm
200 inventory
'''


def check(runtime, data, folder):
    def run(name, room, replay, frames, load=False):
        script = folder / (name + '.txt')
        script.write_text(replay)
        state = folder / (name + '.json')
        args = [str(runtime.resolve()), '--data', str(data.resolve()), '--headless',
                '--room', room, '--frames', str(frames), '--replay', str(script),
                '--state', str(state), '--screenshot', str(folder / (name + '.bmp')),
                '--snapshots', str(folder / 'snapshots'), '--validate-poses']
        if not load:
            args.append('--in-game')
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        assert not result['blocked_commands'], (name, result['event_tasks'])
        return result

    def acquired(state, bit):
        return bool(state['item_flags'][bit // 32] & (0x80000000 >> (bit % 32)))

    def released(state):
        assert all(not state[key] for key in
                   ['dialog_open', 'inventory_open', 'cinematic', 'player_scripted', 'player_hidden'])

    question = run('map-question', 'RM_0060.RDX', MAP, 90)
    assert question['dialog_open'] and question['dialog_index'] == 5
    declined = run('map-no', 'RM_0060.RDX', MAP.replace('100 confirm', '100 right\n101 confirm').replace('180 confirm\n', ''), 240)
    assert not acquired(declined, 133) and 5 not in declined['events']
    released(declined)
    taken = run('map-yes', 'RM_0060.RDX', MAP, 240)
    assert acquired(taken, 133) and 5 in taken['events'] and taken['dialog_index'] == 6
    assert taken['items'] == question['items'], 'original map flag must not consume an item slot'
    released(taken)
    # bhControlMessage (message.c:269-285) bewegt mes_sel ausschliesslich mit sys->pad_ps
    # 0x4 und 0x8. Die senkrechten Padbits 0x1/0x2 (pad.c:162-170) liest die Routine nicht,
    # die Auswahlfelder stehen waagerecht (message.c:150). Hoch/Runter darf die Antwort
    # also nicht verschieben: die Frage wird weiter mit Ja quittiert.
    vertical = run('map-vertical', 'RM_0060.RDX',
                   MAP.replace('100 confirm', '100 up\n101 down\n102 up\n103 confirm'), 240)
    assert acquired(vertical, 133) and 5 in vertical['events'], 'vertical pad bits moved the Yes/No selection'
    released(vertical)
    retry = MAP.replace('100 confirm', '100 right\n101 confirm').replace('180 confirm',
            '180 confirm\n220 confirm\n300 confirm')
    retried = run('map-no-retry', 'RM_0060.RDX', retry, 360)
    assert acquired(retried, 133)
    released(retried)
    saved = run('map-save', 'RM_0060.RDX', MAP +
        '250 debug_key F2\n251 debug_select snapshot_save 1\n252 confirm\n253 debug_key F2\n', 270)
    assert saved['debug_status'].startswith('Snapshot 2 gespeichert')
    restored = run('map-restore', 'RM_0060.RDX',
        '0 debug_key F2\n1 debug_select snapshot_load 1\n2 confirm\n60 confirm\n', 90, True)
    assert acquired(restored, 133) and restored['room'] == 'RM_0060.RDX'
    released(restored)
    # Natural door transitions, no debug room reload: the map remains acquired.
    returned = run('map-room-return', 'RM_0060.RDX', MAP +
        '300 position 40 0 0 0\n301 confirm\n650 position 40 0 74 32768\n651 confirm\n', 1000)
    assert returned['room'] == 'RM_0060.RDX' and returned['room_changes'] == 2
    assert acquired(returned, 133)
    released(returned)
    # Diagnostic precondition: the original RM_0070 init chooses returned
    # Calicos only with evflag 116 (Steve's trade complete). This fixture is
    # confined to validation; the runtime never grants campaign flags.
    snapshot = bytearray((folder / 'snapshots/snapshot-2.rcvxsnap').read_bytes())
    assert snapshot[:8] == b'RCVXSNAP' and struct.unpack_from('<II', snapshot, 8) == (1, 0x838)
    base = 16
    at = base + 0x14 + (116 // 32) * 4
    struct.pack_into('<I', snapshot, at, struct.unpack_from('<I', snapshot, at)[0] | (0x80000000 >> (116 % 32)))
    snapshot[base + 0xA] = 7
    snapshot[base + 0xC:base + 0xF] = b'\0\0\0' # spawn, cut, floor
    struct.pack_into('<fffI', snapshot, base + 0x804, 45.0, 0.0, 12.0, 0)
    at = base + 0x838
    length = struct.unpack_from('<I', snapshot, at)[0]
    assert bytes(snapshot[at+4:at+4+length]) == b'RM_0060.RDX'
    snapshot[at+4:at+4+length] = b'RM_0070.RDX'
    struct.pack_into('<I', snapshot, len(snapshot)-4, zlib.crc32(snapshot[:-4]))
    (folder / 'snapshots/snapshot-3.rcvxsnap').write_bytes(snapshot)
    before = run('calico-question', 'RM_0070.RDX', CALICO, 90, True)
    assert before['dialog_open'] and before['inventory_open'] and before['dialog_index'] == 157
    assert sum(bool(v) for v in before['items'][2:]) == 3 and before['weapon'] == 4
    after = run('calico-sorted', 'RM_0070.RDX', CALICO, 210, True)
    assert after['inventory_open'] and not after['dialog_open'] and after['weapon'] == 4
    assert after['items'][2] >> 16 & 255 == 142 and after['items'][0] == 4
    assert after['items'][3:6] == before['items'][2:5]
    assert acquired(after, 136), 'wide weapon pickup did not complete original room acknowledgement'
    print('Original prison-map Yes/No/retry, acquisition flag, snapshot/room-return persistence '
          'and Calico pickup after three narrow slots, row sorting and retained equipment passed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-pickups-') as temporary:
        folder = args.output.resolve() if args.output else Path(temporary)
        folder.mkdir(parents=True, exist_ok=True)
        check(args.runtime, args.data, folder)


if __name__ == '__main__':
    main()
