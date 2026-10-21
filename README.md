# RECVX Native

**Resident Evil CODE: Veronica X — rebuilt for native platforms.**

RECVX Native brings the PS2 game to a standalone C/C++ engine, preserving its
original gameplay while replacing its console-specific foundations with native
rendering, audio, input and storage. The aim is to make CODE: Veronica X at home
on modern computers and, eventually, across the Apple device family.

**No emulator. No translated PS2 executable. No console required at runtime.**

The project is in active development. It is not yet a finished, fully playable
release. Platform plans below describe the intended destinations, not a promise
that every device already runs the game.

## A real native port

The engine runs the game itself: room scripts, enemy AI, player movement,
combat, animation, cameras, inventory, effects and sound. It does not wrap the
original executable or recreate a PS2 around it.

Original game behaviour is the reference; the original hardware is not the
architecture. Console register writes, DMA transfers and hardware-specific
drivers are replaced at their boundaries with native services. There is no
runtime CPU, VU, GS or sound-chip emulation, and the original engine is not
compiled or linked into the game.

This separation is the foundation for portability: a shared game core with
platform-specific graphics and host services, rather than a different gameplay
implementation for every operating system.

## Native rendering, multiple backends

The renderer translates the game's drawing rules into native rendering work,
with a shared pipeline feeding the available backend:

- **Metal** — the macOS GPU backend, designed for Apple Silicon and the basis
  for planned Apple-device builds.
- **Direct3D 11 — experimental** — the Windows GPU backend. It uses Rasterizer
  Ordered Views to preserve drawing order and programmable blending; hardware
  without the required support falls back to software. Hardware validation is
  still in progress.
- **Software** — a CPU renderer that remains available as a fallback and a
  reference for validating GPU output.
- **OpenGL — planned** — a GPU backend for the Linux port.

The goal is to retain the game's distinctive presentation while offering native
GPU acceleration, higher-resolution output and optional texture filtering and
supersampling. Rendering fidelity is checked against the reference path rather
than judged solely by how close a screenshot looks.

## Console foundations, rewritten for the host

A native port should use the machine it runs on. RECVX Native replaces the
console's platform services without replacing the game's identity.

**File-based saves, not virtual memory cards.** The typewriter and LOAD GAME
flows use ordinary save files. There is no memory-card image, card-port setup or
virtual peripheral to manage. Packaged builds keep saves and settings in the
operating system's user-data directory.

**A native audio engine.** The console audio-driver layer is reimplemented as
native decoders, sequencing and mixing code. ADX music and voice, Sony sound
banks and ADPCM samples feed a host stereo mixer. Sound requests, loops, fades
and positional audio are handled directly, not through emulated sound hardware
or hardware-register access.

**Native input and settings.** Keyboard and controller bindings, graphics
settings and application controls belong to the host layer. Optional door skip,
snapshots and a third-person camera mod are explicit additions; the original
camera and gameplay remain the default.

**Prepared assets, not a mounted disc.** An offline preparation step extracts
and converts the game data. The running game reads loose files, with no ISO or
AFS-container access and no external movie-conversion process running alongside
it.

## Built for mods

Assets are separate from the executable. An external `mods/` directory can
override textures, room data, supported original-format model and motion
packets, music, voices, sound banks and prepared movies without modifying the
application itself.

HD textures can be supplied per asset or through content-keyed packs, allowing
one replacement to cover shared textures across multiple rooms. Remove an
override to return to the base asset. On macOS, external mods do not require
modifying or re-signing the app bundle.

See [assets and mod support](docs/NATIVE_ASSETS.md) for supported formats,
replacement paths and texture-pack tools.

## Platform direction

Cross-platform design is built into the engine. Platform delivery is being
brought up separately from the shared gameplay code.

| Platform | Direction |
|---|---|
| **Mac / Apple Silicon** | Current development platform with Metal and software rendering; standalone Mac builds are part of the release plan. |
| **Windows x64** | Build support and an experimental Direct3D 11 backend are present; validated Windows releases are planned. |
| **Windows ARM64** | Cross-build option in the build tool, with SDL2 compiled from the pinned source; hardware validation is still pending. |
| **Linux** | Planned port with an OpenGL backend. |
| **iOS/iPadOS** | Planned native builds using Metal. |
| **Apple TV** | Planned native build using Metal. |

These are native platform targets, not plans to ship an emulator with the game.

## Research-backed implementation

RECVX Native builds on the preserved decompilation and extends that foundation
with targeted disassembly of the original executable and audio modules.
Decompiler output is a starting point, not the final authority: instruction-level
analysis resolves ambiguous field layouts, operand widths, evaluation order,
random-number consumption and side effects before they become native behaviour.

That research is paired with regression tests, scripted replays, differential
checks and rendering comparisons. An enforced source/build/link/binary policy
checks that the original engine and generated original function bodies do not
enter the native runtime. Unresolved behaviour is recorded rather than replaced
with a guessed approximation.

