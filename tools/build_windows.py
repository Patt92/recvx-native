#!/usr/bin/env python3
"""Cross-build Windows x64 or ARM64 on macOS/Linux with pinned Zig and SDL2."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shlex
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
ZIG_VERSION = "0.16.0"
SDL_VERSION = "2.32.10"
ZIG_HASHES = {
    "aarch64-macos": "b23d70deaa879b5c2d486ed3316f7eaa53e84acf6fc9cc747de152450d401489",
    "x86_64-macos": "0387557ed1877bc6a2e1802c8391953baddba76081876301c522f52977b52ba7",
    "aarch64-linux": "ea4b09bfb22ec6f6c6ceac57ab63efb6b46e17ab08d21f69f3a48b38e1534f17",
    "x86_64-linux": "70e49664a74374b48b51e6f3fdfbf437f6395d42509050588bd49abe52ba3d00",
}
SDL_HASH = "83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2"
# SDL2 ships prebuilt MinGW libraries for x86_64 only. The ARM64 build compiles the
# pinned SDL2 source with the same Zig toolchain instead.
ARCHES = {
    "x64": {"zig": "x86_64-windows-gnu", "processor": "AMD64", "rc": "X64",
            "sdl": "x86_64-w64-mingw32", "build": "windows-x64", "suffix": ""},
    "arm64": {"zig": "aarch64-windows-gnu", "processor": "ARM64", "rc": "ARM64",
              "sdl": None, "build": "windows-arm64", "suffix": "-arm64"},
}


def download(url, path, digest):
    if not path.exists():
        temporary = path.with_suffix(path.suffix + ".partial")
        print(f"Downloading {url}", flush=True)
        with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as output:
            while chunk := response.read(1024 * 1024):
                output.write(chunk)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"checksum mismatch: {temporary}")
        temporary.replace(path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"checksum mismatch: {path}")


def unpack(archive, destination, expected):
    if expected.exists():
        return
    with tarfile.open(archive) as source:
        # Python's data filter rejects path escapes and unsafe archive entries.
        source.extractall(destination, filter="data")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--arch", choices=sorted(ARCHES), default="x64",
                        help="Windows target architecture (default: x64)")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--opengl", choices=["on", "off"], default="on")
    parser.add_argument("--vulkan", choices=["on", "off"], default="on")
    args = parser.parse_args()
    import sys
    subprocess.run([sys.executable, str(ROOT / 'tools/check_no_original_core.py'), str(ROOT)], check=True)
    arch = {"arm64": "aarch64", "AMD64": "x86_64"}.get(platform.machine(), platform.machine())
    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    host = f"{arch}-{system}"
    if host not in ZIG_HASHES:
        raise RuntimeError("unsupported cross-build host; use native CMake on Windows")
    toolchains, dependencies = ROOT / "build/toolchains", ROOT / "build/dependencies"
    toolchains.mkdir(parents=True, exist_ok=True)
    dependencies.mkdir(parents=True, exist_ok=True)
    zig_name = f"zig-{host}-{ZIG_VERSION}"
    zig_archive = toolchains / f"{zig_name}.tar.xz"
    download(f"https://ziglang.org/download/{ZIG_VERSION}/{zig_name}.tar.xz", zig_archive, ZIG_HASHES[host])
    unpack(zig_archive, toolchains, toolchains / zig_name / "zig")
    arch = ARCHES[args.arch]
    sdl = None
    if arch["sdl"]:
        sdl_archive = dependencies / f"SDL2-devel-{SDL_VERSION}-mingw.tar.gz"
        download(f"https://github.com/libsdl-org/SDL/releases/download/release-{SDL_VERSION}/{sdl_archive.name}", sdl_archive, SDL_HASH)
        sdl = dependencies / f"SDL2-{SDL_VERSION}/{arch['sdl']}"
        unpack(sdl_archive, dependencies, sdl / "bin/SDL2.dll")
    zig = toolchains / zig_name / "zig"
    compilers = {}
    for name, command, targeted in (("cc", "cc", True), ("cxx", "c++", True), ("ar", "ar", False), ("ranlib", "ranlib", False)):
        wrapper = toolchains / f"zig{arch['suffix']}-{name}"
        target_option = f' -target {arch["zig"]}' if targeted else ""
        wrapper.write_text(f'#!/bin/sh\nexec {shlex.quote(str(zig))} {command}{target_option} "$@"\n')
        wrapper.chmod(0o755)
        compilers[name] = wrapper.as_posix()
    resource_compiler = toolchains / f"zig{arch['suffix']}-rc"
    resource_compiler.write_text(f'#!/bin/sh\nexec {shlex.quote(str(zig))} rc /:output-format coff /:target {arch["rc"]} "$@"\n')
    resource_compiler.chmod(0o755)
    toolchain = toolchains / f"windows{arch['suffix']}-zig.cmake"
    # Dieser Werkzeugsatz kann keine auditierbare Symboltabelle in die PE legen:
    # llvm-nm/llvm-objdump finden in der von zig/LLD erzeugten Datei weder eine
    # COFF-Symboltabelle noch .debug-Abschnitte, und -g, -fno-strip,
    # -Wl,--no-strip, -Wl,-Map sowie -Wl,--no-insert-timestamp werden
    # abgewiesen. Der Linkeingangs-/Binärnachweis ist deshalb offen; siehe
    # archived/work/native_port_windows_audit.md. Keine Lockerung des Prüfers.
    toolchain.write_text(f'set(CMAKE_SYSTEM_NAME Windows)\nset(CMAKE_SYSTEM_PROCESSOR {arch["processor"]})\n' + "".join(
        f'set({key} "{compilers[name]}" CACHE FILEPATH "" FORCE)\n' for key, name in
        [("CMAKE_C_COMPILER", "cc"), ("CMAKE_CXX_COMPILER", "cxx"), ("CMAKE_AR", "ar"), ("CMAKE_RANLIB", "ranlib")]) +
        f'set(CMAKE_RC_COMPILER "{resource_compiler.as_posix()}" CACHE FILEPATH "" FORCE)\n' +
        'set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /fo <OBJECT> -- <SOURCE>")\n')
    if args.prepare_only:
        print(toolchain)
        return
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(toolchains / "zig-cache"))
    build = ROOT / "build" / arch["build"]
    # CMake can retain the old windres compiler in its language metadata even
    # after the toolchain cache changes. Redetect RC, without clearing the build.
    for metadata in (build / "CMakeFiles").glob("*/CMakeRCCompiler.cmake"):
        if f'set(CMAKE_RC_COMPILER "{resource_compiler.as_posix()}")' not in metadata.read_text():
            metadata.unlink()
    configure = ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DRECVX_ENABLE_OPENGL={args.opengl.upper()}", f"-DRECVX_ENABLE_VULKAN={args.vulkan.upper()}"]
    if sdl is not None:
        configure += [f"-DCMAKE_PREFIX_PATH={sdl}", "-DRECVX_FETCH_SDL2=OFF"]
    else:
        configure += ["-DRECVX_FETCH_SDL2=ON"]
        # Prefer the already unpacked pinned source; a fresh host downloads it via FetchContent.
        source = dependencies / f"SDL2-{SDL_VERSION}"
        if (source / "CMakeLists.txt").is_file():
            configure += [f"-DFETCHCONTENT_SOURCE_DIR_SDL2={source}"]
    subprocess.run(configure, check=True, env=env)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True, env=env)
    print(f"Windows {args.arch} executable: {build / ('recvx-runtime.exe')}")


if __name__ == "__main__":
    main()
