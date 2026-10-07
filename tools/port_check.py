#!/usr/bin/env python3
"""Engine-Portierungscheck: wie viel der Originalengine (src/ps2/veronica/prog)
steckt nachweisbar im nativen Runtime?

Einheit ist die Originalfunktion, gewichtet mit ihren nicht-leeren Zeilen.
Jede Funktion bekommt genau einen Status aus der stärksten verfügbaren Evidenz:

  original   Originalcode wird direkt kompiliert (CMake) oder per Generator
             übernommen (tools/generate_*.py)                       -> 100 %
  opcode     Event-Befehl (bhScenarioJmpT) im nativen EventVm/World gebunden
                                                                    -> 100 %
  pending    Event-Befehl wird nur konsumiert, Wirkung laut Code offen -> 50 %
  ledger     Status aus docs/port_ledger.tsv (done/partial/missing/skip)
  verweis    Name kommt im nativen Code vor (Nachbau/Kommentar), ungeprüft
                                                                    -> 50 %
  fehlt      keine Evidenz                                          ->  0 %

Die Plattformschicht (PS2-SDK, VU1, Memory Card, CRI-Middleware) ersetzt der
Host durch eigene Schichten; sie wird getrennt ausgewiesen und zählt nicht in
den Engine-Prozentwert. "streng" zählt nur 100-%-Evidenz, "gewichtet" auch die
50-%-Stufen. Das Ergebnis ist ein Nachweis-Maß, kein Qualitätsurteil: auch
"original"-Code kann falsch angebunden sein; Laufzeitprüfungen bleiben
tools/verify_*.py.
"""
import argparse
import collections
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROG = ROOT / 'src/ps2/veronica/prog'
NATIVE = ROOT / 'native'
LEDGER = ROOT / 'docs/port_ledger.tsv'

SCORE = {'original': 1.0, 'opcode': 1.0, 'done': 1.0, 'pending': 0.5, 'partial': 0.5,
         'verweis': 0.5, 'missing': 0.0, 'fehlt': 0.0}

# Reihenfolge zählt: erster Treffer bestimmt den Bereich.
DOMAINS = [
    ('Gegner/KI', r'^(en\d+.*|eneset|zonzon1?)\.c$'),
    ('Effekte', r'^(effect|effsub\w*)\.c$'),
    ('Events/Skript', r'^(event|pl_evt|sp_evt|ps2_event)\.c$'),
    ('Spieler/Kampf', r'^(player|playpch2?|subpl|weapon|pwksub|hitchkl?|rutchk)\.c$'),
    ('Raum/Kamera/Tür', r'^(room|map|camera|cut|door|light|objitm|screen)\.c$'),
    ('Menü/Items/Text', r'^(sub1|item|itemview|fileview|message|adv|gameover|ranking)\.c$'),
    ('Modelle/Animation', r'^(Motion|MdlPut|njplus|binfunc|face|face_bh)\.c$'),
    ('Audio', r'^(sdfunc|adxwrap)\.c$'),
    ('Ablauf/Speichern', r'^(main|game|system|flag|expand|dread|backup|bup_00|sync|vibman|pad|ps2_LoadScreen|'
                         r'ps2_SaveScreen|ps2_McSaveFile|ps2_SystemLoadScreen|ps2_SystemSaveScreen|'
                         r'ps2_MovieFunc|ps2_MovieWork)\.c$'),
    ('Plattform (Host-ersetzt)', r'.*'),
]
PLATFORM = 'Plattform (Host-ersetzt)'
KEYWORDS = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'else', 'do'}
SIGNATURE = re.compile(r'^(?!static\s+const\b)(?:[A-Za-z_][\w\s\*]*?[\s\*])?([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(\{)?\s*(//.*)?$')


def domain_of(name):
    return next(label for label, pattern in DOMAINS if re.match(pattern, name))


