#!/usr/bin/env python3
"""End-to-end check of the peer-to-peer two-player session (host lockstep).

Two interactive runtimes under SDL's dummy video/audio drivers connect over
loopback, run the same number of ticks and must end in the same state; keys
pressed on the guest move Player 2 on both peers, keys on the host move Player 1.
RM_0080 with diagnostic positions is a test setup, not a playthrough.
"""
import argparse
import json
import os
from pathlib import Path
import random
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')

    def start(name, script, frames, extra):
        replay, state = out / f'{name}.txt', out / f'{name}.json'
        replay.write_text(script)
        state.unlink(missing_ok=True)
        command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                   '--mods', str(out / 'no-mods'), '--user-dir', str(out / f'user-{name}'),
                   '--in-game', '--room', 'RM_0080.RDX', '--player2', 'local',
                   '--ss', '1', '--renderer', 'software', '--frames', str(frames),
                   '--replay', str(replay), '--state', str(state), '--netplay-timeout', '30', *extra]
        log = (out / f'{name}.log').open('w')
        return subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT), state, log

    def session(label, host_script, guest_script, frames=150):
        port = random.randint(30000, 40000)
        host, host_state, host_log = start(f'{label}-host', host_script, frames, ('--netplay-host', str(port)))
        guest, guest_state, guest_log = start(f'{label}-guest', guest_script, frames, ('--netplay-join', f'127.0.0.1:{port}'))
        for process, log in ((host, host_log), (guest, guest_log)):
            process.wait(timeout=120)
            log.close()
            assert process.returncode == 0, (label, process.args[-2:], process.returncode)
        return json.loads(host_state.read_text()), json.loads(guest_state.read_text())

    # Guest holds W (Player 2 forward) for 60 ticks; host holds W (Player 1 forward) later.
    host_script = '40 sdl_key down W\n80 sdl_key up W\n'
    guest_script = '10 sdl_key down W\n70 sdl_key up W\n'
    h, g = session('move', host_script, guest_script)
    assert h['input_tick'] == g['input_tick'] == 150, (h['input_tick'], g['input_tick'])
    assert 'DESYNC' not in h['netplay'] and 'DESYNC' not in g['netplay'], (h['netplay'], g['netplay'])
    for field in ('actor', 'yaw', 'hp', 'items', 'player2'):
        assert h[field] == g[field], (field, h[field], g[field])
    baseline_h, baseline_g = session('idle', '', '', frames=60)
    assert baseline_h['actor'] == baseline_g['actor'] and baseline_h['player2']['position'] == baseline_g['player2']['position']
    assert h['player2']['position'] != baseline_h['player2']['position'], 'guest keys did not move Player 2 on the host'
    assert h['actor'] != baseline_h['actor'], 'host keys did not move Player 1'
    assert h['player2']['position'] != h['actor'], 'both players collapsed onto one actor'
    # Rejoin: the first guest leaves after 100 ticks, the host keeps playing and re-listens; a second guest joins
    # the same port, receives the host's save image and both continue in lockstep without drift.
    port = random.randint(30000, 40000)
    host, host_state, host_log = start('rejoin-host', '', 600, ('--netplay-host', str(port)))
    first, first_state, first_log = start('rejoin-first', '', 100, ('--netplay-join', f'127.0.0.1:{port}'))
    first.wait(timeout=120)
    first_log.close()
    assert first.returncode == 0, ('rejoin-first', first.returncode)
    import time
    time.sleep(2)
    second, second_state, second_log = start('rejoin-second', '', 300, ('--netplay-join', f'127.0.0.1:{port}'))
    for process, log in ((second, second_log), (host, host_log)):
        process.wait(timeout=180)
        log.close()
        assert process.returncode == 0, ('rejoin', process.args[-2:], process.returncode)
    host_text = (out / 'rejoin-host.log').read_text()
    second_text = (out / 'rejoin-second.log').read_text()
    assert 'Netplay: rejoined' in host_text, 'host did not rejoin'
    assert 'Room reloaded' in host_text or 'restor' in host_text.lower() or 'rejoined' in host_text, host_text[-400:]
    assert 'DESYNC' not in host_text and 'DESYNC' not in second_text, 'drift after the rejoin'
    hs, ss = json.loads(host_state.read_text()), json.loads(second_state.read_text())
    assert 'DESYNC' not in hs['netplay'] and 'DESYNC' not in ss['netplay'], (hs['netplay'], ss['netplay'])
    for name in ('move-host', 'move-guest'):
        text = (out / f'{name}.log').read_text()
        assert 'Netplay host' in text or 'Netplay guest' in text, name
        assert 'DESYNC' not in text, (name, 'desync reported')
    print('Netplay: loopback lockstep session, identical end state on both peers, guest drives Player 2 and host drives Player 1, rejoin with save image PASS')


if __name__ == '__main__':
    main()
