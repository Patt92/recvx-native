#!/usr/bin/env python3
"""Fail-closed native-port policy gate. Never executes generators or game code.

Original sources may be read as semantic references, never compiled. Quarantine
content must not be consumed at all. Data/layout-only generation is distinct
from copied function bodies. There is no waiver, source whitelist or bypass.
"""
import argparse
import sys
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

QUARANTINE = "FORBIDDEN_DONTUSE"
RETIRED_TARGET = re.compile(r"(?<![A-Za-z0-9_])(?:lib)?recvx_(?:original\w*|core_(?:enemy\w*|actor_assets|world_globals|globals|gs|vu\w*|njplus|ninja|model|effect|object|types|view))\b")
DISPATCH = re.compile(r"\b(?:bhControlEnemy|bhDrawEnemy|bhJumpEnemy|bhInitEnemy|bhSetEnemy|core_enemy_world_\w+|recvx_original_\w+|recvx_vu0_\w+|recvx_core_gs_\w+)\b")
# Reviewed generators that copy original procedures, rather than just data.
BODY_GENERATORS = frozenset({
    "generate_enemy_sources.py", "generate_enemy_ai.py", "build_core32.py",
    "generate_status_sources.py", "generate_core_actor_event.py", "generate_face_morph.py",
    "generate_core_view.py", "generate_core_njplus.py", "generate_core_njplus_skin.py",
    "generate_weapon_flash.py", "generate_door_core.py", "generate_player_hair.py",
    "generate_effect_particles.py", "generate_en01_damage.py", "generate_subplayer.py",
    "generate_player_stairs.py", "generate_view_clip.py", "generate_targeting.py",
    "generate_file_folder.py", "generate_inventory_engine.py", "generate_effect_sprites.py",
    "generate_pickup_engine.py", "generate_item_use.py", "generate_object_itmbox.py",
    "generate_event_inventory.py", "generate_computer.py", "generate_box_engine.py",
    "generate_enemy_spawn.py", "generate_gameover.py", "generate_effect_collision.py",
    "generate_player_push.py", "generate_player_combat.py",
    # Mischgenerator: schrieb neben ADX-Datenwerten auch combinations.inc,
    # status_pulse.inc und ui_tables.inc mit kopierten Originalfunktionen. Ein
    # Teil reiner Daten macht einen Generator nicht zulaessig; die ADX-Werte
    # liegen jetzt als natives Datenmodul native/adx_tables.c/.h im Baum.
    "generate_native_tables.py",
})
BODY_INCLUDES = frozenset({
    "door_procedures.inc", "player_hair.inc", "effect_particles.inc", "en01_damage.inc",
    "en04_damage.inc", "subplayer.inc", "player_stairs.inc", "view_clip.inc", "targeting.inc",
    "file_folder.inc", "inventory_engine.inc", "effect_sprites.inc", "pickup_engine.inc",
    "item_use.inc", "item_use00.inc", "object_itmbox.inc", "event_inventory.inc",
    "computer.inc", "box_engine.inc", "enemy_spawn.inc", "gameover.inc",
    "effect_collision.inc", "player_push.inc", "player_combat.inc", "enemy_ai.inc",
    "weapon_flash.inc", "core_njplus_collision.inc", "core_njplus_skin.inc",
    "core_actor_event.inc", "core_face_morph.inc", "core_view.inc",
    # Ausgaben des Mischgenerators generate_native_tables.py: status_pulse.inc
    # enthielt PulseInit/PulseMain/PulseAnim, combinations.inc Combi_00..05 und
    # ui_tables.inc neben den Datentabellen BulletSet/TrigerSet. Die reinen
    # UI-Daten liegen jetzt als native/ui_tables_data.inc im Baum.
    "status_pulse.inc", "combinations.inc", "ui_tables.inc",
})
SOURCE_SUFFIXES = {".c", ".cpp", ".cc", ".h", ".hpp", ".inc"}


def strip_comments(text, cmake=False):
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*'
    if cmake:
        pattern = r'"(?:\\.|[^"\\])*"|#\[(=*)\[.*?\]\1\]|#[^\n]*'
    def replace(match):
        value = match.group(0)
        return value if value.startswith(('"', "'")) else re.sub(r"[^\n]", " ", value)
    return re.sub(pattern, replace, text, flags=re.S)