Implementation details and development status live in the
[porting report](docs/PORT_STATUS.md), [validation guide](docs/VALIDATION.md) and
[verification policy](QUALITY.md), keeping this page focused on the project.

<!-- port-status:external -->

## Build and run

You need your own legally obtained game data. Disc images and game assets are
not distributed with the source code.

All platforms need **Python 3.12+**, **CMake 3.16+**, a C compiler and a
**C++17 toolchain**. Python runs the build and data-preparation scripts; CMake
configures the native build. No Python packages installed with `pip` are needed
for the normal build.

The first build downloads the pinned **SDL2 2.32.10** dependency and needs network
access. **FFmpeg and ffprobe** are build-time tools for preparing movies with
`--iso`; neither is needed to play or to build with an already prepared `data/`
tree. Repeat `--iso` to supply multiple disc images; completed data is reused.

### Windows

Install:

- **Python 3.12+**, with the `py` launcher or `python` available in PowerShell.
- **CMake 3.16+**, with its command-line tools added to `PATH`.
- **Visual Studio or Visual Studio Build Tools**, including the **Desktop
  development with C++** workload, MSVC C/C++ tools and a Windows SDK.
  Native Windows ARM64 builds also need the ARM64 C/C++ build tools.
- **FFmpeg and ffprobe** for `--iso`: either install them on `PATH`, or place
  the Windows executables directly in the repository's `build-tools/` folder:

```text
build-tools/
  ffmpeg.exe
  ffprobe.exe
```

Each tool is checked on `PATH` first. If it is missing there, the build uses
its copy in `build-tools/` automatically, without changing `PATH`. Use a Windows
FFmpeg build compatible with your host; for shared builds, put the required DLLs
from the same distribution beside the executables. Static builds need no
separate FFmpeg DLLs. These local executables and DLLs are ignored by Git.
This lookup also applies when running `tools/cook_movies.py` directly; both
preparation scripts retain `--ffmpeg` and `--ffprobe` for explicit paths.

Run from PowerShell:

```powershell
./build.ps1 --iso "C:\path\to\game.iso" --package
```

With prepared data, omit `--iso`:

```powershell
./build.ps1 --package
```

Windows ARM64 hosts select the native ARM64 target automatically. Check that
`python --version` (or `py -3 --version`) and `cmake --version` work in the same
PowerShell session. A `WinError 2` when starting CMake means it could not be
found; the source-policy check does not verify installed build dependencies.
Launch through `Start.cmd`.

### macOS

Install:

- **Apple Command Line Tools** (Clang and the macOS SDK).
- **Python 3.12+** and **CMake 3.16+**, available as `python3` and `cmake`.
- **FFmpeg**, including `ffprobe`, on `PATH` when using `--iso`.
- **Ninja** if cross-building Windows binaries; the build tool downloads the
  pinned Zig cross-toolchain automatically.

For example, with Homebrew already installed:

```bash
xcode-select --install
```

```bash
brew install python@3.12 cmake ffmpeg ninja
```

Build for Apple Silicon (arm64) and prepare a package:

```bash
./build.sh --iso "/path/to/game.iso" --package
```

With prepared data:

```bash
./build.sh --package
```

Launch through `Start.command`. macOS builds use Metal or software rendering.

### Linux

Install:

- **GCC or Clang** with C++17 support, **Make or Ninja**, **Python 3.12+** and
  **CMake 3.16+**.
- **pkg-config** and development packages for SDL2's host backends: **X11
  and/or Wayland** for windowing, and **ALSA and/or PulseAudio** for audio.
  The build downloads SDL2 itself.
- **FFmpeg**, including `ffprobe`, on `PATH` when using `--iso`.
- **Ninja** for Windows cross-builds; Zig is downloaded automatically.

Example dependencies on Debian/Ubuntu (use a distribution release providing
Python 3.12 or newer):

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 ffmpeg libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev
```

Build and prepare data:

```bash
./build.sh --iso "/path/to/game.iso"
```

With prepared data:

```bash
./build.sh
```

Linux currently uses the software renderer; the OpenGL backend is planned.
Linux packaging is not yet supported, so do not pass `--package`. Run the built
runtime with the prepared data:

```bash
./build/native/recvx-runtime --data data
```

See the [build guide](docs/NATIVE_PORT.md) for further build and packaging options,
and the [launch guide](STARTEN.md) and [controls reference](docs/CONTROLS.md) for
playing.

## Further reading

- [Engine and build architecture](docs/NATIVE_PORT.md)
- [Assets, HD textures and mods](docs/NATIVE_ASSETS.md)
- [Audio and movies](docs/AUDIO.md)
- [Gameplay systems](docs/NATIVE_GAMEPLAY.md)
- [Player models and animation](docs/NATIVE_PLAYER.md)

## Credits and license

The original decompilation foundation is
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).
RECVX Native adds the native engine, platform backends, tooling and further
reverse-engineering work described above.

The project license is in [LICENSE](LICENSE). Dependency licenses accompany the
packages. Resident Evil and CODE: Veronica X belong to their respective rights
holders; this is an independent, unofficial project.
