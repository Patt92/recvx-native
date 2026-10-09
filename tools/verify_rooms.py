#!/usr/bin/env python3
"""Start every exported original room in game mode and report load failures and
blocked event commands. Load failures fail the check; blocked commands are a
progress report (see AGENTS.md "Engine-Lücken")."""
import argparse
import collections
import concurrent.futures
import re
import json
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
    parser.add_argument('--report',type=Path,help='retain per-room exit status, output and enemy gaps as JSON')
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
            return room, result.returncode, result.stdout, result.stderr

        failures, transitions, blocked = {}, {}, collections.Counter()
        results=[]
        blocked_rooms = set()
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            for room, code, stdout, stderr in pool.map(run, rooms):
                gaps=[line for line in stdout.splitlines()+stderr.splitlines() if 'Enemy gaps' in line]
                results.append(dict(room=room,code=code,stdout=stdout,stderr=stderr,enemy_gaps=gaps))
                fatal = [line for line in stderr.splitlines() if line.startswith('Native runtime error')]
                if fatal:
                    if room + '.RDX:' in stdout:
                        # The initial room loaded; a later runtime operation
                        # failed. Keep that failure visible as progression evidence.
                        transitions[room] = fatal[-1]
                        blocked_rooms.add(room)
                    else:
                        failures[room] = fatal[-1]
                    continue
                if code:
                    reason=f'process exited with {code}: '+stderr[-500:]
                    if room+'.RDX:' in stdout:
                        transitions[room]=reason;blocked_rooms.add(room)
                    else:
                        failures[room]=reason
                    continue
                for line in stderr.splitlines():
                    if line.startswith('event '):
                        blocked_rooms.add(room)
                        match = re.search(r'opcode (0x[0-9a-f]+)|Common_controll command \d+', line)
                        blocked[match.group(0) if match else line.split(': ', 1)[-1]] += 1
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(results,indent=2))
    print(f'{len(rooms) - len(failures)}/{len(rooms)} rooms load; '
          f'{len(blocked_rooms)} rooms report blocked event commands.')
    for reason, count in blocked.most_common(15):
        print(f'  blocked: {reason} x{count}')
    for room, reason in transitions.items():
        print(f'  RUNTIME FAILURE AFTER LOAD {room}: {reason}')
    for room, reason in failures.items():
        print(f'  LOAD FAILURE {room}: {reason}')
    if failures or transitions:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
