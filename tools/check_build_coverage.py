#!/usr/bin/env python3
"""Anschlusspruefer: native Quellen ohne Buildziel, Einstiegspunkte ohne Aufrufer.

Die Abnahmeregel der Uebergabe lautet "Implementierung ohne Anschluss ist
offen". Dafuer gab es bisher kein Werkzeug: ``check_no_original_core.py``
prueft die Originalkerngrenze, ``port_check.py`` die belegte Mechanismen-
abdeckung, ``windows_link_audit.py`` die PE-Bauidentitaet. Keines beantwortet
die Frage, ob eine vorhandene native Quelle ueberhaupt in einem Buildziel
steht und ob ihre Einstiegspunkte je aufgerufen werden.

Gemeldet werden drei Zustaende:

``nicht_gebaut``
    Die Quelle liegt im Baum, kommt aber in keinem ``add_library`` /
    ``add_executable`` / ``target_sources`` vor. Sie wird nie kompiliert, also
    auch nicht typgeprueft, nicht getestet und nicht ausgeliefert.

``gebaut_ohne_aufrufer``
    Die Quelle wird kompiliert, aber kein oeffentlicher Einstiegspunkt aus
    ihrem Header wird in einer anderen gebauten Quelle aufgerufen. Das ist der
    Zustand "implementiert, nicht angeschlossen".

``nur_test``
    Einstiegspunkte werden ausschliesslich aus Testquellen aufgerufen, nicht
    aus dem produktiven Pfad. Komponentengeprueft, nicht produktiv gebunden.

Grenzen, bewusst offen gehalten:

* Dies ist ein Textparser ueber die CMake-Dateien, keine CMake-Auswertung.
  Quellen hinter ``if()``-Zweigen gelten als gebaut; unaufgeloeste
  ``${VARIABLE}``-Quellenlisten werden als solche gemeldet, nicht geraten.
* Ein Aufruf wird als Namensvorkommen ``name(`` erkannt. Das findet Aufrufe
  ueber Funktionszeigertabellen nicht, wenn dort nur der nackte Name steht;
  solche Faelle erscheinen deshalb zusaetzlich unter ``nur_referenziert``.
* Der Bericht ist ein Anschlussmass, kein Nachweis richtiger Semantik und
  keine Gameplay-Abnahme. Eine gebaute, aufgerufene Datei kann trotzdem
  unvollstaendig portiert sein.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = ('.c', '.cpp', '.cc')
HEADER_SUFFIXES = ('.h', '.hpp')

_COMMAND = re.compile(r'\b(add_library|add_executable|target_sources)\s*\(', re.IGNORECASE)

# Schluesselwoerter, die in den Quellenlisten dieser drei Befehle stehen duerfen.
_KEYWORDS = frozenset({
    'PRIVATE', 'PUBLIC', 'INTERFACE', 'STATIC', 'SHARED', 'MODULE', 'OBJECT',
    'ALIAS', 'IMPORTED', 'GLOBAL', 'EXCLUDE_FROM_ALL', 'WIN32', 'MACOSX_BUNDLE',
})

# Eine Funktionsdeklaration im Header: optionale Qualifizierer, Rueckgabetyp,
# Name, Klammer, irgendwann ein Semikolon. Makros und Typedefs fallen weg.
_DECL = re.compile(
    r'^\s*(?!typedef\b|return\b|if\b|while\b|for\b|switch\b|else\b)'
    r'(?:extern\s+|static\s+|inline\s+|const\s+|unsigned\s+|signed\s+|struct\s+|enum\s+)*'
    r'[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)*\s+(?:\*\s*)*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*\(',
)


def strip_comments(text: str) -> str:
    """CMake-Kommentare entfernen.

    Einfache Form: ein ``#`` beendet die Zeile. Ein ``#`` innerhalb eines
    Strings wuerde hier falsch behandelt; in den Buildateien dieses Projekts
    kommt das nicht vor und der Fall wird im Bericht nicht behauptet.
    """
    return '\n'.join(line.split('#', 1)[0] for line in text.splitlines())


def parse_targets(text: str) -> dict:
    """Die Quellenlisten der drei Zielbefehle einsammeln.

    Mehrzeilige Aufrufe werden ueber Klammerbalance gelesen, damit eine Quelle
    in der zweiten Zeile eines ``add_executable`` nicht verloren geht.
    """
    text = strip_comments(text)
    targets: dict = {}
    for match in _COMMAND.finditer(text):
        command = match.group(1).lower()
        index = match.end()
        depth = 1
        start = index
        while index < len(text) and depth:
            if text[index] == '(':
                depth += 1
            elif text[index] == ')':
                depth -= 1
            index += 1
        if depth:
            continue  # unbalanciert: nichts behaupten
        tokens = text[start:index - 1].split()
        if not tokens:
            continue
        name = tokens[0]
        entry = targets.setdefault(name, {
            'kind': command if command != 'target_sources' else 'unknown',
            'sources': [],
            'unresolved': [],
            'excluded': False,
        })
        if command != 'target_sources':
            entry['kind'] = command
        for token in tokens[1:]:
            if token.upper() in _KEYWORDS:
                if token.upper() == 'EXCLUDE_FROM_ALL':
                    entry['excluded'] = True
                continue
            if '${' in token:
                entry['unresolved'].append(token)
                continue
            if token.endswith(SOURCE_SUFFIXES):
                entry['sources'].append(token)
    return targets


def discover_cmake_files(root: Path) -> list:
    """Alle CMake-Dateien, die Ziele definieren koennen."""
    found = []
    top = root / 'CMakeLists.txt'
    if top.is_file():
        found.append(top)
    for path in sorted(root.glob('*/CMakeLists.txt')):
        found.append(path)
    for path in sorted(root.glob('cmake/*.cmake')):
        found.append(path)
    return found


def discover_sources(root: Path, directories) -> list:
    out = []
    for directory in directories:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob('*')):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                out.append(path.relative_to(root).as_posix())
    return out


def declaration_statements(text: str) -> list:
    """Den Headertext in logische Deklarationen zerlegen.

    Eine Deklaration darf ueber mehrere Zeilen laufen, deshalb wird am
    Semikolon segmentiert und nicht zeilenweise gelesen. Klammern werden
    transparent gemacht: 37 Header dieses Projekts umschliessen ihre echten
    Deklarationen mit ``extern "C" { ... }``, dessen Inhalt nicht verloren
    gehen darf. Struct-Koerper stoeren dabei nicht, weil Feldnamen keinen
    Klammerausdruck direkt hinter sich haben und Funktionszeigerfelder
    (``void (*fn)(...)``) kein Namensmuster vor der Klammer zeigen.
    Praeprozessorzeilen entfallen, damit ein ``#define foo(x)`` nicht als
    Einstiegspunkt gilt.
    """
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', ' ', text)
    text = '\n'.join(line for line in text.splitlines()
                     if not line.lstrip().startswith('#'))
    # Der Linkage-Specifier faellt weg, sonst steht sein Stringliteral am Anfang
    # desselben Segments wie die erste Deklaration des Blocks.
    text = re.sub(r'\bextern\s*"[^"]*"', ' ', text)
    text = text.replace('{', ' ').replace('}', ' ')
    return [' '.join(part.split()) for part in text.split(';') if part.strip()]


def public_entry_points(root: Path, source: str) -> list:
    """Die im zugehoerigen Header deklarierten Funktionsnamen.

    Ohne Header gibt es keine oeffentliche Schnittstelle, die ein anderer
    Uebersetzungsabschnitt aufrufen koennte; dann bleibt die Liste leer und der
    Anschluss wird nicht behauptet. Klassenmethoden eines C++-Typs sind keine
    freien Einstiegspunkte und werden hier nicht erfasst — fuer Besitzer wie
    ``RoomWorld`` trifft der Pruefer deshalb keine Anschlussaussage.
    """
    stem = Path(source)
    names: list = []
    for suffix in HEADER_SUFFIXES:
        header = root / stem.with_suffix(suffix)
        if not header.is_file():
            continue
        for statement in declaration_statements(header.read_text(errors='replace')):
            match = _DECL.match(statement)
            if match:
                names.append(match.group(1))
    # Reihenfolge erhalten, Duplikate entfernen.
    return list(dict.fromkeys(names))


def is_test_source(source: str) -> bool:
    return source.startswith('tests/') or '/tests/' in source


def scan_references(root: Path, sources, names) -> dict:
    """Pro Name die Quellen finden, die ihn aufrufen oder nennen.

    Ein Durchlauf je Quelle mit zwei kombinierten Mustern ueber alle Namen,
    damit die Pruefung nicht quadratisch viele Dateien liest.
    """
    wanted = sorted(set(names), key=len, reverse=True)
    calls: dict = {name: [] for name in wanted}
    mentions: dict = {name: [] for name in wanted}
    if not wanted:
        return {'calls': calls, 'mentions': mentions}
    alternation = '|'.join(re.escape(name) for name in wanted)
    call_pattern = re.compile(r'\b(' + alternation + r')\s*\(')
    mention_pattern = re.compile(r'\b(' + alternation + r')\b')
    for source in sources:
        path = root / source
        if not path.is_file():
            continue
        text = path.read_text(errors='replace')
        called = set(call_pattern.findall(text))
        for name in called:
            calls[name].append(source)
        for name in set(mention_pattern.findall(text)) - called:
            mentions[name].append(source)
    return {'calls': calls, 'mentions': mentions}


def audit(root: Path, directories) -> dict:
    cmake_files = discover_cmake_files(root)
    targets: dict = {}
    for path in cmake_files:
        for name, entry in parse_targets(path.read_text(errors='replace')).items():
            existing = targets.setdefault(name, {
                'kind': entry['kind'], 'sources': [], 'unresolved': [], 'excluded': False,
            })
            existing['sources'].extend(entry['sources'])
            existing['unresolved'].extend(entry['unresolved'])
            existing['excluded'] = existing['excluded'] or entry['excluded']
            if entry['kind'] != 'unknown':
                existing['kind'] = entry['kind']

    built = set()
    for entry in targets.values():
        built.update(entry['sources'])

    present = discover_sources(root, directories)
    not_built = [source for source in present if source not in built]

    built_present = [source for source in present if source in built]
    findings = {
        'nicht_gebaut': [],
        'gebaut_ohne_aufrufer': [],
        'nur_test': [],
        'nur_referenziert': [],
    }

    for source in not_built:
        names = public_entry_points(root, source)
        findings['nicht_gebaut'].append({
            'quelle': source,
            'zeilen': len((root / source).read_text(errors='replace').splitlines()),
            'einstiegspunkte': names,
        })

    # Einstiegspunkte einmal einsammeln und die gebauten Quellen einmal lesen.
    entry_points = {source: public_entry_points(root, source) for source in built_present}
    every_name = {name for names in entry_points.values() for name in names}
    seen = scan_references(root, built_present, every_name)

    for source in built_present:
        names = entry_points[source]
        if not names:
            continue
        # Die eigene Quelle zaehlt nicht als Aufrufer ihrer eigenen Schnittstelle.
        calling = {}
        naming = {}
        for name in names:
            external_calls = [f for f in seen['calls'].get(name, []) if f != source]
            external_names = [f for f in seen['mentions'].get(name, []) if f != source]
            if external_calls:
                calling[name] = external_calls
            elif external_names:
                naming[name] = external_names
        if calling:
            productive = {name: files for name, files in calling.items()
                          if any(not is_test_source(f) for f in files)}
            if not productive:
                findings['nur_test'].append({
                    'quelle': source,
                    'nur_aus_tests': sorted({f for files in calling.values() for f in files}),
                })
            continue
        if naming:
            findings['nur_referenziert'].append({
                'quelle': source,
                'genannt_in': sorted({f for files in naming.values() for f in files}),
                'einstiegspunkte': sorted(naming),
            })
            continue
        findings['gebaut_ohne_aufrufer'].append({
            'quelle': source,
            'einstiegspunkte': names,
        })

    unresolved = sorted({token for entry in targets.values() for token in entry['unresolved']})
    return {
        'cmake_dateien': [path.name for path in cmake_files],
        'ziele': len(targets),
        'quellen_vorhanden': len(present),
        'quellen_gebaut': len(built_present),
        'unaufgeloeste_variablen': unresolved,
        'befunde': findings,
    }


def format_report(result: dict) -> str:
    lines = []
    lines.append('Anschlusspruefung native Quellen')
    lines.append('=' * 34)
    lines.append(f"CMake-Dateien: {', '.join(result['cmake_dateien'])}")
    lines.append(f"Ziele: {result['ziele']}   "
                 f"Quellen vorhanden: {result['quellen_vorhanden']}   "
                 f"davon gebaut: {result['quellen_gebaut']}")
    if result['unaufgeloeste_variablen']:
        lines.append('Unaufgeloeste Quellenvariablen (nicht geraten): '
                     + ', '.join(result['unaufgeloeste_variablen']))

    findings = result['befunde']

    not_built = findings['nicht_gebaut']
    lines.append('')
    lines.append(f'nicht_gebaut ({len(not_built)}) — liegt im Baum, steht in keinem Ziel:')
    if not not_built:
        lines.append('  keine')
    for item in sorted(not_built, key=lambda i: -i['zeilen']):
        entries = len(item['einstiegspunkte'])
        lines.append(f"  {item['zeilen']:>6} Zeilen  {item['quelle']}"
                     f"  ({entries} Einstiegspunkte)")
    if not_built:
        total = sum(item['zeilen'] for item in not_built)
        lines.append(f"  Summe: {total} Zeilen werden nie kompiliert.")

    for key, caption in (
        ('gebaut_ohne_aufrufer', 'gebaut, kein Einstiegspunkt wird aufgerufen'),
        ('nur_test', 'Einstiegspunkte nur aus Testquellen aufgerufen'),
        ('nur_referenziert', 'Name genannt, aber nicht aufgerufen (Tabelle/Zeiger pruefen)'),
    ):
        items = findings[key]
        lines.append('')
        lines.append(f'{key} ({len(items)}) — {caption}:')
        if not items:
            lines.append('  keine')
        for item in sorted(items, key=lambda i: i['quelle']):
            lines.append(f"  {item['quelle']}")
    lines.append('')
    lines.append('Anschlussmass, kein Semantiknachweis und keine Gameplay-Abnahme.')
    return '\n'.join(lines)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--root', default='.', help='Projektwurzel')
    parser.add_argument('--directory', action='append', default=None,
                        help='zu pruefendes Quellverzeichnis (mehrfach), Standard: native tests')
    parser.add_argument('--json', action='store_true', help='Bericht als JSON')
    parser.add_argument('--strict', action='store_true',
                        help='Rueckgabe 1, wenn ein Befund vorliegt')
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    directories = args.directory or ['native', 'tests']
    result = audit(root, directories)

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print(format_report(result))

    if args.strict and any(result['befunde'].values()):
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
