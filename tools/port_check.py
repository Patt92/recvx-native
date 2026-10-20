#!/usr/bin/env python3
"""Status des eigenständigen nativen Ports.

Der Check bewertet ausschliesslich explizit belegte native Mechanismen aus
``docs/port_ledger.tsv`` und die im nativen EventVm gebundenen Opcodes. Die
Decompilation bleibt eine Semantikreferenz, ist aber *kein* Fortschrittsnenner:
eine fehlende native Entsprechung zu jeder Referenzfunktion bedeutet nicht,
dass der Port einen Originalkern erwartet oder ihn kompilieren soll.

Eine Namenssuche im nativen Baum ist ebenfalls kein Beleg. Alte Ledgernotizen,
die einen extrahierten/generierten/originalen Funktionskoerper beanspruchen,
werden als ``unbestaetigt`` ausgegeben und nicht gewertet. Damit kann der
Bericht den sauberen Port nicht durch historische Kern- oder Generatorarbeit
aufwerten.

Die Kennzahl beschreibt nur den explizit abgegrenzten Belegumfang, niemals den
Fertigstellungsgrad des ganzen Spiels. ``check_no_original_core.py`` bleibt der
verbindliche Schutzpruefer; produktive Laufzeitpruefungen bleiben zusaetzlich
erforderlich.
"""
import argparse
import collections
import json
import re
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PROG = ROOT / 'src/ps2/veronica/prog'
NATIVE = ROOT / 'native'
LEDGER = ROOT / 'docs/port_ledger.tsv'

SCORE = {'opcode': 1.0, 'done': 1.0, 'pending': 0.5, 'partial': 0.5,
         'missing': 0.0}
PORT_SCOPE = frozenset(SCORE)
# Diese Formulierungen belegen einen inzwischen verbotenen Extraktionsweg,
# nicht eine eigenstaendige Implementierung.  Die Begriffe sind bewusst eng:
# "originale Reihenfolge" bleibt eine zulaessige Semantikreferenz.
RETIRED_EVIDENCE = re.compile(
    r'\b(?:extrahiert|extracted|generiert|generated|verbatim|wasm2c|ilp32|'
    r'original(?:e|en|er|es)?\s+(?:function|routine|funktionskoerper))\b', re.I)

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


def read_ledger():
    rules = {}
    if LEDGER.exists():
        for number, line in enumerate(LEDGER.read_text().splitlines(), 1):
            if not line.strip() or line.startswith('#'):
                continue
            fields = line.split('\t')
            if len(fields) < 2 or fields[1] not in ('done', 'partial', 'missing', 'skip'):
                raise SystemExit(f'{LEDGER.name}:{number}: erwartet "<datei.c|datei.c:Funktion|Funktion>\\t<done|partial|missing|skip>\\t<Notiz>"')
            # Ein Schluessel, eine Zeile: sonst gewinnt still die letzte und ein
            # ueberholter Eintrag verdeckt den belegten Stand (oder umgekehrt).
            if fields[0] in rules:
                raise SystemExit(f'{LEDGER.name}:{number}: Schluessel "{fields[0]}" steht doppelt; bestehende Zeile aktualisieren statt eine neue anzuhaengen')
            rules[fields[0]] = (fields[1], fields[2] if len(fields) > 2 else '')
    return rules


def ledger_evidence_valid(note):
    """True only when a ledger note does not rely on retired body extraction."""
    return not RETIRED_EVIDENCE.search(note)


def ledger_location(key):
    """Turn a ledger key into a stable display location without reading a core."""
    if ':' in key:
        filename, function = key.split(':', 1)
        return filename, function
    return '(native mechanism)', key


def classify():
    opcodes, opcode_total = event_opcodes()
    ledger = read_ledger()
    records = []
    for key, (status, note) in sorted(ledger.items()):
        filename, function = ledger_location(key)
        if status != 'skip' and not ledger_evidence_valid(note):
            status = 'unbestaetigt'
            note = ('Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht '
                    'zulaessig: ' + note)
        records.append({'file': filename, 'function': function, 'line': 0, 'weight': 1,
                        'domain': domain_of(filename), 'status': status, 'source': 'ledger', 'note': note})
    return records, opcodes, opcode_total


def summarize(records):
    groups = collections.OrderedDict((label, collections.Counter()) for label, _ in DOMAINS)
    for r in records:
        if r['status'] not in PORT_SCOPE:
            continue
        g = groups[r['domain']]
        g['weight'] += r['weight']
        g['functions'] += 1
        g['strict'] += r['weight'] * (SCORE[r['status']] == 1.0)
        g['weighted'] += r['weight'] * SCORE[r['status']]
        g['done_functions'] += SCORE[r['status']] == 1.0
        g['status:' + r['status']] += r['weight']
    return groups


