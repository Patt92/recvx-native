#!/usr/bin/env python3
"""Verify the original stair, ladder and step actions in the running game.

Nothing here is satisfied by a room simply loading.  Three independent pieces
are checked:

  * the shipped room set has no reachable ladder area and no ladder motion, so
    the two ladder routines (hsu/hsd) can only be run in the component test;
  * the script path (EventVm operand 0xa4, bhPlayerKaidanMotion) starts a real
    descent in RM_1120/RM_1121 with no input at all, walks the figure down
    through its own motion, changes the floor, plays the original step sounds
    and hands control back;
  * the interaction path (Confirm on a type-1/type-2 area) runs the ascent,
    descent and both step variants.  Those four runs use a diagnostic replay
    placement - the player is put in front of the area instead of walking
    there - and are marked as such.

The dnd trace additionally shows that the release frame moves the player by the
ordinary distance and that the full-frame change belongs to the follow camera
switching after the floor change, not to a teleport.
"""
import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

AREA_FIELDS = ('index', 'flags', 'type', 'floor', 'attr', 'position', 'extent', 'parameters')


def atr_entries(path: Path):
    """Interaction records of one room file (events.cpp layout, 36 bytes each)."""
    data = path.read_bytes()
    base = struct.unpack_from('<I', data, 16)[0]
    count = struct.unpack_from('<I', data, base + 0x9c)[0]
    offset = struct.unpack_from('<I', data, base + 0x1c)[0]
    out = []
    for index in range(count):
        entry = offset + index * 36
        flags, kind, _, floor = struct.unpack_from('<BBBb', data, entry)
        attribute = struct.unpack_from('<I', data, entry + 4)[0]
        px, py, pz, w, h, d = struct.unpack_from('<6f', data, entry + 8)
        out.append(dict(zip(AREA_FIELDS, (index, flags, kind, floor, attribute,
                                          (px, py, pz), (w, h, d),
                                          tuple(data[entry + 32:entry + 36])))))
    return out


def wall_entries(path: Path):
    """Collision ATR records of one room file (rom->walp, 36 bytes each)."""
    data = path.read_bytes()
    base = struct.unpack_from('<I', data, 16)[0]
    count = struct.unpack_from('<I', data, base + 0x98)[0]
    offset = struct.unpack_from('<I', data, base + 0x18)[0]
    out = []
    for index in range(count):
        entry = offset + index * 36
        flags, kind, _, floor = struct.unpack_from('<BBBb', data, entry)
        attribute = struct.unpack_from('<I', data, entry + 4)[0]
        px, py, pz, w, h, d = struct.unpack_from('<6f', data, entry + 8)
        out.append(dict(zip(AREA_FIELDS, (index, flags, kind, floor, attribute,
                                          (px, py, pz), (w, h, d),
                                          tuple(data[entry + 32:entry + 36])))))
    return out


def blocking_walls(walls, position, floor, flg, stflg, radius=3.5, height=16.5, room_height=10000.0):
    """ATR records that bhCheckWallEx would resolve at `position`.

    Types 0/1 only - that is what guards the stairs in RM_0050 - with the
    original filter set of hitchk.c:705: type 1 needs plp->flg 0x400 clear (it
    is, once the stair action released the player), attr 0x1 restricts the
    record to its own floor, attr 0x2 drops out with plp->flg 0x4000 and attr
    0x4 with plp->stflg 0x40000000.  The body spans py .. py + h, h == 0
    meaning rom->h.
    """
    x, y, z = position
    hits = []
    for wall in walls:
        if not wall['flags'] & 1 or wall['type'] not in (0, 1):
            continue
        if wall['type'] & 1 and flg & 0x400:
            continue
        if wall['attr'] & 1 and wall['floor'] != floor:
            continue
        if wall['attr'] & 2 and flg & 0x4000:
            continue
        if wall['attr'] & 4 and stflg & 0x40000000:
            continue
        wx, wy, wz = wall['position']
        ww, wh, wd = wall['extent']
        wh = wh or room_height
        if not (0 <= x - (wx - radius) < ww + 2 * radius):
            continue
        if not (0 <= z - (wz - radius) < wd + 2 * radius):
            continue
        if y + height >= wy and y <= wy + wh:
            hits.append(wall['index'])
    return hits


