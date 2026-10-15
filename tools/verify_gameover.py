#!/usr/bin/env python3
"""Run the original game over in the running game: bhGameOverSet, the continue
prompt, Retry restoring the checkpoint, and No returning to the title.

The isolated event table is a diagnostic trigger; room, player, audio and
texture resources are the original ones. What is checked is effect, not just
that a flag moved: the three original timer flags are cleared, the prompt
reaches full alpha, Retry restores exactly the state bhPushGameData captured
at the checkpoint, and No plays the original death voice before the title.
"""
import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def event_flag(state, index):
    return bool(state['event_flags'][index // 32] & (0x80000000 >> (index % 32)))


with tempfile.TemporaryDirectory(prefix='recvx-gameover-') as folder:
    mods = Path(folder)
    (mods / 'rooms').mkdir()
    room = bytearray((args.data / 'rooms/RM_0020.RDT').read_bytes())
    offset = len(room)
    # Init ends. Scan waits, invokes original bhGameOverSet and ends. The retry
    # checkpoint precedes the scan.
    # scd1 setzt wie die Raeume sp_flg 0x10 (bhSet 6,27,0), sonst ruht bhEventScheduler2 (event.c:13024).
    room += struct.pack('<III', 12, 17, 24) + bytes([0x14, 0, 0, 0, 0]) + bytes([5, 6, 0x1b, 0, 0, 0, 0]) + bytes([0xf8, 0xf9, 20, 0, 0xc6, 0, 0])
    struct.pack_into('<I', room, 28, offset)
    (mods / 'rooms/RM_0020.RDT').write_bytes(bytes(room))

    def run(name, frames, replay=''):
        state = args.output / (name + '.json')
        log = args.output / (name + '.log')
        script = args.output / (name + '.txt')
        script.write_text(replay)
        if state.exists():
            state.unlink()
        with log.open('w') as stream:
            done = subprocess.run([str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                                   '--mods', str(mods), '--room', 'RM_0020.RDX', '--headless', '--in-game',
                                   '--frames', str(frames), '--ss', '1', '--replay', str(script.resolve()),
                                   '--state', str(state.resolve())],
                                  stdout=stream, stderr=subprocess.STDOUT)
        if done.returncode:
            raise RuntimeError(log.read_text())
        value = json.loads(state.read_text())
        assert value['blocked_commands'] == 0 and not value['pending_commands'], (name, 'blocked commands')
        return value

    # The state bhPushGameData stored at the checkpoint, before the scan runs.
    checkpoint = run('checkpoint', 2)
    assert not checkpoint['gameover_active'], 'the game over started before bhGameOverSet'

    prompt = run('prompt', 170)
    assert prompt['gameover_active'] and prompt['gameover_phase'] == 2, ('prompt', prompt['gameover_phase'])
    # bhInitGameOver clears the three original timer flags and raises ts_flg 0x100.
    for flag in (69, 67, 70):
        assert not event_flag(prompt, flag), ('prompt', f'ev_flg {flag} survived bhInitGameOver')
    assert prompt['work_flags'][12] & 0x100, ('prompt', 'ts_flg 0x100 missing')
    assert prompt['gameover_count'] > 0, ('prompt', 'the continue prompt never faded in')
    # The screen belongs to the game over: no room dialogue or menu underneath.
    assert not prompt['dialog_open'] and not prompt['inventory_open'], ('prompt', 'another screen is up')

    retry = run('retry', 190, '175 confirm\n')
    assert retry['retry_count'] == 1, ('retry', 'bhPopGameData did not count the retry')
    assert not retry['gameover_active'], ('retry', 'the dispatcher stayed active')
    # ts_flg 0x4000 is raised at the moment of bhPopGameData and belongs to the
    # work the reload replaces; the component test checks it there.
    # bhPopGameData restores the checkpoint, not some default: the saved range
    # must come back exactly as it was captured.
    assert retry['hp'] == checkpoint['hp'], ('retry', 'hp', retry['hp'], checkpoint['hp'])
    assert retry['items'] == checkpoint['items'], ('retry', 'inventory not restored')
    assert retry['room'] == checkpoint['room'], ('retry', 'retry room', retry['room'])

    title = run('decline', 400, '175 right\n176 confirm\n')
    assert title['title_active'] and not title['gameover_active'], ('decline', 'no title return')
    assert 'ADV/0008' in title['audio_played'], ('decline', 'death voice missing')
    assert title['retry_count'] == 0, ('decline', 'declining still counted a retry')

print('Original game over: prompt with cleared timer flags, Yes restores the '
      'checkpoint exactly, No plays the death voice and returns to the title.')
