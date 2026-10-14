#!/usr/bin/env python3
"""Schreibmaschinen-Spielstand (bup_00.c ControlTypewriter / ps2_SaveScreen):

Einen Original-SAVEFILE-Hoststand erzeugen (dasselbe Format wie die
Schreibmaschinen-Slots, snapshot.h SaveFile) und aus dem Titel ueber LOAD GAME
(Cursor 0 -> Titel-Returncode 3 -> Ladebildschirm) wieder laden. Raum, Position,
Inventar, Ausruestung und HP muessen uebereinstimmen.

Der erzeugte Stand wird ueber --saves in ein eigenes Verzeichnis geschrieben,
nie ins echte Benutzerverzeichnis."""
import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', help='Optional executable such as wine64')
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []
    walk = [line for line in (ROOT / 'tests/native/replays/lighter-walk.txt').read_text().splitlines() if not line.startswith('#')]
    with tempfile.TemporaryDirectory(prefix='recvx-typewriter-') as folder:
        root = Path(folder)
        controls = root / 'controls.ini'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)
        saves = root / 'saves'
        saves.mkdir()

        def run(name, frames, replay, extra=()):
            (root / (name + '.txt')).write_text(replay)
            state = root / (name + '.json')
            subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                     '--controls', str(controls), '--snapshots', str(root / 'slots'), '--saves', str(saves), '--headless',
                                     '--frames', str(frames), '--replay', str(root / (name + '.txt')), '--state', str(state), *extra],
                           check=True, stdout=subprocess.DEVNULL)
            return json.loads(state.read_text())

        # 1) Originalstand an einer echten Position schreiben (F5/Debug-Snapshot,
        #    dasselbe SAVEFILE-Format wie die Schreibmaschinen-Slots).
        saved = run('save', 3310, '\n'.join(walk) + '\n3200 debug_key F2\n3201 debug_select room 5\n3202 confirm\n3203 debug_key F2\n'
                    '3260 keys forward\n3290 keys none\n3300 debug_key F2\n3301 debug_select snapshot_save 0\n3302 confirm\n3303 debug_key F2\n')
        assert saved['debug_status'].startswith('Snapshot 1 gespeichert'), saved['debug_status']
        snapshot = root / 'slots' / 'snapshot-1.rcvxsnap'
        assert snapshot.is_file(), 'Snapshot-Datei fehlt'
        # 2) Als Schreibmaschinen-Slot 1 bereitstellen.
        shutil.copyfile(snapshot, saves / 'save-1.rcvxsav')

        # 3) Aus dem Titel LOAD GAME waehlen (Cursor 0 ist mit vorhandenem
        #    Spielstand waehlbar) und Slot 1 laden.
        loaded = run('load', 700, '60 confirm\n200 confirm\n420 confirm\n')
        assert not loaded['title_active'] and not loaded['inventory_open'], loaded
        for key in ('room', 'actor', 'yaw', 'items', 'weapon', 'hp'):
            assert saved[key] == loaded[key], f'{key}: {saved[key]} != {loaded[key]}'
        assert not loaded['blocked_commands'], loaded
        # 4) Echter Raumpfad RM_0010: Farbband (31, Zahl 1) geben, Schreibmaschine
        #    (Interaktion 1) untersuchen, "USE AN INK RIBBON" -> Ja. Das Skript setzt
        #    cb_flg 0x200000, bhCheckSubTask (system.c:569) startet die Aufgabe,
        #    mcWriteStartSaveFile (0x271A50) zaehlt save_ct und verbraucht das Band.
        for f in saves.iterdir():
            f.unlink()
        room = ('30 debug_key F2\n31 debug_select give 31\n32 confirm\n33 debug_key F2\n'
                '40 position 4 0 -26 16384\n70 confirm\n130 confirm\n190 confirm\n250 confirm\n')
        opened = run('room-open', 200, room, ('--in-game', '--room', 'RM_0010.RDX'))
        st, cb, ts = opened['work_flags'][5], opened['work_flags'][10], opened['work_flags'][12]
        assert st & 0x8000000 and cb & 0x200000 and not ts & 0x8000 and opened['work_flags'][6] == 0x30, opened['work_flags']
        done = run('room-save', 260, room, ('--in-game', '--room', 'RM_0010.RDX'))
        st, cb, ts = done['work_flags'][5], done['work_flags'][10], done['work_flags'][12]
        assert not st & 0x8000000 and not cb & 0x200000 and ts & 0x8000 and done['work_flags'][6] == 0xffffffff, done['work_flags']
        assert not any((item >> 16) & 255 == 31 for item in done['items']), done['items']
        slot = saves / 'save-1.rcvxsav'
        assert slot.is_file(), 'Raumspeicherung schrieb keinen Slot'
        data = slot.read_bytes()
        assert int.from_bytes(data[16:20], 'little') == 10 and int.from_bytes(data[20:24], 'little') == 1, data[16:24]
    print('Schreibmaschinen-Spielstand aus dem Titel ueber LOAD GAME geladen (Raum/Position/Inventar/HP gleich).')
    print('RM_0010: Farbband -> Ja startet den Speicherbildschirm; save_ct 1, Band verbraucht, Systemflags zurueck.')


if __name__ == '__main__':
    main()
