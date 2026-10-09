#!/usr/bin/env python3
"""Replay the RM_0090 3D scanner: Eagle Plate use, original terminal task, lid animation.

The position is a diagnostic placement in front of the scanner. The sequence
runs through the original room scripts (bhEvtComSet -> bhSysCall computer task
-> bhExitSpEvtComputer) with ordinary OK input. Not a full playthrough.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXED = '''0 debug_key F2
1 debug_select give 56
2 confirm
3 debug_key F2
60 position 35 0 63.5 32768
'''
REPLAY = FIXED + ''.join(f'{frame} confirm\n' for frame in range(80, 1000, 12))


def check(runtime, data, folder):
    (folder / 'scanner.txt').write_text(REPLAY)

    def run(name, frames):
        state = folder / (name + '.json')
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
                '--headless', '--in-game', '--room', 'RM_0090.RDX', '--frames', str(frames),
                '--replay', str(folder / 'scanner.txt'), '--state', str(state),
                '--screenshot', str(folder / (name + '.bmp'))],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        assert not result['blocked_commands'], (name, result['event_tasks'])
        return result

    terminal = run('terminal', 190)
    flags = terminal['work_flags']
    assert flags[10] & 0x400000 and flags[5] & 0x4000000 and flags[6] == 0x30, ('terminal not active', flags)
    assert 20 in terminal['events'], 'scanner event 20 not started'
    assert 'SYSTEM/MIDI/2/106' in terminal['audio_played'] and 'SYSTEM/MIDI/2/110' in terminal['audio_played'], 'terminal MIDI sounds missing'
    assert not terminal['audio_unavailable'] and not terminal['sequence_unavailable'], 'terminal sound unresolved'
    # The task itself: script 4 of sp_evt.c on screen, not just its request flags.
    screen = terminal['computer']
    assert screen and screen['active'] and screen['number'] == 4, ('terminal task not running', screen)
    assert screen['text'][0] == 'Activating the enhanced', ('terminal text', screen['text'])
    assert screen['draws'] > 20 and not screen['draw_overflow'], ('terminal drew nothing', screen['draws'])
    assert not screen['unknown_opcodes'], 'the terminal interpreter met an undefined opcode'
    assert screen['message'] == 5, ('command 241 did not request room message 5', screen['message'])
    assert screen['flg'] & 0x200, 'command 245 did not lock the cancel button of the scanner'
    done = run('finished', 900)
    flags = done['work_flags']
    assert not flags[10] & 0x400000 and not flags[5] & 0x4000000 and flags[6] == 0xffffffff, ('terminal did not exit', flags)
    assert flags[11] & 0x4 and flags[11] & 0x8000 and flags[12] & 0x20000, ('bhExitSpEvtComputer gm_flg/ts_flg', flags)
    assert done['computer'] and not done['computer']['active'], 'the terminal task is still running'
    assert 25 in done['events'], 'script did not continue after the terminal (lid animation)'
    for field in ['dialog_open', 'cinematic', 'player_scripted', 'player_hidden']:
        assert not done[field], (field, 'scanner sequence did not return control')
    assert done['event_camera'] == -1 and done['fixed_camera'] == -1, 'event camera retained'
    print('Scanner terminal, script continuation and control return verified (RM_0090).')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        check(args.runtime, args.data, args.output)
    else:
        with tempfile.TemporaryDirectory() as folder:
            check(args.runtime, args.data, Path(folder))


if __name__ == '__main__':
    main()
