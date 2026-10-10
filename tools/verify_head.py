#!/usr/bin/env python3
"""Verify the original player head control in the running game (Auftrag B06).

Checked against the productive runtime, not against a component fixture:

  * the look flag EXP1_I(0) 0x4 is owned by the action dispatch.  The seven
    normal locomotion routines set it in their mode3-0 entry block and the
    stair/ladder/step routines clear it together with the four return paths;
    both halves are observed while walking and while a room script runs
    bhPlayerKaidanMotion;
  * the four return paths run with their original 182-unit threshold and
    quarter step, and they write the node 4/5 pose overrides that the draw
    pass and the hair control then use;
  * the blink sequence walks PlyEyeTab once per blink, with the original
    ready/idle counters;
  * head and hair read one and the same owP[5].mtx per frame: the head point
    EXP1_F(12..20) and the hair root njCalcPoint(owP[5].mtx, obwp[2].lo*) stay
    exactly the link offset apart in every frame, so there is no extra delay
    between the two consumers of the pose.

Not satisfiable in the productive runtime and reported, never substituted:
bhLookNearEnemy can only lock an enemy work whose epw->flg carries bit 0.  The
only owner of that bit is the enemy core, which native/en01_damage.c declares
as a porting gap and which reports flg 0.  A faithful read therefore finds no
searchable work, and the runtime says so in head.unbound.  The search, the
target switch, the hold branch and the rotation are exercised in
tests/native/player_head_test.cpp instead.
"""
import argparse
import json
import math
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# player.c:206-209.  The blink walks this table once and then forces 0.
PLY_EYE_TAB = (0, 0, 1, 1, 2, 2, 1, 0)
# player.c:1289-1291 lkmtab[0]: obwp[2] hangs on head node 5 at this offset.
LINK_OFFSET = (0.0, 1.5869, 0.7747)
LINK_LENGTH = math.sqrt(sum(v * v for v in LINK_OFFSET))

BLINK_ENABLE, BLINK_RUN, LOOK, LOOK_HOLD, LOCKED = 0x1, 0x2, 0x4, 0x8, 0x10
RETURN_ALL = 0x1E0

failures = []


