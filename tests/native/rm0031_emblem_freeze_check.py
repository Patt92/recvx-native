#!/usr/bin/env python3
"""RM_0031: placing the Sp. Alloy Emblem (item 86) freezes player and enemies until the scene ends.

scd1 @0x62e starts events 0-2 when the emblem is used on floor area 1 (cb_flg 0x400, value[8] 86);
event 0 opens with bhCineSet 3 (cb_flg 0x4). Leaving the status screen with cb_flg 0x4 set,
ItemTaskCheck (sub1.c:3212-3220) only ORs 0xCC into sp_flg, which StatusMain had set to 0x30
(system.c:606-607): sp_flg 0x1 (bhControlPlayer) and 0x2 (bhControlEnemy, eneset.c:352) stay clear
until event 0's bhCineSet 4 sets sp_flg |= ~0 (event.c bhCineSet). Before: sp_flg stayed ~0 and
the active zombie (work 4) walked and could attack during the scene.

Diagnostic setup: emblem from the debug menu, player placed on the floor area."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

REPLAY = ('0 debug_key F2\n1 debug_select give 86\n2 confirm\n3 debug_key F2\n'
          '10 position 0 0 160 32768\n200 inventory\n240 right\n250 confirm\n260 confirm\n')


def run(runtime, data, root, frames):
    replay = root / 'replay.txt'
    replay.write_text(REPLAY)
    state = root / f'state-{frames}.json'
    result = subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
                             '--mods', str(root / 'no-mods'), '--user-dir', str(root / f'user-{frames}'),
                             '--headless', '--in-game', '--room', 'RM_0031.RDX', '--replay', str(replay),
                             '--frames', str(frames), '--state', str(state), '--enemy-ai', 'on'],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert result.returncode == 0, result.stdout[-2000:]
    return json.loads(state.read_text())


def zombie(state):
    return next(e for e in state['enemies'] if e['index'] == 4)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        scene = [run(args.runtime, args.data, root, frames) for frames in (290, 310, 330)]
        after = run(args.runtime, args.data, root, 350)
    for state in scene:
        assert state['cinematic'], 'emblem scene not running'
        assert 86 not in [(v >> 16) & 255 for v in state['items'][1:] if v], 'emblem not used'
        assert state['work_flags'][6] & 0x3 == 0, f"sp_flg {state['work_flags'][6]:#x}: player/enemy pass not stopped"
        assert not zombie(state)['inactive'], 'zombie 4 is not the active enemy of this setup'
    positions = {tuple(zombie(state)['position']) for state in scene}
    assert len(positions) == 1, f'zombie moved during the emblem scene: {sorted(positions)}'
    assert not after['cinematic'] and after['work_flags'][6] & 0x3 == 0x3, 'scene end did not release sp_flg'
    print('RM_0031 emblem scene: sp_flg 0xCC after the status exit, zombie still until bhCineSet 4 passed')


if __name__ == '__main__':
    main()
