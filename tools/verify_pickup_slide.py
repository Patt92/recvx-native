#!/usr/bin/env python3
"""GetItem ignores the pad while the status screen slides in (sub1.c GetItem,
every testmode requires !(statusflg & 0x40)). RM_7090 handgun bullets (ETC 5);
the placement is diagnostic, the room script and pickup flow are original."""
import argparse, json, subprocess
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--data', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def run(name, script, frames):
    replay = a.output / (name + '.txt'); replay.write_text(script)
    state = a.output / (name + '.json'); state.unlink(missing_ok=True)
    subprocess.run([str(a.runtime.resolve()), '--data', str(a.data.resolve()), '--in-game', '--room', 'RM_7090.RDX',
                    '--headless', '--frames', str(frames), '--replay', str(replay.resolve()), '--state', str(state.resolve())],
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, check=True)
    return json.loads(state.read_text())

base = '10 position 27.5 0 50 16384\n30 confirm\n34 confirm\n'
early = run('early-confirm', base, 60)
assert early['dialog_open'] and early['dialog_index'] == 157, (early['dialog_index'], early['items'][:6])
assert all((v >> 16) & 255 != 12 for v in early['items'][1:12]), early['items'][:12]
late = run('settled-confirm', base + '60 confirm\n', 70)
assert late['dialog_index'] == 158 and any((v >> 16) & 255 == 12 for v in late['items'][2:12]), late['items'][:12]
print('GetItem question ignores Confirm during the slide-in and accepts it afterwards.')
