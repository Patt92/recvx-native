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
            if re.search(r'\bworld\(t,\s*(?:cursor,\s*)?op', line) or 'pending' in line.lower() or 'remain' in line.lower():
                pending.update(ops)
    bound = set()
    for name in ('world.cpp', 'runtime.cpp'):
        text = (NATIVE / name).read_text()
        bound.update(int(v, 0) for v in re.findall(r'\b(?:op|opcode)\s*==\s*(0x[0-9a-fA-F]+|\d+)', text))
        # Tabellengebundene Befehle (z. B. die bhObjLinkSet*-Familie: {0x32,{1,2}}, ...).
        bound.update(int(v, 0) for v in re.findall(r'\{(0x[0-9a-fA-F]+),\s*\{\d+,\s*\d+\}\}', text))
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

# The generated report and README excerpt use the same English labels. Ledger
# domain names remain stable identifiers in the input table.
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
    summary_index = next((i for i, line in enumerate(lines) if line.startswith('Explicit evidence scope:')), None)
    events_index = next((i for i, line in enumerate(lines) if line.startswith('Event commands (bhScenarioJmpT):')), None)
    table_index = next((i for i, line in enumerate(lines) if line.startswith('| Area | Mechanisms | Evidence scope |')), None)
    if summary_index is None or events_index is None or table_index is None:
        raise SystemExit('PORT_STATUS: summary or event-command line not readable')
    summary = re.fullmatch(
        r'Explicit evidence scope: \*\*(.+?)\*\* weighted, \*\*(.+?)\*\* fully evidenced \((.+?)\)\.',
        lines[summary_index])
    events = re.fullmatch(
        r'Event commands \(bhScenarioJmpT\): (\d+)/(\d+) bound, (\d+) consumed only, (\d+) missing\.',
        lines[events_index])
    scope = re.fullmatch(r'(\d+)/(\d+) mechanisms; not overall game completion', summary.group(3)) if summary else None
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
        area = cells[0]
        if area not in README_AREAS.values():
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


