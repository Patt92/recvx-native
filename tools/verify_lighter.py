#!/usr/bin/env python3
"""Original opening/menu equipment -> native lighter, UV cycle and unequip.

No injected inventory, effect spawns or event flags. An optional pre-port native
binary is a counterprobe, not an original engine reference.
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
    controls = root / 'controls.ini'
    subprocess.run([str(args.runtime.resolve()), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)
    opening = '0 new_game\n1300 confirm\n1500 confirm\n1550 confirm\n1555 confirm\n1560 inventory\n'

    def run(name, frames, replay, runtime=args.runtime):
        script, state = root / (name + '.txt'), root / (name + '.json')
        script.write_text(replay)
        with (root / (name + '.log')).open('w') as log:
            subprocess.run([str(runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                            '--controls', str(controls), '--snapshots', str(root / 'snapshots'), '--headless',
                            '--frames', str(frames), '--replay', str(script), '--state', str(state),
                            '--screenshot', str(root / (name + '.bmp'))], check=True, stdout=log, stderr=subprocess.STDOUT)
        return json.loads(state.read_text())

    equipped = [run('equipped-' + str(i), 4000 + i, opening) for i in range(9)]
    phases = []
    for v in equipped:
        assert v['weapon'] == 1 and v['hp'] == 160 and not v['blocked_commands'], v
        assert not any(v[key] for key in ('inventory_open', 'player_hidden', 'player_scripted', 'cinematic'))
        assert v['work_flags'][13] & 0x1000000, 'native player lighter bit missing'
        flames = [e for e in v['effects'] if e['id'] == 9 and e['type'] == 2 and e['visible']]
        assert len(flames) == 1 and flames[0]['texture'] == 8 and flames[0]['atlas'] == 0, flames
        phases.append(flames[0]['phase'])
    assert set(phases) == set(range(1, 9)) and phases[0] == phases[8], phases
    off_inputs = opening + '4000 inventory\n4060 confirm\n4065 confirm\n4070 inventory\n'
    off = run('unequipped', 4150, off_inputs)
    assert off['weapon'] == 0 and not (off['work_flags'][13] & 0x1000000), off
    assert not any(e['type'] == 2 and e['visible'] for e in off['effects']), off['effects']
    assert not off['blocked_commands'] and not off['inventory_open'], off
    if args.baseline:
        before = run('before', 4000, opening, args.baseline)
        for key in ('room', 'actor', 'yaw', 'weapon', 'hp', 'items', 'events', 'blocked_commands'):
            assert before[key] == equipped[0][key], (key, before[key], equipped[0][key])
        assert not any(e['id'] == 9 and e['type'] == 2 and e['visible'] for e in before['effects'])
    print('Original opening/menu: one visible hand-linked lighter, eight UV phases, unchanged HP/progress, normal unequip passed.')


if __name__ == '__main__':
    main()
