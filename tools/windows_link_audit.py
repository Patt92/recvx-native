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

import os
import re
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
def ninja_program(build_dir: Path) -> str:
    """Das Ninja, das diesen Build erzeugt hat (CMAKE_MAKE_PROGRAM), sonst ninja aus PATH."""
    cache = build_dir / "CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(errors="replace").splitlines():
            if line.startswith("CMAKE_MAKE_PROGRAM:"):
                value = line.split("=", 1)[1].strip()
                if value:
                    return value
    return "ninja"


def ninja_commands(build_dir: Path, target: str) -> str:
    result = subprocess.run([ninja_program(build_dir), "-C", str(build_dir), "-t", "commands", target],
                            capture_output=True, text=True)
    if result.returncode or not result.stdout.strip():
        raise RuntimeError(f"link command for {target} is not recoverable from the build system")
    return result.stdout


def link_command(build_dir: Path, target: str):
    """Den tatsaechlichen Linkbefehl des Ziels mit aufgeloesten Response-Dateien."""
    commands = ninja_commands(build_dir, target)
    # ninja listet alle Befehle des Ziels; gesucht ist der, der genau dieses
    # Binary mit -o erzeugt - nicht etwa die nachgelagerte Pruefung.
    # ninja listet alle Befehle des Ziels, und CMake haengt Nachlaufschritte mit
    # && an dieselbe Zeile. Gesucht ist genau der Teilbefehl, der dieses Binary
    # mit -o erzeugt.
    argv = None
    for line in commands.strip().splitlines():
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