README_BEGIN = '<!-- port-status:begin -->'
README_END = '<!-- port-status:end -->'
README_EXTERNAL = '<!-- port-status:external -->'
PORT_STATUS = ROOT / 'docs/PORT_STATUS.md'

# README.md is written in English; docs/PORT_STATUS.md stays German. The block is parsed
# from the German report and rendered with English labels so a later sync cannot put the
# German wording back into the public README.
README_AREAS = {
    'Gegner/KI': 'Enemies/AI',
    'Effekte': 'Effects',
    'Events/Skript': 'Events/script',
    'Spieler/Kampf': 'Player/combat',
    'Raum/Kamera/Tür': 'Room/camera/door',
    'Menü/Items/Text': 'Menu/items/text',
    'Modelle/Animation': 'Models/animation',
    'Audio': 'Audio',
    'Ablauf/Speichern': 'Flow/saving',
    'Plattform (Host-ersetzt)': 'Platform (host-replaced)',
}

def readme_block_from_status(status):
    lines = status.splitlines()
    summary_index = next((i for i, line in enumerate(lines) if line.startswith('Expliziter Belegumfang:')), None)
    events_index = next((i for i, line in enumerate(lines) if line.startswith('Event-Befehle (bhScenarioJmpT):')), None)
    table_index = next((i for i, line in enumerate(lines) if line.startswith('| Bereich | Mechanismen | Belegumfang |')), None)
    if summary_index is None or events_index is None or table_index is None:
        raise SystemExit('PORT_STATUS: summary or event-command line not readable')
    summary = re.fullmatch(
        r'Expliziter Belegumfang: \*\*(.+?)\*\* gewichtet, \*\*(.+?)\*\* voll belegt \((.+?)\)\.',
        lines[summary_index])
    events = re.fullmatch(
        r'Event-Befehle \(bhScenarioJmpT\): (\d+)/(\d+) gebunden, (\d+) nur konsumiert, (\d+) fehlen\.',
        lines[events_index])
    scope = re.fullmatch(r'(\d+)/(\d+) Mechanismen; kein Gesamtfortschritt', summary.group(3)) if summary else None
    if not summary or not events or not scope:
        raise SystemExit('PORT_STATUS: summary or event-command line has an unexpected shape')
    table = [lines[table_index]]
    for line in lines[table_index + 1:]:
        if not line.startswith('|'):
            break
        table.append(line)
    if len(table) < 3:
        raise SystemExit('PORT_STATUS: progress table missing or empty')
    rows = ['| Area | Mechanisms | Evidence scope | Fully evidenced | Partial | Open |',
            '|---|---:|---:|---:|---:|---:|']
    for line in table[1:]:
        if set(line.replace('|', '').replace(':', '').replace(' ', '')) <= {'-'}:
            continue
        cells = [cell.strip() for cell in line.strip().strip('|').split('|')]
        if len(cells) != 6:
            raise SystemExit(f'PORT_STATUS: unexpected table row {line!r}')
        area = README_AREAS.get(cells[0])
        if area is None:
            raise SystemExit(f'PORT_STATUS: unknown area label {cells[0]!r}')
        rows.append('| ' + ' | '.join([area, *cells[1:]]) + ' |')
    lines = [README_BEGIN,
             '<!-- Progress table and figures synced directly from docs/PORT_STATUS.md. -->',
             '',
             f'Explicit native evidence scope: **{summary.group(1)}** weighted, '
             f'**{summary.group(2)}** fully evidenced '
             f'({scope.group(1)}/{scope.group(2)} mechanisms; not overall game completion).', '',
             f'Event commands (bhScenarioJmpT): {events.group(1)}/{events.group(2)} bound, '
             f'{events.group(3)} consumed only, {events.group(4)} missing.', '', *rows, '',
             'Documented evidence scope only, not overall game completion.', README_END]
    return '\n'.join(lines)

def update_readme(path, status):
    text = path.read_text()
    # Landing pages can explicitly keep progress in docs/PORT_STATUS.md. Preserve the
    # required --readme build/commit invocation without reinserting a progress table.
    if README_EXTERNAL in text:
        if README_BEGIN in text or README_END in text:
            raise SystemExit(f'{path}: external status and inline status markers conflict')
        if '](docs/PORT_STATUS.md)' not in text:
            raise SystemExit(f'{path}: external status requires a link to docs/PORT_STATUS.md')
        return
    block = readme_block_from_status(status)
    pattern = re.compile(re.escape(README_BEGIN) + r'.*?' + re.escape(README_END), re.S)
    if not pattern.search(text):
        raise SystemExit(f'{path}: markers {README_BEGIN} / {README_END} missing')
    path.write_text(pattern.sub(lambda _: block, text, count=1))