def cmake_statements(text):
    """Balanced commands, including quoted paths/parentheses and multiline calls."""
    text = strip_comments(text, cmake=True)
    cursor = 0
    while match := re.search(r"([A-Za-z_]\w*)\s*\(", text[cursor:]):
        start = cursor + match.end()
        position, depth, quote = start, 1, False
        while position < len(text) and depth:
            char = text[position]
            if char == "\\":
                position += 2
                continue
            if char == '"':
                quote = not quote
            elif not quote:
                depth += (char == "(") - (char == ")")
            position += 1
        if depth:
            raise ValueError("unbalanced CMake command")
        yield match.group(1).lower(), text[start:position-1]
        cursor = position


def forbidden_path(value):
    value = str(value).replace("\\", "/")
    return QUARANTINE in value or bool(re.search(r"(?:^|[/\s\";])src/ps2/veronica/prog/[^\s\";]+\.(?:c|cpp)\b", value))


def check_cmake(root):
    problems = []
    paths = []
    for directory, children, files in os.walk(root):
        # Never descend into archives/caches, including quarantine. Dependencies
        # do not become semantic references for native engine reimplementation.
        children[:] = [name for name in children if name not in
                       {"archived", "build", "data", "mods", ".git", "third_party", "_deps", QUARANTINE}
                       and not (Path(directory) / name).is_symlink()]
        paths.extend(Path(directory) / name for name in files
                     if name == "CMakeLists.txt" or name.endswith(".cmake"))
    if root / "CMakeLists.txt" not in paths:
        return ["CMakeLists.txt missing"]
    for path in paths:
        if forbidden_path(path.resolve()):
            problems.append(f"{path}: forbidden CMake symlink/input")
            continue
        try:
            for command, arguments in cmake_statements(path.read_text()):
                label = str(path.relative_to(root))
                if QUARANTINE in arguments:
                    problems.append(f"{label}: {command} consumes quarantine content")
                if RETIRED_TARGET.search(arguments):
                    problems.append(f"{label}: {command} references retired engine/component target")
                if command in {"add_library", "add_executable", "target_sources", "file", "aux_source_directory"} and forbidden_path(arguments):
                    sources = re.findall(r"src/ps2/veronica/prog/[^\s\";]+\.(?:c|cpp)\b", arguments)
                    problems.append(f"{label}: {command} compiles/collects original source (reference only): {', '.join(sources)}")
                if command in {"add_custom_command", "add_custom_target", "execute_process"}:
                    # DEPENDS on a reference is allowed; executing a procedure
                    # extractor is not. Check only the executable COMMAND part.
                    executable = " ".join(re.findall(
                        r"\bCOMMAND\s+(.*?)(?=\b(?:COMMAND|DEPENDS|WORKING_DIRECTORY|COMMENT|VERBATIM|OUTPUT|BYPRODUCTS)\b|$)",
                        arguments, re.S))
                    for generator in BODY_GENERATORS:
                        if generator in executable:
                            problems.append(f"{label}: {command} runs original-function generator {generator}")
                    # Reine Daten-/Layoutgeneratoren sind zulaessig (Nutzer-
                    # klarstellung 2026-10-09). Gesperrt sind Funktionskoerper-
                    # und Mischgeneratoren (BODY_GENERATORS oben), verbotene
                    # Ausgaben (check_build) und der folgende Fall.
                    # Mischgenerator-Fall: ein Erzeuger schreibt neben Daten
                    # auch gesperrte Funktionsdateien, und eine nachgeschaltete
                    # Loeschung soll das heilen. Erzeugen und wieder entfernen
                    # ist kein Schutz — die Funktionskoerper entstehen trotzdem.
                    # Daten gehoeren als natives Datenmodul in den Baum.
                    removed = {Path(name).name for name in re.findall(
                        r"[^\s\";]+\.inc\b",
                        " ".join(re.findall(r"\bCOMMAND\b[^\n]*?\b(?:rm|remove)\b(.*?)(?=\bCOMMAND\b|$)",
                                            arguments, re.S)))}
                    for name in sorted(removed & BODY_INCLUDES):
                        problems.append(
                            f"{label}: {command} generates then deletes copied function output {name}"
                            " (generate-then-delete is not a safeguard)")
        except (OSError, ValueError) as error:
            problems.append(f"{path}: cannot audit CMake: {error}")
    return problems


