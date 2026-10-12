#!/usr/bin/env python3
"""A01-A04 Spielerkampf: Tabellenregression, Komponententest und Runtime-Replay.

Drei getrennte Stufen, jede einzeln abschaltbar:

  --tables    Reine Originaldatentabellen des Kampfs gegen die Quelle
              (weapon.c:118 WpnAngTab, player.c:214 PlyLegRoute).
  --unit      Komponententest tests/native/combat_state_test.cpp: er treibt
              native/player_combat_core.c direkt und prueft die Originalfolgen
              Ziehen/Zielen/Schuss/Rueckstoss/Nachladen/Wegstecken, Zielwahl
              mit Wandverdeckung, Zielwechsel, Hoch-/Tiefzielen, Messerhieb
              mit Treffer/Fehlschlag/Wandrueckstoss und die Schadensbuchung in
              echten RECVX_ENEMY_WORKs.
  --runtime   Originalablauf in der Runtime: Steve-Szene laeuft durch, der
              M93R wird per Skript uebergeben, danach Zielen, zwei Schuesse,
              Munitionsabzug, Schussklang und Wegstecken.

Der Runtime-Schritt braucht den Anschluss aus
work/native_port_A01_A04_combat.patch: native/runtime.cpp nimmt sys->st_flg
0x4 in seine `blocked`-Bedingung auf, und genau dieses Bit setzt
bhCPM1_act_atk (player.c:4483) selbst. Ohne den Patch sperrt sich der
Kampfpass nach einem Tick selbst aus; dieses Skript meldet das namentlich.
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / 'src/ps2/veronica/prog'
NATIVE = ROOT / 'native'


def numbers(text):
    return [int(v) for v in re.findall(r'-?\d+', text)]


def table_block(path, marker, closing='};'):
    text = path.read_text(errors='replace')
    start = text.index(marker)
    end = text.index(closing, start)
    return text[start + len(marker):end]


def check_tables():
    """Die beiden Tabellen in native/player_combat_core.c gegen die Quelle."""
    native = (NATIVE / 'player_combat_core.c').read_text()

    # weapon.c:118 const int WpnAngTab[21][3]
    want = numbers(table_block(ORIGINAL / 'weapon.c', 'const int WpnAngTab[21][3] = \n{'))
    got = numbers(table_block(NATIVE / 'player_combat_core.c',
                              'const int32_t RECVX_CB_WPN_ANG_TAB[21][3] = {'))
    if len(want) != 63:
        raise SystemExit(f'WpnAngTab in weapon.c has {len(want)} values, expected 63')
    if want != got:
        raise SystemExit(f'WpnAngTab differs from weapon.c:118\n original={want}\n native  ={got}')

    # player.c:214 const char PlyLegRoute[2][7]
    want = numbers(table_block(ORIGINAL / 'player.c', 'const char PlyLegRoute[2][7] = \n{'))
    got = numbers(table_block(NATIVE / 'player_combat_core.c',
                              'const int8_t RECVX_CB_PLY_LEG_ROUTE[2][7] = {'))
    if len(want) != 14:
        raise SystemExit(f'PlyLegRoute in player.c has {len(want)} values, expected 14')
    if want != got:
        raise SystemExit(f'PlyLegRoute differs from player.c:214\n original={want}\n native  ={got}')

    # Der Kampfkern darf keinen Originalkoerper einziehen.
    for forbidden in ('player_combat.inc', 'player_combat_types.inc', 'targeting.inc',
                      'FORBIDDEN_DONTUSE'):
        if forbidden in native:
            raise SystemExit(f'player_combat_core.c references {forbidden}')
    print('Tabellen: WpnAngTab (weapon.c:118) und PlyLegRoute (player.c:214) '
          'stimmen wertgleich mit der Quelle; kein Originalkoerper eingezogen.')


def check_unit(build):
    """Komponententest eigenstaendig uebersetzen und laufen lassen."""
    out = build / 'own'
    out.mkdir(parents=True, exist_ok=True)
    includes = ['-I', str(NATIVE), '-I', str(NATIVE / 'enemies')]
    objects = []
    for name in ('player_combat_core', 'ninja_math', 'ninja_matrix'):
        obj = out / f'{name}.o'
        subprocess.run(['clang', '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
                        '-Wno-unused-parameter', *includes,
                        '-c', str(NATIVE / f'{name}.c'), '-o', str(obj)], check=True)
        objects.append(str(obj))
    binary = out / 'combat_state_test'
    subprocess.run(['clang++', '-std=c++20', '-O1', '-g', *includes, '-o', str(binary),
                    str(ROOT / 'tests/native/combat_state_test.cpp'), *objects], check=True)
    subprocess.run([str(binary)], check=True, cwd=ROOT)


def check_runtime(runtime, data, emulator):
    prefix = [emulator] if emulator else []

    def path(p):
        absolute = str(Path(p).resolve())
        if emulator and runtime.suffix.lower() == '.exe':
            return 'Z:' + absolute.replace('/', '\\')
        return absolute

    with tempfile.TemporaryDirectory(prefix='recvx-combat-') as folder:
        state = Path(folder) / 'handgun.json'
        subprocess.run(prefix + [str(runtime.resolve()), '--data', path(data), '--headless',
                                 '--in-game', '--room', 'RM_0030.RDX', '--replay',
                                 path(ROOT / 'tests/native/replays/combat-handgun.txt'),
                                 '--frames', '3700', '--state', path(state)],
                       check=True, cwd=ROOT)
        saved = json.loads(state.read_text())
        # Originales Skriptgeschenk und automatische Ausruestung (sub1.c WeaponSet).
        if saved['weapon'] != 4:
            raise SystemExit(f"Steve gift/equipment failed: weapon={saved['weapon']}")
        if saved['combat_shots'] != 2 or saved['combat_attacks'] != 2:
            raise SystemExit(
                'Der Kampfpass hat nicht zweimal gefeuert '
                f"(shots={saved['combat_shots']}, attacks={saved['combat_attacks']}). "
                'Fehlt der Anschluss aus work/native_port_A01_A04_combat.patch? '
                'native/runtime.cpp nimmt sys->st_flg 0x4 in `blocked` auf, und '
                'bhCPM1_act_atk (player.c:4483) setzt genau dieses Bit selbst.')
        rounds = [(item & 0xffff) for item in saved['items'] if (item >> 16) & 255 == 9]
        if rounds != [10]:
            raise SystemExit(f'bhCountBullet did not consume two rounds: {rounds}')
        shots = [entry for entry in saved['audio_played'] if entry.startswith('ARMS/0001/0005/')]
        if shots != ['ARMS/0001/0005/slot8', 'ARMS/0001/0005/slot9']:
            raise SystemExit(f'weapon shot sound/alternating slots incorrect (CallPlayerWeaponSeEx): {shots}')
        if saved['motion'] != 42:
            raise SystemExit(f"bhCPM2_act_wre did not return to idle: motion={saved['motion']}")
        if saved['blocked_commands']:
            raise SystemExit('event commands blocked during the combat replay')
        # Ohne die Gegnertabelle (C02) gibt es keine Aufschaltung und keinen
        # Schaden; das ist der dokumentierte Anschlussvertrag, kein Ersatzwert.
        print('Runtime: Steve-Geschenk -> M93R ausgeruestet -> Zielen -> zwei Schuesse '
              '-> Munition 12->10 -> Schussklang -> Wegstecken auf Leerlauf bestanden. '
              'Aufschaltung/Schaden bleiben offen, solange world die Gegnertabelle '
              'nicht exponiert (combat_hits=%d, target=%d).'
              % (saved['combat_hits'], saved['combat_target_right']))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--data', type=Path)
    parser.add_argument('--emulator')
    parser.add_argument('--build', type=Path, default=ROOT / 'build/a01-combat')
    parser.add_argument('--tables', action='store_true')
    parser.add_argument('--unit', action='store_true')
    parser.add_argument('--runtime-only', action='store_true')
    args = parser.parse_args()
    stages = (args.tables, args.unit, args.runtime_only)
    run_all = not any(stages)
    if run_all or args.tables:
        check_tables()
    if run_all or args.unit:
        check_unit(args.build)
    if (run_all or args.runtime_only) and args.runtime and args.data:
        check_runtime(args.runtime, args.data, args.emulator)
    elif run_all or args.runtime_only:
        print('Runtime-Stufe uebersprungen: --runtime und --data angeben.', file=sys.stderr)


if __name__ == '__main__':
    main()