def percent(part, whole):
    return 100.0 * part / whole if whole else 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--json', type=Path, help='vollständige Funktionsliste als JSON schreiben')
    parser.add_argument('--markdown', type=Path, help='Bericht als Markdown schreiben (z. B. docs/PORT_STATUS.md)')
    parser.add_argument('--readme', type=Path, help='Fortschrittstabelle aus docs/PORT_STATUS.md synchronisieren')
    parser.add_argument('--top', type=int, default=15, help='größte fehlende Funktionen anzeigen')
    parser.add_argument('--domain', help='nur diesen Bereich auflisten (Teilstring)')
    parser.add_argument('--fail-under', type=float, help='Exit 1, wenn der gewichtete Engine-Wert darunter liegt')
    args = parser.parse_args()

    records, opcodes, opcode_total = classify()
    groups = summarize(records)
    engine = collections.Counter()
    for label, g in groups.items():
        if label != PLATFORM:
            engine.update(g)
    opcode_counts = collections.Counter(status for _, status in opcodes.values())
    opcode_done = sum(1 for _, s in opcodes.values() if s == 'opcode')

    lines = []
    unconfirmed = [r for r in records if r['status'] == 'unbestaetigt']
    lines.append('# Nativer Portstatus')
    lines.append('')
    lines.append('Dieser Bericht wertet nur explizit belegte native Mechanismen. Die vollständige '
                 'Originaldecompilation ist absichtlich weder Nenner noch Fehlermenge.')
    lines.append('')
    lines.append(f'Expliziter Belegumfang: **{percent(engine["weighted"], engine["weight"]):.1f} %** gewichtet, '
                 f'**{percent(engine["strict"], engine["weight"]):.1f} %** voll belegt '
                 f'({engine["done_functions"]}/{engine["functions"]} Mechanismen; kein Gesamtfortschritt).')
    lines.append('')
    lines.append(f'Event-Befehle (bhScenarioJmpT): {opcode_done}/{opcode_total} gebunden, '
                 f'{opcode_counts["pending"]} nur konsumiert, {opcode_counts["fehlt"]} fehlen.')
    lines.append('')
    lines.append('| Bereich | Mechanismen | Belegumfang | voll belegt | teilweise | offen |')
    lines.append('|---|---:|---:|---:|---:|---:|')
    for label, g in groups.items():
        if not g['weight']:
            continue
        half = g['status:partial'] + g['status:pending']
        lines.append(f'| {label} | {g["done_functions"]}/{g["functions"]} | '
                     f'{percent(g["weighted"], g["weight"]):.1f} % | {percent(g["strict"], g["weight"]):.1f} % | '
                     f'{half} | {g["status:missing"]} |')
    lines.append('')
    lines.append('Nur Ledger-Einträge mit eigenständiger nativer Evidenz zählen. Namensgleichheit, '
                 'Originalquellen und quarantänierte Generatoren zählen nie als Portbeleg.')
    lines.append('')
    selection = [r for r in records if r['status'] in {'partial', 'missing'} and r['domain'] != PLATFORM]
    if args.domain:
        selection = [r for r in selection if args.domain.lower() in r['domain'].lower()]
    selection.sort(key=lambda r: -r['weight'] * (1 - SCORE[r['status']]))
    lines.append(f'## Größte offene Funktionen{" (" + args.domain + ")" if args.domain else ""}')
    lines.append('')
    for r in selection[:args.top]:
        lines.append(f'- `{r["file"]}{":" + str(r["line"]) if r["line"] else ""}` {r["function"]} ({r["status"]}'
                     f'{", " + r["note"] if r["note"] else ""})')
    if unconfirmed:
        lines.append('')
        lines.append('## Unbestätigte historische Ledger-Einträge')
        lines.append('')
        for r in unconfirmed:
            lines.append(f'- `{r["file"]}` {r["function"]}: {r["note"]}')
    report = '\n'.join(lines)
    print(report)
    if args.markdown:
        args.markdown.write_text(report + '\n\nErzeugt mit `python3 tools/port_check.py --markdown ' + str(args.markdown) + '`.\n')
    if args.readme:
        status = report if args.markdown and args.markdown.resolve() == PORT_STATUS.resolve() else PORT_STATUS.read_text()
        update_readme(args.readme, status)
    if args.json:
        args.json.write_text(json.dumps({'scope': 'explicit_native_evidence_only',
                                         'engine_weighted': percent(engine['weighted'], engine['weight']),
                                         'engine_strict': percent(engine['strict'], engine['weight']),
                                         'opcodes': {'total': opcode_total, **opcode_counts},
                                         'functions': records}, indent=1))
    if args.fail_under is not None and percent(engine['weighted'], engine['weight']) < args.fail_under:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