def strip_code(text):
    """Kommentare/Strings durch Leerzeichen ersetzen, Zeilen bleiben erhalten."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(c + ' ' * (j - i - 1) + (c if j < n else ''))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def functions(path):
    """[(name, start_line, weight)] aller Funktionsdefinitionen auf Dateiebene."""
    raw = path.read_text(errors='replace')
    lines = strip_code(raw).split('\n')
    found, depth, i = [], 0, 0
    while i < len(lines):
        line = lines[i]
        if depth == 0 and line[:1] not in ('', ' ', '\t', '#', '{', '}'):
            match = SIGNATURE.match(line.rstrip())
            if match and match.group(1) not in KEYWORDS:
                j = i
                if not match.group(3):
                    j = i + 1
                    while j < len(lines) and not lines[j].strip():
                        j += 1
                    if j >= len(lines) or not lines[j].strip().startswith('{'):
                        i += 1
                        continue
                level, k = 0, j
                while k < len(lines):
                    level += lines[k].count('{') - lines[k].count('}')
                    if level <= 0 and '{' in ''.join(lines[j:k + 1]):
                        break
                    k += 1
                weight = sum(1 for body in lines[i:k + 1] if body.strip())
                found.append((match.group(1), i + 1, weight))
                i = k + 1
                continue
        depth += line.count('{') - line.count('}')
        depth = max(depth, 0)
        i += 1
    return found


def compiled_files():
    text = (ROOT / 'CMakeLists.txt').read_text()
    return set(re.findall(r'src/ps2/veronica/prog/(\w+\.c)\b', text.split('add_library', 1)[1]))


def generated_functions():
    """Namen der Funktionen, die die Generatoren wörtlich aus dem Original übernehmen."""
    names = set()
    with tempfile.TemporaryDirectory(prefix='recvx-portcheck-') as folder:
        for script in sorted((ROOT / 'tools').glob('generate_*.py')):
            result = subprocess.run([sys.executable, str(script), str(ROOT), folder],
                                    capture_output=True, text=True, cwd=ROOT / 'tools')
            if result.returncode:
                print(f'Warnung: {script.name} fehlgeschlagen: {result.stderr.strip()[-200:]}', file=sys.stderr)
        for inc in Path(folder).glob('*.inc'):
            text = strip_code(inc.read_text(errors='replace'))
            names.update(re.findall(r'(?:^|[\s;}])(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?[A-Za-z_]\w*[\s\*]+'
                                    r'([A-Za-z_]\w*)\s*\([^;{}()]*\)\s*\{', text, re.M))
    return names


def event_opcodes():
    """{funktionsname: (opcode, status)} aus bhScenarioJmpT gegen native Bindungen."""
    source = (PROG / 'event.c').read_text()
    start = source.index('bhScenarioJmpT[256])()')
    table = re.findall(r'[A-Za-z_]\w*', source[source.index('{', start):source.index('};', start)])
    vm = (NATIVE / 'events.cpp').read_text()
    body = vm[vm.index('int EventVm::instruction'):]
    handled, pending = set(), set()
    for line in body.split('\n'):
        if re.match(r'    case\s', line):
            # Grouped outer cases share a forwarding branch. Stop before a
            # nested switch/block so comparison/controller cases don't count.
            prefix = line.split('{', 1)[0].split('world(', 1)[0]
            ops = {int(v, 0) for v in re.findall(r'case\s+(0x[0-9a-fA-F]+|\d+)\s*:', prefix)}
            handled.update(ops)
            if re.search(r'\bworld\(t,\s*op', line) or 'pending' in line.lower() or 'remain' in line.lower():
                pending.update(ops)
    bound = set()
    for name in ('world.cpp', 'runtime.cpp'):
        bound.update(int(v, 0) for v in re.findall(r'\b(?:op|opcode)\s*==\s*(0x[0-9a-fA-F]+|\d+)',
                                                    (NATIVE / name).read_text()))
    result = {}
    for op, name in enumerate(table):
        if name.startswith('dm'):
            continue
        if op not in handled:
            status = 'fehlt'
        elif op in pending and op not in bound:
            status = 'pending'
        else:
            status = 'opcode'
        # Mehrfachbelegung: stärkste Evidenz gewinnt.
        if name not in result or SCORE[status] > SCORE[result[name][1]]:
            result[name] = (op, status)
    return result, len([n for n in table if not n.startswith('dm')])


def native_text():
    parts = []
    for path in NATIVE.rglob('*'):
        if path.suffix in ('.cpp', '.h', '.c', '.inc') and 'third_party' not in path.parts:
            parts.append(path.read_text(errors='replace'))
    return '\n'.join(parts)


def read_ledger():
    rules = {}
    if LEDGER.exists():
        for number, line in enumerate(LEDGER.read_text().splitlines(), 1):
            if not line.strip() or line.startswith('#'):
                continue
            fields = line.split('\t')
            if len(fields) < 2 or fields[1] not in ('done', 'partial', 'missing', 'skip'):
                raise SystemExit(f'{LEDGER.name}:{number}: erwartet "<datei.c|datei.c:Funktion|Funktion>\\t<done|partial|missing|skip>\\t<Notiz>"')
            rules[fields[0]] = (fields[1], fields[2] if len(fields) > 2 else '')
    return rules


def classify():
    compiled, generated = compiled_files(), generated_functions()
    opcodes, opcode_total = event_opcodes()
    native = native_text()
    ledger = read_ledger()
    used_rules = set()
    records = []
    for path in sorted(PROG.glob('*.c')):
        for name, line, weight in functions(path):
            key = f'{path.name}:{name}'
            rule = next((r for r in (key, name, path.name) if r in ledger), None)
            if rule:
                used_rules.add(rule)
                status, note = ledger[rule]
                source = 'ledger'
            elif path.name in compiled or (name in generated and len(name) > 3):
                status, note, source = 'original', '', 'original'
            elif path.name == 'event.c' and name in opcodes:
                status, note, source = opcodes[name][1], f'opcode 0x{opcodes[name][0]:02x}', 'opcode'
            elif len(name) > 4 and re.search(r'\b' + re.escape(name) + r'\b', native):
                status, note, source = 'verweis', '', 'verweis'
            else:
                status, note, source = 'fehlt', '', 'fehlt'
            records.append({'file': path.name, 'function': name, 'line': line, 'weight': weight,
                            'domain': domain_of(path.name), 'status': status, 'source': source, 'note': note})
    unused = sorted(set(ledger) - used_rules)
    return records, opcodes, opcode_total, unused


def summarize(records):
    groups = collections.OrderedDict((label, collections.Counter()) for label, _ in DOMAINS)
    for r in records:
        if r['status'] == 'skip':
            continue
        g = groups[r['domain']]
        g['weight'] += r['weight']
        g['functions'] += 1
        g['strict'] += r['weight'] * (SCORE[r['status']] == 1.0)
        g['weighted'] += r['weight'] * SCORE[r['status']]
        g['done_functions'] += SCORE[r['status']] == 1.0
        g['status:' + r['status']] += r['weight']
    return groups


def percent(part, whole):
    return 100.0 * part / whole if whole else 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--json', type=Path, help='vollständige Funktionsliste als JSON schreiben')
    parser.add_argument('--markdown', type=Path, help='Bericht als Markdown schreiben (z. B. docs/PORT_STATUS.md)')
    parser.add_argument('--top', type=int, default=15, help='größte fehlende Funktionen anzeigen')
    parser.add_argument('--domain', help='nur diesen Bereich auflisten (Teilstring)')
    parser.add_argument('--fail-under', type=float, help='Exit 1, wenn der gewichtete Engine-Wert darunter liegt')
    args = parser.parse_args()

    records, opcodes, opcode_total, unused = classify()
    groups = summarize(records)
    engine = collections.Counter()
    for label, g in groups.items():
        if label != PLATFORM:
            engine.update(g)
    opcode_counts = collections.Counter(status for _, status in opcodes.values())
    opcode_done = sum(1 for _, s in opcodes.values() if s == 'opcode')

    lines = []
    lines.append('# Engine-Portierungsstand')
    lines.append('')
    lines.append(f'Engine gesamt: **{percent(engine["weighted"], engine["weight"]):.1f} %** gewichtet, '
                 f'**{percent(engine["strict"], engine["weight"]):.1f} %** streng '
                 f'({engine["done_functions"]}/{engine["functions"]} Funktionen voll belegt, '
                 f'{engine["weight"]} Originalzeilen ohne Plattformschicht).')
    lines.append('')
    lines.append(f'Event-Befehle (bhScenarioJmpT): {opcode_done}/{opcode_total} gebunden, '
                 f'{opcode_counts["pending"]} nur konsumiert, {opcode_counts["fehlt"]} fehlen.')
    lines.append('')
    lines.append('| Bereich | Funktionen | Zeilen | gewichtet | streng | original | opcode | verweis/teilw. | fehlt |')
    lines.append('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
    for label, g in groups.items():
        if not g['weight']:
            continue
        half = g['status:verweis'] + g['status:partial'] + g['status:pending']
        lines.append(f'| {label} | {g["done_functions"]}/{g["functions"]} | {g["weight"]} | '
                     f'{percent(g["weighted"], g["weight"]):.1f} % | {percent(g["strict"], g["weight"]):.1f} % | '
                     f'{g["status:original"] + g["status:done"]} | {g["status:opcode"]} | {half} | '
                     f'{g["status:fehlt"] + g["status:missing"]} |')
    lines.append('')
    lines.append('Spalten original…fehlt in Originalzeilen. Plattformschicht zählt nicht zum Engine-Wert.')
    lines.append('')
    selection = [r for r in records if SCORE.get(r['status'], 1) < 1.0 and r['domain'] != PLATFORM]
    if args.domain:
        selection = [r for r in selection if args.domain.lower() in r['domain'].lower()]
    selection.sort(key=lambda r: -r['weight'] * (1 - SCORE[r['status']]))
    lines.append(f'## Größte offene Funktionen{" (" + args.domain + ")" if args.domain else ""}')
    lines.append('')
    for r in selection[:args.top]:
        lines.append(f'- `{r["file"]}:{r["line"]}` {r["function"]} ({r["weight"]} Zeilen, {r["status"]}'
                     f'{", " + r["note"] if r["note"] else ""})')
    if unused:
        lines.append('')
        lines.append('Ledger-Einträge ohne passende Funktion: ' + ', '.join(unused))
    report = '\n'.join(lines)
    print(report)
    if args.markdown:
        args.markdown.write_text(report + '\n\nErzeugt mit `python3 tools/port_check.py --markdown ' + str(args.markdown) + '`.\n')
    if args.json:
        args.json.write_text(json.dumps({'engine_weighted': percent(engine['weighted'], engine['weight']),
                                         'engine_strict': percent(engine['strict'], engine['weight']),
                                         'opcodes': {'total': opcode_total, **opcode_counts},
                                         'functions': records}, indent=1))
    if args.fail_under is not None and percent(engine['weighted'], engine['weight']) < args.fail_under:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