# --- MSVC link.exe (nativer Windows-Build mit Ninja) -------------------------
# Derselbe Nachweis in drei Stufen. Unterschiede zum zig-Pfad: der Linkbefehl
# nennt das Ziel mit /out:, CMake ruft link.exe ueber `cmake -E vs_link_exe`
# auf, und die impliziten Bibliotheken (/DEFAULTLIB der Objekte, LIB-Suchpfad)
# stehen in keinem Cache, sondern meldet der Linker selbst (/VERBOSE:LIB).
def windows_split(text: str):
    """Argumente nach den Regeln von CommandLineToArgvW / der MSVC-Laufzeit.

    shlex (POSIX) verschluckt die Backslashes der Windows-Pfade.
    """
    tokens, current, quoted, have, index = [], [], False, False, 0
    while index < len(text):
        char = text[index]
        if char == "\\":
            run = 0
            while index < len(text) and text[index] == "\\":
                run += 1
                index += 1
            if index < len(text) and text[index] == '"':
                current.append("\\" * (run // 2))
                if run % 2:
                    current.append('"')
                    index += 1
                have = True
            else:
                current.append("\\" * run)
                have = True
            continue
        if char == '"':
            if quoted and index + 1 < len(text) and text[index + 1] == '"':
                current.append('"')
                index += 2
                continue
            quoted = not quoted
            have = True
        elif char in " \t" and not quoted:
            if have:
                tokens.append("".join(current))
                current, have = [], False
        else:
            current.append(char)
            have = True
        index += 1
    if quoted:
        raise ValueError("unbalanced quote")
    if have:
        tokens.append("".join(current))
    return tokens


def msvc_option(token: str):
    """(name, Wert) einer link.exe-Option `/name:wert` bzw. `-name:wert`, sonst None."""
    if token[:1] not in "/-" or len(token) < 2:
        return None
    name, _, value = token[1:].partition(":")
    return name.lower(), value


def vs_link_command(tokens, target: str):
    """link.exe-Befehl, wie ihn `cmake -E vs_link_exe` tatsaechlich ausfuehrt.

    cmVSLink (CMake cmcmd.cxx): ohne /INCREMENTAL haengt der Wrapper
    /MANIFEST:EMBED,ID=1 (exe) bzw. ID=2 (dll) an, sofern kein /MANIFEST:NO
    steht, dazu /MANIFESTINPUT: je Datei hinter --manifests. Der inkrementelle
    Zweig (rc/mt in zwei Durchgaengen) wird nicht nachgebildet: fail-closed.
    Ein Modellfehler hier kann nur einen Byteunterschied im Vergleichslink
    erzeugen, nie einen falschen Nachweis.
    """
    mode = next((token for token in tokens if token in ("vs_link_exe", "vs_link_dll")), None)
    if mode is None:
        return list(tokens)
    if "--" not in tokens:
        raise RuntimeError("vs_link wrapper without -- before the link command")
    split = tokens.index("--")
    wrapper, argv = tokens[:split], list(tokens[split + 1:])
    manifests = []
    if "--manifests" in wrapper:
        manifests = [token for token in wrapper[wrapper.index("--manifests") + 1:] if not token.startswith("--")]
    options = [msvc_option(token) for token in argv]
    if any(option and option[0] == "incremental" and option[1].lower() in ("", "yes") for option in options):
        raise RuntimeError(f"incremental vs_link of {target} is not reproducible by the audit")
    generates = not any(option and option[0] == "manifest" and option[1].lower() == "no" for option in options)
    if generates or manifests:
        argv.append(f"/MANIFEST:EMBED,ID={'1' if mode == 'vs_link_exe' else '2'}")
        argv += [f"/MANIFESTINPUT:{manifest}" for manifest in manifests]
    return argv


def read_response(path: Path) -> str:
    data = path.read_bytes()
    if data[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return data.decode("utf-16")
    return data.decode("utf-8-sig")


def msvc_link_from_commands(commands: str, target: str, build_dir: Path):
    """Der Teilbefehl, der `target` mit link.exe /out: erzeugt; None, wenn keiner das tut."""
    for line in commands.strip().splitlines():
        for part in line.split("&&"):
            try:
                tokens = windows_split(part.strip())
            except ValueError:
                continue
            outputs = [option[1] for option in map(msvc_option, tokens)
                       if option and option[0] == "out"]
            if not any(Path(value.replace("\\", "/")).name.lower() == target.lower() for value in outputs):
                continue
            argv = vs_link_command(tokens, target)
            if not argv or Path(argv[0].replace("\\", "/")).name.lower() != "link.exe":
                raise RuntimeError(f"/out:{target} is not produced by link.exe: {argv[:1]}")
            expanded = []
            for token in argv:
                if token.startswith("@"):
                    response = Path(token[1:]) if Path(token[1:]).is_absolute() else build_dir / token[1:]
                    if not response.is_file():
                        raise RuntimeError(f"response file {response} named by the link command is missing")
                    expanded += windows_split(read_response(response))
                else:
                    expanded.append(token)
            return expanded
    return None


def msvc_library_paths(argv, environ):
    paths = [Path(value) for option in map(msvc_option, argv[1:])
             if option and option[0] == "libpath" for value in [option[1]]]
    paths += [Path(entry) for entry in environ.get("LIB", "").split(";") if entry]
    return paths


def msvc_explicit_inputs(argv, build_dir: Path, environ):
    """Objekte und Bibliotheken des Befehls, aufgeloest wie link.exe: Build-, /LIBPATH-, LIB-Verzeichnisse."""
    inputs, missing = [], []
    search = [build_dir] + msvc_library_paths(argv, environ)
    for token in argv[1:]:
        if msvc_option(token):
            continue
        path = Path(token)
        if path.suffix.lower() not in INPUT_SUFFIXES:
            continue
        candidates = [path] if path.is_absolute() else [base / path for base in search]
        found = next((candidate for candidate in candidates if candidate.is_file()), None)
        if found is None:
            missing.append(token)
        else:
            inputs.append(found.resolve())
    return inputs, missing


# /VERBOSE:LIB meldet jede durchsuchte Bibliothek als eigene Zeile "<Text> <Pfad>.lib:". Der Text ist
# lokalisiert (englisch "Searching", deutsch u.a. "Durchsuchen von"; VSLANG wirkt nur mit installiertem
# Sprachpaket), der Pfad nicht. Erkannt wird deshalb nur der Pfad am Zeilenende: absolut ab dem
# Laufwerksbuchstaben (Leerzeichen in "Program Files" bleiben erhalten), sonst das letzte Wort.
LIBRARY_LINE = re.compile(r"^\s*\S.*?\.lib:\s*$", re.IGNORECASE | re.MULTILINE)
DRIVE_PATH = re.compile(r"(?:[A-Za-z]:[\\/]|\\\\).*\.lib(?=:\s*$)", re.IGNORECASE)


def searched_libraries(output: str):
    """Die Bibliotheken, die link.exe /VERBOSE:LIB als durchsucht meldet, unabhaengig von der Meldungssprache."""
    found = []
    for match in LIBRARY_LINE.finditer(output):
        line = match.group(0).strip()
        absolute = DRIVE_PATH.search(line)
        path = absolute.group(0) if absolute else line[:-1].split()[-1]
        if Path(path) not in found:
            found.append(Path(path))
    return found


def msvc_redirect(argv, directory: Path, name: str):
    """Ausgaben des Vergleichslinks ins Pruefverzeichnis: /out: (gleicher Dateiname), /implib:, /pdb:."""
    rebuilt, redirected = [], False
    for token in argv:
        option = msvc_option(token)
        if option and option[0] in ("out", "implib", "pdb") and option[1]:
            path = directory / (name if option[0] == "out" else Path(option[1].replace("\\", "/")).name)
            rebuilt.append(f"/{option[0]}:{path}")
            redirected |= option[0] == "out"
        else:
            rebuilt.append(token)
    if not redirected:
        raise RuntimeError("link command has no /out: output to redirect")
    return rebuilt


def msvc_comparison_link(argv, build_dir: Path, exe: Path, env=None):
    """Denselben link.exe-Befehl erneut ausfuehren; dazu /VERBOSE:LIB, das nur Meldungen erzeugt."""
    environ = dict(env if env is not None else os.environ, VSLANG="1033")
    with tempfile.TemporaryDirectory() as directory:
        rebuilt = msvc_redirect(argv, Path(directory), exe.name) + ["/VERBOSE:LIB"]
        result = subprocess.run(rebuilt, cwd=build_dir, capture_output=True, text=True,
                                errors="replace", env=environ)
        if result.returncode:
            raise RuntimeError("comparison link failed: "
                               + ((result.stdout + result.stderr).strip().splitlines() or [""])[-1][:200])
        target = Path(directory) / exe.name
        if not target.is_file():
            raise RuntimeError("comparison link produced no binary")
        output = result.stdout + result.stderr
        return target.read_bytes(), searched_libraries(output), output


def audit_msvc(exe: Path, build_dir: Path, argv, nm: str, forbidden, env=None):
    environ = env if env is not None else os.environ
    problems = []
    inputs, missing = msvc_explicit_inputs(argv, build_dir, environ)
    for name in missing:
        problems.append(f"link input {name} named by the build is not found in the build or LIB directories")
    if not inputs:
        problems.append("link command names no auditable input")
    try:
        rebuilt, searched, output = msvc_comparison_link(argv, build_dir, exe, env)
    except RuntimeError as error:
        return problems + [str(error)]
    if not searched:
        head = " | ".join(line.strip() for line in output.splitlines() if line.strip())[:400]
        problems.append("link /VERBOSE:LIB reported no searched library; implicit inputs are unverifiable"
                        f" (output: {head or 'empty'})")
    for library in searched:
        library = library if library.is_absolute() else build_dir / library  # link.exe laeuft im Buildverzeichnis
        if not library.is_file():
            problems.append(f"library {library} searched by link.exe is missing")
        elif library.resolve() not in inputs:
            inputs.append(library.resolve())
    problems += archives_readable(inputs, nm)
    problems += audit_inputs(inputs, nm, forbidden)
    problems += compare_images(exe.read_bytes(), rebuilt)
    return problems


def audit(exe: Path, build_dir: Path, target: str, cache_dir: Path, nm: str, forbidden, env=None):
    """Alle drei Stufen. Rueckgabe: Liste der Befunde (leer = bestanden)."""
    problems = []
    try:
        msvc = msvc_link_from_commands(ninja_commands(build_dir, target), target, build_dir)
    except RuntimeError as error:
        return [str(error)]
    if msvc is not None:
        return audit_msvc(exe, build_dir, msvc, nm, forbidden, env)
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
