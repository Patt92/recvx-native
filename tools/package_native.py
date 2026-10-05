#!/usr/bin/env python3
"""Package the native development runtime with the complete cooked asset tree."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


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
        (output / "Start.cmd").write_text('@echo off\r\n"%~dp0recvx-runtime.exe" --data "%~dp0data" --mods "%~dp0mods" --controls "%~dp0controls.ini" %*\r\nif errorlevel 1 pause\r\n')
        data_destination = output / "data"
    else:
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
        shutil.copy2(library.resolve(), frameworks / library_name)
        run("install_name_tool", "-change", linked, f"@executable_path/../Frameworks/{library_name}", str(binaries / "recvx-runtime"))
        run("install_name_tool", "-id", f"@rpath/{library_name}", str(frameworks / library_name))
        launcher = binaries / "Start"
        launcher.write_text('#!/bin/sh\nrecvx_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$recvx_dir/recvx-runtime" --data "$recvx_dir/../Resources/data" --mods "$recvx_dir/../../../mods" --controls "$recvx_dir/../../../controls.ini" "$@"\n')
        launcher.chmod(0o755)
        with (contents / "Info.plist").open("wb") as info:
            plistlib.dump({"CFBundleExecutable": "Start", "CFBundleIdentifier": "org.recvx.native", "CFBundleName": "RECVX Native",
                          "CFBundlePackageType": "APPL", "CFBundleVersion": "0.5.0", "CFBundleShortVersionString": "0.5.0", "LSMinimumSystemVersion": "11.0", "NSHighResolutionCapable": True}, info)
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
    shutil.copy2(ROOT / "docs/CONTROLS.md", output / "CONTROLS.md")
    shutil.copy2(ROOT / "controls.ini", output / "controls.ini")
    shutil.copy2(sdl_license, output / "SDL2-LICENSE.txt")
    (output / "README.txt").write_text("RECVX native development build 0.5.0\n\nIncludes the complete locally cooked disc data: no ISO or AFS files are read by this build.\nOriginal room/player graphics, inventory/status sprites, item models and dialogs run natively.\nThe inventory has item management, equipment, combination routines and health.\nThe event interpreter executes original conditions, tasks and messages and reports pending world bindings.\nOriginal FMVs with stereo PCM, ADX music/voices, first-room actor motions/cameras and box/cylinder/diagonal wall collision are integrated.\nFull effects, face animation, footsteps/sound banks, combat, room transitions, file/map screens and saves remain incomplete. This is not yet a complete playable game.\n\nWindows: open Start.cmd. macOS: open RECVX Native.app.\nTab: inventory; arrows: select; Enter: confirm/action; Escape: cancel/quit.\nW/S or I/K: forward/back; A/D or J/L: turn; Shift: run; Space: cinematic skip; F2: wireframe.\nGamepad: left stick/D-pad; A: action; B: run/cancel; Y: inventory; Start: skip.\nEdit controls.ini and restart to change keyboard/gamepad bindings and deadzone. See CONTROLS.md.\nExamine: left/right rotate an original item model; Escape returns.\nUse --preview for the existing room/free-camera inspector.\n\nSee ASSETS-AND-MODS.md for replaceable TGA textures and model packets.\nSee GAMEPLAY-STATUS.md for the exact implemented and pending event/gameplay features.\nAI-assisted native port. Windows x64 and universal Intel/Apple Silicon macOS.\n")
    if args.platform == "macos":
        run("codesign", "--force", "--sign", "-", str(output / "RECVX Native.app"))
    print(output)


if __name__ == "__main__":
    main()
