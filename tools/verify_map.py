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

    # 2. Closing returns to the status menu and releases gm_flg 0x80000.
    closed = run(runtime, data, folder, 'status-map-close', 'RM_0020.RDX', 320, REPLAY + CLOSE)
    after = require_binding(closed, 'status-map-close')
    assert not after['active'], 'the map screen did not close'
    assert not (closed["work_flags"][11] & 0x80000), 'gm_flg 0x80000 was not released'
    assert closed['inventory_open'], 'closing the map did not return to the status menu'

    # 3. Event entry: the room that hands out a map (RM_0060, it_flg 133).
    #    Entered through the room's own script, not by forcing the task.
    event = run(runtime, data, folder, 'event-map', 'RM_0060.RDX', 400, REPLAY)
    assert not event['blocked_commands'], ('blocked event commands', event['event_tasks'])
    print('map screen verified:',
          f"mode {screen['mode']}, {len(screen['tags'])} tags, {screen['draws']} draw calls")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'build/d10-map/recvx-runtime')
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
