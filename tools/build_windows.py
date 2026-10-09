#!/usr/bin/env python3
"""Build Windows x64 on macOS/Linux using pinned official Zig and SDL2 SDKs."""
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
    parser.add_argument("--jobs", type=int, default=4)
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
    sdl_archive = dependencies / f"SDL2-devel-{SDL_VERSION}-mingw.tar.gz"
    download(f"https://github.com/libsdl-org/SDL/releases/download/release-{SDL_VERSION}/{sdl_archive.name}", sdl_archive, SDL_HASH)
    sdl = dependencies / f"SDL2-{SDL_VERSION}/x86_64-w64-mingw32"
    unpack(sdl_archive, dependencies, sdl / "bin/SDL2.dll")
    zig = toolchains / zig_name / "zig"
    compilers = {}
    for name, command in {"cc": "cc -target x86_64-windows-gnu", "cxx": "c++ -target x86_64-windows-gnu", "ar": "ar", "ranlib": "ranlib"}.items():
        wrapper = toolchains / f"zig-{name}"
        wrapper.write_text(f'#!/bin/sh\nexec {shlex.quote(str(zig))} {command} "$@"\n')
        wrapper.chmod(0o755)
        compilers[name] = wrapper.as_posix()
    toolchain = toolchains / "windows-zig.cmake"
    toolchain.write_text('set(CMAKE_SYSTEM_NAME Windows)\nset(CMAKE_SYSTEM_PROCESSOR AMD64)\n' + "".join(
        f'set({key} "{compilers[name]}" CACHE FILEPATH "" FORCE)\n' for key, name in
        [("CMAKE_C_COMPILER", "cc"), ("CMAKE_CXX_COMPILER", "cxx"), ("CMAKE_AR", "ar"), ("CMAKE_RANLIB", "ranlib")]))
    if args.prepare_only:
        print(toolchain)
        return
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(toolchains / "zig-cache"))
    build = ROOT / "build/windows-x64"
    subprocess.run(["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                    f"-DCMAKE_PREFIX_PATH={sdl}", "-DCMAKE_BUILD_TYPE=Release", "-DRECVX_FETCH_SDL2=OFF"], check=True, env=env)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True, env=env)
    print(f"Windows executable: {build / 'recvx-runtime.exe'}")


if __name__ == "__main__":
    main()