def original_header_shadow(root, native, include):
    """Flag an include that silently resolves to an original header.

    A quoted include first searches the including file's own directory, so a
    name that also exists under native/ resolves natively. A name that does NOT
    exist under native/ falls through to the -I search path, and the native
    build carries -I include/ps2/veronica/prog. Such a name pulls original
    declarations into a native translation unit, which the policy forbids even
    though the include text itself names no forbidden path.
    """
    name = Path(include).name
    if name != include:
        return False  # keep this check to bare header names
    if (native / include).exists():
        return False
    for other in ("build-tools/ninja", "build-tools", "include"):
        if (root / other / include).exists():
            return False
    return (root / "include/ps2/veronica/prog" / include).exists()


def check_native(root):
    problems = []
    native = root / "native"
    if not native.is_dir():
        return ["native directory missing"]
    for path in native.rglob("*"):
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        label = path.relative_to(root)
        if path.is_symlink() and forbidden_path(path.resolve()):
            problems.append(f"{label}: symlink to forbidden content")
            continue  # do not open a quarantine symlink
        code = strip_comments(path.read_text(errors="replace"))
        for match in re.finditer(r'^\s*#\s*include\s*["<]([^">]+)', code, re.M):
            include = match.group(1)
            if QUARANTINE in include or Path(include).name in BODY_INCLUDES or forbidden_path(include):
                problems.append(f"{label}: copied/forbidden function include {include}")
            elif original_header_shadow(root, native, include):
                problems.append(
                    f"{label}: include {include} has no native header and resolves to"
                    " the original include/ps2/veronica/prog header")
        symbols = sorted(set(DISPATCH.findall(code)))
        if symbols:
            problems.append(f"{label}: forbidden engine/hardware symbols: {', '.join(symbols)}")
        if re.search(r"\b(?:SYS_WORK|SYS_STATE_MOCK|SYS_STATE|COMBAT_SYS|PUSH_SYS|BH_PWORK)\s*\*?\s*(?:sys|plp|player|player_work|ply|ene|enemies)\b", code):
            problems.append(f"{label}: separate original system/player/enemy work owner")
    return problems


def check_build(directory):
    if QUARANTINE in str(directory.resolve()):
        return ["refusing to inspect a quarantined build directory"]
    problems = []
    database = directory / "compile_commands.json"
    if not database.is_file():
        problems.append(f"{directory}: compile_commands.json missing; transitive compile audit unavailable")
    else:
        if forbidden_path(database.resolve()):
            return ["compilation database points to forbidden content"]
        try:
            entries = json.loads(database.read_text())
            if not entries:
                problems.append("empty compilation database; no compiled-source evidence")
            for entry in entries:
                if forbidden_path(entry["file"]):
                    problems.append(f"compiled forbidden source: {entry['file']}")
                command = entry.get("command", " ".join(entry.get("arguments", [])))
                if QUARANTINE in command or "generated/enemy/" in command.replace("\\", "/") or "generated/status/" in command.replace("\\", "/"):
                    problems.append(f"compiled original module set: {entry['file']}")
        except (OSError, ValueError, KeyError) as error:
            problems.append(f"cannot audit compilation database: {error}")
    for path in (directory / "CMakeFiles").glob("*/link.txt"):
        if forbidden_path(path.resolve()):
            problems.append(f"{path}: forbidden link metadata symlink")
            continue
        code = path.read_text(errors="replace")
        if RETIRED_TARGET.search(code) or QUARANTINE in code:
            problems.append(f"{path}: forbidden transitive link input")
    ninja = directory / "build.ninja"
    if ninja.is_file():
        if forbidden_path(ninja.resolve()):
            return problems + ["build.ninja points to forbidden content"]
        for line in ninja.read_text(errors="replace").splitlines():
            if line.startswith("build ") and (RETIRED_TARGET.search(line) or QUARANTINE in line or forbidden_path(line)):
                problems.append("build.ninja: forbidden source or link input: " + line[:240])
    generated = directory / "generated"
    for path in generated.rglob("*"):
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            if forbidden_path(path.resolve()):
                problems.append(f"{path}: generated symlink to forbidden content")
                continue
            if path.name in BODY_INCLUDES or path.suffix in {".c", ".cpp"}:
                problems.append(f"{path}: generated original function/module output remains")
            elif re.search(r"\b[A-Za-z_]\w*\s*\([^;{}]*\)\s*\{", strip_comments(path.read_text(errors="replace"))):
                problems.append(f"{path}: generated function body (not data/layout only)")
    return problems


