#!/usr/bin/env python3
"""Host 3rd-person camera mod: aiming and shooting keep the shoulder camera.

The mod is a host addition, not original behaviour. Its gate (runtime.cpp
third_person_free) must not read sys->st_flg 0x4: bhCPM1_act_atk (player.c:4483)
sets that bit itself on every combat pass, so the old gate handed the view back
to the fixed room camera exactly while aiming. The gate now follows the player
work: bhCPM0_action (mode0 1) outside the mode2 14..22 special actions, plus the
original locks (event/fixed camera, scripted/hidden player, cinematic cb_flg 0x4,
film, door, title).

Checks, all with the original Steve scene of RM_0030 (combat-handgun.txt, no
position or flag edits):
  1. cinematic / event camera: the mod stays off while the scene runs;
  2. aiming (3400 keys aim) and the first shot (3500 attack): third_person_active
     stays true, combat_aiming true;
  3. the combat mechanics are unchanged: combat_shots/attacks, motion, yaw and
     position equal a run without the mod;
  4. a stair action (RM_1120 script start of kdd, mode2 15) keeps the original
     camera.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    replay = ROOT / 'tests/native/replays/combat-handgun.txt'
    with tempfile.TemporaryDirectory(prefix='recvx-third-person-aim-') as temporary:
        root = Path(temporary)
        out = args.output.resolve() if args.output else root
        out.mkdir(parents=True, exist_ok=True)
        controls = root / 'controls.ini'
        subprocess.run([str(runtime), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)

        def run(name, room, frames, mod, script=None):
            state = out / (name + '.json')
            state.unlink(missing_ok=True)
            command = [str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'),
                       '--user-dir', str(root / 'user'), '--controls', str(controls),
                       '--debug-config', str(root / 'debug.ini'),
                       '--third-person-config', str(root / 'third_person.ini'),
                       '--in-game', '--room', room, '--renderer', 'software', '--enemy-ai', 'on',
                       '--headless', '--frames', str(frames), '--state', str(state)]
            if script is not None:
                command += ['--replay', str(script)]
            if mod:
                command.append('--third-person')
            with (out / (name + '.log')).open('w') as log:
                subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=300)
            s = json.loads(state.read_text())
            assert s['room'] == room and not s['blocked_commands'], (name, s['room'], s['blocked_commands'])
            return s

        # 1. Steve scene: event camera / cinematic keep the original view.
        scene = run('scene', 'RM_0030.RDX', 1000, True, replay)
        assert scene['event_camera'] >= 0 or scene['work_flags'][10] & 4 or scene['player_scripted'], (
            'tick 1000 of RM_0030 is not inside the Steve scene', scene['event_camera'], scene['work_flags'][10])
        assert not scene['third_person_active'], 'the chase camera stayed on during the event camera / cinematic'
        print('RM_0030 scene: event camera / cinematic hand the view back (third_person_active false).')

        # 2. Weapon raised, no shot yet (3400 keys aim, first attack at 3500).
        aim = run('aim', 'RM_0030.RDX', 3450, True, replay)
        assert aim['weapon'] == 4 and aim['combat_aiming'], ('M93R not raised at tick 3450', aim['weapon'], aim['combat_aiming'])
        assert aim['combat_shots'] == 0, aim['combat_shots']
        assert aim['event_camera'] == -1 and not aim['player_scripted'] and not aim['work_flags'][10] & 4
        assert aim['work_flags'][5] & 4, 'bhCPM1_act_atk did not set st_flg 0x4 (player.c:4483); the regression setup is wrong'
        assert aim['third_person_active'], 'aiming handed the view back to the fixed camera'
        print('Aiming: st_flg 0x4 set by bhCPM1_act_atk, third_person_active stays true.')

        # 3. After the first shot, with and without the mod: same combat state.
        shot = run('shot', 'RM_0030.RDX', 3520, True, replay)
        plain = run('shot-plain', 'RM_0030.RDX', 3520, False, replay)
        assert shot['combat_shots'] == 1 and shot['combat_attacks'] == 1, (shot['combat_shots'], shot['combat_attacks'])
        assert shot['third_person_active'] and shot['combat_aiming'], 'the shot handed the view back to the fixed camera'
        for key in ('combat_shots', 'combat_attacks', 'combat_hits', 'weapon', 'motion', 'animation_frame', 'yaw', 'actor', 'player_work'):
            assert shot[key] == plain[key], (key, shot[key], plain[key])
        assert not plain['third_person_active'], 'the mod reports active without --third-person'
        # The host view differs from the original camera only through the mod (the game camera is untouched).
        assert shot['camera_pose'] == plain['camera_pose'], ('the mod changed the original CAM_WORK pose', shot['camera_pose'], plain['camera_pose'])
        print('Shot: combat_shots 1 with and without the mod, same player work, original camera pose unchanged.')

        # 4. Stair action: RM_1120 starts bhCPM2_act_kdd (mode2 15) from its script without input.
        stairs = run('stairs', 'RM_1120.RDX', 10, True)
        st = stairs['stairs'] or {}
        assert st.get('active') and st.get('mode2') == 15, ('RM_1120 did not start kdd', st)
        assert not stairs['third_person_active'], 'the chase camera stayed on during a stair action'
        print('Stairs (mode2 15): original camera kept.')


if __name__ == '__main__':
    main()
