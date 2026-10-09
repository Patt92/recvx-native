#!/usr/bin/env python3
"""Verify real door sequences in the running game.

Three things are checked, and none of them is satisfied by a room simply
loading: a door demo requested by an original DoorCall has to run its own
sequence (camera, fade, leaf motion and the door sounds in their original
order), it has to end in the destination room with the controls back, and the
confirm skip has to cut it short and still arrive at the same place. The
direct transition (door type 255, no demo) is checked separately so the two
cannot be confused.
"""
import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def door_call_room(source: Path, destination: Path, attribute: int, stage: int,
                   room: int, spawn: int, door_type: int):
    """Room with an extra event table whose scan script issues one DoorCall."""
    data = bytearray(source.read_bytes())
    offset = len(data)
    script = bytes([0xF9, 20, 0,
                    0x33, 0, attribute & 0xFF, (attribute >> 8) & 0xFF,
                    stage, room, spawn, door_type,
                    0, 0])
    data += struct.pack('<III', 12, 17, 18) + bytes([0x14, 0, 0, 0, 0, 0]) + script
    struct.pack_into('<I', data, 28, offset)
    destination.write_bytes(bytes(data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', help='Optional executable such as wine64')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []

    with tempfile.TemporaryDirectory(prefix='recvx-door-') as temporary:
        folder = args.output if args.output else Path(temporary)
        folder.mkdir(parents=True, exist_ok=True)
        mods = Path(temporary) / 'mods'
        (mods / 'rooms').mkdir(parents=True)
        # RM_0020 keeps its own data; only the event table is replaced so the
        # DoorCall of the original opcode runs. The destination is the room the
        # stairs lead back to, with the cell door packet (sound bank 7).
        door_call_room(args.data / 'rooms/RM_0020.RDT', mods / 'rooms/RM_0020.RDT',
                       attribute=0, stage=0, room=1, spawn=0, door_type=7)

        def run(name, frames, replay='', use_mods=True, room='RM_0020.RDX', extra=()):
            state = folder / (name + '.json')
            script = folder / (name + '.txt')
            script.write_text(replay)
            if state.exists():
                state.unlink()
            command = prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve())]
            if use_mods:
                command += ['--mods', str(mods)]
            command += ['--headless', '--in-game', '--room', room, '--frames', str(frames),
                        '--replay', str(script.resolve()), '--state', str(state.resolve())]
            command += list(extra)
            with (folder / (name + '.log')).open('w') as log:
                done = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            if done.returncode:
                raise RuntimeError((folder / (name + '.log')).read_text())
            value = json.loads(state.read_text())
            assert not value['blocked_commands'], (name, 'blocked event commands')
            return value

        # --- the demo while it runs -------------------------------------
        running = run('running', 60)
        assert running['door_active'], ('running', 'the DoorCall started no door demo')
        assert running['door_ticks'] > 20, ('running', 'the demo finished in a handful of passes',
                                            running['door_ticks'])
        assert running['room'] == 'RM_0020.RDX', ('running', 'the room changed before the demo ended')
        assert not running['door_skipped'], ('running', 'the demo reported a skip')
        sounds = [s for s in running['audio_played'] if s.startswith('DOOR/')]
        assert sounds and sounds[0].endswith('/0000'), ('running', 'no opening door sound', sounds)
        bank = sounds[0].rsplit('/', 1)[0]  # the packet names its own sound bank
        # bhControlDoor holds the second request group until FadeProc1 sets
        # status 0x20, so the closing sound cannot be here yet.
        assert bank + '/0001' not in sounds, ('running', 'the closing sound arrived with the opening one', sounds)

        # --- the demo to its own end -------------------------------------
        done = run('finished', 400)
        assert done['room'] == 'RM_0010.RDX' and done['room_changes'] == 1, \
            ('finished', 'the demo did not arrive in the destination room', done['room'])
        assert not done['door_active'] and not done['door_skipped'], ('finished', 'the demo did not end')
        assert done['door_ticks'] > 100, ('finished', 'the sequence was far shorter than a door demo',
                                          done['door_ticks'])
        # door.c requests the opening sound, the leaf sound and -- only after
        # FadeProc1 released the deferred group -- the closing one.
        sounds = [s for s in done['audio_played'] if s.startswith('DOOR/')]
        assert len(sounds) >= 2 and all(s.startswith(bank + '/') for s in sounds), \
            ('finished', 'the door sounds did not all come from the packet bank', sounds)
        assert sounds[0] == bank + '/0000' and sounds[-1] == bank + '/0001', \
            ('finished', 'the door sounds did not arrive in their original order', sounds)
        for field in ['cinematic', 'player_hidden', 'player_scripted', 'dialog_open', 'movie_active']:
            assert not done[field], ('finished', field, 'the controls were not handed back')

        # --- the confirm skip ---------------------------------------------
        skipped = run('skipped', 400, '40 confirm\n')
        assert skipped['door_skipped'], ('skipped', 'the confirm did not skip the demo')
        assert skipped['room'] == 'RM_0010.RDX' and skipped['room_changes'] == 1, \
            ('skipped', 'the skipped demo did not arrive in the destination room')
        assert skipped['door_ticks'] < done['door_ticks'], \
            ('skipped', 'the skip did not shorten the sequence', skipped['door_ticks'], done['door_ticks'])
        assert not skipped['door_active'], ('skipped', 'the skipped demo stayed active')
        for field in ['cinematic', 'player_hidden', 'player_scripted']:
            assert not skipped[field], ('skipped', field, 'the controls were not handed back')

        # --- a direct transition has no demo at all -------------------------
        stairs = run('stairs', 180, use_mods=False, room='RM_0010.RDX',
                     extra=['--spawn', '1', '--replay', str(ROOT / 'tests/native/replays/first-stairs.txt')])
        assert stairs['room'] == 'RM_0020.RDX' and stairs['room_changes'] == 1, \
            ('stairs', 'the direct transition did not change the room')
        assert stairs['door_ticks'] == 1 and not stairs['door_active'], \
            ('stairs', 'a door type 255 transition ran a demo', stairs['door_ticks'])
        assert not [s for s in stairs['audio_played'] if s.startswith('DOOR/')], \
            ('stairs', 'a direct transition played door sounds')
        assert not stairs['player_hidden'] and not stairs['blocked_commands']

        # --- ordinary movement keeps its own footsteps ----------------------
        walked = run('walk', 240, '0 keys forward\n60 keys back\n120 keys forward run\n180 keys none\n',
                     use_mods=False, room='RM_0010.RDX', extra=['--spawn', '1'])
        assert any(s.startswith('MULTSPQ1/0001/2/') for s in walked['audio_played']), \
            ('walk', 'hall movement has no original footsteps')
        assert not walked['door_active'] and not walked['room_changes'], \
            ('walk', 'walking started a transition')

    print('Original door demo verified: DoorCall runs its own sequence with the door sounds in order, '
          'arrives in the destination room with the controls back, the confirm skip shortens it to the '
          'same destination, and a type 255 transition still runs no demo.')


if __name__ == '__main__':
    main()
