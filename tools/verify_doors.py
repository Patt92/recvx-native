#!/usr/bin/env python3
"""Verify real door sequences in the running game.

Three things are checked, and none of them is satisfied by a room simply
loading: a door demo requested by an original DoorCall has to run its own
sequence (camera, fade, leaf motion and the door sounds in their original
order), and it has to end in the destination room with the controls back --
only through FadeProc1, since the original demo has no abort path of its own.
The confirm skip is checked separately as what it is: a deliberate host
deviation (see native/doors.h) that the user asked for. It has to shorten the
sequence, reach the same destination, silence the door voices it cuts, and
leave every other channel alone. The direct transition (door type 255, no
demo) is checked separately so the three cannot be confused.
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
    """The room with its own EVT_WORK kept, plus one extra event that issues
    one DoorCall.

    Replacing the whole event block (scd0, scd1 and the Event_init table)
    would drop the room's own initial scenario and scheduler, which set up its
    effects and lights; RM_0020 then shows effect work the room keeps hidden
    and changes from pass to pass, so the frames under the fade would no
    longer show the room as it is. The block is therefore copied unchanged
    behind the file: the offset table gains one entry for the new event, and
    a four byte Event_exec (0x14, level 15, new event) in front of scd0 falls
    through into the room's own scd0. Scripts address only relative to their
    own pc, so the shifted copy runs as the original.
    """
    data = bytearray(source.read_bytes())
    script = bytes([0xF9, 20, 0,
                    0x33, 0, attribute & 0xFF, (attribute >> 8) & 0xFF,
                    stage, room, spawn, door_type,
                    0, 0])
    base = struct.unpack_from('<I', data, 28)[0]
    header = struct.unpack_from('<I', data, base)[0]
    following = [value for value in struct.unpack_from('<12I', data, 16) if value > base]
    end = min(following + [len(data)])
    block = bytes(data[base:end])
    table = struct.unpack_from('<%dI' % (header // 4), block, 0)
    event = len(table) - 2
    prefix = bytes([0x14, 0, 15, event])
    shift = 4 + len(prefix)
    body = prefix + block[header:]
    entries = [header + 4] + [value + shift for value in table[1:]] + [header + 4 + len(body)]
    offset = len(data)
    data += struct.pack('<%dI' % len(entries), *entries) + body + script
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

        def shot(name, frames, replay=''):
            picture = folder / (name + '.bmp')
            if picture.exists():
                picture.unlink()
            value = run(name, frames, replay, extra=['--screenshot', str(picture.resolve())])
            raw = picture.read_bytes()
            offset, width, height = (struct.unpack_from('<I', raw, 10)[0],
                                     struct.unpack_from('<i', raw, 18)[0],
                                     struct.unpack_from('<i', raw, 22)[0])
            stride = (width * 3 + 3) & ~3
            lit = energy = 0
            for y in range(height):
                row = raw[offset + y * stride: offset + y * stride + width * 3]
                for x in range(width):
                    pixel = row[x * 3: x * 3 + 3]
                    if any(pixel):
                        lit += 1
                        energy += sum(pixel)
            value['energy'] = energy
            return value, lit

        # --- bhStartDoorDemo: the fade out before the door task -------------
        # The DoorCall only raises cb_flg 0x1 (bhSetDoorDemo). In the next game
        # pass bhStartDoorDemo case 0 (room.c:949-996) sets bhSetScreenFade
        # (0xFF000000, 10) and enables task 11. bhSysCallDoordemo waits in mode 2
        # until cb_flg 0x2 is clear (system.c:978) and only then stops task 7
        # (ts_flg 0x80, system.c:984); mode 3 clears the OT (:994-1007) and mode 4
        # runs bhSetDoor and the first bhControlDoor pass (:1008-1013). Task 7 keeps
        # drawing the room under the fade (game.c:61-112) until mode 2 stops it.
        # ScreenFade with ct 10: hkn 11, the first control pass only counts
        # (screen.c:56-62), then nine steps of 255/10 and the exact target on the
        # tenth pass (screen.c:76-93).
        sweep = []
        for frames in range(1, 40):
            value, lit = shot('fade-out-%d' % frames, frames)
            sweep.append((frames, value, lit))
            if value['door_ticks']:
                break
        assert sweep[0][1]['door_active'] and not sweep[0][1]['screen_fade']['running'] \
            and sweep[0][1]['door_ticks'] == 0, ('fade-out', 'the DoorCall frame is not the request frame', sweep[0][1]['screen_fade'])
        begin = next((i for i, entry in enumerate(sweep) if entry[1]['screen_fade']['running']), None)
        assert begin is not None, ('fade-out', 'bhStartDoorDemo case 0 never started bhSetScreenFade(0xFF000000, 10)',
                                   [(entry[0], entry[1]['door_ticks']) for entry in sweep])
        fade = sweep[begin][1]['screen_fade']
        assert fade['target'] == 0xFF000000 and fade['an'] == 0, ('fade-out', 'bhStartDoorDemo case 0 did not start its fade', fade)
        end = next((i for i in range(begin, len(sweep)) if not sweep[i][1]['screen_fade']['running']), None)
        assert end is not None, ('fade-out', 'the door task started before cb_flg 0x2 was clear')
        assert end - begin == 9, ('fade-out', 'the fade did not take its ten bhControlScreenFade passes', begin, end)
        for i in range(begin + 1, end):
            an = sweep[i][1]['screen_fade']['an']
            assert abs(an - 25.5 * (i - begin)) < 1e-3, ('fade-out', 'the fade left its 255/10 ramp', i - begin, an)
        assert sweep[end][1]['screen_fade']['an'] == 255, ('fade-out', 'the fade did not end on its target')
        for i in range(0, end + 2):
            assert sweep[i][1]['door_ticks'] == 0, ('fade-out', 'bhControlDoor ran before mode 4', sweep[i][0])
            assert sweep[i][1]['room'] == 'RM_0020.RDX', ('fade-out', 'the room changed during the fade out')
        assert len(sweep) == end + 3 and sweep[end + 2][1]['door_ticks'] == 1, \
            ('fade-out', 'the first door pass is not two frames after the fade (modes 2, 3, 4)',
             [(entry[0], entry[1]['door_ticks']) for entry in sweep])
        # The room stays on screen under the darkening fade while task 7 runs.
        for i in range(0, end):
            assert sweep[i][2] > 0, ('fade-out', 'the room was not drawn under the fade', sweep[i][0])
        for i in range(begin + 1, end):
            assert sweep[i][1]['energy'] < sweep[i - 1][1]['energy'], \
                ('fade-out', 'the frame did not darken with the fade', sweep[i][0])
        assert sweep[end][2] == 0, ('fade-out', 'the frame was not black when the fade reached FF000000')

        first_pass = sweep[end + 2][0]

        def at(passes):
            """Frame count whose last frame runs bhControlDoor pass `passes`."""
            return first_pass + passes - 1

        # --- the fade ramp of the demo ----------------------------------
        # The packet fades out over its first ramp and then holds for a long
        # wait before the second one, so this sample sits inside the ramp:
        # FadeProc1 clears status 0x100 for the whole hold.
        ramp = run('ramp', at(19))
        assert ramp['door_active'], ('ramp', 'the DoorCall started no door demo')
        assert ramp['door_ticks'] >= 10, ('ramp', 'the demo finished in a handful of passes',
                                         ramp['door_ticks'])
        # FadeProc1 is running, so the fade status is up and its rate is inside
        # the open ramp the parameter block walks through.
        assert ramp['door']['status'] & 0x100, ('ramp', 'the fade status was never raised')
        assert 0 < ramp['door']['fade_rate'] < 1, ('ramp', 'the fade rate stands outside its ramp',
                                                   ramp['door']['fade_rate'])
        # The hold after the ramp has not ended, so the second group of sound
        # requests is still held back.
        assert not ramp['door']['status'] & 0x20, ('ramp', 'the fade released the closing sounds early')

        # --- the demo while it runs -------------------------------------
        running = run('running', at(59))
        assert running['door_active'], ('running', 'the DoorCall started no door demo')
        assert running['door_ticks'] > 40, ('running', 'the demo finished in a handful of passes',
                                           running['door_ticks'])
        assert running['room'] == 'RM_0020.RDX', ('running', 'the room changed before the demo ended')
        state = running['door']
        # The fade out is over and the demo is holding before the fade back in.
        assert not state['status'] & 0x100, ('running', 'the fade out never ended')
        assert state['status'] & 0x200, ('running', 'the door stopped being drawn')
        # ViewProc has moved the camera away from the origin, and the door
        # model is on screen.
        assert state['camera'][:3] != [0, 0, 0], ('running', 'the camera never left the origin')
        # LightProc1 put its ambient in and switched its own point lights on.
        assert state['light'] == 1, ('running', 'the packet did not select LightProc1', state['light'])
        assert state['light_on'], ('running', 'the light procedure never ran')
        assert state['points'], ('running', 'no point light was switched on')
        # bhInitDoor leaves the rate at one; DokiProcLgt is not requested here.
        assert state['light_rate'] == [1, 1, 1], ('running', 'the point-light rate was moved without DokiProcLgt',
                                                  state['light_rate'])
        for point in state['points']:
            assert point['range'][1] > point['range'][0], ('running', 'a point light has no falloff range')
            assert any(c > 0 for c in point['color']), ('running', 'a point light came on black')
        # This packet has no shake procedure, so nothing may shake.
        assert state['shake'] == 0 and not state['shake_sounds'] and not state['shake_patterns'], \
            ('running', 'the demo shook without a shake procedure')
        # dmo_clip has no entry for this door, so the gameplay range applies.
        assert not state['clip_case'] and state['clip'] == [2, 20000], \
            ('running', 'the door demo reported another clip range', state['clip'])
        # DoorProc1 has moved a node of the model.
        assert any(node[:6] != [0, 0, 0, 0, 0, 0] for node in state['nodes']), \
            ('running', 'no door node moved')
        sounds = [s for s in running['audio_played'] if s.startswith('DOOR/')]
        assert sounds and sounds[0].endswith('/0000'), ('running', 'no opening door sound', sounds)
        bank = sounds[0].rsplit('/', 1)[0]  # the packet names its own sound bank
        # bhControlDoor holds the second request group until FadeProc1 sets
        # status 0x20, so the closing sound cannot be here yet.
        assert bank + '/0001' not in sounds, ('running', 'the closing sound arrived with the opening one', sounds)

        # --- the demo to its own end -------------------------------------
        done = run('finished', at(400))
        assert done['room'] == 'RM_0010.RDX' and done['room_changes'] == 1, \
            ('finished', 'the demo did not arrive in the destination room', done['room'])
        assert not done['door_active'], ('finished', 'the demo did not end')
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

        # --- the room handover frame (ts_flg 0x80, task 7 still off) --------
        # bhSysCallDoordemo mode 2/10 sets ts_flg 0x100|0x80 (system.c:984,
        # system.c:1044), so neither bhSysCallGame nor bhSysCallEvent runs for
        # the rest of the demo.  The room loader clears both bits only in its
        # very last step, after bhInitEvent: system.c:1886 (bhSysCallMonitor,
        # mn_md0 4 / mn_md1 16; bhInitEvent is mn_md1 10, system.c:1816).
        # bhSysCallMonitor is task 20 and njUserMain walks the table in index
        # order (main.c:195-201), so task 7 does not run in the frame the bit
        # falls.  The next pass runs bhControlEnemy..bhControlLight and only
        # then bhAllDrawModel (game.c:48-74).  The destination room can
        # therefore never be drawn before its first enemy pass -- which is
        # what used to happen here: the frame that loaded the room drew it with
        # the enemy works still in their bhSetEnemy/bhInitEvent pose, so the
        # zombies flashed at a wrong place for one frame.
        handover = None
        for frames in range(at(done['door_ticks']) - 1, at(done['door_ticks']) + 4):
            value, lit = shot('handover-%d' % frames, frames)
            if value['room_changes'] == 1:
                handover = (frames, value, lit)
                break
        assert handover, ('handover', 'the demo never reached its destination room in the sampled window',
                          done['door_ticks'])
        frames, value, lit = handover
        assert value['room'] == 'RM_0010.RDX', ('handover', 'the handover frame is not in the destination room')
        assert value['door_active'], \
            ('handover', 'task 7 was released in the same frame that loaded the room; '
                         'system.c:1886 clears ts_flg 0x80 behind task 7', frames)
        assert lit == 0, \
            ('handover', 'the destination room was drawn in the frame that loaded it, before its '
                         'first bhControlEnemy pass', frames, lit)
        released, lit_after = shot('handover-release', frames + 1)
        assert not released['door_active'], ('handover', 'ts_flg 0x80 was never released', frames + 1)
        assert released['room'] == 'RM_0010.RDX' and released['room_changes'] == 1, \
            ('handover', 'the release frame left the destination room')
        # bhStartDoorDemo case 1 (room.c:997-1009) runs first in the released
        # task 7: bhSetScreenFade(0, 5) from the black fade, fade_pbk 0, then
        # bhControlScreenFade counts its first pass (game.c:38-40). The room is
        # drawn under the fade and comes up in four steps of 255/5.
        fade = released['screen_fade']
        assert fade['running'] and fade['target'] == 0 and fade['an'] == 255 and fade['previous'] == 0, \
            ('handover', 'case 1 did not start the fade in', fade)
        assert lit_after == 0, ('handover', 'the release frame was not covered by the black fade', lit_after)
        previous = 0
        for step in range(1, 5):
            value, lit = shot('fade-in-%d' % step, frames + 1 + step)
            assert value['room_changes'] == 1 and not value['door_active']
            an = value['screen_fade']['an']
            expected = 0 if step == 4 else 255 - 51 * step
            assert abs(an - expected) < 1e-3, ('fade-in', 'the fade in left its 255/5 ramp', step, an)
            assert value['screen_fade']['running'] == (step < 4), ('fade-in', 'cb_flg 0x2 at the wrong pass', step)
            assert lit > 0 and value['energy'] > previous, ('fade-in', 'the destination room did not brighten', step)
            previous = value['energy']

        # --- the host skip (deliberate deviation from the original) ---------
        # BEWUSSTE ABWEICHUNG, NICHT WEGPORTIEREN (Nutzerentscheidung 2026-10-13):
        # the original has no abort path -- bhStartDoorDemo (room.c:978) clears
        # pad_ps/pad_on, bhSysCallDoordemo (system.c:939) reads no pad, and
        # bhControlDoor (door.c:386-528,638-646) only ends on FadeProc1's status
        # 0x40. The confirm skip is a host comfort function the user asked for.
        # It has to reach the same destination with the controls back, and it
        # must not carry a half-played door sound into the destination room.
        skipped = run('skipped', 400, '40 confirm\n')
        assert skipped['door_skipped'], ('skipped', 'the confirm did not skip the demo')
        assert skipped['room'] == 'RM_0010.RDX' and skipped['room_changes'] == 1, \
            ('skipped', 'the skipped demo did not arrive in the destination room')
        assert skipped['door_ticks'] < done['door_ticks'], \
            ('skipped', 'the skip did not shorten the sequence', skipped['door_ticks'], done['door_ticks'])
        assert not skipped['door_active'], ('skipped', 'the skipped demo stayed active')
        assert not any(skipped['door_voices_active']), \
            ('skipped', 'a door voice was still sounding after the skip', skipped['door_voices_active'])
        for field in ['cinematic', 'player_hidden', 'player_scripted']:
            assert not skipped[field], (field, 'the controls were not handed back')

        # A skip while bhStartDoorDemo's fade out still runs: the host skip adds
        # no wait of its own (neither for the fade nor for door task modes 2-4).
        # It ends the transfer in the frame of the confirm, before any door pass.
        early_index = sweep[begin + 3][0]
        early = run('skip-fade-out', early_index + 1, '%d confirm\n' % early_index)
        assert early['door_skipped'] and early['room'] == 'RM_0010.RDX' and early['room_changes'] == 1, \
            ('skip-fade-out', 'the skip during the fade out did not reach the destination at once')
        assert early['door_ticks'] == 0 and not [s for s in early['audio_played'] if s.startswith('DOOR/')], \
            ('skip-fade-out', 'the skip ran a door pass or requested a door sound', early['door_ticks'])
        assert not any(early['door_voices_active']), ('skip-fade-out', 'a door voice survived the skip')

        # The same frame with and without the confirm. Measured: the opening
        # sound of this packet starts around pass 35 and is still sounding on
        # its door voice at frame 41, so the undisturbed run shows a sounding
        # voice there and the skipped one must show none. Without the targeted
        # stop the voice would run on into RM_0010 until Com_ExecRoomFadeIn's
        # 250-tick fade caught it.
        reference = run('skip-reference', at(40) + 1)
        assert reference['door_active'] and any(reference['door_voices_active']), \
            ('skip-reference', 'no door voice was sounding at the sampled frame -- the skip check has nothing to cut',
             reference['door_voices_active'])
        cut = run('skip-cut', at(40) + 1, '%d confirm\n' % at(40))
        assert cut['door_skipped'] and cut['room'] == 'RM_0010.RDX' and cut['room_changes'] == 1, \
            ('skip-cut', 'the skip did not reach the destination room in the sampled frame')
        assert not any(cut['door_voices_active']), \
            ('skip-cut', 'the skip let a door voice sound into the destination room', cut['door_voices_active'])
        # The door sound that was cut keeps its provenance entry; only the voice ends.
        assert [s for s in cut['audio_played'] if s.startswith('DOOR/')] == [bank + '/0000'], \
            ('skip-cut', 'the skip changed which door sounds were requested',
             [s for s in cut['audio_played'] if s.startswith('DOOR/')])

        # --- the heartbeat light and the heartbeat shake ---------------------
        # dmo_atr 0x20 runs DokiProcLgt, 0x40 replaces the packet's shake by
        # DkiPru. Both ride on the same original DoorCall, only the attribute
        # differs, so the comparison isolates the two procedures.
        door_call_room(args.data / 'rooms/RM_0020.RDT', mods / 'rooms/RM_0020.RDT',
                       attribute=0x60, stage=0, room=1, spawn=0, door_type=7)
        doki = run('doki', at(59))
        shaken = doki['door']
        assert shaken['status'] & 0x40000000, ('doki', 'the heartbeat light status is missing')
        assert shaken['status'] & 0x20000000, ('doki', 'the heartbeat shake status is missing')
        # DokiProcLgt drives the rate off the steady one and lifts the ambient
        # offset into the 0.4 clamp of LightSubAmb.
        assert shaken['light_rate'] != [1, 1, 1], ('doki', 'DokiProcLgt never moved the point-light rate')
        assert abs(shaken['ambient'][0] - 0.4) < 1e-5, ('doki', 'the heartbeat ambient did not reach the clamp',
                                                        shaken['ambient'])
        assert abs(state['ambient'][0] - 0.1) < 1e-5, ('running', 'the steady ambient is not the packet value',
                                                       state['ambient'])
        # The same point light, now scaled by the pulsing rate.
        assert shaken['points'] and shaken['points'][0]['color'] != state['points'][0]['color'], \
            ('doki', 'the heartbeat rate did not reach the point-light colour')
        # DkiPru: pattern 13 with PruSndTbl sound 10, one request every 21
        # passes from the first one.
        assert shaken['shake'] == 1, ('doki', 'the heartbeat shake procedure was not selected')
        assert shaken['shake_sounds'] >= 2, ('doki', 'the heartbeat shake requested too few sounds',
                                             shaken['shake_sounds'])
        assert shaken['shake_patterns'] == 2 * shaken['shake_sounds'], \
            ('doki', 'the heartbeat shake lost a vibration pattern')
        assert shaken['shake_sounds'] == 1 + (doki['door_ticks'] - 1) // 21, \
            ('doki', 'the heartbeat shake lost its interval', shaken['shake_sounds'], doki['door_ticks'])
        assert 'COMMON/0010' in doki['audio_played'], ('doki', 'PruSndTbl sound 10 was not played',
                                                       [s for s in doki['audio_played'] if s.startswith('COMMON/')])
        # StartVibrationEx(2, 13) now reaches the vibration unit: every request
        # passes its gate; without a controller nothing starts.
        assert doki['vibration_requests'] == shaken['shake_patterns'], \
            ('doki', 'the vibration requests did not reach the vibration unit',
             doki['vibration_requests'], shaken['shake_patterns'])
        # The skip stops the door voices only. The heartbeat shake gives this
        # fixture a second sounding channel: its PruSndTbl sound starts on the
        # system SE slot at pass 44 and is still sounding at frame 45, while the
        # door voice of the opening sound is sounding too. A skip there has to
        # silence the door voice and leave the system SE alone -- a blanket
        # "stop everything" (BGM, ambient, other voices) would fail this.
        doki_reference = run('doki-skip-reference', at(44) + 1)
        assert any(doki_reference['door_voices_active']) and doki_reference['system_sound_active'][0], \
            ('doki-skip', 'the sampled frame has no door voice and no system sound to tell apart',
             doki_reference['door_voices_active'], doki_reference['system_sound_active'])
        doki_cut = run('doki-skip', at(44) + 1, '%d confirm\n' % at(44))
        assert doki_cut['door_skipped'] and doki_cut['room_changes'] == 1, \
            ('doki-skip', 'the skip did not run')
        assert not any(doki_cut['door_voices_active']), \
            ('doki-skip', 'the skip left a door voice sounding', doki_cut['door_voices_active'])
        assert doki_cut['system_sound_active'] == doki_reference['system_sound_active'], \
            ('doki-skip', 'the skip cut a channel that is not a door voice',
             doki_cut['system_sound_active'], doki_reference['system_sound_active'])

        # The attribute changes the two procedures, not the sequence itself.
        done_doki = run('doki-finished', at(400))
        assert done_doki['room'] == 'RM_0010.RDX' and done_doki['door_ticks'] == done['door_ticks'], \
            ('doki', 'the heartbeat attribute changed the length of the sequence',
             done_doki['door_ticks'], done['door_ticks'])
        door_call_room(args.data / 'rooms/RM_0020.RDT', mods / 'rooms/RM_0020.RDT',
                       attribute=0, stage=0, room=1, spawn=0, door_type=7)

        # --- a direct transition has no demo at all -------------------------
        # bhSysCallDoordemo mode 0 (system.c:949): dor_tp 255 goes straight to
        # mode 10, so the same original DoorCall with door type 255 loads the
        # destination without a demo, without door sounds and without a pass of
        # bhControlDoor. (The earlier fixture walked RM_0010 spawn 1 into its
        # stair door, which runs a real demo; it only looked direct because the
        # host skip cut that demo at its first pass.)
        door_call_room(args.data / 'rooms/RM_0020.RDT', mods / 'rooms/RM_0020.RDT',
                       attribute=0, stage=0, room=1, spawn=0, door_type=255)
        # Mode 10 waits for the same fade (system.c:1027) before the room loads.
        direct_mid = run('direct-fade', sweep[begin + 4][0])
        assert direct_mid['room'] == 'RM_0020.RDX' and direct_mid['screen_fade']['running'], \
            ('direct', 'a type 255 transition did not wait for the fade out', direct_mid['room'])
        direct = run('direct', 60)
        assert direct['room'] == 'RM_0010.RDX' and direct['room_changes'] == 1, \
            ('direct', 'the direct transition did not change the room')
        assert direct['door_ticks'] == 0 and not direct['door_active'], \
            ('direct', 'a door type 255 transition ran a demo', direct['door_ticks'])
        assert not [s for s in direct['audio_played'] if s.startswith('DOOR/')], \
            ('direct', 'a direct transition played door sounds')
        assert not direct['player_hidden'] and not direct['blocked_commands']
        door_call_room(args.data / 'rooms/RM_0020.RDT', mods / 'rooms/RM_0020.RDT',
                       attribute=0, stage=0, room=1, spawn=0, door_type=7)

        # --- ordinary movement keeps its own footsteps ----------------------
        walked = run('walk', 240, '0 keys forward\n60 keys back\n120 keys forward run\n180 keys none\n',
                     use_mods=False, room='RM_0010.RDX', extra=['--spawn', '1'])
        assert any(s.startswith('MULTSPQ1/0001/2/') for s in walked['audio_played']), \
            ('walk', 'hall movement has no original footsteps')
        assert not walked['door_active'] and not walked['room_changes'], \
            ('walk', 'walking started a transition')

    print('Original door demo verified: DoorCall runs its own sequence with the door sounds in order and '
          'arrives in the destination room with the controls back. The host skip (deliberate deviation, '
          'see native/doors.h) shortens it to the same destination, silences the door voices it cuts and '
          'leaves the other channels sounding; a type 255 transition still runs no demo.')


if __name__ == '__main__':
    main()
