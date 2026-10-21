#!/usr/bin/env python3
"""Check the real-data title screen (adv.c Adv_BioCvTitle) and the rapid lighter flow after a running film skip.

Title timeline at 30 Hz: Mode 0..3 (frames 0..3), GetSamurai(50) fade-in (30 frames),
PRESS START from frame 34, Start -> 8-frame plate fade-out, 8-frame fade-in, Mode 9.
"""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def bmp_rows(path):
    """Return (width, height, rows) of a 24-bit bottom-up BMP as lists of (r, g, b)."""
    data = Path(path).read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert bits == 24 and height > 0, (bits, height)
    stride = (width * 3 + 3) & ~3
    rows = []
    for y in range(height):
        base = offset + (height - 1 - y) * stride
        rows.append([(data[base + x * 3 + 2], data[base + x * 3 + 1], data[base + x * 3]) for x in range(width)])
    return width, height, rows


def band_mean(rows, x0, x1, y0, y1):
    total = count = 0
    for y in range(y0, y1):
        for r, g, b in rows[y][x0:x1]:
            total += max(r, g, b)
            count += 1
    return total / count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', help='Optional executable such as wine64')
    parser.add_argument('--screenshots', type=Path, help='Keep the title screenshots in this directory')
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []
    with tempfile.TemporaryDirectory(prefix='recvx-frontend-') as folder:
        root = Path(folder)

        def run(name, frames, replay='', screenshot=False):
            state = root / (name + '.json')
            command = prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                       '--headless', '--frames', str(frames), '--state', str(state)]
            if replay:
                inputs = root / (name + '.txt')
                inputs.write_text(replay)
                command += ['--replay', str(inputs)]
            if screenshot:
                command += ['--screenshot', str(root / (name + '.bmp'))]
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
            if screenshot and args.screenshots:
                args.screenshots.mkdir(parents=True, exist_ok=True)
                shutil.copy(root / (name + '.bmp'), args.screenshots / (name + '.bmp'))
            return json.loads(state.read_text())

        # Idle title: PRESS START plate (Mode 6), no sound, no game.
        title = run('title-press-start', 120, screenshot=True)
        assert title['title_active'] and not title['settings_active'] and title['title_mode'] == 6
        assert not title['events'] and not title['audio_played'] and not title['movie_active']
        # Start: CheckStartButton SE 3, plate fades, Mode 9 with Cursor 1 (LOAD GAME needs a memory card).
        start = run('title-game-mode', 120, '40 confirm\n', screenshot=True)
        assert start['title_mode'] == 9 and start['title_cursor'] == 1, (start['title_mode'], start['title_cursor'])
        assert start['audio_played'] == ['COMMON/0003'], start['audio_played']
        assert start['vibration_requests'] == 0
        # Confirm during the Mode 3/4 fade-in is ignored (CheckStartButton only runs in Mode 6).
        early = run('title-early-start', 60, '20 confirm\n')
        assert early['title_mode'] == 6 and not early['audio_played'], early['title_mode']
        # CheckButton: Up on NEW GAME stays (Flag 0), Down clamps on the host QUIT row (MaxFlag 0: no wrap); SE 2 per repeat.
        nav = run('title-cursor', 160, '40 confirm\n70 up\n71 down\n72 down\n73 down\n74 down\n')
        assert nav['title_cursor'] == 3 and nav['audio_played'] == ['COMMON/0003'] + ['COMMON/0002'] * 5, nav['audio_played']
        # Cancel: SE 0, Mode 10 fade-out, back to PRESS START; Cursor[0] persists.
        cancel = run('title-cancel', 160, '40 confirm\n70 down\n71 cancel\n')
        assert cancel['title_mode'] == 6 and cancel['title_cursor'] == 2 and cancel['audio_played'][-1] == 'COMMON/0000', cancel['audio_played']
        # OPTION: SE 3 and the GetSamurai(50) fade lead to the host settings screen (Adv_GameOptionScreen is not ported).
        settings = run('title-option', 140, '40 confirm\n70 down\n71 confirm\n', screenshot=True)
        assert settings['title_active'] and settings['settings_active'] and not settings['events']
        assert settings['audio_played'] == ['COMMON/0003', 'COMMON/0002', 'COMMON/0003'], settings['audio_played']
        # Back from the option screen: Mode 0 with FromTitle 2, fade-in, plate with the cursor on OPTION.
        back = run('title-option-back', 230, '40 confirm\n70 down\n71 confirm\n110 cancel\n')
        assert not back['settings_active'] and back['title_mode'] == 9 and back['title_cursor'] == 2, (back['title_mode'], back['title_cursor'])
        # Host QUIT row: SE 3, leave fade, then the runtime stops (title task re-initialised: Mode 0 instead of 9).
        quit_run = run('title-quit', 200, '40 confirm\n70 down\n71 down\n72 down\n73 confirm\n')
        assert quit_run['title_cursor'] == 3 and quit_run['title_mode'] == 0 and quit_run['audio_played'][-1] == 'COMMON/0003', quit_run
        # Plate pixels (neutral mid-grey text pixels of the atlas plates against the picture): PRESS START plate at (64,304)
        # 512x32; game-mode rows at the original four-row positions 272/304/336/368 (PosYDef[1]); LOAD GAME / OPTION are
        # unselected (0x7F7F7F grey), NEW GAME is the pulsing white cursor row (fewer grey pixels), QUIT is the host font row.
        def grey_text(rows, y0, y1):
            return sum(1 for y in range(y0, y1) for r, g, b in rows[y][64:577] if abs(r - g) < 14 and abs(g - b) < 14 and 95 < max(r, g, b) < 150)
        _, _, rows = bmp_rows(root / 'title-press-start.bmp')
        assert grey_text(rows, 304, 336) > 3 * grey_text(rows, 272, 304), 'PRESS START plate missing'
        _, _, rows = bmp_rows(root / 'title-game-mode.bmp')
        above, load, new, option, quit_row = (grey_text(rows, y, y + 33) for y in (240, 272, 304, 336, 368))
        assert load > 500 and option > 500 and above < 250, (above, load, option)
        assert new < load, ('NEW GAME not the bright cursor row', new, load)
        assert quit_row > above + 100, ('QUIT row missing', quit_row, above)
        film = run('film', 1500, '0 new_game\n')
        assert not film['title_active'] and film['movie_active'] and film['movie_frame'] > 0
        assert 'ADV/0003' in film['audio_played'], 'original title announcement missing'
        assert film['vibration_requests'] >= 1, 'TitleCall StartVibrationEx(16) missing'
        # Hint completion owns inventory opening; held movement must never escape it.
        hint = run('hint-position', 1450, '0 new_game\n1300 confirm\n')
        automatic = run('automatic-inventory', 1600, '0 new_game\n1300 confirm\n1500 confirm\n1501 keys forward\n')
        assert automatic['inventory_open'] and automatic['weapon'] == 0
        assert automatic['actor'] == hint['actor'] and not automatic['blocked_commands']
        assert automatic['audio_played'][-2:]==['COMMON/0003','COMMON/0007'], 'automatic inventory uses wrong opening/slide sounds'
        assert not any(sound.startswith('MULTSPQ1/') for sound in automatic['audio_played']), 'inventory plays walking sounds'
        # Confirm stops an already running movie, followed by original lighter equipment.
        # ItemCommand slides the command window for 4 frames; WeaponSet keeps the status screen open.
        lighter = run('lighter', 4000, '0 new_game\n1300 confirm\n1500 confirm\n1548 up\n1550 confirm\n1555 confirm\n1560 inventory\n')
        assert not lighter['movie_active'] and not lighter['cinematic'] and not lighter['inventory_open']
        assert lighter['weapon'] == 1 and 0 in lighter['events'] and not lighter['blocked_commands']
        assert 'VOICE1/0016' in lighter['audio_played']
        assert not lighter['player_hidden'] and not lighter['player_scripted']
    print('Title task (PRESS START, game-mode plate, cursor/cancel/option/quit), active film skip and rapid hint/inventory/lighter/Rodrigo flow passed.')


if __name__ == '__main__':
    main()
