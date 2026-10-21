#!/usr/bin/env python3
"""RM_0010 ATR 3 (item 12, attr 0x1): crouch pickup before GetItem.

hitchk.c:4847-4890 bhCheckExmAtari case 4 enters bhCPM2_act_cro (mode2 19,
player.c:3887) for an item ATR with attr 0x1. GetItem is requested only at the
end of motion 45 (flg 0x400000 -> cb_flg 0x10), then bhCheckSubTask opens the
status task; its close calls bhStandPlayerMotion (sub1.c:3105). The model is
deactivated by the room script's bhItmCk, never by GetItem.
Only a player placement shortens the walk; no injected modes or flags.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-crouch-') as temporary:
        root = Path(temporary)
        out = args.output.resolve() if args.output else root
        out.mkdir(parents=True, exist_ok=True)
        controls = root / 'controls.ini'
        subprocess.run([str(runtime), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)

        def run(name, frames, inputs):
            script, state = out / (name + '.txt'), out / (name + '.json')
            script.write_text('\n'.join(['20 position 2.5 0 -19 0', '21 confirm'] + inputs) + '\n')
            state.unlink(missing_ok=True)
            command = [str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'),
                       '--user-dir', str(root / 'user'), '--controls', str(controls),
                       '--debug-config', str(root / 'debug.ini'), '--in-game', '--room', 'RM_0010.RDX',
                       '--renderer', 'software', '--enemy-ai', 'on', '--headless',
                       '--frames', str(frames), '--replay', str(script), '--state', str(state)]
            with (out / (name + '.log')).open('w') as log:
                subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            s = json.loads(state.read_text())
            assert s['room'] == 'RM_0010.RDX' and not s['blocked_commands'], (name, s['blocked_commands'])
            return s

        crouch = run('crouching', 30, [])
        work = crouch['player_work']
        assert work['mode'][2] == 19 and crouch['motion'] == 45, ('ATR attr 0x1 did not enter bhCPM2_act_cro', work, crouch['motion'])
        assert work['flg'] & 0x10000 and work['stflg'] & 0x10000 and crouch['work_flags'][5] & 4, ('act_cro case 0 flags missing', work)
        assert not crouch['work_flags'][10] & 0x10 and not crouch['inventory_open'] and not crouch['dialog_open'], 'GetItem started before the crouch motion ended'
        print('Item ATR attr 0x1 enters mode2 19 / motion 45; GetItem waits for the motion end.')

        question = run('question', 76, [])
        assert question['work_flags'][10] & 0x10 and question['inventory_open'], 'motion end did not request GetItem (cb_flg 0x10)'
        assert question['dialog_open'] and question['dialog_index'] == 157, ('GetItem question 157 missing', question['dialog_index'])
        assert not question['player_work']['flg'] & 0x10000 and not question['work_flags'][5] & 4, 'act_cro end did not release flg/st_flg'
        print('Motion end: cb_flg 0x10, status task, original question 157.')

        done = run('taken', 200, ['80 confirm', '140 confirm', '180 confirm'])
        assert any((item >> 16) == 12 for item in done['items']), 'Yes did not insert item 12'
        model = [item for item in done['room_items'] if item['index'] == 1]
        assert model and model[0]['identity'] == 12 and model[0]['inactive'] and not model[0]['hidden'], ('bhItmCk did not deactivate the model', model)
        assert not done['inventory_open'] and done['player_work']['mode'][2] != 19 and done['motion'] == 42, (
            'status close did not stand the player (bhStandPlayerMotion)', done['player_work'], done['motion'])
        assert not done['player_work']['flg'] & 0x10000 and not done['work_flags'][5] & 4
        print('Yes: item 12 taken, model script-deactivated, status close stands the player.')


if __name__ == '__main__':
    main()
