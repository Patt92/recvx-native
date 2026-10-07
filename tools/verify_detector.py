#!/usr/bin/env python3
"""Replay the original RM_0090 alarm, switch response and metal-free passage.

Positions are diagnostic placements. All decisions and events run through the
original room scripts and ordinary confirm/choice input. This is not a full
playthrough or a physical-controller test.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

from verify_progression import REPLAY as BOX_REPLAY

ROOT = Path(__file__).resolve().parents[1]
REPLAY = '''0 debug_key F2
1 debug_select give 5
2 confirm
3 debug_key F2
60 position 34.5 0 5 0
100 position 44.5 0 5 0
180 confirm
220 confirm
300 position 34.5 0 5 0
320 position 44.5 0 5 0
510 position 52.5 0 3.6 0
521 confirm
560 confirm
600 confirm
620 confirm
800 position 34.5 0 5 0
820 position 44.5 0 5 0
900 confirm
940 confirm
1000 position 34.5 0 5 0
1020 position 44.5 0 5 0
'''


def check(runtime, data, folder):
    scripts = {'yes': REPLAY, 'no': REPLAY.replace('620 confirm', '620 right\n621 confirm'),
               'box': BOX_REPLAY}
    for name, script in scripts.items():
        (folder / (name + '.txt')).write_text(script)

    def run(name, script, frames):
        state = folder / (name + '.json')
        with (folder / (name + '.log')).open('w') as log:
            subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
                '--headless', '--in-game', '--room', 'RM_0090.RDX', '--frames', str(frames),
                '--replay', str(folder / (script + '.txt')), '--state', str(state),
                '--screenshot', str(folder / (name + '.bmp')), '--validate-poses'],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = json.loads(state.read_text())
        assert not result['blocked_commands'], (name, result['event_tasks'])
        assert not result['sequence_unavailable'], (name, result['sequence_unavailable'])
        return result

    def lamps(state, red):
        effects = {e['slot']: e for e in state['effects']}
        for green_slot, red_slot in [(58, 59), (60, 61)]:
            green, alarm = effects[green_slot], effects[red_slot]
            assert green['type'] == (0 if red else 1) and green['visible'] != red, green
            assert alarm['type'] == (3 if red else 0) and alarm['visible'] == red, alarm
            if red:
                assert alarm['texture'] == 414 and alarm['atlas'] == 2, alarm
        return effects[61]['phase']

    alarm = run('alarm', 'yes', 520)
    phase = lamps(alarm, True)
    assert alarm['event_sound_active'][4] and 41 in alarm['events'], 'alarm/lamp event missing'
    next_frame = run('blink', 'yes', 521)
    assert lamps(next_frame, True) != phase, 'red lamp does not animate'
    reset = run('reset', 'yes', 790)
    lamps(reset, False)
    assert not any(reset['event_sound_active']), 'alarm remains after release switch'
    assert reset['fixed_camera'] == -1 and reset['event_camera'] == -1, 'switch camera retained'
    for field in ['dialog_open', 'cinematic', 'player_scripted', 'player_hidden']:
        assert not reset[field], (field, 'switch did not return control')
    for event in [7, 8, 10, 40]:
        assert event in reset['events'], ('original switch event missing', event)
    for sequence in [0, 1, 19]:
        sound = 'MULTSPQ1/0013/2/seq' + str(sequence) + '/prog' + str(sequence)
        assert reset['audio_played'].count(sound) >= 2, ('both shutter directions lack SFX', sound)
    declined = run('decline', 'no', 790)
    lamps(declined, True)
    assert declined['message_selection'] == 1 and declined['event_sound_active'][4], 'No resets alarm'
    repeat = run('repeat', 'yes', 1220)
    lamps(repeat, True)
    assert repeat['event_sound_active'][4], 'alarm cannot retrigger'
    assert repeat['audio_played'].count('MULTSPQ1/0013/2/seq2/prog2') == 2, 'repeat alarm request'
    safe = run('deposit-pass', 'box', 550)
    lamps(safe, False)
    assert not safe['event_sound_active'][4], 'metal-free passage sounds alarm'
    assert not any('/seq2/' in s for s in safe['audio_played']), 'metal-free alarm request'
    run('box', 'box', 364)
    print('Original alarm SQ loop, animated lamps, Yes/No switch handover, both shutter SFX, '
          'repeat alarm and metal-free passage verified; screenshots use real UI/resources.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='recvx-detector-') as temporary:
        folder = args.output.resolve() if args.output else Path(temporary)
        folder.mkdir(parents=True, exist_ok=True)
        check(args.runtime, args.data, folder)


if __name__ == '__main__':
    main()
