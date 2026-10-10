#!/usr/bin/env python3
"""Windows-PE-Audit ohne Symbole im Binary.

Der von zig/LLD erzeugte PE traegt weder eine COFF-Symboltabelle noch
.debug-Abschnitte (Belege in work/native_port_windows_audit.md). Statt das
Binary direkt zu pruefen, fuehrt dieses Modul den Nachweis in drei Stufen:

 1. Den tatsaechlichen Linkbefehl des Ziels aus dem Buildsystem uebernehmen,
    einschliesslich Response-Dateien.
 2. Jede Eingabe pruefen: Objekte, Archive mit allen Mitgliedern,
    Importbibliotheken und die impliziten Laufzeitartefakte des Werkzeugsatzes.
 3. Mit genau diesen Eingaben ein Vergleichsbinary linken und Byte fuer Byte
    gegen das auszuliefernde Binary stellen. Abweichen duerfen nur Felder, die
    aus den PE-Strukturen geparst sind (COFF-Zeitstempel, Zeitstempel der
    Debug-Verzeichniseintraege, GUID und Age des RSDS-Satzes) - keine ganzen
    Abschnitte und keine freien Bytebereiche.

Jede Stufe faellt geschlossen aus: fehlende Eingaben, nicht lesbare Archive
und unerklaerte Byteunterschiede sind Fehler.
"""
from __future__ import annotations

import shlex
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

OBJECT_SUFFIXES = {".obj", ".o"}
ARCHIVE_SUFFIXES = {".a", ".lib"}
INPUT_SUFFIXES = OBJECT_SUFFIXES | ARCHIVE_SUFFIXES


