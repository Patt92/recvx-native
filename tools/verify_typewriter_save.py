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
import os
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
    parser.add_argument('--output', type=Path, help='Pruefstaende, Logs und Bilder behalten')
    parser.add_argument('--sdl-cancel', action='store_true', help='Escape-Abbruch im SDL-Spiel statt Headless pruefen')
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []
    walk = [line for line in (ROOT / 'tests/native/replays/lighter-walk.txt').read_text().splitlines() if not line.startswith('#')]
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='recvx-typewriter-') as folder:
        root = Path(folder)
        output = args.output.resolve() if args.output else root
        controls = root / 'controls.ini'
        subprocess.run(prefix + [str(args.runtime.resolve()), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)
        saves = root / 'saves'
        saves.mkdir()

        def run(name, frames, replay, extra=(), save_dir=None, screenshot=False, sdl=False):
            script = output / (name + '.txt')
            script.write_text(replay)
            state = output / (name + '.json')
            state.unlink(missing_ok=True)
            command = prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()), '--mods', str(root / 'no-mods'),
                                '--user-dir', str(root / 'user'), '--controls', str(controls), '--snapshots', str(root / 'slots'),
                                '--saves', str(save_dir if save_dir is not None else saves), '--ss', '1',
                                '--frames', str(frames), '--replay', str(script), '--state', str(state), *extra]
            env = os.environ.copy()
            if sdl:
                env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
            else:
                command.append('--headless')
            if screenshot:
                picture = output / (name + '.bmp')
                picture.unlink(missing_ok=True)
                command += ['--screenshot', str(picture)]
            with (output / (name + '.log')).open('w') as log:
                result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            assert result.returncode == 0, (output / (name + '.log')).read_text()
            return json.loads(state.read_text())

        if args.sdl_cancel:
            room_args = ('--in-game', '--room', 'RM_0010.RDX')
            run('cancel-fixture', 20,
                '5 debug_key F2\n6 debug_select give 31\n7 confirm\n8 debug_select snapshot_save 0\n9 confirm\n10 debug_key F2\n',
                room_args)
            entry = ('0 sdl_key down F9\n2 sdl_key up F9\n10 position 4 0 -26 16384\n'
                     '40 confirm\n100 confirm\n160 confirm\n')
            for name, keys in (
                ('cancel-files', '220 sdl_key down Escape\n222 sdl_key up Escape\n'),
                ('cancel-confirm', '210 confirm\n220 sdl_key down Escape\n222 sdl_key up Escape\n'
                                   '230 sdl_key down Escape\n232 sdl_key up Escape\n'),
            ):
                state = run(name, 5000, entry + keys + '250 sdl_key down F10\n', room_args,
                            screenshot=True, sdl=True)
                assert 'Typewriter: Speicherbildschirm' in (output / (name + '.log')).read_text(), 'Speichermenue wurde nicht erreicht'
                assert not state['typewriter']['active'], state['typewriter']
                assert state['typewriter']['save_count'] == state['typewriter']['checkpoint_save_count'] == 0, state['typewriter']
                assert not state['work_flags'][5] & 0x8000000 and not state['work_flags'][10] & 0x200000, state['work_flags']
                assert state['work_flags'][12] & 0x8000 and state['work_flags'][6] == 0xffffffff, state['work_flags']
                assert any((item >> 16) & 255 == 31 for item in state['items']), state['items']
                assert not list(saves.iterdir()), 'Abbrechen hat eine Speicherdatei geschrieben'
            print('SDL-Escape: Slotliste verlassen; Speicherrueckfrage zurueck und dann verlassen; kein Bandverbrauch und keine Speicherdatei.')
            return

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
        loaded = run('load', 700, '60 confirm\n200 confirm\n420 confirm\n480 confirm\n')
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
                '40 position 4 0 -26 16384\n70 confirm\n130 confirm\n190 confirm\n250 confirm\n310 confirm\n')
        room_args = ('--in-game', '--room', 'RM_0010.RDX')

        def fresh_run(name, frames, replay=room, screenshot=False):
            folder = root / name
            folder.mkdir()
            return run(name, frames, replay, room_args, folder, screenshot), folder

        def flag56(state):
            return bool(state['event_flags'][1] & 0x80)

        opened, _ = fresh_run('room-open', 200)
        st, cb, ts = opened['work_flags'][5], opened['work_flags'][10], opened['work_flags'][12]
        assert st & 0x8000000 and cb & 0x200000 and not ts & 0x8000 and opened['work_flags'][6] == 0x30, opened['work_flags']
        assert opened['typewriter']['phase'] == 0 and opened['typewriter']['message'] == 0, opened['typewriter']
        typing, _ = fresh_run('room-typing', 312, screenshot=True)
        assert typing['typewriter']['message'] == 484, typing['typewriter']
        assert typing['typewriter']['save_count'] == 1 and typing['typewriter']['checkpoint_save_count'] == 0, typing['typewriter']
        # RoomNameSwitch(1) -> Type_Space[0] = 28. Ende nach 6*28-3 Tipp-Paessen.
        end_frame = 311 + (6 * 28 - 3)
        last, _ = fresh_run('room-last-character', end_frame)
        assert last['typewriter']['message'] == 484 and last['typewriter']['checkpoint_save_count'] == 0, last['typewriter']
        success, _ = fresh_run('room-success', end_frame + 1, screenshot=True)
        assert success['typewriter']['message'] == 485 and success['typewriter']['checkpoint_save_count'] == 1, success['typewriter']
        done, saved_dir = fresh_run('room-save', 510)
        st, cb, ts = done['work_flags'][5], done['work_flags'][10], done['work_flags'][12]
        assert not st & 0x8000000 and not cb & 0x200000 and ts & 0x8000 and done['work_flags'][6] == 0xffffffff, done['work_flags']
        assert not done['typewriter']['active'] and done['typewriter']['checkpoint_save_count'] == 1, done['typewriter']
        assert not any((item >> 16) & 255 == 31 for item in done['items']), done['items']
        slot = saved_dir / 'save-1.rcvxsav'
        assert slot.is_file(), 'Raumspeicherung schrieb keinen Slot'
        data = slot.read_bytes()
        assert int.from_bytes(data[16:20], 'little') == 10 and int.from_bytes(data[20:24], 'little') == 1, data[16:24]
        assert not flag56(done), done['event_flags']

        # Reale Host-I/O-Stoerung: nur die temporaere Zieldatei blockieren, keine Kartenentnahme simulieren.
        def failed_run(name, frames, replay):
            folder = root / name
            folder.mkdir()
            blocked = folder / 'save-1.rcvxsav.tmp'
            blocked.mkdir()
            state = run(name, frames, replay, room_args, folder, screenshot=True)
            assert not (folder / 'save-1.rcvxsav').exists(), 'Schreibfehler erzeugt dennoch einen Spielstand'
            assert state['typewriter']['save_count'] == 1 and state['typewriter']['checkpoint_save_count'] == 0, state['typewriter']
            assert not any((item >> 16) & 255 == 31 for item in state['items']), state['items']
            assert flag56(state), state['event_flags']
            return state

        failed = failed_run('write-error', 320, room)
        assert failed['typewriter']['message'] == 486, failed['typewriter']
        back = failed_run('write-error-files', 350, room + '340 confirm\n')
        assert back['typewriter']['message'] == 0 and back['typewriter']['active'], back['typewriter']
        question = failed_run('write-error-question', 362, room + '340 confirm\n360 cancel\n')
        assert question['typewriter']['message'] == 253 and question['typewriter']['active'], question['typewriter']
        returned = failed_run('write-error-return', 382, room + '340 confirm\n360 cancel\n380 confirm\n')
        assert returned['typewriter']['message'] == 0 and returned['typewriter']['active'], returned['typewriter']
        left = failed_run('write-error-leave', 422, room + '340 confirm\n360 cancel\n380 up\n400 confirm\n')
        assert not left['typewriter']['active'] and left['work_flags'][12] & 0x8000, left['typewriter']

        # Fehler -> Slotliste -> anderer, beschreibbarer Slot: Sonderspeichern loescht Flag 56.
        retry_dir = root / 'write-retry'
        retry_dir.mkdir()
        (retry_dir / 'save-1.rcvxsav.tmp').mkdir()
        retry_replay = room + '340 confirm\n360 down\n380 confirm\n400 confirm\n'
        retried = run('write-retry', 410, retry_replay, room_args, retry_dir)
        assert not flag56(retried), retried['event_flags']
        assert retried['typewriter']['message'] == 484 and retried['typewriter']['save_count'] == 1, retried['typewriter']
        assert retried['typewriter']['checkpoint_save_count'] == 0, retried['typewriter']
        special = (retry_dir / 'save-2.rcvxsav').read_bytes()
        assert special[25:29] == bytes((6, 0, 0, 0)), special[25:29]
        assert not int.from_bytes(special[40:44], 'little') & 0x80, 'Flag 56 steht im Sonderspielstand'
        assert not (retry_dir / 'save-1.rcvxsav').exists(), 'Fehler-Slot wurde unerwartet beschrieben'
        failed_run('write-error-again', 390, room + '340 confirm\n360 confirm\n380 confirm\n')
        completed_dir = root / 'write-retry-complete'
        completed_dir.mkdir()
        (completed_dir / 'save-1.rcvxsav.tmp').mkdir()
        completed = run('write-retry-complete', 620, retry_replay, room_args, completed_dir)
        assert not completed['typewriter']['active'] and not flag56(completed), completed['typewriter']
        assert completed['typewriter']['save_count'] == completed['typewriter']['checkpoint_save_count'] == 1, completed['typewriter']
    print('Schreibmaschinen-Spielstand aus dem Titel ueber LOAD GAME geladen (Raum/Position/Inventar/HP gleich).')
    print('RM_0010: Continue-Punkt erst nach der Tippanimation; Schreibfehler, Wiederholung und Abbruchrueckfrage geprueft.')


if __name__ == '__main__':
    main()
