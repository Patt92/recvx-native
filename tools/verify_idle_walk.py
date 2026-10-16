#!/usr/bin/env python3
"""Wait motion (act_sta) -> held forward: the walk starts with its own motion and blend.

pad.c:146 sets sys->pad_old from sys->pad_on *after* the previous pass, so the
direction bits act_sta clears (player.c:2275) make the next bhControlPlayerPad
see pad_old & 1 == 0 and reset mode3 (player.c:1866). act_wlk case 0 then sets
the walk motion, frame 5 and an 8-pass blend (player.c:2299-2325). Without that,
the wait motion keeps playing while plp->spd still advances the body.
An optional older binary is a counterprobe, not an original engine reference.
"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    replay = '0 keys forward\n40 keys none\n400 keys forward\n'

    def run(name, frames, runtime):
        script, state = root / (name + '.txt'), root / (name + '.json')
        script.write_text(replay)
        state.unlink(missing_ok=True)
        with (root / (name + '.log')).open('w') as log:
            subprocess.run([str(runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                            '--headless', '--in-game', '--room', 'RM_0080.RDX', '--frames', str(frames),
                            '--replay', str(script), '--state', str(state)], check=True, stdout=log, stderr=subprocess.STDOUT)
        return json.loads(state.read_text())

    waiting = run('waiting', 399, args.runtime)
    assert waiting['player_work']['mtn_no'] == 10, waiting['player_work']  # act_sta wait motion after 300 passes
    walk = [run('walk-%d' % f, f, args.runtime) for f in (401, 403)]
    for w in walk:
        assert w['player_work']['mtn_no'] != 10, ('wait motion still drawn while walking', w['player_work'])
    assert walk[1]['player_work']['hokan_count'] > 0, ('walk entered without the original blend', walk[1]['player_work'])
    if args.baseline:
        before = run('before-403', 403, args.baseline)
        print('baseline', before['player_work'])
    print('Wait motion -> held forward: walk motion and its blend start on the next pass (pad_old after the pass).')


if __name__ == '__main__':
    main()
