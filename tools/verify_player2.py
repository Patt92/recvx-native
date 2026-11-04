#!/usr/bin/env python3
"""Targeted Player-2 extension checks through the production runtime.

RM_0080 and replay commands are diagnostic setups, not natural co-op gameplay.
The NPC command provider itself is exercised by tests/native/player2_test.cpp.
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
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)

    def run(name, mode, script='', frames=18, extra=()):
        replay, state = out / f'{name}.txt', out / f'{name}.json'
        replay.write_text(script)
        state.unlink(missing_ok=True)
        command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                   '--mods', str(out / 'no-mods'), '--user-dir', str(out / 'user'),
                   '--headless', '--in-game', '--room', 'RM_0080.RDX',
                   '--player2', mode, '--frames', str(frames), '--replay', str(replay),
                   '--state', str(state), *extra]
        with (out / f'{name}.log').open('w') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        return json.loads(state.read_text())

    baseline = run('off', 'off')
    local = run('local', 'local', '0 player2_keys forward\n')
    assert 'player2' not in baseline, 'disabled extension allocated a Player-2 work'
    assert local['player2']['motion'] == 0 and local['player2']['frame'] > 0, local['player2']
    assert local['player2']['position'] != baseline['actor'], 'Player 2 did not move in the runtime'
    for field in ('actor', 'yaw', 'player_work', 'hp', 'items'):
        assert local[field] == baseline[field], (field, local[field], baseline[field])
    npc = run('npc', 'npc', '0 player2_keys forward\n')
    assert npc['player2']['motion'] == 42, 'local commands reached the NPC source'
    assert 'NPC driver not bound' in npc['player2']['gaps'], 'missing NPC source hidden'
    menu = run('menu', 'local', '0 player2_keys forward\n', extra=('--inventory',))
    assert menu['inventory_open'] and menu['player2']['frame'] == 0, menu['player2']
    switched = '0 player2_keys forward\n8 player2_mode off\n'
    before = run('before-disable', 'local', switched, frames=8)
    disabled = run('disabled', 'local', switched, frames=20)
    assert disabled['player2']['mode'] == 'off'
    for field in ('position', 'frame'):
        assert disabled['player2'][field] == before['player2'][field], 'disabled actor advanced'
    switched += '20 player2_mode npc\n24 player2_mode local\n24 player2_keys right\n'
    resumed = run('resumed', 'local', switched, frames=32)
    assert resumed['player2']['mode'] == 'local' and resumed['player2']['frame'] != disabled['player2']['frame']
    debug = run('debug-mode', 'local', '0 debug_key F2\n0 debug_select player2 2\n0 confirm\n', frames=2)
    assert debug['debug_open'] and debug['player2']['mode'] == 'npc', 'debug menu did not apply NPC mode'
    (out / 'view.bmp').unlink(missing_ok=True)
    run('view', 'local', '0 player2_keys right\n20 player2_keys forward\n40 player2_keys none\n',
        frames=40, extra=('--screenshot', str(out / 'view.bmp')))
    print('Player 2 production runtime: independent motion, NPC routing, inventory hold, off/local/npc switches PASS')


if __name__ == '__main__':
    main()
