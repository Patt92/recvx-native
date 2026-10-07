#!/usr/bin/env python3
"""Check the real-data main menu and rapid lighter flow after a running film skip."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--emulator', help='Optional executable such as wine64')
    args = parser.parse_args()
    prefix = [args.emulator] if args.emulator else []
    with tempfile.TemporaryDirectory(prefix='recvx-frontend-') as folder:
        root = Path(folder)

        def run(name, frames, replay=''):
            state = root / (name + '.json')
            command = prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                       '--headless', '--frames', str(frames), '--state', str(state)]
            if replay:
                inputs = root / (name + '.txt')
                inputs.write_text(replay)
                command += ['--replay', str(inputs)]
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
            return json.loads(state.read_text())

        title = run('title', 120)
        assert title['title_active'] and not title['settings_active']
        assert not title['events'] and not title['audio_played'] and not title['movie_active']
        settings = run('settings', 120, '0 down\n1 confirm\n')
        assert settings['title_active'] and settings['settings_active'] and not settings['events']
        film = run('film', 1500, '0 new_game\n')
        assert not film['title_active'] and film['movie_active'] and film['movie_frame'] > 0
        assert 'ADV/0003' in film['audio_played'], 'original title announcement missing'
        # Hint completion owns inventory opening; held movement must never escape it.
        hint = run('hint-position', 1450, '0 new_game\n1300 confirm\n')
        automatic = run('automatic-inventory', 1600, '0 new_game\n1300 confirm\n1500 confirm\n1501 keys forward\n')
        assert automatic['inventory_open'] and automatic['weapon'] == 0
        assert automatic['actor'] == hint['actor'] and not automatic['blocked_commands']
        assert automatic['audio_played'][-2:]==['COMMON/0003','COMMON/0007'], 'automatic inventory uses wrong opening/slide sounds'
        assert not any(sound.startswith('MULTSPQ1/') for sound in automatic['audio_played']), 'inventory plays walking sounds'
        # Confirm stops an already running movie, followed by original lighter equipment.
        # ItemCommand slides the command window for 4 frames; WeaponSet keeps the status screen open.
        lighter = run('lighter', 4000, '0 new_game\n1300 confirm\n1500 confirm\n1550 confirm\n1555 confirm\n1560 inventory\n')
        assert not lighter['movie_active'] and not lighter['cinematic'] and not lighter['inventory_open']
        assert lighter['weapon'] == 1 and 0 in lighter['events'] and not lighter['blocked_commands']
        assert 'VOICE1/0016' in lighter['audio_played']
        assert not lighter['player_hidden'] and not lighter['player_scripted']
    print('Main menu idle/settings, active film skip and rapid hint/inventory/lighter/Rodrigo flow passed.')


if __name__ == '__main__':
    main()