# English descriptions for entries shown in the public Markdown report. The
# detailed, source-backed notes remain in port_ledger.tsv; this map does not
# affect classification or scores. A new entry without a translation stays
# visibly linked to the ledger instead of leaking German into the report.
REPORT_NOTES_EN = {
    ('adv.c', 'Adv_GameOptionScreen'): 'Original option plates for display, sound, vibration, and key configuration remain unported; OPTION opens documented host settings instead.',
    ('adv.c', 'CheckMcSelectPortInfoState'): 'Original memory-card port query remains unported; host slot selection reads occupied saves from the user-data directory.',
    ('adv.c', 'FindFirstVmDrive'): 'Original memory-card search remains unported; host saves in the platform user-data directory enable LOAD GAME when a slot is occupied.',
    ('effsub4.c', 'bhEff_Sub367'): 'No producer of exp0 (EFFSUB367_WORK) or op->func was found in original C or ELF; bhEff367 remains open (effsub4.c:2007).',
    ('MdlPut.c', 'bhPutModel'): 'Mirror pass from MdlPut.c:57-79/89-111 is partially connected: qualifying models draw a second time under cam.mtx*njMirror, with light based on unmirrored cam.mtx. Player floor, enemy, and object/item flags are connected; hair and weapons rely on measured status bytes. Child enemies check their own byte. mirror_pass has two red control cases.',
    ('Motion.c', 'bhSetMotion:Spieler'): 'Player node pose uses the existing object tree before motion advance, including Hokan countdown. Action/event, combat, damage, grab, push, and draw edits are connected; player_motion_blend and pose traces cover transitions. Stairs, Game Over, and script-driven player motion still do not update this tree.',
    ('adv.c', 'AdvGetCancelButton'): 'Host Cancel is used instead of the original keytype table {32,128,64}.',
    ('adv.c', 'AdvGetOkButton'): 'Host Confirm represents both Start (0x800) and keytype OK; original keytype tables {192,96,160} are not bound.',
    ('adv.c', 'ResetAdvSystem'): 'Only title fields NextReturnCode/ExtraFlag/NextMode/GenFlag and StopAdvScreenSaver(1) are connected; texture lists, option fields, and VM fields are missing.',
    ('camera.c', 'bhControlCamera'): 'The runtime owns cam.ofx/ofy/ofz across rooms, and RoomWorld::camera adds the offset to cam.px. Original writers EN02/EN13/EN17, event 0x5a, and bhEff30a are connected. Room load clears the offset as bhInitCamera does. Runtime observation of room-change reset and a separate bhGetCameraPosition check remain open.',
    ('cut.c', 'bhControlActiveCamera'): 'For stair state, the camera follows owP[0] from the last bhCalcModel rather than the player position. RM_3080 no longer stalls; camera_follow_target has a red control case. Free-control EXP_WORK interpolation, bhCheckCutAttribute floor input, and eye-camera return remain open.',
    ('cut.c', 'bhControlEventCamera'): 'RoomWorld::control_event_camera follows the mode0 0/1/2/3 state machine and 20 spline slots; the paused camera holds its pose. RM_0040 event 5 now holds key 1 as in RDT. A native owner for pt_flg 0x1 and proven CAM_WORK narrowing for cam.pers remain open.',
    ('cut.c', 'bhSetCut'): 'RoomWorld::cut_fog_handover carries cut fog and fog_ct rules through initialize_camera_clip. Other bhSetCut clip/hide portions remain as previously recorded.',
    ('cut.c', 'bhSetEventCamera'): 'Opcode 0x13 mode 0 now installs signed key position/angles, perspective, lock angles, and fog in both branches before event-camera initialization. Previous-key fields, cam.ln/flags, and bhSetEventHideObjLgt remain open.',
    ('cut.c', 'bhSetFixedCut'): 'Opcode 0x2a mode 0 transfers fog and installs cam.ncut before bhChangeViewClipRM, so the near plane comes from the new cut. native_ground_world has a red control case.',
    ('en02.c', 'bhEne02'): 'Historical extraction/generator evidence is invalid for the native port. A native Gulp Worm state machine on WorldEntity covers init, movement, underground pursuit, emergence/swallowing, rocks, retreat, damage, sand effects, player control, camera offset, audio, and hit marks; en02_world and RM_5830 headless checks are recorded. The uninitialized ang in SetSandEffectMain case 8 is evidenced by disassembly; three sprites are omitted while particles run. RM_3091/RM_7220 and visible window acceptance remain open.',
    ('event.c', 'bhArmsItemSet'): 'Historical extraction/generator evidence is invalid. First hit, bag range, and equip slot were examined; the full WeaponSet/monitor state machine remains missing.',
    ('event.c', 'bhItemGetGet'): 'Historical extraction/generator evidence is invalid. Inventory bank, flags, quantity, and equip slot were checked; independent ply_wno/monitor behavior is incomplete.',
    ('event.c', 'bhItemGetGetEx'): 'Historical extraction/generator evidence is invalid. Free slots, shotgun, and equipment were checked; independent ply_wno/monitor behavior is incomplete.',
    ('event.c', 'bhPlItemChangeM'): 'Historical extraction/generator evidence is invalid. First hit, packed ID, and original quantity/flag removal were checked.',
    ('event.c', 'bhPlItemLost'): 'Historical extraction/generator evidence is invalid. Fixed slot ranges, first hit, and compaction were examined; the full WeaponSet/monitor state machine remains missing.',
    ('event.c', 'bhPlItemLostEx'): 'Historical extraction/generator evidence is invalid. Transfer to bank 14 and fixed compaction were examined; the full WeaponSet/monitor state machine remains missing.',
    ('event.c', 'bhPlItemTamaSet'): 'Historical extraction/generator evidence is invalid. First hit and u16 little-endian quantity were checked.',
    ('hitchk.c', 'bhCheckWall2Box'): 'Historical extraction/generator evidence is invalid. Original query and geometry helpers were extracted; static/mobile ATR walls and prior object position were bound, with wall and second-crate tests.',
    ('hitchk.c', 'bhGetGroundPosition'): 'Historical extraction/generator evidence is invalid. The generated ground query kept room/mobile index and live ATR work. Player push/stair binding, linked casing effect, EE random state, and first-free eff[512] mapping were reported; other random consumers, exact EasyLight, full effect lifecycle, and general player work remain open.',
    ('objitm.c', 'bhObj001'): 'Historical extraction/generator evidence is invalid. Mobile walls, side probes, work indices, speed, and box collision were tested; auxiliary metc/mflr consumers for crate climbing remain incomplete.',
    ('player.c', 'bhCPM2_act_psh'): 'Historical extraction/generator evidence is invalid. MTN49/50/reverse49, Hokan, and curve9 were bound and tested; physical vibration callback remains missing.',
    ('ps2_NaSystem.c', 'njColorBlendingModeSys'): 'Historical extraction/generator evidence is invalid. Native chunk_blend.cpp models nj/GS saved state, ALPHA register selection, and RGB=(A-B)*C+D; FBA=0, PABE, GS packet quantization, and dc_alpha remain unmodeled. Blend, effect, weapon-flash, lighter, and runtime checks are recorded.',
    ('sub1.c', 'GetItem'): 'Historical extraction/generator evidence is invalid. GetItem including branch 153 and subsequent ItemSort were extracted; StatusCancel, sprite transitions, and full StatusMain were not completely bound.',
    ('sub1.c', 'ItemSort'): 'Historical extraction/generator evidence is invalid. Standard inventory sorting after pickup and mutation was tested; special flgchk in the full StatusMain remains incomplete.',
}


