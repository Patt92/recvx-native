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
              Munitionsabzug, Schussklang und Wegstecken. Anschliessend im
              Diagnoseraum RM_0080 Aufschaltung auf einen echten Gegner
              (pp.lck_tbl ueber den Dienst `enemies`, sys->ene / sys->ewk_n)
              und gebuchter Schaden gegen denselben Replay ohne Schuss.

Der Runtime-Schritt braucht den Anschluss aus
work/native_port_A01_A04_combat.patch: native/runtime.cpp nimmt sys->st_flg
0x4 in seine `blocked`-Bedingung auf, und genau dieses Bit setzt
bhCPM1_act_atk (player.c:4483) selbst. Ohne den Patch sperrt sich der
Kampfpass nach einem Tick selbst aus; dieses Skript meldet das namentlich.
"""
import argparse
import json
import math
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
    # rand() der Wasserzweige/1-aus-30: der prozessweite EE-Generator (core_random).
    obj = out / 'core_random.o'
    subprocess.run(['clang++', '-std=c++20', '-O1', '-g', *includes,
                    '-c', str(NATIVE / 'core_random.cpp'), '-o', str(obj)], check=True)
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
        # Aufschaltung und Schaden prueft check_runtime_enemy am echten Gegner.
        print('Runtime: Steve-Geschenk -> M93R ausgeruestet -> Zielen -> zwei Schuesse '
              '-> Munition 12->10 -> Schussklang -> Wegstecken auf Leerlauf bestanden '
              '(combat_hits=%d, target=%d).'
              % (saved['combat_hits'], saved['combat_target_right']))


def check_runtime_enemy(runtime, data, emulator):
    """Aufschaltung auf einen echten Gegner und Schadensbuchung im Spielablauf.

    Der Kampfkern liest die Gegnertabelle (sys->ene / sys->ewk_n) ueber den
    Dienst `enemies` (pwksub.c:244 bhSearchNearEnemy, playpch.c:318 bhSearchEnemy,
    weapon.c:614 bhCheckGunAtari). Ohne diese Bindung bleibt pp.sch_lst/pp.lck_tbl
    leer -- combat_target_right bleibt -1 und bhCheckGunAtari bucht keinen
    Schaden. Verglichen wird derselbe Replay einmal mit und einmal ohne die
    Abdruckzeilen; die Differenz der Gegner-HP ist der gebuchte Schaden.
    """
    prefix = [emulator] if emulator else []

    def path(p):
        absolute = str(Path(p).resolve())
        if emulator and runtime.suffix.lower() == '.exe':
            return 'Z:' + absolute.replace('/', '\\')
        return absolute

    lines = (ROOT / 'tests/native/replays/combat-aim-enemy.txt').read_text().splitlines()
    with tempfile.TemporaryDirectory(prefix='recvx-combat-aim-') as folder:
        root = Path(folder)

        def run(name, replay):
            script = root / (name + '.txt')
            script.write_text(replay)
            state = root / (name + '.json')
            subprocess.run(prefix + [str(runtime.resolve()), '--data', path(data), '--headless',
                                     '--in-game', '--room', 'RM_0030.RDX', '--replay', path(script),
                                     '--frames', '4300', '--state', path(state), '--enemy-ai', 'on'],
                           check=True, cwd=ROOT)
            return json.loads(state.read_text())

        def zombie_hp(saved):
            return sum(e['hp'] for e in saved['enemies'] if e['identity'] == 1)

        fired = run('aim', '\n'.join(lines) + '\n')
        control = run('control', '\n'.join(line.replace(' attack', '') for line in lines) + '\n')

        if fired['room'].split('.')[0] != 'RM_0080':
            raise SystemExit(f"Raumwechsel nach RM_0080 fehlgeschlagen: {fired['room']}")
        if fired['weapon'] != 4:
            raise SystemExit(f'M93R nicht ausgeruestet: weapon={fired["weapon"]}')
        if fired['combat_shots'] < 3 or fired['combat_attacks'] < 3:
            raise SystemExit(f"Der Kampfpass hat nicht gefeuert: shots={fired['combat_shots']}, "
                             f"attacks={fired['combat_attacks']}")
        aim = fired['combat_aim']
        if aim['near_calls'] < 1 or aim['near_seen'] < 1:
            raise SystemExit('bhSearchNearEnemy hat die Gegnertabelle nie gelesen; der Dienst '
                             '`enemies` (sys->ene / sys->ewk_n) ist nicht gebunden')
        if aim['near_found'] < 1:
            raise SystemExit('keine Aufschaltung: bhSearchNearEnemy findet kein Ziel '
                             f"(seen={aim['near_seen']}, filter={aim['near_filter']}, "
                             f"cone={aim['near_cone']}, wall={aim['near_wall']})")
        # Die Aufschaltung dreht den Spieler auf das Ziel (plp->ayp -> plp->ay). Die
        # Blickrichtung ist (-sin ay, -cos ay) (player.c:1583, fpx = px - sin*arn); die
        # Sollrichtung kommt aus der tatsaechlichen Lage von Spieler und naechstem Zombie.
        px, _, pz = fired['actor']
        target = min((e for e in fired['enemies'] if e['identity'] == 1),
                     key=lambda e: math.hypot(e['position'][0] - px, e['position'][2] - pz))
        dx, dz = target['position'][0] - px, target['position'][2] - pz
        want = int(round(math.atan2(-dx, -dz) * 32768 / math.pi)) % 65536
        diff = (fired['yaw'] - want) % 65536
        turned = min(diff, 65536 - diff)
        moved = min((fired['yaw'] - 8192) % 65536, (8192 - fired['yaw']) % 65536)
        if turned > 4096 or moved < 1024:
            raise SystemExit(f'der Spieler dreht nicht auf das Ziel (yaw={fired["yaw"]}, '
                             f'Ziel {want}, Platzierung war 8192)')
        if fired['combat_hits'] < 1:
            raise SystemExit('kein Treffer gebucht (bhCheckGunAtari ohne Gegnertabelle)')
        if zombie_hp(fired) >= zombie_hp(control):
            raise SystemExit('kein Gegner hat Schaden genommen: '
                             f'{zombie_hp(fired)} >= {zombie_hp(control)}')
        if fired['blocked_commands']:
            raise SystemExit('event commands blocked during the combat replay')
        print('Runtime: M93R -> RM_0080 -> Aufschaltung (yaw %d -> %d Grad neben dem Ziel) -> '
              '%d Schuesse, %d Treffer, Gegner-HP %d statt %d ohne Schuss.'
              % (8192, turned, fired['combat_shots'], fired['combat_hits'],
                 zombie_hp(fired), zombie_hp(control)))


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
        check_runtime_enemy(args.runtime, args.data, args.emulator)
    elif run_all or args.runtime_only:
        print('Runtime-Stufe uebersprungen: --runtime und --data angeben.', file=sys.stderr)


if __name__ == '__main__':
    main()
