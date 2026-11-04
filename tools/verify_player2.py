#!/usr/bin/env python3
"""Targeted Player-2 extension checks through the production runtime.

RM_0080 and replay commands are diagnostic setups, not natural co-op gameplay.
The NPC command provider itself is exercised by tests/native/player2_test.cpp.
"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)

    def run(name, mode, script='', frames=18, extra=(), room='RM_0080.RDX'):
        replay, state = out / f'{name}.txt', out / f'{name}.json'
        replay.write_text(script)
        state.unlink(missing_ok=True)
        command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                   '--mods', str(out / 'no-mods'), '--user-dir', str(out / 'user'),
                   '--headless', '--in-game', '--room', room,
                   '--player2', mode, '--frames', str(frames), '--replay', str(replay),
                   '--state', str(state), *extra]
        with (out / f'{name}.log').open('w') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        return json.loads(state.read_text())

    baseline = run('off', 'off')
    local = run('local', 'local', '0 player2_keys forward\n')
    assert 'player2' not in baseline, 'disabled extension allocated a Player-2 work'
    assert local['player2']['motion'] == 0 and local['player2']['frame'] > 0, local['player2']
    assert local['player2']['position'] != baseline['actor'], 'Player 2 did not move in the runtime'
    for field in ('actor', 'yaw', 'player_work', 'hp', 'items'):
        assert local[field] == baseline[field], (field, local[field], baseline[field])
    npc = run('npc', 'npc', '0 player2_keys forward\n')
    assert npc['player2']['motion'] == 42, 'local commands reached the NPC source'
    assert npc['player2']['decision'] == 'idle', npc['player2']
    assert 'NPC driver not bound' not in npc['player2']['gaps'], 'built-in NPC source was not installed'
    menu = run('menu', 'local', '0 player2_keys forward\n', extra=('--inventory',))
    assert menu['inventory_open'] and menu['player2']['frame'] == 0, menu['player2']
    switched = '0 player2_keys forward\n8 player2_mode off\n'
    before = run('before-disable', 'local', switched, frames=8)
    disabled = run('disabled', 'local', switched, frames=20)
    assert disabled['player2']['mode'] == 'off'
    for field in ('position', 'frame'):
        assert disabled['player2'][field] == before['player2'][field], 'disabled actor advanced'
    switched += '20 player2_mode npc\n24 player2_mode local\n24 player2_keys right\n'
    resumed = run('resumed', 'local', switched, frames=32)
    assert resumed['player2']['mode'] == 'local' and resumed['player2']['frame'] != disabled['player2']['frame']
    debug = run('debug-mode', 'local', '0 debug_key F2\n0 debug_select player2 2\n0 confirm\n', frames=2)
    assert debug['debug_open'] and debug['player2']['mode'] == 'npc', 'debug menu did not apply NPC mode'
    follow = run('follow', 'npc', '0 keys forward\n75 keys none\n', frames=90)
    model = run('debug-model', 'npc', '0 debug_key F2\n0 debug_select player2_model 13\n0 confirm\n', frames=2)
    assert model['player2_model'] == 13 and model['player2']['mode'] == 'npc' and model['player2']['hp'] == 160, 'debug menu did not switch the companion model'
    assert npc['player2_model'] == 11, npc['player2_model']
    assert follow['player2']['position'] != baseline['actor'], 'NPC did not follow in the runtime'
    down_script = '0 player2_damage 99999\n30 player2_damage 99999\n30 player2_position 0 0 0 0\n'
    down = run('down', 'npc', down_script, frames=60)
    held = run('held', 'npc', down_script, frames=120)
    assert down['player2']['hp'] == 1 and down['player2']['life'] == 2 and down['player2']['motion'] == 45
    for field in ('position', 'frame', 'hp', 'life', 'attacks', 'hits'):
        assert down['player2'][field] == held['player2'][field], ('downed NPC changed', field)
    dead = run('dead', 'local', '0 player2_damage 99999\n', frames=90)
    assert dead['player2']['hp'] == 0 and dead['player2']['life'] == 4, 'local player inherited NPC protection'
    # Explicit diagnostic actor placement; original room enemies, their entry
    # and damage processing stay on the production native World path.
    fight_script = '0 position -25 0 33.15 8192\n0 player2_position -17 0 33.15 8192\n'
    fight_options = ('--enemy-ai', 'on', '--enemy-grab', 'off', '--enemy-attacks', 'off')
    no_npc = run('combat-without-npc', 'off', fight_script, frames=160, extra=fight_options)
    fight = run('combat-npc', 'npc', fight_script, frames=160, extra=fight_options)
    assert fight['player2']['attacks'] > 0 and fight['player2']['hits'] > 0, fight['player2']
    assert sum(e['hp'] for e in fight['enemies'] if e['identity'] == 1) < sum(e['hp'] for e in no_npc['enemies'] if e['identity'] == 1), 'NPC attacks did not reduce real enemy HP'
    # Enemy binding: RM_0031's free zombies bite the nearer actor. Player 2 is placed beside zombie 5
    # (diagnostic position); Player 1 keeps its spawn and must stay untouched for 600 ticks.
    bite_options = ('--enemy-ai', 'on', '--enemy-grab', 'on', '--enemy-attacks', 'on')
    # 450 ticks: Player 2 is in its second grab, Player 1 still untouched (later the released, dragged Player 2 is
    # farther away than Player 1 and the nearest-target rule sends the zombie on to Player 1).
    bitten = run('enemy-bite', 'local', '0 player2_position 28 0 75 0\n', frames=450, extra=bite_options, room='RM_0031.RDX')
    assert bitten['player2']['hp'] < 160 and bitten['player2']['grabs'] >= 1 and bitten['player2']['driven'], ('zombie did not grab/bite Player 2', bitten['player2'])
    assert bitten['hp'] == 160 and not bitten['player_scripted'], ('Player 1 was bitten instead', bitten['hp'])
    # Save files: a two-player session is recorded (format 3) and restored by a runtime started without
    # --player2; a save without the block loads as single player.
    snaps = out / 'snaps'
    for stale in snaps.glob('*'):
        stale.unlink()
    saved = run('save-2p', 'npc', '0 debug_key F2\n0 debug_select snapshot_save 0\n0 confirm\n', frames=3, extra=('--snapshots', str(snaps)))
    assert (snaps / 'snapshot-1.rcvxsnap').exists(), 'snapshot not written'
    loaded = run('load-2p', 'off', '0 debug_key F2\n0 debug_select snapshot_load 0\n0 confirm\n2 debug_key F2\n', frames=6, extra=('--snapshots', str(snaps)))
    assert loaded['player2_model'] == 11 and loaded['player2']['mode'] == 'npc' and loaded['player2']['hp'] == 160, ('two-player save did not restore the companion', loaded.get('player2'))
    single = run('save-1p', 'off', '0 debug_key F2\n0 debug_select snapshot_save 1\n0 confirm\n', frames=3, extra=('--snapshots', str(snaps)))
    back = run('load-1p', 'npc', '0 debug_key F2\n0 debug_select snapshot_load 1\n0 confirm\n2 debug_key F2\n', frames=6, extra=('--snapshots', str(snaps)))
    assert back['player2']['mode'] == 'off', ('single-player save kept the companion on', back['player2'])
    # Player AI (host auto-fight for Player 1): the same decision drives Player 1 through the real
    # combat controller and knife state machine; the debug menu equips the knife (diagnostic setup).
    ai_script = '0 position -17 0 33.15 8192\n0 debug_key F2\n1 debug_select equip 8\n2 confirm\n4 debug_key F2\n'
    no_ai = run('player-ai-off', 'off', ai_script, frames=160, extra=(*fight_options, '--player-ai', 'off'))
    ai = run('player-ai-on', 'off', ai_script, frames=160, extra=(*fight_options, '--player-ai', 'on'))
    assert not no_ai['player_ai']['on'] and no_ai['combat_attacks'] == 0 and not no_ai['debug_open'], no_ai['player_ai']
    assert ai['player_ai']['on'] and ai['combat_attacks'] > 0 and ai['combat_hits'] > 0, ai['player_ai']
    assert sum(e['hp'] for e in ai['enemies'] if e['identity'] == 1) < sum(e['hp'] for e in no_ai['enemies'] if e['identity'] == 1), 'Player AI did not reduce real enemy HP'
    yielded = run('player-ai-yield', 'off', ai_script + '5 keys back\n', frames=40, extra=(*fight_options, '--player-ai', 'on'))
    assert yielded['player_ai']['decision'] == 'yield' and yielded['combat_attacks'] == 0, yielded['player_ai']
    (out / 'down.bmp').unlink(missing_ok=True)
    run('down-view', 'npc', '0 player2_damage 99999\n0 keys right\n20 keys forward\n38 keys none\n',
        frames=70, extra=('--screenshot', str(out / 'down.bmp')))
    (out / 'combat.bmp').unlink(missing_ok=True)
    run('combat-view', 'npc', fight_script, frames=26, extra=(*fight_options, '--screenshot', str(out / 'combat.bmp')))
    (out / 'view.bmp').unlink(missing_ok=True)
    run('view', 'local', '0 player2_keys right\n20 player2_keys forward\n40 player2_keys none\n',
        frames=40, extra=('--screenshot', str(out / 'view.bmp')))
    print('Player 2 runtime: follow, real enemy knife damage, immutable 1-HP crouch, mortal local player, menu hold, mode switches, Player AI auto-fight/yield, enemy bite binding and two-player save files PASS')


if __name__ == '__main__':
    main()
