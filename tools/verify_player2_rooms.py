#!/usr/bin/env python3
"""Companion room leash through real native doors and explicit diagnostic setups.

Positions are replay diagnostics. The event fixture appends DoorCall to the
original room events, using verify_doors.py; this is not a playthrough claim.
Off-room NPCs are resident records, not a second running room simulation.
"""
import argparse
import json
import struct
from pathlib import Path
import subprocess

from verify_doors import door_call_room


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)

    def run(name, script='', frames=32, mode='npc', mods=None, picture=False):
        replay, state = out / f'{name}.txt', out / f'{name}.json'
        replay.write_text(script)
        state.unlink(missing_ok=True)
        command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                   '--mods', str(mods or out / 'no-mods'), '--user-dir', str(out / 'user'),
                   '--headless', '--in-game', '--room', 'RM_0080.RDX',
                   '--player2', mode, '--frames', str(frames), '--ss', '1',
                   '--renderer', 'software', '--replay', str(replay), '--state', str(state)]
        if picture:
            shot = out / f'{name}.bmp'
            shot.unlink(missing_ok=True)
            command += ['--screenshot', str(shot)]
        with (out / f'{name}.log').open('w') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=45)
        return json.loads(state.read_text())

    first_door = '0 position -36.15 0 49.5252 16384\n5 confirm\n6 confirm\n'
    far = '0 player2_position -10 0 33 0\n' + first_door
    behind = run('one-behind', far, frames=15, picture=True)
    assert behind['room'] == 'RM_0050.RDX' and behind['player2']['room'] == 'RM_0050.RDX'
    assert behind['player2']['present'] and behind['player2']['room_distance'] == 0
    assert behind['player2']['teleports'] == 0 and not behind['trade_open']
    held = run('absent-held', far, frames=60)
    assert held['player2']['present'] and held['player2']['travel']=='door-follow', 'distant NPC did not arrive through the used door'
    near = run('near-escort', first_door, frames=15)
    assert near['player2']['present'] and near['player2']['room'] == near['room']
    assert near['player2']['travel'] == 'door-follow' and near['player2']['teleports'] == 0
    second_door = '20 position -165.5 8 83 0\n25 confirm\n26 confirm\n'
    caught = run('room-limit', far + second_door, picture=True)
    assert caught['room'] == 'RM_0060.RDX' and caught['player2']['room'] == caught['room']
    assert caught['player2']['present'] and caught['player2']['room_distance'] == 0
    assert caught['player2']['teleports'] == 0 and caught['player2']['travel'] == 'door-follow'
    returned = run('return', far + '20 position -35 0 49.5252 16384\n25 confirm\n26 confirm\n', frames=27)
    assert returned['room'] == 'RM_0080.RDX' and returned['player2']['present']
    assert returned['player2']['travel']=='door-follow' and returned['player2']['teleports'] == 0
    human = run('human', far, frames=15, mode='local')
    assert human['player2']['present'] and human['player2']['travel'] == 'local-follow'

    independent = '0 player2_position -26 0 49.5252 16384\n0 player2_door 0\n'
    own = run('independent', independent, frames=60, picture=True)
    baseline = run('baseline', frames=60)
    assert own['room'] == 'RM_0080.RDX' and own['player2']['room'] == 'RM_0050.RDX'
    assert not own['player2']['present'] and own['player2']['room_distance'] == 1
    assert own['player2']['travel'] == 'independent-door' and own['player2']['teleports'] == 0
    for field in ('actor', 'yaw', 'hp', 'items', 'work_flags', 'item_flags', 'event_flags', 'room_changes', 'door_active'):
        assert own[field] == baseline[field], ('NPC door changed primary progression', field)
    together = run('independent-rejoin', independent + '60 position -36.15 0 49.5252 16384\n65 confirm\n66 confirm\n', frames=67)
    assert together['room'] == 'RM_0050.RDX' and together['player2']['present']
    assert together['player2']['teleports'] == 0 and together['player2']['travel'] == 'rejoined'

    mods = out / 'event-mods'
    (mods / 'rooms').mkdir(parents=True, exist_ok=True)
    fixture = mods / 'rooms/RM_0080.RDT'
    door_call_room(args.data.resolve() / 'rooms/RM_0080.RDT', fixture, 0, 0, 6, 1, 2)
    # Delay the appended diagnostic event until the real incapacity clip has
    # finished. Keep the original init and scd1 body. A diagnostic bhSet prefix
    # enables its scheduler (sp_flg 0x10), otherwise this room parks that task.
    data = bytearray(fixture.read_bytes())
    assert data[-13] == 0xF9
    # bhSleep must enter through F8 and initialize its loop counter; F9 alone
    # is only the continuation opcode (event.c bhSleep, EventVm 0xf8/0xf9).
    data[-13:] = bytes([0xF8, 0xF9, 90, 0]) + data[-10:]
    base = struct.unpack_from('<I', data, 28)[0]
    header = struct.unpack_from('<I', data, base)[0]
    offsets = struct.unpack_from(f'<{header // 4}I', data, base)
    start = base + offsets[1]
    end = base + min(offset for offset in offsets if offset > offsets[1])
    scan = bytes([5, 6, 27, 0, 0, 0]) + data[start:end]
    struct.pack_into('<I', data, base + 4, len(data) - base)
    data += scan
    fixture.write_bytes(data)
    event = run('event', '0 player2_position -10 0 33 0\n92 confirm\n', frames=105, mods=mods)
    assert event['room'] == 'RM_0060.RDX' and event['player2']['present']
    assert event['player2']['travel'] == 'event-teleport' and event['player2']['teleports'] == 1
    down_before = run('down-before', '0 player2_damage 99999\n', frames=70)
    down = run('down-event', '0 player2_damage 99999\n92 confirm\n', frames=105, mods=mods, picture=True)
    assert down['room'] == 'RM_0060.RDX' and down['player2']['present']
    assert down['player2']['hp'] == 1 and down['player2']['life'] == 2 and down['player2']['teleports'] == 1
    for field in ('motion', 'frame', 'items', 'attacks', 'hits'):
        assert down['player2'][field] == down_before['player2'][field], ('teleport changed immutable NPC', field)
    down_limit = run('down-limit', '0 player2_damage 99999\n' + first_door + second_door, frames=90)
    assert down_limit['player2']['present'] and down_limit['player2']['room'] == down_limit['room'] == 'RM_0060.RDX'
    assert down_limit['player2']['life'] == 2 and down_limit['player2']['hp'] == 1 and down_limit['player2']['teleports'] == 1
    view = run('down-event-view', '0 player2_damage 99999\n92 confirm\n100 position 46.5 0 25.5436 32768\n', frames=105, mods=mods, picture=True)
    assert view['player2']['position'] == down['player2']['position'] and view['player2']['hp'] == 1
    menu = run('door-menu', '0 debug_key F2\n1 debug_select player2_door 0\n', frames=3, picture=True)
    assert menu['debug_open']
    print('Companion rooms runtime: actual independent door, one-room hold, backtrack/rejoin, second-door catch-up, nearby escort, scripted transport and downed 1-HP pose preservation PASS')


if __name__ == '__main__':
    main()
