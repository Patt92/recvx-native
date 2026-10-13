#!/usr/bin/env python3
"""Replay original detector/box scripts with diagnostic trigger placement.

This checks script handover and the real input path, not a complete playthrough.
Resource tests in recvx-progression-tests also cover the original fire atlases,
segmented zombie motions, Cerberus activation and non-looping Steve music.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPLAY = '''0 debug_key F2
1 debug_select give 5
2 confirm
3 debug_key F2
60 position 34.5 0 5 0
100 position 44.5 0 5 0
180 confirm
220 confirm
320 position 44.5 0 10 0
321 confirm
350 right
351 confirm
354 confirm
360 up
361 confirm
362 down
366 confirm
370 cancel
400 position 34.5 0 5 0
420 position 44.5 0 5 0
560 position 44.5 0 10 0
561 confirm
590 right
591 confirm
594 confirm
600 up
601 confirm
602 down
606 confirm
610 cancel
'''


def check(runtime, data, folder, prefix):
    replay = folder / 'security-path.txt'
    replay.write_text(REPLAY)
    for name, frames in [('announcement', 110), ('handover', 270),
                         ('box', 364), ('deposit-pass', 550), ('withdraw', 680)]:
        state = folder / (name + '.json')
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run(prefix + [str(runtime.resolve()), '--data', str(data.resolve()),
                '--headless', '--in-game', '--room', 'RM_0090.RDX', '--frames', str(frames),
                '--replay', str(replay), '--state', str(state)], cwd=ROOT,
                stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        assert not result['blocked_commands'], (name, result['event_tasks'])
        assert result['audio_played'].count('VOICE1/0455') == 1, (name, 'announcement missing/restarted')
        if name == 'announcement':
            # Original message 16 is FF02/1/FFFF (one-tick empty message).
            # The announcement is VOICE1/0455, not an invented subtitle.
            assert result['dialog_index'] == 16, 'original message not dispatched'
            assert result['fixed_camera'] == 9 and result['player_hidden'], result
        elif name == 'box':
            assert result['box_open'] and result['inventory_open'] and result['box_base'] == 224, result
        else:
            for field in ['player_hidden', 'player_scripted', 'cinematic', 'dialog_open', 'inventory_open', 'box_open']:
                assert not result[field], (name, field)
            assert result['fixed_camera'] == -1, name
        # sub1.c ItemBox testmode 2 scrollt die Kistenliste mit +/-19 px je Bild
        # und schiebt den Index erst nach 38/|number| Bildern vor; Bestaetigen
        # ist waehrenddessen gesperrt (7606). Der Lighter liegt deshalb schon bei
        # Bild 364 in der Box, die Waffe erst danach (Replay: down 362, confirm 366).
        if name == 'box':
            assert result['security_items_2'][:1] == [327681], (name, 'lighter deposit')
        if name == 'deposit-pass':
            assert result['security_items_2'][:2] == [327681, 3604481], (name, 'gun/lighter deposit')
            assert result['items'][1] == result['items'][3] == 0, name
        if name == 'withdraw':
            assert result['items'][1] == 3604481 and result['items'][3] == 327681, 'withdrawal'
            assert not any(result['security_items_2']), 'storage not emptied'
    # ItemBox: taking a box item onto the STANDARD cell is the original swap via
    # pip[1] (ItemBoxChange 0x002A5834 keeps chk2 = 1 and routes the pocket entry
    # through lt_c3/lt_c2; the decompiled listing wrongly rejects it). One back
    # press returns to the item cursor, Up from the STANDARD cell reaches EXIT,
    # and EXIT closes through StatusCancel.
    wrong = folder / 'wrong-slot.txt'
    wrong.write_text(REPLAY.split('321 confirm')[0] + '321 confirm\n340 confirm\n345 cancel\n350 up\n355 confirm\n360 confirm\n'
                     '370 down\n372 right\n374 confirm\n376 confirm\n380 up\n382 up\n384 confirm\n')
    # 378: right after the STANDARD-cell exchange. 398: the EXIT press and
    # SpriteH's nine closing ticks have completed (384 press, 388 StatusCancel).
    # The room script re-requests the box afterwards, because the
    # player still stands on its trigger; the walk-away close is covered above.
    for frames, closed in [(378, False), (398, True)]:
        state = folder / ('wrong-slot-%d.json' % frames)
        with (folder / ('wrong-slot-%d.log' % frames)).open('w') as log:
            subprocess.run(prefix + [str(runtime.resolve()), '--data', str(data.resolve()),
                '--headless', '--in-game', '--room', 'RM_0090.RDX', '--frames', str(frames),
                '--replay', str(wrong), '--state', str(state)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        # ItemBoxChange 0x002A5834: taking the standard weapon (lighter 3604481)
        # from the box onto a pocket cell routes it through pip[1] and sends that
        # cell's item (handgun 327681) to storage. The decompiled listing refuses
        # this instead (chk2 = 0, message 159), which would leave the handgun in
        # the pocket and storage empty.
        pocket, storage = result['items'][1:12], result['security_items_2']
        assert result['items'][1] == 3604481, ('standard cell', frames, result['items'][:4])
        assert [v for v in storage if v] == [327681], ('standard-cell storage', frames, storage[:4])
        assert not any(v == 327681 for v in pocket), ('handgun duplicated', frames, result['items'][:12])
        owned = sorted(v for v in list(pocket) + list(storage) if v)
        assert owned == [327681, 3604481, 9502721], ('conservation', frames, owned)
        assert result['box_open'] != closed, ('EXIT', frames)
        if closed:
            assert not result['inventory_open'] and not result['work_flags'][10] & 0x1c0000, ('EXIT release', frames)
    print('Original detector announcement/camera, control handover, box deposit, pass, withdrawal and the STANDARD-cell swap verified. Trigger positions are diagnostic; box drawing is a separate visual check.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-progression-') as temporary:
        folder = args.output.resolve() if args.output else Path(temporary)
        folder.mkdir(parents=True, exist_ok=True)
        check(args.runtime, args.data, folder, [args.emulator] if args.emulator else [])


if __name__ == '__main__':
    main()
