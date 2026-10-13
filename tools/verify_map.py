#!/usr/bin/env python3
"""Open the map screen (map.c) through the real status menu and check its state.

The map item is granted through the debug catalogue and the player is placed by
the debug command: both are DIAGNOSTIC setup, marked as such, not a natural
playthrough. Everything after that runs through the ordinary path: the status
menu's MAP button (sub1.c MainCommand, maincsr 2) raises the map task, the task
runs bhInitMap -> bhControlMap -> bhExitMap, and closing it returns to the
status menu with sys->gm_flg bit 0x80000 released again.

This script needs the runtime binding from work/native_port_D10_map.patch
(CMake target, runtime task, MAP button, modal input). Without it the runtime
reports no `map` section and the script fails with that reason instead of
pretending the screen works.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# ItmDat entry 0 (map.c:44): item 59 carries map flag 96 of group 9.
MAP_ITEM = 59

# Diagnostic setup, then the ordinary status-menu path:
#   inventory  -> StatusMenu slides in (SpriteH, about 16 ticks)
#   cancel     -> MainCommand bar, maincsr 3 = ITEM (CursorMove 0x1000)
#   left       -> maincsr 2 = MAP
#   confirm    -> press MAP (4 + 1 ticks) and raise the map task
REPLAY = f'''0 debug_key F2
2 debug_select give {MAP_ITEM}
4 confirm
8 debug_key F2
40 inventory
80 cancel
88 left
96 confirm
'''

# Ein Abbruch verlaesst die Karte (FtskMapExit). Weitere Abbrueche wuerden den
# danach wieder geoeffneten Statusbildschirm sofort schliessen, system.c case 6.
CLOSE = '240 cancel\n'

# Waehrend der Kartentask laeuft, setzt sub1.c:3141 st_flg 0x40000 und
# bhCheckSubTask (system.c:561, Maske 0x1C040008) sperrt Statusbildschirm und
# Interaktion; nur MapPadMain liest das Pad. Inventar/OK duerfen also nichts
# oeffnen oder ausloesen, die Karte bleibt offen.
LOCKED = '150 inventory\n160 confirm\n170 up\n'
ST_FLG_MAP = 0x40000


def read_bmp(path):
    """24-bit bottom-up BMP (SoftwareRenderer::save_bmp) -> (width, height, pixel(x, y) -> (r, g, b))."""
    data = path.read_bytes()
    assert data[:2] == b'BM', 'not a BMP'
    offset = int.from_bytes(data[10:14], 'little')
    width = int.from_bytes(data[18:22], 'little')
    height = int.from_bytes(data[22:26], 'little')
    row = (width * 3 + 3) & ~3
    def pixel(x, y):
        at = offset + (height - 1 - y) * row + x * 3
        return data[at + 2], data[at + 1], data[at]
    return width, height, pixel


# map.c MapCol[0] 0xFF000800 (background fill) and MapCol[1] 0xFF003000 (grid).
BACKGROUND = (0x00, 0x08, 0x00)
GRID = (0x00, 0x30, 0x00)


def check_picture(name, folder):
    width, height, pixel = read_bmp(folder / f'{name}.bmp')
    assert (width, height) == (640, 480), (width, height)
    # FsubZoomScreen masks the outside with a 0x80000000 plate at -8.
    # Background/grid probes must sit inside ScrIn (56,64)-(585,417).
    assert pixel(100, 100) == BACKGROUND, ('background inside viewport', pixel(100, 100))
    assert pixel(5, 5) == (0, 4, 0), ('frame plate blend', pixel(5, 5))
    # MapDrawLine targets 0.5 + floor(x): row 96 and column -16+3*32=80.
    assert pixel(100, 96) == GRID and pixel(100, 97) == BACKGROUND, ('grid row 96', pixel(100, 96), pixel(100, 97))
    assert pixel(80, 100) == GRID and pixel(81, 100) == BACKGROUND, ('grid column 80', pixel(80, 100), pixel(81, 100))
    foreground = sum(1 for y in range(96, 400, 2) for x in range(64, 550, 2)
                     if pixel(x, y) not in (BACKGROUND, GRID, (0, 0, 0), (0, 4, 0)))
    assert foreground > 500, ('no coloured map geometry inside the viewport', foreground)
    # FsubModeMessage: X button sprite (24x24 around 464,441) from the map texture.
    button = sum(1 for y in range(429, 453) for x in range(452, 476)
                 if pixel(x, y) not in (BACKGROUND, GRID, (0, 0, 0), (0, 4, 0)))
    assert button > 0, 'mode message sprite missing at 464,441'
    return foreground


def run(runtime, data, folder, name, room, frames, replay):
    (folder / f'{name}.txt').write_text(replay)
    state = folder / f'{name}.json'
    with (folder / f'{name}.log').open('w') as log:
        subprocess.run([str(runtime.resolve()), '--data', str(data.resolve()),
            '--headless', '--in-game', '--room', room, '--frames', str(frames),
            '--replay', str(folder / f'{name}.txt'), '--state', str(state),
            '--screenshot', str(folder / f'{name}.bmp')],
            cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    return json.loads(state.read_text())


def require_binding(result, name):
    if 'map' not in result:
        raise SystemExit(
            f'{name}: the runtime reports no "map" state section.\n'
            'The map screen is implemented in native/map_screen.* but not yet wired into\n'
            'the runtime. Apply work/native_port_D10_map.patch (CMake target, runtime task,\n'
            'MAP button, modal input) and run this script again.')
    return result['map']


def check(runtime, data, folder):
    # 1. Status menu -> MAP in an original room, with the map item owned.
    opened = run(runtime, data, folder, 'status-map', 'RM_0020.RDX', 240, REPLAY)
    assert not opened['blocked_commands'], ('blocked event commands', opened['event_tasks'])
    screen = require_binding(opened, 'status-map')

    assert screen['active'], 'the MAP button did not raise the map task'
    # bhInitMap: MP_SET_SUB from the status menu sets status bit 2 and plays no
    # opening SE (map.c:166); the sub-screen entry is system.c:853.
    assert screen['status'] & 0x4, ('map not entered as MP_SET_SUB', hex(screen['status']))
    # The map data really came from MRY.AFS over sys->dor_partid (sdfunc.c:441).
    assert screen['requested_files'][:1] == [opened['player']], \
        ('index file is not the player id', screen['requested_files'])
    assert screen['partition'] == 5, ('map data not read from MRY.AFS', screen['partition'])
    # MapCnvStatus2Flag put the owned map into flag group 9.
    assert screen['item_flags'].get('96'), 'owning item 59 did not set map flag 9/96'
    # The screen has to show something: background grid plus map geometry.
    assert screen['draws'] > 10, ('the map screen drew almost nothing', screen['draws'])
    if screen['no_map']:
        raise SystemExit('status-map: the stage floor has no map data (MP_MOD_WAIT_NOMAP); '
                         'check stage/floor selection before claiming the screen works')
    assert screen['tags'], 'no room tags were entered (MapTagEntry)'
    assert screen['mode'] in (7, 8), ('map not in a view mode', screen['mode'])
    assert opened['work_flags'][5] & ST_FLG_MAP, 'MAP from the status bar did not set st_flg 0x40000 (sub1.c:3141)'
    # Teil B: the draw list reached the renderer and the frame shows the map.
    rendered = screen['rendered']
    assert screen['initialized'], 'map task not initialised while active'
    assert rendered['fills'] >= 1 and rendered['lines'] >= 30, ('background/grid not rendered', rendered)
    assert rendered['objects'] > 0 and rendered['objects_without_model'] == 0, ('floor objects not rendered', rendered)
    assert rendered['sprites'] >= 2 and rendered['sprites_without_texture'] == 0, ('sprites not rendered', rendered)
    assert rendered['markers_without_model'] == 0 and rendered['compass'] == 1, ('markers/compass', rendered)
    assert rendered['marker_models'] == 4 and rendered['map_textures'] >= 1, ('index resources', rendered)
    foreground = check_picture('status-map', folder)

    # 2. Input lock while the map task owns the screen (bhCheckSubTask gate).
    locked = run(runtime, data, folder, 'status-map-locked', 'RM_0020.RDX', 240, REPLAY + LOCKED)
    lock_screen = require_binding(locked, 'status-map-locked')
    assert lock_screen['active'], 'inventory/OK during the map closed the map task'
    assert not locked['inventory_open'], 'the status screen opened over the running map'
    assert not locked['dialog_open'], 'OK during the map reached an interaction'
    assert locked['events'] == opened['events'], ('OK during the map dispatched events', locked['events'])

    # 2b. Task 7 (bhMainSequence -> bhControlPlayer) is stopped while the map owns
    #     the screen: system.c:777 sets ts_flg 0x180 for the game/event entry, and
    #     the status-menu entry already left sp_flg 0x30 from bhCheckSubTask
    #     (system.c:608) so bhControlPlayer (player.c:1365, sp_flg & 0x1) cannot run.
    #     A held direction must therefore not move the player.
    walked = run(runtime, data, folder, 'status-map-move', 'RM_0020.RDX', 240, REPLAY + '150 keys forward\n')
    assert walked['map']['active'], 'the map closed while a direction was held'
    assert walked['actor'] == opened['actor'], (
        'the player moved while the map was open', opened['actor'], walked['actor'])
    assert (walked['motion'], walked['animation_frame']) == (opened['motion'], opened['animation_frame']), (
        'the player motion advanced while the map was open',
        (opened['motion'], opened['animation_frame']), (walked['motion'], walked['animation_frame']))

    # 3. Closing returns to the status menu and releases gm_flg 0x80000 and st_flg 0x40000.
    closed = run(runtime, data, folder, 'status-map-close', 'RM_0020.RDX', 320, REPLAY + CLOSE)
    after = require_binding(closed, 'status-map-close')
    assert not after['active'], 'the map screen did not close'
    assert not (closed["work_flags"][11] & 0x80000), 'gm_flg 0x80000 was not released'
    assert not (closed["work_flags"][5] & ST_FLG_MAP), 'st_flg 0x40000 was not released (system.c:886)'
    assert closed['inventory_open'], 'closing the map did not return to the status menu'
    # bhSetScreenFade(0xFF000000, 8) on exit (map.c:540) ran to black before FtskMapExit
    # released the task; the status screen holds it undrawn (system.c:921 draws only
    # on its own exit).
    assert closed['screen_fade']['an'] == 255 and not closed['screen_fade']['running'], ('exit fade', closed['screen_fade'])

    # 3b. Leaving the status screen afterwards: bhSetScreenFade(fade_pbk, 3) with
    #     fade_pbk 0 (system.c:921) clears the black over the game picture.
    returned = run(runtime, data, folder, 'status-map-return', 'RM_0020.RDX', 360, REPLAY + CLOSE + '290 cancel\n300 cancel\n')
    assert not returned['inventory_open'] and not returned['map']['active'], 'status screen did not close after the map'
    assert returned['screen_fade']['an'] == 0 and not returned['screen_fade']['running'], ('return fade', returned['screen_fade'])
    _, _, pixel = read_bmp(folder / 'status-map-return.bmp')
    assert any(pixel(x, y) != (0, 0, 0) for y in range(0, 480, 16) for x in range(0, 640, 16)), 'game picture stayed black after the map'

    # Re-enter with the same MapScreen/MapRenderer owners after returning to
    # gameplay: resources must be reloaded, not a stale generation-1 cache.
    again_replay = REPLAY + CLOSE + '290 cancel\n300 cancel\n340 inventory\n380 cancel\n388 left\n396 confirm\n'
    again = run(runtime, data, folder, 'status-map-reopen', 'RM_0020.RDX', 480, again_replay)
    assert again['map']['active'] and again['map']['initialized'], 'map did not re-open'
    assert again['map']['rendered']['objects'] > 0, 'floor resources lost on re-entry'
    check_picture('status-map-reopen', folder)

    # 4. Event entry: the room that hands out a map (RM_0060, it_flg 133).
    #    Entered through the room's own script, not by forcing the task.
    event = run(runtime, data, folder, 'event-map', 'RM_0060.RDX', 400, REPLAY)
    assert not event['blocked_commands'], ('blocked event commands', event['event_tasks'])
    print('map screen verified:',
          f"mode {screen['mode']}, {len(screen['tags'])} tags, {screen['draws']} draw calls, "
          f"{rendered['objects']} floor objects, {rendered['sprites']} sprites, {foreground} foreground samples")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'build/macos-arm64/recvx-runtime')
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--out', type=Path, help='keep logs, state and screenshots here')
    arguments = parser.parse_args()
    if not arguments.runtime.exists():
        raise SystemExit(f'runtime not found: {arguments.runtime}')
    if arguments.out:
        arguments.out.mkdir(parents=True, exist_ok=True)
        return check(arguments.runtime, arguments.data, arguments.out)
    with tempfile.TemporaryDirectory(prefix='recvx map ') as folder:
        return check(arguments.runtime, arguments.data, Path(folder))


if __name__ == '__main__':
    raise SystemExit(main())