def report_note(record):
    return REPORT_NOTES_EN.get((record['file'], record['function']),
                               'See the detailed evidence and open bindings in docs/port_ledger.tsv.')


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
    lines.append('# Native port status')
    lines.append('')
    lines.append('This report counts only native mechanisms with explicit evidence. The complete '
                 'original decompilation is deliberately neither the denominator nor a list of missing work.')
    lines.append('')
    lines.append(f'Explicit evidence scope: **{percent(engine["weighted"], engine["weight"]):.1f} %** weighted, '
                 f'**{percent(engine["strict"], engine["weight"]):.1f} %** fully evidenced '
                 f'({engine["done_functions"]}/{engine["functions"]} mechanisms; not overall game completion).')
    lines.append('')
    lines.append(f'Event commands (bhScenarioJmpT): {opcode_done}/{opcode_total} bound, '
                 f'{opcode_counts["pending"]} consumed only, {opcode_counts["fehlt"]} missing.')
    lines.append('')
    lines.append('| Area | Mechanisms | Evidence scope | Fully evidenced | Partial | Open |')
    lines.append('|---|---:|---:|---:|---:|---:|')
    for label, g in groups.items():
        if not g['weight']:
            continue
        half = g['status:partial'] + g['status:pending']
        lines.append(f'| {README_AREAS[label]} | {g["done_functions"]}/{g["functions"]} | '
                     f'{percent(g["weighted"], g["weight"]):.1f} % | {percent(g["strict"], g["weight"]):.1f} % | '
                     f'{half} | {g["status:missing"]} |')
    lines.append('')
    lines.append('Only ledger entries with independent native evidence count. Matching names, '
                 'original sources, and quarantined generators never count as port evidence.')
    lines.append('')
    selection = [r for r in records if r['status'] in {'partial', 'missing'} and r['domain'] != PLATFORM]
    if args.domain:
        selection = [r for r in selection if args.domain.lower() in r['domain'].lower()]
    selection.sort(key=lambda r: -r['weight'] * (1 - SCORE[r['status']]))
    lines.append(f'## Largest open functions{" (" + args.domain + ")" if args.domain else ""}')
    lines.append('')
    for r in selection[:args.top]:
        lines.append(f'- `{r["file"]}{":" + str(r["line"]) if r["line"] else ""}` {r["function"]} '
                     f'({r["status"]}, {report_note(r)})')
    if unconfirmed:
        lines.append('')
        lines.append('## Unconfirmed historical ledger entries')
        lines.append('')
        for r in unconfirmed:
            lines.append(f'- `{r["file"]}` {r["function"]}: {report_note(r)}')
    report = '\n'.join(lines)
    print(report)
    if args.markdown:
        args.markdown.write_text(report + '\n\nGenerated with `python3 tools/port_check.py --markdown ' + str(args.markdown) + '`.\n')
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
