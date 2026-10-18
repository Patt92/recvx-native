#!/usr/bin/env python3
"""Retry nach einem Greif-Tod: ein Game Over je Tod, kein Sofort-Neustart.

Originalmechanismus. bhCPM0_enedie (player.c:6709-6714) startet den Game-Over-
Dispatcher nur, solange ts_flg 0x4000 steht, und loescht das Bit im selben
Zweig:

    if (((plp->flg & 0x2)) && ((sys->ts_flg & 0x4000)))
    {
        sys->ts_flg &= ~0x4000;
        *(int*)&sys->gov_md0 = 0;
    }

bhGameOverSet (event.c:8681-8689) verbraucht dasselbe Bit. plp->flg 0x2 bleibt
nach dem Tod stehen, deshalb ist das Bit das einzige, was einen zweiten Start
verhindert, waehrend in den 70 Bildern von bhMainGameOver md1 0 (gameover.c:181,
sp_flg erst ab md1 1/10) der normale Spielpass weiterlaeuft. Zurueckgegeben wird
es erst beim Ausgang (gameover.c:516 und :593, jeweils vor bhPopGameData).

Fehlte das Tor am nativen bhCPM0_enedie-Besitzer, forderte jeder Pass der md1-0-
Phase den Start erneut an. bhPopGameData lud dann zwar den Raum und stellte HP
und Lage her, das stehengebliebene Verlangen startete aber sofort ein zweites
Game Over: genau der Nutzerbefund "nach Retry sterbe ich direkt wieder, der
zweite Retry hat geklappt".

Diagnoseaufstellung, kein natuerliches Durchspielen: RM_0080 Spawn 2 mit der
Lage vor Zombie 0 wie in tools/verify_enemy_ai.py (`position -3 0 24 0`),
--enemy-ai on --enemy-grab on. Der Greif-Tod (en01.c:6861 plp->mode0 6,
PlyDG00 setzt plp->flg 0x2 am letzten Bild der Todesmotion, en01.c:7748)
erreicht gemessen bei Bild 1800 die Fortsetzungsabfrage (md1 2).
"""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'no-mods').mkdir(exist_ok=True)

DEATH_FRAME = 1800      # md1 2, Fortsetzungsabfrage steht (vgl. verify_enemy_ai `death`)
CONFIRM_FRAME = 1805    # OK auf YES; md1 3 wartet die 10er-Blende ab
AFTER_RETRY = 1900      # deutlich nach bhPopGameData und dem Raumladen
SECOND_DEATH = 3700     # zweiter Greif-Tod nach dem Retry
FRONT = '0 position -3 0 24 0\n'


def run(name, frames, replay):
    state = args.output / (name + '.json')
    log = args.output / (name + '.log')
    script = args.output / (name + '.txt')
    script.write_text(replay)
    if state.exists():
        state.unlink()  # --state ueberschreibt eine vorhandene Datei nicht
    command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
               '--mods', str(args.output / 'no-mods'), '--room', 'RM_0080.RDX', '--spawn', '2',
               '--headless', '--in-game', '--frames', str(frames), '--ss', '1',
               '--enemy-ai', 'on', '--enemy-grab', 'on',
               '--replay', str(script.resolve()), '--state', str(state.resolve())]
    with log.open('w') as stream:
        done = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    if done.returncode:
        raise RuntimeError(f'{name}: runtime exit {done.returncode}\n' + log.read_text())
    value = json.loads(state.read_text())
    assert value['blocked_commands'] == 0, (name, 'blocked commands')
    return value


# Der Greif-Tod erreicht die Fortsetzungsabfrage und hat das Tor verbraucht.
death = run('enedie-death', DEATH_FRAME, FRONT)
assert death['hp'] < 0 and death['gameover_active'] and death['gameover_phase'] == 2, (
    'enedie-death', 'kein Game Over in der Fortsetzungsabfrage',
    death['hp'], death['gameover_active'], death['gameover_phase'])
assert not death['work_flags'][12] & 0x4000, (
    'enedie-death', 'ts_flg 0x4000 nicht verbraucht (player.c:6711)')

# Erster Retry: bhPopGameData laedt den Raum, der Spieler lebt und spielt weiter.
retry = run('enedie-retry', AFTER_RETRY, FRONT + f'{CONFIRM_FRAME} confirm\n')
assert retry['retry_count'] == 1, ('enedie-retry', 'bhPopGameData hat nicht gezaehlt', retry['retry_count'])
assert not retry['gameover_active'], (
    'enedie-retry', 'das Game Over startete nach dem ersten Retry sofort erneut; '
    'das ts_flg-0x4000-Tor von player.c:6709 fehlt am nativen bhCPM0_enedie-Besitzer',
    retry['gameover_phase'])
assert retry['hp'] > 0 and not retry['player_scripted'], (
    'enedie-retry', 'Spieler nach dem Retry nicht frei', retry['hp'], retry['player_scripted'])
assert retry['work_flags'][12] & 0x4000, (
    'enedie-retry', 'ts_flg 0x4000 nach dem Raumladen nicht neu gesetzt (gameover.c:516)')

# Das neu bewaffnete Tor laesst den naechsten echten Tod wieder durch.
again = run('enedie-second-death', SECOND_DEATH, FRONT + f'{CONFIRM_FRAME} confirm\n1850 position -3 0 24 0\n')
assert again['hp'] < 0 and again['gameover_active'] and again['gameover_phase'] == 2, (
    'enedie-second-death', 'der zweite echte Tod startet kein Game Over',
    again['hp'], again['gameover_active'], again['gameover_phase'])
assert again['retry_count'] == 1, ('enedie-second-death', 'unerwarteter Retry', again['retry_count'])

print('Retry nach Greif-Tod: ein Game Over je Tod, ts_flg 0x4000 verbraucht und beim Raumladen neu gesetzt.')
