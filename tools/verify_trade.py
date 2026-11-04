#!/usr/bin/env python3
"""Targeted trade-window checks in the production native runtime.

RM_0080, debug item grants and replay placement are diagnostic fixtures, not a
natural playthrough. SDL dummy checks use the physical InputManager event path;
real controller hardware is not covered here.
"""
import argparse
import json
import os
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

    def run(name, script='5 trade\n', frames=18, mode='npc', sdl=False, extra=()):
        replay, state, picture = (out / f'{name}.{ext}' for ext in ('txt', 'json', 'bmp'))
        replay.write_text(script)
        state.unlink(missing_ok=True)
        picture.unlink(missing_ok=True)
        command = [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                   '--mods', str(out / 'no-mods'), '--user-dir', str(out / 'user'),
                   '--in-game', '--room', 'RM_0080.RDX', '--player2', mode,
                   '--ss', '1', '--renderer', 'software', '--frames', str(frames),
                   '--replay', str(replay), '--state', str(state),
                   '--screenshot', str(picture), *extra]
        env = os.environ.copy()
        if sdl:
            env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
        else:
            command.append('--headless')
        with (out / f'{name}.log').open('w') as log:
            subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=45)
        return json.loads(state.read_text())

    short = run('open', frames=8)
    actions = '5 trade\n6 left\n7 left\n8 trade_actions\n'
    popup = run('partner-actions', actions)
    assert popup['trade_actions'] and popup['player2']['items'][0] == 2
    unequipped = run('partner-unequip', actions + '9 confirm\n')
    assert not unequipped['trade_actions'] and unequipped['player2']['items'][0] == 0
    assert unequipped['items'] == popup['items'], 'partner equipment changed primary bag'
    cancelled_action = run('partner-action-cancel', actions + '9 cancel\n')
    assert cancelled_action['player2']['items'] == popup['player2']['items']
    healing = ('0 debug_key F2\n1 debug_select give 21\n2 confirm\n3 debug_key F2\n'
               '4 player2_damage 100\n5 trade\n6 right\n7 confirm\n8 right\n9 confirm\n10 trade_actions\n')
    healing_popup = run('partner-use', healing)
    healed = run('partner-healed', healing + '11 confirm\n')
    assert healing_popup['trade_actions'] and healing_popup['player2']['hp'] == 60
    assert healed['player2']['hp'] == 110 and all(v >> 16 != 21 for v in healed['player2']['items'][2:])
    assert healed['hp'] == healing_popup['hp'] and healed['items'] == healing_popup['items']
    physical_actions = '5 trade\n6 left\n7 left\n8 sdl_key down R\n9 sdl_key up R\n'
    physical_popup = run('partner-physical-actions', physical_actions, frames=60, sdl=True)
    assert physical_popup['trade_actions'], 'configured physical Aim did not open partner actions'
    physical_equip = run('partner-physical-unequip', physical_actions + '12 sdl_key down Return\n13 sdl_key up Return\n', frames=60, sdl=True)
    assert not physical_equip['trade_actions'] and physical_equip['player2']['items'][0] == 0
    armed = ('0 position -25 0 33.15 8192\n0 player2_position -17 0 33.15 8192\n'
             '0 debug_key F2\n1 debug_select give_count 18\n2 confirm\n3 debug_select give 5\n4 confirm\n5 debug_key F2\n'
             '10 trade\n11 right\n12 confirm\n13 confirm\n14 trade_actions\n15 confirm\n16 trade\n'
             '17 player2_keys aim\n40 player2_keys aim attack\n41 player2_keys aim\n70 player2_keys aim attack\n71 player2_keys aim\n')
    fired = run('partner-equipped-firearm', armed, frames=100, mode='local', extra=('--enemy-ai','on','--enemy-grab','off','--enemy-attacks','off'))
    assert fired['player2']['items'][0] == 2 and fired['player2']['items'][2] >> 16 == 5
    assert (fired['player2']['items'][2] & 65535) < 18 and fired['player2']['attacks'] > 0, 'equipped partner weapon did not reach native firing/ammo owner'
    reload_setup = ('0 debug_key F2\n1 debug_select give 5\n2 confirm\n3 debug_select give_count 25\n4 confirm\n5 debug_select give 12\n6 confirm\n7 debug_key F2\n'
                    '10 trade\n11 right\n12 confirm\n13 confirm\n14 trade_actions\n15 confirm\n16 left\n19 confirm\n20 right\n21 confirm\n22 left\n23 trade_actions\n24 down\n')
    reload_popup = run('partner-reload-menu', reload_setup, frames=25, mode='local')
    assert reload_popup['trade_actions'] and (reload_popup['player2']['items'][2] & 65535) == 1
    reloaded = run('partner-reloaded', reload_setup + '25 confirm\n', frames=26, mode='local')
    assert reloaded['trade_note'] == 'Partner weapon reloaded.'
    assert reloaded['player2']['items'][2] & 65535 == 18 and reloaded['player2']['items'][3] & 65535 == 8
    assert reloaded['items'] == reload_popup['items'], 'partner reload borrowed primary ammunition'
    reload_cancel = run('partner-reload-cancel', reload_setup + '25 cancel\n', frames=26, mode='local')
    assert reload_cancel['player2']['items'] == reload_popup['player2']['items']
    reload_fire = run('partner-reload-fire', reload_setup + '25 confirm\n26 trade\n27 player2_keys aim\n50 player2_keys aim attack\n51 player2_keys aim\n80 player2_keys aim attack\n81 player2_keys aim\n', frames=110, mode='local')
    assert reload_fire['player2']['attacks'] > 0 and (reload_fire['player2']['items'][2] & 65535) < 18, 'reloaded weapon did not fire after closing exchange'
    held = run('held', frames=60)
    assert short['trade_open'] and not short['inventory_open'], short['trade_note']
    for field in ('actor', 'yaw', 'motion', 'animation_frame', 'player_work',
                  'player_joints', 'player2', 'hp', 'items', 'play_time', 'events',
                  'event_tasks', 'vm_flags', 'enemies', 'effects', 'room_objects',
                  'camera_clock', 'background_delay', 'ranking'):
        assert short[field] == held[field], ('simulation advanced during trade', field)
    cancelled = run('cancel-selection', '5 trade\n6 confirm\n7 cancel\n')
    assert cancelled['trade_open'] and not cancelled['trade_selecting']
    assert cancelled['items'] == short['items'] and cancelled['player2']['items'] == short['player2']['items']

    # The initial manual must not be entrusted to an NPC, including a swap.
    refused = run('npc-refused', '5 trade\n6 confirm\n7 confirm\n')
    assert refused['trade_note'] == 'This item cannot be traded.'
    assert refused['items'] == short['items'] and refused['player2']['items'] == short['player2']['items']
    human = run('human-swap', '5 trade\n6 confirm\n7 confirm\n', mode='local')
    assert human['trade_note'] == 'Items transferred.'
    assert (human['items'][2] >> 16) == 8 and (human['player2']['items'][2] >> 16) == 145

    key = ('0 debug_key F2\n1 debug_select give 43\n2 confirm\n'
           '3 debug_key F2\n5 trade\n6 right\n7 confirm\n8 confirm\n')
    key_refused = run('npc-key-refused', key)
    assert key_refused['trade_note'] == 'This item cannot be traded.'
    assert any((v >> 16) == 43 for v in key_refused['items'][2:])
    assert all((v >> 16) != 43 for v in key_refused['player2']['items'][2:])
    key_human = run('human-key', key, mode='local')
    assert key_human['trade_note'] == 'Items transferred.'
    assert any((v >> 16) == 43 for v in key_human['player2']['items'][2:])

    # Diagnostic grant uses the real debug callback and room-independent inventory.
    supplied = ('0 debug_key F2\n1 debug_select give_count 25\n2 confirm\n'
                '3 debug_select give 12\n4 confirm\n5 debug_key F2\n10 trade\n11 right\n12 confirm\n')
    destination = run('destination', supplied)
    assert destination['trade_open'] and destination['trade_selecting'] and destination['trade_pane'] == 1
    exchanged = run('exchanged', supplied + '13 confirm\n')
    assert exchanged['trade_note'] == 'Items transferred.'
    assert any((v >> 16) == 8 for v in exchanged['items'][2:])
    assert (exchanged['player2']['items'][2] >> 16) == 12 and (exchanged['player2']['items'][2] & 65535) == 25
    assert exchanged['player2']['items'][0] == 0, 'trading equipped knife did not holster it'
    resumed = run('resumed', supplied + '13 confirm\n14 inventory\n', frames=40)
    assert not resumed['trade_open'] and not resumed['inventory_open']
    assert resumed['player2']['items'] == exchanged['player2']['items']
    assert resumed['player2']['frame'] != exchanged['player2']['frame'], 'simulation did not resume'
    for name, prefix, mode in (
        ('absent', '', 'off'),
        ('distant', '0 player2_position 1000 0 1000 0\n', 'local'),
        ('downed', '0 player2_damage 99999\n', 'npc'),
    ):
        unavailable = run(name, prefix + '5 trade\n', mode=mode)
        assert not unavailable['trade_open'] and not unavailable['inventory_open'], name
    status = run('status-owner', '5 trade\n', extra=('--inventory',))
    assert status['inventory_open'] and not status['trade_open']

    wide = ('0 debug_key F2\n1 debug_select give 1\n2 confirm\n'
            '3 debug_select side_pack 1\n4 confirm\n5 debug_key F2\n10 trade\n')
    # The wide-item view also exercises the five-row, ten-unit bag layout.
    wide_view = run('wide', wide, mode='local')
    assert wide_view['trade_open'] and wide_view['inventory_capacity'] == 10

    # Physical SDL events: the chord must consume Tab before ordinary inventory.
    opened = run('sdl-shift-tab', '5 sdl_key down Left Shift\n6 sdl_key down Tab\n'
                 '8 sdl_key up Tab\n9 sdl_key up Left Shift\n', frames=100, sdl=True)
    assert opened['trade_open'] and not opened['inventory_open']
    plain = run('sdl-plain-tab', '5 sdl_key down Tab\n8 sdl_key up Tab\n', frames=100, sdl=True)
    assert plain['inventory_open'] and not plain['trade_open']
    closed = run('sdl-close', '5 sdl_key down Left Shift\n6 sdl_key down Tab\n'
                 '8 sdl_key up Tab\n9 sdl_key up Left Shift\n'
                 '12 sdl_key down Escape\n14 sdl_key up Escape\n', frames=100, sdl=True)
    assert not closed['trade_open'] and not closed['inventory_open']
    print('Trade runtime: two real inventories, modal simulation hold, swap, NPC restriction, cancel/resume, availability and SDL Shift+Tab/Tab/Escape PASS')


if __name__ == '__main__':
    main()