def check(ok, why):
    print(('  ok   ' if ok else '  FAIL ') + why)
    if not ok:
        failures.append(why)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', type=Path, default=ROOT / 'build/hair-claude-20261010/recvx-runtime')
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--mods', type=Path, default=Path('/nonexistent'))
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix='recvx-head-') as temporary:
        folder = Path(temporary)
        scripts = {}

        def run(name, room, frames, replay=''):
            script = scripts.get(replay)
            if script is None:
                script = folder / f'input{len(scripts)}.txt'
                script.write_text(replay)
                scripts[replay] = script
            state = folder / f'{name}.json'
            command = [str(args.runtime), '--data', str(args.data), '--mods', str(args.mods),
                       '--room', room, '--headless', '--in-game', '--frames', str(frames),
                       '--replay', str(script), '--state', str(state)]
            done = subprocess.run(command, capture_output=True, text=True)
            if done.returncode:
                raise SystemExit(f'{name}: runtime failed\n{done.stderr[-800:]}')
            return json.loads(state.read_text())

        # --- the action dispatch owns the look flag ----------------------
        # bhResetPlayer closes with *(int*)&plp->mode0 = 1 (player.c:1152), so
        # the first bhCPM1_act_bas pass enters bhCPM2_act_std's case 0 and sets
        # EXP1_I(0) |= 0x4.  Walking keeps it; nothing else may set it.
        print('RM_0120, natural walk forward (no diagnostic placement)')
        walk = '0 keys forward\n'
        first = run('walk-1', 'RM_0120.RDX', 1, walk)
        check(first['head']['flags'] & LOOK, 'act_std entry enables the look on the first frame')
        check(first['head']['flags'] & BLINK_ENABLE, 'bhSetPlayer leaves exp1[0] bit 0 set')
        check(first['head']['look_timer'] == 1, 'bhLookNearEnemy counts its first search frame')
        for frames in (40, 90, 140, 200):
            state = run(f'walk-{frames}', 'RM_0120.RDX', frames, walk)
            head = state['head']
            check(head['flags'] & LOOK, f'frame {frames}: walking keeps the look enabled')
            check(head['look_timer'] == frames, f'frame {frames}: the search ran on every frame')
            check(not head['flags'] & (LOOK_HOLD | LOCKED),
                  f'frame {frames}: no hold and no lock is invented')
            check(any('epw->flg bit 0' in name for name in head['unbound']),
                  f'frame {frames}: the missing enemy flg owner is reported')
            check(state['motion'] == 0 and head['overrides'] == 0,
                  f'frame {frames}: with no target and no return the motion pose is left alone')

        # Standing and turning stay inside the locomotion set, so the look
        # never drops: act_std [0] = 42, the run turn [3] = 4 (player.c:226).
        print('RM_0120, turning in place and standing')
        turn = '0 keys right\n30 keys none\n'
        for frames, motion in ((20, 42), (45, 42)):
            state = run(f'turn-{frames}', 'RM_0120.RDX', frames, turn)
            check(state['head']['flags'] & LOOK, f'frame {frames}: turning/standing keeps the look')
            check(state['motion'] == motion, f'frame {frames}: stays on PlMtnAct motion {motion}')

        # --- the blink sequence -----------------------------------------
        # player.c:6972-7005.  rand() > 0.97 with blink_timer past the ready
        # count starts the run; PlyEyeTab is walked once and then forced to 0.
        print('RM_0120, blink sequence during the same walk')
        trace = []
        for frames in range(50, 63):
            head = run(f'blink-{frames}', 'RM_0120.RDX', frames, walk)['head']
            trace.append((frames, head['flags'], head['blink'], head['blink_timer'], head['eye']))
        started = [row for row in trace if row[1] & BLINK_RUN]
        check(bool(started), 'a blink starts on its own during a natural walk')
        if started:
            begin = started[0]
            check(begin[2] == 1 and begin[3] == 0,
                  'the blink start resets the frame counter and the idle counter')
            sequence = [(row[2], row[4]) for row in started]
            check(all(eye == PLY_EYE_TAB[(frame - 1) & 7] for frame, eye in sequence),
                  f'the eye texture follows PlyEyeTab: {sequence}')
            after = [row for row in trace if row[0] > started[-1][0]]
            check(bool(after) and not after[0][1] & BLINK_RUN and after[0][2] == 0 and after[0][4] == 0,
                  'the eighth frame closes the run, resets the counter and forces texture 0')
        idle = [row for row in trace if row[0] < started[0][0]] if started else []
        check(all(row[4] == 0 for row in idle), 'outside the run the open eye texture is selected')

        # --- the stair binding ------------------------------------------
        # bhPlayerKaidanMotion needs no input at all.  bhCPM2_act_kdd's case 0
        # (player.c:3153) clears 0x4 and raises 0x1E0; the four returns then run
        # in the same frame, and the action end (player.c:3115 *(int*)&mode0 = 1)
        # hands the work back to act_std, which sets 0x4 again.
        print('RM_1120, room script stair (bhPlayerKaidanMotion, no input)')
        frames = {f: run(f'stair-{f}', 'RM_1120.RDX', f, '') for f in (1, 2, 5, 30, 60, 70)}
        entry = frames[1]['head']
        check(not entry['flags'] & LOOK, 'the stair entry gives up the look target')
        check(entry['flags'] & RETURN_ALL, 'the stair entry raises the four return paths')
        check(entry['overrides'] >= 1, 'the running return writes a node 4/5 pose override')
        check(frames[1]['stairs'] and frames[1]['stairs']['active'], 'the script stair really runs')
        settled = frames[2]['head']
        check(not settled['flags'] & (LOOK | RETURN_ALL) and settled['overrides'] == 0,
              'a return below the 182 threshold clears its own bit and stops overriding')
        for f in (5, 30, 60):
            state = frames[f]
            check(not state['head']['flags'] & LOOK,
                  f'frame {f}: the look stays off for the whole stair action')
            check(state['stairs'] and state['stairs']['active'], f'frame {f}: the action still owns the work')
        done = frames[70]
        check(not (done['stairs'] and done['stairs']['active']), 'the stair action hands the work back')
        check(done['motion'] == 42, 'the action end returns to PlMtnAct[..][0]')
        check(done['head']['flags'] & LOOK, 'act_std re-enables the look after the stair action')
        check(not done['head']['flags'] & RETURN_ALL, 'the return paths have finished')
        check(not (done['stairs'] and done['stairs']['unsupported'] & 0x100),
              'the head work is no longer reported as an unbound stair service')

        # --- head and hair share one pose -------------------------------
        # EXP1_F(12..20) is owP[5].mtx[12..14] at the end of the head control;
        # the hair root is njCalcPoint of the same matrix with obwp[2].lo*.
        # Equal distance in every frame means both read the same frame's matrix.
        print('RM_1120, head point and hair root in the same frame')
        for f, state in sorted(frames.items()):
            hair = state['hair']
            if not hair or f == 1:
                continue  # frame 1 is the documented hair reset frame (B04)
            distance = math.dist(state['head']['point'], hair['root'])
            check(abs(distance - LINK_LENGTH) < 1e-3,
                  f'frame {f}: head point and hair root are {distance:.4f} apart (offset {LINK_LENGTH:.4f})')

    print()
    if failures:
        print(f'{len(failures)} check(s) failed')
        raise SystemExit(1)
    print('Player head control verified in the productive runtime.')


if __name__ == '__main__':
    main()
