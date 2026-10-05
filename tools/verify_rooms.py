#!/usr/bin/env python3
"""Start every exported original room in game mode and report load failures and
blocked event commands. Load failures fail the check; blocked commands are a
progress report (see AGENTS.md "Engine-Lücken")."""
import argparse
import collections
import concurrent.futures
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--emulator')
    args = parser.parse_args()
    rooms = sorted(p.stem for p in (args.data / 'rooms').glob('RM_*.RDT'))
    if not rooms:
        raise SystemExit('no exported rooms in ' + str(args.data / 'rooms'))
    with tempfile.TemporaryDirectory(prefix='recvx-rooms-') as folder:
        controls = Path(folder) / 'controls.ini'
        prefix = [args.emulator] if args.emulator else []
        subprocess.run(prefix + [str(args.runtime.resolve()), '--write-controls', str(controls)],
                       check=True, stdout=subprocess.DEVNULL)

        def run(room):
            result = subprocess.run(prefix + [str(args.runtime.resolve()), '--data', str(args.data.resolve()),
                                    '--controls', str(controls), '--room', room + '.RDX', '--in-game',
                                    '--headless', '--frames', str(args.frames)],
                                    capture_output=True, text=True, cwd=ROOT, timeout=300)
            return room, result.returncode, result.stderr

        failures, blocked = {}, collections.Counter()
        blocked_rooms = set()
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            for room, code, stderr in pool.map(run, rooms):
                fatal = [line for line in stderr.splitlines() if line.startswith('Native runtime error')]
                if fatal:
                    failures[room] = fatal[-1]
                    continue
                for line in stderr.splitlines():
                    if line.startswith('event '):
                        blocked_rooms.add(room)
                        match = re.search(r'opcode (0x[0-9a-f]+)|Common_controll command \d+', line)
                        blocked[match.group(0) if match else line.split(': ', 1)[-1]] += 1
    print(f'{len(rooms) - len(failures)}/{len(rooms)} rooms load; '
          f'{len(blocked_rooms)} rooms report blocked event commands.')
    for reason, count in blocked.most_common(15):
        print(f'  blocked: {reason} x{count}')
    for room, reason in failures.items():
        print(f'  LOAD FAILURE {room}: {reason}')
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
