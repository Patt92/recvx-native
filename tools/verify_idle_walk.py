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
import os
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
        script, state, log_path = root / (name + '.txt'), root / (name + '.json'), root / (name + '.log')
        script.write_text(replay)
        state.unlink(missing_ok=True)
        with log_path.open('w') as log:
            subprocess.run([str(runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                            '--headless', '--in-game', '--room', 'RM_0080.RDX', '--frames', str(frames),
                            '--replay', str(script), '--state', str(state)], check=True, stdout=log, stderr=subprocess.STDOUT,
                           env={**os.environ, 'RECVX_PLAYER_POSE_TRACE': '1'})
        poses = [line.split() for line in log_path.read_text().splitlines() if line.startswith('POSETRACE')]
        return json.loads(state.read_text()), [dict(f.split('=', 1) for f in p[1:5]) for p in poses]

    # Replay line 400 is the press: frame 400 is act_sta's release pass, frame 401 the first walk pass.
    waiting, _ = run('waiting', 400, args.runtime)
    assert waiting['player_work']['mtn_no'] == 10, waiting['player_work']  # act_sta wait motion after 300 passes
    walk, poses = run('walk', 404, args.runtime)
    assert walk['player_work']['mtn_no'] != 10, ('wait motion still drawn while walking', walk['player_work'])
    first = next((p for p in poses if p['mtn'] != '10'), None)
    assert first and first['hokan'] == '8' and first['frm'] == str(5 * 65536), ('walk entered without act_wlk case 0 (frame 5, 8-pass blend)', first)
    if args.baseline:
        before, _ = run('before', 404, args.baseline)
        print('baseline mtn_no after the press:', before['player_work']['mtn_no'])
    print('Wait motion -> held forward: walk motion and its blend start on the next pass (pad_old after the pass).')


if __name__ == '__main__':
    main()
