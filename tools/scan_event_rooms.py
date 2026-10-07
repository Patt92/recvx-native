#!/usr/bin/env python3
"""Scan original rooms without inputs; retain blockers AND pending effects.

This is a lower bound on event coverage, not a playthrough. Every original
opcode is listed, including commands not reached by this run.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    if args.frames < 1 or not 1 <= args.jobs <= 4:
        parser.error('positive frames and 1..4 jobs required')
    runtime, data, output = args.runtime.resolve(), args.data.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    rooms = sorted((data / 'rooms').glob('*.RDT'))
    if not rooms:
        parser.error('no original room files')

    def run(room):
        state_path = output / (room.stem + '.json')
        command = [str(runtime), '--data', str(data), '--mods', str(output / 'no-mods'),
                   '--in-game', '--headless', '--room', room.stem + '.RDX',
                   '--frames', str(args.frames), '--state', str(state_path)]
        with (output / (room.stem + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        state = json.loads(state_path.read_text()) if state_path.exists() else None
        return {'room': room.stem, 'exit_code': result.returncode, 'state': state,
                'log': (output / (room.stem + '.log')).read_text()}

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(run, rooms))
    (output / 'scan.json').write_text(json.dumps(results, indent=2) + '\n')
    source = (ROOT / 'src/ps2/veronica/prog/event.c').read_text()
    start = source.index('bhScenarioJmpT[256])()')
    names = re.findall(r'[A-Za-z_]\w*', source[source.index('{', start):source.index('};', start)])
    assert len(names) == 256
    vm = (ROOT / 'native/events.cpp').read_text()
    # Outer switch cases are at four spaces. Capture every grouped case on
    # that line, excluding nested controller/operator switches.
    cases = set()
    for line in vm.splitlines():
        if re.match(r'    case\s', line):
            prefix = line.split('{', 1)[0].split('world(', 1)[0]
            cases.update(int(n, 0) for n in re.findall(r'case\s+(0x[\da-f]+|\d+)\s*:', prefix))
    pending, blockers = Counter(), Counter()
    loaded = blocked = failed = 0
    for result in results:
        state = result['state']
        if state is not None:
            loaded += 1
            blocked += bool(state['blocked_commands'])
            for opcode in {c['opcode'] for c in state['pending_commands']}:
                pending[opcode] += 1
        else:
            failed += 1
        for error in result['log'].splitlines():
            if error.startswith('event '):
                blockers[error.split(': ', 1)[-1]] += 1
    report = [f'{loaded}/{len(rooms)} rooms reached a report; {blocked} report blocked commands; {failed} failed before report.',
              f'{args.frames} ticks, spawn 0, no input, no mods. No full gameplay proof.', '', 'Blockers:']
    report += [f'  {n}x {reason}' for reason, n in blockers.most_common()]
    report += ['', 'All original opcodes (parser presence is NOT proof of complete behavior):',
               'opcode\troutine\tparser\trooms_with_pending_binding']
    report += [f'0x{op:02x}\t{name}\t{"present" if op in cases else "missing"}\t{pending[op]}'
               for op, name in enumerate(names) if not name.startswith('dm')]
    text = '\n'.join(report) + '\n'
    (output / 'summary.txt').write_text(text)
    print('\n'.join(report[:len(blockers) + 4]))


if __name__ == '__main__':
    main()
