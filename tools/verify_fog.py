#!/usr/bin/env python3
"""Nebelkette gegen Originalraeume: bhControlEOR (sync.c:143-255) installiert den
Nebel der Ereigniskamera bzw. des Raumschnitts, der Renderer mischt ihn nach der
GS-Formel auf die CNK-Streifen.

Erwartungen kommen aus den Raumdaten, nicht aus dem nativen Code:
  RM_0000, Vorspann uebersprungen (Ereigniskamera 37, Key 0: flg 0x0009,
    fog_col 0x000a0f, nr 128, fr -31): Nebel an mit genau diesen Werten; das
    Bild der Zelle ohne Feuerzeug ist dunkel (mittlere Helligkeit < 24/255),
    deutlich dunkler als ohne Nebel/Abschwaechung (42/255 vor der Portierung).
  RM_0000, Feuerzeug-Szene (Ereigniskamera 1, Key 0: flg 0x0001): Nebel aus.
  RM_0080, normaler Raumschnitt 1 (CAM_WRK flg 0x0108, fog_col 0x17212b,
    nr 128, fr -117): Nebel an mit den Schnittwerten (bhSetCut cut.c:349-377).
Keine blockierten Skriptbefehle, keine njGenerateFogTable3-Luecke.
"""
import argparse
import json
import struct
import subprocess
from pathlib import Path


def mean_brightness(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width = struct.unpack_from('<i', data, 18)[0]
    height = struct.unpack_from('<i', data, 22)[0]
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    row = (width * bpp + 3) // 4 * 4
    total = 0
    count = 0
    for y in range(0, height, 4):
        base = offset + (height - 1 - y) * row
        for x in range(0, width, 4):
            q = base + x * bpp
            total += data[q] + data[q + 1] + data[q + 2]
            count += 3
    return total / count


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    runtime, data = str(args.runtime.resolve()), str(args.data.resolve())

    def run(name, room, frames, replay, screenshot=True):
        script, state, log = out / (name + '.txt'), out / (name + '.json'), out / (name + '.log')
        script.write_text(replay)
        if state.exists():
            state.unlink()
        command = [runtime, '--data', data, '--mods', str(out / 'no-mods'), '--room', room, '--headless',
                   '--in-game', '--frames', str(frames), '--ss', '1', '--replay', str(script), '--state', str(state)]
        if screenshot:
            command += ['--screenshot', str(out / (name + '.bmp'))]
        with log.open('w') as stream:
            done = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
        text = log.read_text()
        assert done.returncode == 0, (name, text[-2000:])
        assert 'njGenerateFogTable3' not in text, (name, 'fog table generation reported a gap')
        value = json.loads(state.read_text())
        assert value['blocked_commands'] == 0, (name, value['blocked_commands'])
        return value

    cell = run('cell-dark', 'RM_0000.RDX', 600, '0 new_game\n1 skip\n')
    assert cell['event_camera'] == 37, cell['event_camera']
    assert cell['fog'] == {'enabled': True, 'color': 0x000a0f, 'near': 128, 'far': -31, 'ct': 0}, cell['fog']
    dark = mean_brightness(out / 'cell-dark.bmp')
    assert dark < 24, ('cell without lighter too bright', dark)

    lighter = run('lighter', 'RM_0000.RDX', 1700,
                  '0 new_game\n1300 confirm\n1500 confirm\n1548 up\n1550 confirm\n1555 confirm\n1560 inventory\n')
    assert lighter['weapon'] == 1 and lighter['event_camera'] == 1, (lighter['weapon'], lighter['event_camera'])
    assert lighter['fog']['enabled'] is False, lighter['fog']

    room = run('rm0080-cut', 'RM_0080.RDX', 300, '', screenshot=False)
    assert room['event_camera'] == -1, room['event_camera']
    assert room['fog'] == {'enabled': True, 'color': 0x17212b, 'near': 128, 'far': -117, 'ct': 0}, room['fog']

    summary = {'cell_brightness': dark, 'cell_fog': cell['fog'], 'lighter_fog': lighter['fog'], 'rm0080_fog': room['fog']}
    (out / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))
    print('verify_fog: PASS')


if __name__ == '__main__':
    main()