def player_motion_sizes(path: Path):
    """Sizes of the motion slots of one SYSTEM/00xx player asset."""
    data = path.read_bytes()
    if len(data) < 8:
        return []
    model_end = 4 + struct.unpack_from('<I', data, 0)[0]
    if model_end + 4 > len(data):
        return []
    motion_end = model_end + 4 + struct.unpack_from('<I', data, model_end)[0]
    sizes, offset = [], model_end + 4
    while offset + 4 <= motion_end:
        size = struct.unpack_from('<I', data, offset)[0]
        if size == 0xFFFFFFFF:
            break
        sizes.append(size)
        offset += 4 + size
    return sizes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', help='Optional executable such as wine64')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []

    failures = []

    def check(condition, why):
        if not condition:
            failures.append(why)
            print(f'  FAIL {why}')
        else:
            print(f'  ok   {why}')

    def close(value, target, tolerance, why):
        check(abs(value - target) <= tolerance, f'{why} ({value} vs {target})')

    # --- shipped data ---------------------------------------------------
    print('shipped rooms')
    rooms = sorted((args.data / 'rooms').glob('*.RDT'))
    stairs = steps = ladders = 0
    for room in rooms:
        for area in atr_entries(room):
            if area['type'] == 1:
                if area['attr'] & 1:
                    ladders += 1
                else:
                    stairs += 1
            elif area['type'] == 2:
                steps += 1
    check(len(rooms) == 205, f'all 205 original rooms are present ({len(rooms)})')
    check(stairs == 150, f'150 type-1 stairs areas carry attr&1 == 0 ({stairs})')
    check(ladders == 0, f'no type-1 area selects the ladder routines ({ladders})')
    check(steps == 87, f'87 type-2 step areas ({steps})')

    print('player assets')
    assets = sorted((args.data / 'archives/SYSTEM').glob('001[0-4].bin'))
    check(len(assets) == 5, f'five player bodies ({len(assets)})')
    # hsu requests 62/63/64 and hsd 65/66/67 (native/player_stairs.c); no other
    # routine in this package reads a slot in that range.
    ladder_slots = tuple(range(62, 68))
    for asset in assets:
        sizes = player_motion_sizes(asset)
        empty = [i for i in ladder_slots if i >= len(sizes) or sizes[i] == 0]
        check(empty == list(ladder_slots),
              f'{asset.name}: the ladder motions {ladder_slots[0]}..{ladder_slots[-1]} are empty')

    with tempfile.TemporaryDirectory(prefix='recvx-stairs-') as temporary:
        folder = args.output if args.output else Path(temporary)
        folder.mkdir(parents=True, exist_ok=True)
        mods = Path(temporary) / 'mods'  # deliberately absent: the shipped rooms only

        def run(name, room, frames, replay=''):
            state = folder / f'{name}.json'
            if state.exists():
                raise SystemExit(f'{state} already exists; the runtime refuses to overwrite it')
            script = folder / f'{name}.txt'
            script.write_text(replay)
            command = prefix + [str(args.runtime), '--data', str(args.data),
                                '--mods', str(mods), '--room', room, '--headless',
                                '--in-game', '--frames', str(frames),
                                '--replay', str(script), '--state', str(state)]
            done = subprocess.run(command, capture_output=True, text=True)
            if done.returncode:
                raise SystemExit(f'{name}: runtime failed\n{done.stderr[-800:]}')
            return json.loads(state.read_text())

        def summary(state):
            stairs_state = state.get('stairs') or {}
            return dict(stairs_state, blocked=state['blocked_commands'],
                        actor=state['actor'], yaw=state['yaw'],
                        camera_pose=state['camera_pose'],
                        flg=state['work_flags'][13], status=state['work_flags'][14],
                        st_flg=state['work_flags'][5])

        # --- script path -------------------------------------------------
        # bhPlayerKaidanMotion is reachable without any input; native/input.cpp
        # only calls the interaction hook on a Confirm press, so an action that
        # starts in a head-less run with no replay must have come from op 0xa4.
        for room in ('RM_1120.RDX', 'RM_1121.RDX'):
            tag = room[:7]
            print(f'script path {room} (no input at all)')
            first = summary(run(f'{tag}-1', room, 1))
            check(first['active'] and first['mode2'] == 15 and first['mode3'] == 1,
                  f'{room} frame 1 starts kdd in its original sub state')
            check(first['motion'] == 39 and first['floor'] == 1,
                  f'{room} frame 1 runs motion 39 on floor 1')
            close(first['actor'][1], 20.0, 0.001, f'{room} frame 1 stands on the original height')

            walking = summary(run(f'{tag}-60', room, 60))
            check(walking['active'] and walking['mode3'] == 5 and walking['motion'] == 32,
                  f'{room} frame 60 is inside the original descent motion')
            close(walking['actor'][1], 4.0, 0.001, f'{room} frame 60 has walked down 16 units')

            released = summary(run(f'{tag}-75', room, 75))
            check(not released['active'] and released['mode0'] == 1 and released['motion'] == 42,
                  f'{room} hands the player back on the rest motion')
            check(released['floor'] == 0 and abs(released['actor'][1]) < 0.001,
                  f'{room} ends on the lower floor')
            close(released['actor'][2], -40.5005, 0.001, f'{room} ends at the original node')
            check(released['st_flg'] & 4 == 0, f'{room} clears sys->st_flg 0x4')
            check(released['blocked'] == 0, f'{room} leaves no blocked event command')

            state = run(f'{tag}-75-audio', room, 75)
            check(not state['audio_unavailable'] and not state['sequence_unavailable'],
                  f'{room} reports no unavailable sound')
            check(any(played.startswith('MULTSPQ1/0032/2/') for played in state['audio_played']),
                  f'{room} plays the original room bank step sounds')

        # --- control release ---------------------------------------------
        print('control release (RM_1120)')
        held = summary(run('release-100', 'RM_1120.RDX', 100, '100 keys forward\n'))
        later = summary(run('release-160', 'RM_1120.RDX', 160, '100 keys forward\n'))
        latest = summary(run('release-240', 'RM_1120.RDX', 240, '100 keys forward\n'))
        close(held['actor'][2], -40.5005, 0.001, 'control releases on the ending frame')
        check(later['actor'][2] > held['actor'][2] + 20 and latest['actor'][2] > later['actor'][2] + 20,
              'the player walks on normally once the action has ended')

        print('locomotion stays locked during the action')
        early = summary(run('locked-40', 'RM_1120.RDX', 40, '2 keys forward\n'))
        close(early['actor'][1], 12.0, 0.001,
              'input during the action does not change the walk-down trajectory')
        close(early['actor'][2], -57.6939, 0.001,
              'input during the action does not move the player forward')

        # --- interaction path --------------------------------------------
        # Diagnostic placement: the replay puts the player in front of the area
        # instead of walking there.  Only the action itself is under test.
        print('interaction path (diagnostic placement)')
        # 0x280 = HOKAN | MTN_ATTR.  RECVX_STAIR_UNBOUND_HEADWORK (0x100) was part
        # of this set until B06 bound the head control; release_head_look now has
        # a real owner, so the bit must no longer appear.  0x4 (EFFECT_OFF) only
        # the dnd splash adds.
        # The end nodes below are the positions the original mechanisms produce
        # once the player work carries its planted foot:
        #   * kdu reaches its landing over bhFixPositionXYZ (player.c:3033 sets
        #     plp->flg 0x8040000, player.c:1538-1558 runs the fix for every
        #     frame of the finishing phase).  Without it the figure stayed 1.06
        #     units past the stair riser - inside the type-1 ATR wall that
        #     guards the steps - and the next ordinary wall pass threw it back
        #     onto the lower surface while the work kept the upper height.
        #   * owP is the pose bhSetMotion sampled in the same frame, before the
        #     frame counter advanced (Motion.c:70-108 samples, then adds).  The
        #     routines that read owP[0] for their result - the kdd/dnu/dnd end
        #     blocks - therefore land one motion frame earlier than the host
        #     used to report, which is the 0.03..0.48 unit difference here.
        cases = (
            ('kdu', 'RM_0050.RDX', 3, '60 position -186 0 88 0\n61 confirm\n',
             dict(mode2=14, floor=2, unsupported=0x280), (-170.6260, 8.0, 87.8682), 4),
            ('kdd', 'RM_0050.RDX', 4, '60 position -183 8 85 0\n61 confirm\n',
             dict(mode2=15, floor=0, unsupported=0x280), (-196.0960, 0.0, 85.0881), 4),
            ('dnu', 'RM_2050.RDX', 7, '60 position 40 0 46 0\n61 confirm\n',
             dict(mode2=16, floor=1, unsupported=0x280), (39.9974, 9.0, 41.1307), 0),
            ('dnd', 'RM_2050.RDX', 9, '60 position 35 9 35 0\n61 confirm\n',
             dict(mode2=17, floor=0, unsupported=0x284), (35.0, 0.0, 26.6055), 0))
        areas = {room: atr_entries(args.data / 'rooms' / room)
                 for room in ('RM_0050.RDT', 'RM_2050.RDT')}
        for name, room, index, replay, expect, end, depth in cases:
            area = areas[room.replace('.RDX', '.RDT')][index]
            check(area['type'] in (1, 2), f'{name} uses the ATR record {index} of {room}')
            close(area['parameters'][2], depth, 0, f'{name} area depth parameter')
            state = summary(run(f'{name}', room, 140, replay))
            check(not state['active'] and state['motion'] == 42,
                  f'{name} finished and released the player')
            check(state['mode2'] == expect['mode2'], f'{name} ran the original action {expect["mode2"]}')
            check(state['floor'] == expect['floor'], f'{name} ended on floor {expect["floor"]}')
            check(state['unsupported'] == expect['unsupported'],
                  f'{name} reports exactly its unbound services {expect["unsupported"]:#x}')
            check(state['blocked'] == 0, f'{name} leaves no blocked event command')
            for axis, target in enumerate(end):
                close(state['actor'][axis], target, 0.001, f'{name} ends at the original node')

        # --- no teleport --------------------------------------------------
        print('release frame is an ordinary step (RM_2050 dnd)')
        trace = {frames: summary(run(f'dnd-{frames}', 'RM_2050.RDX', frames,
                                     '60 position 35 9 35 0\n61 confirm\n'))
                 for frames in (86, 88, 90)}
        check(trace[86]['active'] and trace[86]['actor'] == [35, 9, 35],
              'the work position holds the start height for the whole descent')
        check(not trace[88]['active'], 'the release frame ends the action')
        close(trace[88]['actor'][2], 26.6055, 0.001, 'the release frame moves the player by the node')
        check(trace[90]['camera_pose'][:3] != trace[88]['camera_pose'][:3],
              'the follow camera switches after the floor change, not with the release')

        # --- the ascent has to end on walkable ground ---------------------
        # Reported symptom in the dog courtyard RM_0050: after the small
        # staircase the figure was thrown back and kept the upper height, so it
        # walked below while the camera stayed anchored above.  The landing is
        # guarded by two type-1 ATR walls (the riser on floor 0 and its
        # counterpart on floor 2); their resolved bands overlap, so an ascent
        # that stops short leaves the player inside them and the next ordinary
        # wall pass pushes it back over the lower surface.  The check is taken
        # from the room data, not from a remembered coordinate.
        print('ascent lands clear of the stair guard walls (RM_0050)')
        walls = wall_entries(args.data / 'rooms' / 'RM_0050.RDT')
        ascent = '60 position -186 0 88 0\n61 confirm\n'
        landed = summary(run('kdu-landing', 'RM_0050.RDX', 140, ascent))
        close(landed['actor'][1], 8.0, 0.001, 'the ascent ends on the upper floor height')
        inside = blocking_walls(walls, landed['actor'], 2, landed['flg'], landed['status'])
        check(not inside, f'the landing is clear of the type-1 stair walls (ATR {inside})')
        walking = {frames: summary(run(f'kdu-walk-{frames}', 'RM_0050.RDX', frames,
                                       ascent + '100 keys forward\n'))
                   for frames in (140, 180)}
        for frames, state in walking.items():
            close(state['actor'][1], 8.0, 0.001, f'frame {frames} keeps the upper floor height')
            check(not blocking_walls(walls, state['actor'], 2, state['flg'], state['status']),
                  f'frame {frames} walks on without entering a type-1 wall')
        check(walking[180]['actor'][0] > walking[140]['actor'][0] + 10,
              'the figure walks on along the upper walkway instead of being pushed back')
        check(walking[180]['camera_pose'][:3] != landed['camera_pose'][:3],
              'the follow camera tracks the figure on the upper floor')

    print()
    if failures:
        print(f'{len(failures)} check(s) failed')
        raise SystemExit(1)
    print('All stair, ladder and step checks passed.')


if __name__ == '__main__':
    main()
