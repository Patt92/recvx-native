#!/usr/bin/env python3
"""RM_0070 original scripted Director's Memo pickup and reader completion.
Player placement is diagnostic; scripts, inventory and progress flags are intact.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--data', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
runtime = str(a.runtime.resolve())
subprocess.run([runtime, '--write-controls', str(out / 'controls.ini')], check=True, stdout=subprocess.DEVNULL)
shutil.copyfile(Path(__file__).resolve().parents[1] / 'debug.ini', out / 'debug.ini')
script = out / 'director.txt'
script.write_text('61 position 25 0 41 49152\n61 confirm\n100 cancel\n160 confirm\n180 confirm\n')
results = []
for name, frames in [('reader', 90), ('closed', 230)]:
    state = out / (name + '.json')
    with (out / (name + '.log')).open('w') as log:
        subprocess.run([runtime, '--data', str(a.data.resolve()), '--mods', str(out / 'no-mods'),
                        '--controls', str(out / 'controls.ini'), '--debug-config', str(out / 'debug.ini'),
                        '--snapshots', str(out / 'snapshots'), '--headless', '--in-game',
                        '--room', 'RM_0070.RDX', '--frames', str(frames), '--replay', str(script),
                        '--state', str(state)], check=True, stdout=log, stderr=subprocess.STDOUT)
    v = json.loads(state.read_text())
    assert not v['blocked_commands'], v['event_tasks']
    assert v['file_flags'] & (1 << 8), 'Director Memo ownership missing'
    assert all(((item >> 16) & 255) != 48 for item in v['items']), 'Memo occupies an inventory slot'
    results.append(v)
assert results[0]['files_open'] and results[0]['file_number'] == 8, 'original reader not opened'
assert results[0]['items'] == results[1]['items'], 'closing file changed inventory'
assert not any(results[1][k] for k in ['files_open', 'inventory_open', 'dialog_open']), 'file completion reopened a modal'
print('Original RM_0070 Director Memo: file 8, reader close, unchanged inventory and no second pickup passed.')
