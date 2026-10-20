#!/usr/bin/env python3
"""Package the native development runtime with the complete cooked asset tree."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

PACKAGE_README = """RECVX Native - Entwicklungsbuild

Native Portierung von Resident Evil CODE: Veronica X (PS2). Das Paket enthaelt
die lokal vorbereiteten Spieldaten; das Spiel liest keine ISO- oder AFS-Dateien.
Noch kein vollstaendig spielbares Spiel.

Start: macOS "RECVX Native.app", Windows Start.cmd.
Bedienung und Tasten: STARTEN.md, CONTROLS.md.
Benutzerdaten: Windows %APPDATA%\\RECVX, macOS ~/Library/Application Support/RECVX.
Pfade fuer einzelne INIs, Snapshots und Spielstaende koennen mit
--controls, --debug-config, --video-config, --third-person-config,
--snapshots und --saves ueberschrieben werden; --user-dir setzt das gemeinsame
Benutzerdatenverzeichnis.
Portstand der Spielsysteme: GAMEPLAY-STATUS.md.
Austauschbare Ressourcen: ASSETS-AND-MODS.md.
Debugmenue (F2): DEBUG_MENU.md.
"""


def run(*args):
    subprocess.run(args, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["windows", "macos"], required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True, help="Complete cooked data from prepare_native_data.py")
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    if output.exists():
        raise RuntimeError("package destination already exists")
    source = build / ("recvx-runtime.exe" if args.platform == "windows" else "recvx-runtime")
    if not source.is_file():
        raise RuntimeError(f"missing runtime: {source}")
    run(sys.executable, str(ROOT / 'tools/check_no_original_core.py'), str(ROOT),
        '--binary', str(source))
    data_root = args.data.resolve()
    if not (data_root / "native-data.version").is_file() or not (data_root / "asset-manifest.json").is_file():
        raise RuntimeError("complete cooked data and its manifest are required")
    if any(f.suffix.lower() in {".iso", ".afs"} for f in data_root.rglob("*")):
        raise RuntimeError("cooked data still contains ISO/AFS containers")
    sdl_license = next((p for p in [build / '_deps/sdl2-src/LICENSE.txt',
        build.parent / '_deps/sdl2-src/LICENSE.txt', ROOT / 'build/dependencies/SDL2-2.32.10/LICENSE.txt'] if p.is_file()), None)
    if sdl_license is None:
        raise RuntimeError('SDL2 license missing; use the pinned SDL2 build dependency')
    output.mkdir(parents=True)
    if args.platform == "windows":
        shutil.copy2(source, output / source.name)
        shutil.copy2(build / "SDL2.dll", output / "SDL2.dll")
        shutil.copy2(build / "recvx-native.exe", output / "recvx-native.exe")
        zig = next((ROOT / "build/toolchains").glob("zig-*-0.16.0"), None)
        if zig:
            shutil.copy2(zig / "lib/libcxx/LICENSE.TXT", output / "LLVM-LICENSE.txt")
            shutil.copy2(zig / "lib/libc/mingw/COPYING", output / "MinGW-LICENSE.txt")
        (output / "Start.cmd").write_text('@echo off\r\n"%~dp0recvx-runtime.exe" --data "%~dp0data" --mods "%~dp0mods" %*\r\nset "recvx_exit=%errorlevel%"\r\nif not "%recvx_exit%" == "0" pause\r\nexit /b %recvx_exit%\r\n')
        data_destination = output / "data"
    else:
        if subprocess.check_output(["lipo", "-archs", str(source)], text=True).split() != ["arm64"]:
            raise RuntimeError("macOS packaging requires an arm64-only build; run ./build.sh --platform macos")
        contents = output / "RECVX Native.app/Contents"
        binaries, frameworks = contents / "MacOS", contents / "Frameworks"
        binaries.mkdir(parents=True)
        frameworks.mkdir()
        shutil.copy2(source, binaries / "recvx-runtime")
        dependencies = subprocess.check_output(["otool", "-L", str(source)], text=True)
        linked = next(line.strip().split(" (", 1)[0] for line in dependencies.splitlines()[1:] if "libSDL2" in line)
        library_name = Path(linked).name
        library = build / "_deps/sdl2-build" / library_name
        if not library.exists():
            raise RuntimeError("macOS packaging requires RECVX_FETCH_SDL2=ON")
        if subprocess.check_output(["lipo", "-archs", str(library.resolve())], text=True).split() != ["arm64"]:
            raise RuntimeError("macOS SDL2 must also be arm64-only")
        shutil.copy2(library.resolve(), frameworks / library_name)
        run("install_name_tool", "-change", linked, f"@executable_path/../Frameworks/{library_name}", str(binaries / "recvx-runtime"))
        run("install_name_tool", "-id", f"@rpath/{library_name}", str(frameworks / library_name))
        launcher = binaries / "Start"
        launcher.write_text('#!/bin/sh\nrecvx_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$recvx_dir/recvx-runtime" --data "$recvx_dir/../Resources/data" --mods "$recvx_dir/../../../mods" "$@"\n')
        launcher.chmod(0o755)
        resources = contents / "Resources"
        resources.mkdir()
        shutil.copy2(ROOT / "native/resources/app-icon.icns", resources / "app-icon.icns")
        with (contents / "Info.plist").open("wb") as info:
            plistlib.dump({"CFBundleExecutable": "Start", "CFBundleIdentifier": "org.recvx.native", "CFBundleName": "RECVX Native",
                          "CFBundlePackageType": "APPL", "CFBundleIconFile": "app-icon.icns", "CFBundleVersion": "0.5.0", "CFBundleShortVersionString": "0.5.0", "LSMinimumSystemVersion": "11.0", "NSHighResolutionCapable": True}, info)
        data_destination = contents / "Resources/data"
        run("codesign", "--force", "--sign", "-", str(frameworks / library_name))
        run("codesign", "--force", "--sign", "-", str(binaries / "recvx-runtime"))
    shutil.copytree(data_root, data_destination)
    mods_destination = output / "mods"
    mods_destination.mkdir()
    shutil.copy2(ROOT / "docs/NATIVE_ASSETS.md", output / "ASSETS-AND-MODS.md")
    shutil.copy2(ROOT / "docs/NATIVE_GAMEPLAY.md", output / "GAMEPLAY-STATUS.md")
    shutil.copy2(ROOT / "LICENSE", output / "LICENSE.txt")
    shutil.copy2(ROOT / "native/third_party/stb/LICENSE", output / "STB-LICENSE.txt")
    shutil.copy2(ROOT / "docs/AUDIO.md", output / "AUDIO.md")
    shutil.copy2(ROOT / "docs/VALIDATION.md", output / "VALIDATION.md")
    shutil.copy2(ROOT / "docs/NATIVE_PLAYER.md", output / "NATIVE_PLAYER.md")
    shutil.copy2(ROOT / "docs/CONTROLS.md", output / "CONTROLS.md")
    shutil.copy2(ROOT / "STARTEN.md", output / "STARTEN.md")
    shutil.copy2(ROOT / "debug.ini", output / "debug.ini")
    shutil.copy2(ROOT / "docs/DEBUG_MENU.md", output / "DEBUG_MENU.md")
    shutil.copy2(ROOT / "docs/INVENTORY-COVERAGE.md", output / "INVENTORY-COVERAGE.md")
    shutil.copy2(ROOT / "controls.ini", output / "controls.ini")
    shutil.copy2(sdl_license, output / "SDL2-LICENSE.txt")
    (output / "README.txt").write_text(PACKAGE_README)
    if args.platform == "macos":
        run("codesign", "--force", "--sign", "-", str(output / "RECVX Native.app"))
    print(output)


if __name__ == "__main__":
    main()