# --- Stufe 3: PE-Metadatenfelder ------------------------------------------
def pe_metadata_fields(data: bytes):
    """(Name, Offset, Laenge) der Felder, die zwei Links unterscheiden duerfen.

    Alles wird aus dem PE geparst; es gibt keine festen Offsets und keine
    pauschal ausgeblendeten Abschnitte.
    """
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("no MZ header")
    lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[lfanew:lfanew + 4] != b"PE\0\0":
        raise ValueError("no PE signature")
    coff = lfanew + 4
    sections, optional_size = struct.unpack_from("<HH", data, coff + 2)[0], struct.unpack_from("<H", data, coff + 16)[0]
    optional = coff + 20
    magic = struct.unpack_from("<H", data, optional)[0]
    if magic not in (0x10B, 0x20B):
        raise ValueError("unknown optional header magic")
    directories = optional + (112 if magic == 0x20B else 96)
    debug_rva, debug_size = struct.unpack_from("<II", data, directories + 6 * 8)
    table = optional + optional_size
    layout = []
    for index in range(sections):
        entry = table + index * 40
        name = data[entry:entry + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, entry + 8)
        layout.append((name, virtual_address, virtual_size, raw_pointer, raw_size))

    def offset_of(rva: int):
        for _, virtual_address, virtual_size, raw_pointer, raw_size in layout:
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_pointer + (rva - virtual_address)
        return None

    fields = [("COFF TimeDateStamp", coff + 4, 4)]
    if debug_rva and debug_size:
        base = offset_of(debug_rva)
        if base is None:
            raise ValueError("debug directory outside every section")
        for index in range(debug_size // 28):
            entry = base + index * 28
            if entry + 28 > len(data):
                raise ValueError("debug directory truncated")
            _, _, _, _, kind, size, _, pointer = struct.unpack_from("<IIHHIIII", data, entry)
            fields.append((f"debug[{index}].TimeDateStamp", entry + 4, 4))
            if kind == 2 and size >= 24 and data[pointer:pointer + 4] == b"RSDS":
                # CV_INFO_PDB70: RSDS, GUID (16), Age (4), Pfad. Der Pfad bleibt
                # verbindlich; nur GUID und Age sind linkabhaengig.
                fields.append((f"debug[{index}].RSDS GUID+Age", pointer + 4, 20))
    return fields


def compare_images(shipped: bytes, rebuilt: bytes):
    problems = []
    if len(shipped) != len(rebuilt):
        return [f"comparison link differs in size: {len(shipped)} vs {len(rebuilt)} bytes"]
    try:
        allowed = pe_metadata_fields(shipped)
    except ValueError as error:
        return [f"shipped binary is not a readable PE: {error}"]
    try:
        if pe_metadata_fields(rebuilt) != allowed:
            return ["comparison link places its metadata fields differently"]
    except ValueError as error:
        return [f"comparison link is not a readable PE: {error}"]
    unexplained = []
    for index in range(len(shipped)):
        if shipped[index] == rebuilt[index]:
            continue
        if not any(start <= index < start + length for _, start, length in allowed):
            unexplained.append(index)
    if unexplained:
        preview = ", ".join(hex(value) for value in unexplained[:8])
        problems.append(
            f"{len(unexplained)} byte(s) differ outside the parsed metadata fields: {preview}")
    return problems


# --- Stufe 1: Linkbefehl ---------------------------------------------------
def link_command(build_dir: Path, target: str):
    """Den tatsaechlichen Linkbefehl des Ziels mit aufgeloesten Response-Dateien."""
    result = subprocess.run(["ninja", "-C", str(build_dir), "-t", "commands", target],
                            capture_output=True, text=True)
    if result.returncode or not result.stdout.strip():
        raise RuntimeError(f"link command for {target} is not recoverable from the build system")
    # ninja listet alle Befehle des Ziels; gesucht ist der, der genau dieses
    # Binary mit -o erzeugt - nicht etwa die nachgelagerte Pruefung.
    # ninja listet alle Befehle des Ziels, und CMake haengt Nachlaufschritte mit
    # && an dieselbe Zeile. Gesucht ist genau der Teilbefehl, der dieses Binary
    # mit -o erzeugt.
    argv = None
    for line in result.stdout.strip().splitlines():
        for part in line.split("&&"):
            try:
                tokens = [token for token in shlex.split(part) if token != ":"]
            except ValueError:
                continue
            if not tokens:
                continue
            for index, token in enumerate(tokens[:-1]):
                if token == "-o" and Path(tokens[index + 1]).name == target:
                    argv = tokens
                    break
    if argv is None:
        raise RuntimeError(f"no link command producing {target} in the build system")
    expanded = []
    for token in argv:  # Response-Dateien aufloesen
        if token.startswith("@"):
            response = (build_dir / token[1:]).resolve()
            if not response.is_file():
                raise RuntimeError(f"response file {response} named by the link command is missing")
            expanded += shlex.split(response.read_text())
        else:
            expanded.append(token)
    if not expanded:
        raise RuntimeError("empty link command")
    return expanded


def explicit_inputs(argv, build_dir: Path):
    """Jede im Linkbefehl genannte Eingabe, als vorhandener Pfad."""
    inputs, missing = [], []
    for token in argv[1:]:
        if token.startswith("-"):
            continue
        path = Path(token)
        if path.suffix.lower() not in INPUT_SUFFIXES:
            continue
        resolved = path if path.is_absolute() else build_dir / path
        if resolved.is_file():
            inputs.append(resolved.resolve())
        else:
            missing.append(str(path))
    return inputs, missing


def implicit_inputs(cache_dir: Path):
    """Die Laufzeitartefakte des Werkzeugsatzes als Obermenge.

    zig legt libc++/mingw-Objekte im eigenen Cache ab und nennt sie im
    Linkbefehl nicht. Geprueft wird deshalb alles, was dort fuer diesen Build
    liegt: enthaelt keines davon ein verbotenes Symbol, gilt das auch fuer die
    tatsaechlich gelinkte Teilmenge.
    """
    if not cache_dir.is_dir():
        raise RuntimeError(f"toolchain cache {cache_dir} is missing; implicit inputs are unverifiable")
    found = [path.resolve() for suffix in sorted(INPUT_SUFFIXES)
             for path in cache_dir.rglob(f"*{suffix}") if path.is_file()]
    if not found:
        raise RuntimeError(f"toolchain cache {cache_dir} holds no link artifacts")
    return found


# --- Stufe 2: Symbole aller Eingaben --------------------------------------
def audit_inputs(paths, nm: str, forbidden):
    """Verbotene Symbole in Objekten und allen Archivmitgliedern."""
    problems = []
    for start in range(0, len(paths), 64):
        batch = paths[start:start + 64]
        result = subprocess.run([nm, *(str(path) for path in batch)],
                                capture_output=True, text=True)
        if result.returncode and not result.stdout.strip():
            problems.append(f"symbol audit failed for {len(batch)} input(s), first {batch[0]}: "
                            f"{result.stderr.strip().splitlines()[:1]}")
            continue
        current = str(batch[0])
        names = {}
        for line in result.stdout.splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.endswith(":") and not stripped.split()[0].isalnum():
                current = stripped[:-1]
                continue
            if stripped.endswith(":") and " " not in stripped:
                current = stripped[:-1]
                continue
            names.setdefault(current, set()).add(stripped.split()[-1].lstrip("_"))
        for origin, symbols in names.items():
            hits = forbidden(symbols)
            if hits:
                problems.append(f"{origin}: forbidden linked symbols: {', '.join(sorted(hits))}")
    return problems


def archives_readable(paths, nm: str):
    """Ein Archiv ohne lesbares Inhaltsverzeichnis ist kein Nachweis."""
    problems = []
    for path in paths:
        if path.suffix.lower() not in ARCHIVE_SUFFIXES:
            continue
        result = subprocess.run([nm, "--print-armap", str(path)], capture_output=True, text=True)
        if result.returncode or not result.stdout.strip():
            problems.append(f"{path}: archive members are not auditable")
    return problems


# --- Stufe 3: Vergleichslink ----------------------------------------------
def comparison_link(argv, build_dir: Path, exe: Path, env=None):
    """Denselben Befehl erneut ausfuehren und das Ergebnis zurueckgeben.

    Das Vergleichsbinary bekommt denselben Dateinamen in einem eigenen
    Verzeichnis, damit der im RSDS-Satz hinterlegte Name unveraendert bleibt.
    """
    with tempfile.TemporaryDirectory() as directory:
        target = Path(directory) / exe.name
        rebuilt = []
        for index, token in enumerate(argv):
            rebuilt.append(str(target) if index and argv[index - 1] == "-o" else token)
        if str(target) not in rebuilt:
            raise RuntimeError("link command has no -o output to redirect")
        result = subprocess.run(rebuilt, cwd=build_dir, capture_output=True, text=True, env=env)
        if result.returncode:
            raise RuntimeError("comparison link failed: "
                               + (result.stderr.strip().splitlines() or [""])[-1][:200])
        if not target.is_file():
            raise RuntimeError("comparison link produced no binary")
        return target.read_bytes()


def audit(exe: Path, build_dir: Path, target: str, cache_dir: Path, nm: str, forbidden, env=None):
    """Alle drei Stufen. Rueckgabe: Liste der Befunde (leer = bestanden)."""
    problems = []
    try:
        argv = link_command(build_dir, target)
    except RuntimeError as error:
        return [str(error)]
    inputs, missing = explicit_inputs(argv, build_dir)
    for name in missing:
        problems.append(f"link input {name} named by the build is missing")
    if not inputs:
        problems.append("link command names no auditable input")
    try:
        inputs += implicit_inputs(cache_dir)
    except RuntimeError as error:
        problems.append(str(error))
    problems += archives_readable(inputs, nm)
    problems += audit_inputs(inputs, nm, forbidden)
    try:
        rebuilt = comparison_link(argv, build_dir, exe, env)
    except RuntimeError as error:
        problems.append(str(error))
        return problems
    problems += compare_images(exe.read_bytes(), rebuilt)
    return problems