sys.path.insert(0, str(Path(__file__).resolve().parent))


def forbidden_symbols(names):
    """Die Verbotsliste auf eine Symbolmenge angewandt."""
    return sorted(name for name in names
                  if DISPATCH.fullmatch(name) or name in {"sys", "plp", "rom", "ene", "eff"})


def check_windows_binary(path, build_dir, nm, cache_dir=None):
    """PE ohne Symboltabelle: Linkeingaben pruefen und Vergleichslink stellen.

    Keine Ausnahme fuer das fehlende Symbolverzeichnis - der Nachweis wird
    vollstaendig ueber die tatsaechlichen Linkeingaben und die Bytegleichheit
    mit einem Vergleichslink gefuehrt (tools/windows_link_audit.py).
    """
    import windows_link_audit
    if build_dir is None:
        return [f"{path}: Windows binary without its build directory cannot be audited"]
    cache = cache_dir or Path(build_dir).resolve().parents[0] / "toolchains/zig-cache"
    try:
        return [f"{path}: {problem}" for problem in windows_link_audit.audit(
            Path(path).resolve(), Path(build_dir).resolve(), Path(path).name, Path(cache), nm,
            forbidden_symbols)]
    except Exception as error:  # fail closed
        return [f"{path}: Windows link audit failed: {error}"]


def check_binary(path, nm=None, build_dir=None):
    if not path.is_file() or forbidden_path(path.resolve()):
        return [f"{path}: missing or quarantined binary"]
    command = nm or shutil.which("llvm-nm") or shutil.which("nm")
    if not command:
        return ["nm/llvm-nm unavailable; refusing unverified executable"]
    # Include local symbols too: static linkage must not hide a retired engine.
    result = subprocess.run([command, str(path)], capture_output=True, text=True)
    if result.returncode or not result.stdout.strip():
        # Ein PE ohne Symbolverzeichnis ist kein Nachweis - und keine Ausnahme.
        # Der Nachweis laeuft dann ueber Linkeingaben und Vergleichslink.
        if path.suffix.lower() == ".exe":
            return check_windows_binary(path, build_dir, command)
        return [f"{path}: symbol audit failed/empty (stripped binary is not evidence of absence)"]
    # Strip the object-symbol ABI underscore, not the original symbol itself.
    names = {line.split()[-1].lstrip("_") for line in result.stdout.splitlines() if line.split()}
    forbidden = forbidden_symbols(names)
    return [f"{path}: forbidden linked symbols: {', '.join(forbidden)}"] if forbidden else []


def audit(root, build=None, binaries=(), nm=None):
    if QUARANTINE in str(root.resolve()):
        return ["refusing to inspect quarantined sources"]
    problems = check_cmake(root) + check_native(root)
    if build is not None:
        problems += check_build(build)
    for binary in binaries:
        problems += check_binary(binary, nm, build)
    return sorted(set(problems))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--binary", type=Path, action="append", default=[])
    parser.add_argument("--nm", help="Explicit platform nm/llvm-nm for COFF or cross-built objects")
    args = parser.parse_args()
    problems = audit(args.root.resolve(), args.build_dir, args.binary, args.nm)
    if problems:
        print("BLOCKED: original-core/native-port policy violations:")
        for problem in problems:
            print(" - " + problem)
        print("No exceptions. Complete docs/NATIVE_REIMPLEMENTATION_ROADMAP.md; do not restore quarantine content.")
        return 1
    print("PASS: audited source/build/binary scope has no forbidden core; not a gameplay acceptance.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
