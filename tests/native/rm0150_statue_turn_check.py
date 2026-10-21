#!/usr/bin/env python3
"""RM_0150: sword pickup -> gas -> walking contact -> statue rotation.

No scripted object commands or forced player modes: original room events run
from the sword interaction. Diagnostic placements only shorten walking paths.
objitm.c:1738 requires live plp->mode2 3/4/5 from the preceding player pass
(game.c:49-51). A component fixture setting mode2 itself misses this binding.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def replay(snapshot, sdl, stop):
    lines = []
    if snapshot:
        lines = (['0 sdl_key down F9', '2 sdl_key up F9'] if sdl else
                 ['0 debug_key F2', '1 debug_select snapshot_load 0', '2 confirm'])
    lines += ['30 position 53 0 40 49152', '31 confirm', '80 confirm',
              '180 confirm', '300 confirm', '600 position 34 0 36 16384']
    if sdl:
        lines += ['601 sdl_key down W', f'{stop} sdl_key up W']
    else:
        # Headless replay keys update held movement, but pad_on is rebuilt
        # only on a keys row. Object readers need the held bit every frame.
        lines += [f'{tick} keys forward' for tick in range(601, stop)]
        lines += [f'{stop} keys none']
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--snapshot', type=Path, help='Optional existing slot 1, read only')
    parser.add_argument('--sdl', action='store_true', help='Use physical W through InputManager with SDL dummy drivers')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    before = hashlib.sha256(args.snapshot.read_bytes()).hexdigest() if args.snapshot else None
    with tempfile.TemporaryDirectory(prefix='recvx-statue-') as temporary:
        root = Path(temporary)
        out = args.output.resolve() if args.output else root
        out.mkdir(parents=True, exist_ok=True)
        controls = root / 'controls.ini'
        subprocess.run([str(runtime), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)

        def run(name, frames, stop=1201, sdl=False):
            script, state = out / (name + '.txt'), out / (name + '.json')
            script.write_text(replay(bool(args.snapshot), sdl, stop))
            state.unlink(missing_ok=True)
            command = [str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'),
                       '--user-dir', str(root / 'user'), '--controls', str(controls),
                       '--debug-config', str(root / 'debug.ini'), '--in-game', '--room', 'RM_0150.RDX',
                       '--ss', '1', '--renderer', 'software', '--enemy-ai', 'on',
                       '--frames', str(3 * frames + 100 if sdl else frames),
                       '--replay', str(script), '--state', str(state)]
            if args.snapshot:
                command += ['--snapshots', str(args.snapshot.resolve().parent)]
                assert args.snapshot.name == 'snapshot-1.rcvxsnap'
            env = os.environ.copy()
            if sdl:
                env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
            else:
                command += ['--headless']
            with (out / (name + '.log')).open('w') as log:
                subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            s = json.loads(state.read_text())
            assert s['room'] == 'RM_0150.RDX' and not s['debug_open'], (name, s['room'], s['debug_open'])
            assert not s['blocked_commands'] and not s['pending_commands'], (name, s['pending_commands'])
            return s

        walking = run('walking-contact', 620)
        assert walking['player_work']['mode'][2] == 22, ('walking contact never entered rpsh', walking['player_work'])
        assert any((item >> 16) == 103 for item in walking['items']), 'sword not acquired through GetItem'
        # GetItem hides no model; the room script's bhItmCk (cb_flg 0x800) sets
        # itwp stflg 0x1000000 only (event.c:2127), never bhSetDispObj mdflg.
        sword = [item for item in walking['room_items'] if item['identity'] == 103]
        assert sword and all(item['inactive'] and not item['hidden'] for item in sword), ('sword model not script-deactivated', sword)
        assert all(event in walking['events'] for event in (5, 6, 7, 8)), 'original sword/gas events missing'
        assert not (walking['event_flags'][0] & 0x20000000), 'statue solved before rotating'
        print('Original sword pickup and gas; walking contact enters mode2 22 without forced modes.')

        released = run('release-and-regrip', 900, stop=650)
        assert released['player_work']['mode'][2] != 22, ('releasing forward never exits rpsh', released['player_work'])
        assert not (released['work_flags'][14] & 0x10080), 'grip flags survive release'
        assert not (released['event_flags'][0] & 0x20000000), 'short grip incorrectly solves puzzle'
        print('Releasing forward exits the grip without setting puzzle completion.')

        # bhLoadWork/Player_controll 128 clear mode2 (event.c:12722-12737/
        # :9640): the rpsh action must end with the player work, not continue
        # as a host latch through the stop-gas events.
        after_event = run('after-event', 1150)
        assert 'turning' in after_event and not after_event['turning'], (
            'rpsh remains active after the script reset', after_event.get('turning'), after_event['player_work'])
        assert after_event['player_work']['mode'][2] == 0, (
            'player mode2 still in rpsh after the script reset', after_event['player_work'])

        completed = run('complete', 1300)
        assert completed['event_flags'][0] & 0x20000000, 'rotation never sets ev_flg 2'
        assert all(event in completed['events'] for event in (15, 16, 17, 18, 19)), 'original gas-stop sequence not dispatched'
        assert completed['player_work']['mode'][2] != 22 and not (completed['work_flags'][14] & 0x10080)
        assert not completed['cinematic'] and not completed['player_scripted'] and not completed['dialog_open']
        # player.c:4430/4441 StartVibrationEx(0, 8) from bhCPM2_act_rpsh reaches the
        # runtime vibration unit (vibman.c StartVibrationBasic counts the request).
        assert completed['vibration_requests'] > 0, ('rpsh StartVibrationEx(0, 8) never reached the vibration unit', completed['vibration_requests'])
        print('Statue rotates; ev_flg 2 dispatches original gas-stop events; player released.')
        if args.sdl:
            # SDL --frames counts host-loop iterations, not simulation ticks.
            # Check the completed sequence after a generous loop budget, not
            # an intermediate pose at an assumed tick count.
            physical = run('sdl-complete', 1800, sdl=True)
            assert physical['event_flags'][0] & 0x20000000, 'held physical W never completes the statue'
            assert all(event in physical['events'] for event in (15, 16, 17, 18, 19))
            assert not physical['cinematic'] and not physical['player_scripted']
            assert not (physical['work_flags'][14] & 0x10080)
            print('SDL F9 + held W via InputManager: original puzzle completion and release verified.')
    if args.snapshot:
        assert hashlib.sha256(args.snapshot.read_bytes()).hexdigest() == before, 'user snapshot changed'
        print('Snapshot slot 1 unchanged: ' + before)


if __name__ == '__main__':
    main()
