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

## Platform direction

Cross-platform design is built into the engine. Platform delivery is being
brought up separately from the shared gameplay code.

| Platform | Direction |
|---|---|
| **Mac / Apple Silicon** | Current development platform with Metal and software rendering, plus optional Vulkan through MoltenVK; standalone Mac builds are part of the release plan. |
| **Windows x64** | Build support, the Direct3D 11 backend and optional Vulkan and Direct3D 12 backends are present; validated Windows releases are planned. |
| **Windows ARM64** | Native build with Visual Studio 2026 Insiders (MSVC 19.52, Ninja) on Windows 11 ARM64, with Direct3D 11 on the Parallels Display Adapter (WDDM GPU) and software rendering verified. |
| **Linux** | Native build with OpenGL selected by default and optional Vulkan; Linux driver/hardware validation remains open. |
| **iOS/iPadOS** | Runs on iPhone (iOS 27) with a Bluetooth game controller — the native build plays as it is, sideloaded as an IPA (`./build.sh --platform ios`). No touch controls yet. |
| **Android** | Native arm64 APK (`./build.sh --platform android`, API 26+) with optional Vulkan as GPU default and software fallback; game data loose on the device. Runs in the arm64 emulator; real devices, touch controls and release signing are open. |
| **Web (Firefox, Chrome)** | The native engine as WebAssembly, served by a Docker container on port 8722 (`docker compose up -d`, or the prebuilt [`ghcr.io/patt92/recvx-native-web`](https://github.com/Patt92/recvx-native/pkgs/container/recvx-native-web) image); software rasterizer on worker threads, WebGL 2 presentation, saves in the browser. Verified in Firefox and Chromium on macOS from the title through free play and LOAD GAME. |
| **Apple TV** | Native tvOS build sharing the iOS path (`./build.sh --platform tvos`): Metal, bundled compressed data, Bluetooth game controller. Builds and links; not yet started on a device. |

These are native platform targets, not plans to ship an emulator with the game.

## Quick start

You need your own copy of the game as a disc image (`.iso`). The downloads contain
no game files.

1. **Download** the build for your platform from the
   [latest release](https://github.com/Patt92/recvx-native/releases):
   `recvx-windows-x64.zip`, `recvx-windows-arm64.zip`, `recvx-linux-x64.tar.gz` or
   `recvx-android-arm64.apk`.
2. **Prepare the game data** (Windows and Linux). Unpack the download, put your disc
   image into its `iso` folder, install [Python 3.12+](https://www.python.org/downloads/)
   and [FFmpeg](https://ffmpeg.org/download.html), then run `Extract-ISO.cmd` (Windows) or
   `./extract-iso.sh` (Linux). This creates `data` and `mods` next to the runtime;
   expect a few minutes and about 8 GB.
3. **Play.** Start `recvx-runtime.exe` (Windows) or `./bin/recvx-runtime` (Linux).

**Android:** prepare `data` on a computer as in step 2, install the APK and copy
`data` to the device with `adb` as described in [Android](#install-and-play).

**Browser (Firefox, Chrome):** with prepared `data/` in a source checkout, run
`python3 tools/web_site.py --index-only` and `docker compose up -d --build`,
then open http://localhost:8722/ — see [Web](#web-firefox-chrome).

Building from source, macOS and iOS: see [Build and run](#build-and-run).

## Native rendering, multiple backends

The renderer translates the game's drawing rules into native rendering work,
with a shared pipeline feeding the available backend:

- **Metal** — the macOS GPU backend, designed for Apple Silicon and the basis
  for planned Apple-device builds.
- **Direct3D 11** — the Windows GPU backend. It uses Rasterizer Ordered Views
  to preserve drawing order and programmable blending; hardware without the
  required support falls back to software. Verified on Windows 11 ARM64 with
  the Parallels Display Adapter (WDDM GPU).
- **Direct3D 12** — an optional Windows backend, never the platform default.
  It runs the same HLSL as the Direct3D 11 backend on a D3D12 device with
  Rasterizer Ordered Views, one command list per frame and a fence before the
  read-back; GPUs without ROV support fall back to software. It exists to give
  later DirectX 12 presentation stages (FSR, see below) a device to attach to;
  its output is held to the same bit-for-bit parity as the other backends and
  is not yet validated on hardware.
- **Software** — a CPU renderer that remains available as a fallback and a
  reference for validating GPU output.
- **Vulkan** — an optional backend on macOS, Windows and Linux, and the GPU
  default on Android (no desktop OpenGL or Metal there). It requires ordered fragment shader interlock (the
  counterpart of Metal framebuffer fetch and Direct3D 11 Rasterizer Ordered
  Views) and storage-image color, depth and shadow masks; devices without the
  extension fall back to software rather than approximating the blend. The
  shaders are compiled to SPIR-V at build time and the loader is opened at run
  time. Its output is checked bit-for-bit against the software reference.
- **OpenGL** — the Linux GPU default, optionally selectable on macOS and Windows.
  Requires OpenGL 4.1 Core and an available display; uses ordered copies (or
  texture barriers when supported), not fixed-function blending. Driver-specific
  parity and performance still require validation; unavailable GPUs fall back to software.

The goal is to retain the game's distinctive presentation while offering native
GPU acceleration, higher-resolution output and optional texture filtering and
supersampling. Rendering fidelity is checked against the reference path rather
than judged solely by how close a screenshot looks.

## Upscaling and frame generation

Upscaling, frame generation and post-processing form a host presentation
stage that sits after the rasterizer and is independent of the renderer
selection. The stage receives the finished 640×480 frame after every 2D
drawer, together with the depth buffer and the last geometry projection, and
changes presentation only: the game image, the 30 Hz tick, input, events,
screenshots and headless runs are unchanged. Every feature is off by default,
selectable in the OPTION screen, the debug menu and `video.ini`, and falls
back to the plain SDL copy with "(not available)" when the device or OS lacks
it. No feature silently substitutes a different filter.

A shared **frame-rate** setting (`30`, `60`, `120`, `unlimited`) sets the
presentation cadence for every backend while the game tick stays at the
original 30 Hz; without frame generation, repeated presents show the same
frame. The full setting reference is in
[docs/NATIVE_PORT.md](docs/NATIVE_PORT.md#metalfx-presentation-stage-macos)
and [its Windows counterpart](docs/NATIVE_PORT.md#fsr-presentation-stage-windows).

**Widescreen (mod):** `widescreen = on` in `video.ini`, the OPTION screen or
the debug menu switches to 16:9 like the GameCube and PS2 widescreen patches:
a wider 3D view at the same height, HUD and menus stretched as on the
consoles, movies kept at 4:3 with side bars. It works with every renderer and
presentation path.

### MetalFX (macOS)

Presents through the SDL window's Metal layer. Because it consumes only the
finished frame, it works on top of every macOS raster backend (Metal, Vulkan
via MoltenVK, OpenGL, software). Needs macOS 13 or newer with Metal.

- **Upscaler** — `metalfx-spatial` or `metalfx-temporal`, from 640×480 to
  the window's 4:3 fit. Temporal uses depth and camera-reprojected motion
  vectors; the 2D layer is treated as a stationary foreground because the 2D
  drawers write at integer 640×480 positions. Temporal is limited to the
  device's scale range (2× on current devices), a spatial pass covers the rest.
- **Frame generation** — `metalfx` (`MTLFXFrameInterpolator`, macOS 26 or
  newer): one intermediate frame per game tick between the previous and the
  current image, on the stage's own presentation thread; the main thread
  hands over a copy and never blocks on vsync. Verified stable at 60 fps on
  an M4 Pro.
- **Post-processing** — contrast-adaptive sharpening and a CRT filter
  (scanlines, or a slot mask with bloom in linear light), including the 4:3
  fit into the window.

### AMD FSR 4 (Windows)

Presents through a DXGI flip swap chain on the SDL window's HWND from a
Direct3D 12 device of its own, so it works on top of every Windows raster
backend (Direct3D 11 default, Direct3D 12, Vulkan, software). FSR itself comes
only from AMD's signed ffx-api DLLs; the repository ships the MIT ffx-api
headers (`native/third_party/FidelityFX-SDK/include`), never the DLLs. Without
the DLLs next to the runtime (or in `RECVX_FSR_SDK`) the rows read "(not
available)" and the plain SDL copy presents.

- **Upscaler** — `fsr4` (FP8 model) or `fsr4-int8` (INT8 model, for GPUs
  without FP8 matrix support), from 640×480 to the window's 4:3 fit. FSR 4 ML
  upscaling needs Windows 10/11 and a Radeon RX 9000 or newer (or a driver that
  exposes it); on other GPUs the same upscaler DLL falls back to FSR 3.1.x,
  which the status line reports. Inputs are the finished frame in sRGB
  (`FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE`), inverted finite depth from the
  original near/far clip, camera-reprojected motion vectors (the same as the
  MetalFX stage) and a reactive mask that excludes the 2D layer from history.
  No subpixel jitter and no per-object motion vectors yet: the engine does not
  produce them, moving characters are reprojected as part of the camera.
- **Frame generation** — not wired in yet. `amd_fidelityfx_framegeneration_dx12.dll`
  (FSR 3.1/4 frame interpolation and swap chain) is loaded by the same loader,
  but frame interpolation expects ≥ 60 fps of input while the game tick is
  30 Hz; it needs the presentation-thread cadence of the MetalFX stage and a
  UI composition path before it can be offered.
- **Post-processing** — the same sharpening and CRT chain as the MetalFX
  stage, ported to HLSL compute.

Linux and macOS: FSR 3.1 through the Vulkan backend stays a TODO (the SDK's
Vulkan sources are not part of this checkout; see
[docs/NATIVE_PORT.md](docs/NATIVE_PORT.md#fsr-presentation-stage-windows)).
OpenGL has no FSR path (no SDK backend, no SDL GL/Vulkan interop).

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

The development app started by `Start.command` reads `data/` and `mods/` **loose**
from the project folder, so mod and data changes need no rebuild. Packages and
mobile apps bundle their data with the project's mods baked in at build time
and ship no `mods/` folder; DDS packs go into `mods/hd-source/` and are imported
by every `./build.sh` run. The complete flow (which folder is read, written and
bundled) is in [docs/BUILD.md](docs/BUILD.md).

See [assets and mod support](docs/NATIVE_ASSETS.md) for supported formats,
replacement paths and texture-pack tools.

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
- **CMake 3.16+**: either on `PATH`, or Visual Studio's bundled CMake. In the
  Visual Studio Installer it is the **C++ CMake tools** component; depending on
  the installation it only appears after selecting the **Linux and Mac
  development with C++** workload. The build
  finds Visual Studio's bundled CMake for every version and edition, including
  Preview and Insiders (via `vswhere -prerelease` and the `Microsoft Visual Studio`
  folders), and also checks
  `C:\Program Files\CMake\bin`.
- **Visual Studio or Visual Studio Build Tools**, including the **Desktop
  development with C++** workload, MSVC C/C++ tools and a Windows SDK.
  Native Windows ARM64 builds also need the ARM64 C/C++ build tools.
- **Optional Vulkan backend:** the Vulkan SDK (headers and `glslc`) at build
  time and the Vulkan loader (`vulkan-1.dll`) at run time. The Windows
  cross-build has no Vulkan headers by default and omits the backend unless a
  native build or an explicit SDK path supplies them.
- **Optional Direct3D 12 backend:** built by default from the Windows SDK or
  mingw headers (`--d3d12 off` / `-DRECVX_ENABLE_D3D12=OFF` omits it);
  `d3d12.dll`, `dxgi.dll` and `d3dcompiler_47.dll` are loaded at run time.
  Select it with `--renderer d3d12`, `renderer = d3d12` in `video.ini` or the
  debug menu; `RECVX_D3D12_DRIVER=warp` runs it on Microsoft's WARP software
  device, `RECVX_D3D12_DEBUG=1` enables the D3D12 debug layer.
- **Optional FSR 4 presentation stage:** built by default from the vendored
  ffx-api headers (`--fsr off` / `-DRECVX_ENABLE_FSR=OFF` omits it). At run
  time it loads AMD's signed `amd_fidelityfx_loader_dx12.dll` (or the SDK-1.x
  name `amd_fidelityfx_dx12.dll`) and `amd_fidelityfx_upscaler_dx12.dll` from
  the runtime's folder, `RECVX_FSR_SDK`, or `native/third_party/FidelityFX-SDK`
  in the development tree; the INT8 model is the same upscaler DLL in a
  `FSR4 INT8` subfolder. The DLLs are not part of the repository.
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

Windows ARM64 hosts select the native ARM64 target automatically, also when
an x64 Python runs under emulation. `--platform windows` or
`--platform windows-arm64` overrides the choice. The build prints the chosen
target and CMake path before configuring; the source-policy check does not
verify installed build dependencies. Launch through `Start.cmd`.

Native Windows builds use **Ninja** with the Visual Studio developer
environment (the build runs `vcvarsall.bat` for the host/target pair itself, or
reuses an open Developer PowerShell). Visual Studio's C++ CMake tools ship
Ninja; the build finds it next to the bundled CMake or on `PATH`. The Visual
Studio generator (`-A ARM64`/`-A x64`) is only the fallback without Ninja, or
when `CMAKE_GENERATOR` names it. The fallback writes every custom build step
(SDL2 download, header generation, DLL copies) as a `.cmd` file into the build
directory, which AppLocker or software-restriction policies on managed
machines block with "Dieses Programm wurde durch eine Gruppenrichtlinie
geblockt" and MSB8066 in `sdl2-populate.vcxproj`; Ninja runs these commands
directly. A build directory configured by a different generator is recreated.

### macOS

Install:

- **Apple Command Line Tools** (Clang and the macOS SDK).
- **Python 3.12+** and **CMake 3.16+**, available as `python3` and `cmake`.
- **FFmpeg**, including `ffprobe`, on `PATH` when using `--iso`.
- **Ninja** if cross-building Windows binaries; the build tool downloads the
  pinned Zig cross-toolchain automatically.
- **Optional Vulkan backend:** Vulkan headers and `glslc` at build time, plus a
  Vulkan loader (MoltenVK on macOS) at run time. Without headers or `glslc`,
  CMake warns and omits the backend; the rest of the build is unaffected.

For example, with Homebrew already installed:

```bash
xcode-select --install
```

```bash
brew install python@3.12 cmake ffmpeg ninja vulkan-headers shaderc molten-vk
```

Build for Apple Silicon (arm64) and prepare a package:

```bash
./build.sh --iso "/path/to/game.iso" --package
```

With prepared data:

```bash
./build.sh --package
```

Launch through `Start.command`. macOS defaults to Metal; OpenGL and Vulkan are
optional (`--renderer opengl`, `--renderer vulkan`), and software remains
available (`--renderer software`). Vulkan is built when the Vulkan headers and
`glslc` are present and is opened at run time (the Vulkan loader, or MoltenVK on
macOS); it is never selected automatically. `--vulkan off` or CMake
`-DRECVX_ENABLE_VULKAN=OFF` omits it explicitly.

`./build.sh` alone builds `build/start/macos-arm64/RECVX Native.app`, a small development
app without data: its launcher passes the project's `data/` and `mods/` folders by path and
the game reads them **loose** (no rebuild after data or mod changes).
Only `--package` copies the data (`data/`, or `data_compressed/` with `--compressed`) into a
standalone app, with the project's mods baked in and no mods folder; see [docs/BUILD.md](docs/BUILD.md).
`--compressed [folder]` additionally writes a lossless compressed tree from `data/`
(colour-mapped TGA, recooked movies; default `data_compressed`, kept if it already exists),
e.g. `./build.sh --iso "/path/to/game.iso" --compressed`.
At startup the runtime prints the data folder with its texture format, the mods folder
with its number of HD textures, and per room how many textures came from HD, mod files or
`data/`; the debug menu entry "Textur-Mods (HD)" switches mods off and reloads the room in place.

### iOS / iPadOS

The iOS build runs the game on an iPhone with iOS 27 and a Bluetooth game
controller; it is played through sideloading, not the App Store. Needs a Mac
with Xcode (iOS SDK) and prepared data (see macOS above).

**Build an unsigned IPA** (the normal route; no Apple developer account needed
to build):

```bash
./build.sh --platform ios --ios-bundle-data --compressed --package
```

- Writes `build/ios-device/recvx-runtime.app` and packages it as
  `build/packages/ios-arm64-v<version>/RECVX-Native-unsigned.ipa`
  (`--output <new-dir>` chooses another folder; existing packages are never
  overwritten).
- `--ios-bundle-data` copies the prepared data into the app (about 3 GB with
  `--compressed`); without it the app reads `Documents/data`, which you fill
  through Finder or Files file sharing after installing.
- Apps built with the iOS 27 SDK must adopt the UIKit scene lifecycle; the
  `Info.plist` declares a scene manifest and `native/ios_scene.mm` attaches the
  SDL window to that scene. Without it the app terminates at launch.

**Install the unsigned IPA.** Apple only runs signed apps, so the IPA has to be
signed with a certificate tied to your Apple ID before it goes on the device:

- **AltStore / SideStore** (and similar sideloaders such as Sideloadly): import
  the IPA and the store signs it with your Apple ID. A free Apple ID gives a
  7-day certificate with 3 active apps; the store re-signs ("refreshes") it
  before it expires, SideStore does so on the device itself. With a paid Apple
  Developer Program membership the certificate lasts one year.
- **zsign** with your own certificate: `zsign -k dev.p12 -p <password>
  -m dev.mobileprovision -o RECVX-Native.ipa RECVX-Native-unsigned.ipa`, then
  install the signed IPA through Xcode (Devices and Simulators), Apple
  Configurator or `xcrun devicectl device install app`. The provisioning
  profile must cover the bundle identifier (`org.recvx.native`, change it with
  `--ios-bundle-id`). Paid certificate: valid one year; free developer
  certificate: 7 days, then re-sign and reinstall (saves in `Documents` stay).
- **Sign in the build** instead: `--ios-profile <file.mobileprovision>` signs
  the app with the matching keychain identity and `--package` then writes a
  signed `RECVX-Native.ipa`; `--install --ios-device <id>` installs it directly.

Neither route touches the game: the IPA is the same native build, only the
signature differs. Input is a Bluetooth game controller (same bindings as the
desktop controller page); touch controls do not exist yet.
`./build.sh --platform ios-simulator --install` builds and installs an ad hoc
signed simulator app (`build/ios-simulator`).

### Android

A native arm64 APK (`arm64-v8a`, Android 8.0 / API 26 or newer). It has run in
the Android emulator (title, input, intro movie, first room); real phones,
tablets, controllers and performance are **not** checked yet. The game data stays
outside the APK.

#### Install and play

You need a computer with **adb** ([Android platform tools](https://developer.android.com/tools/releases/platform-tools))
and USB debugging enabled on the device.

1. **Prepare `data`** on Windows or Linux with the extractor of the release package
   (see [Quick start](#quick-start), step 2). The folder is the same for every platform.
2. **Install** `recvx-android-arm64.apk` from the
   [latest release](https://github.com/Patt92/recvx-native/releases):

   ```bash
   adb install recvx-android-arm64.apk
   ```

3. **Start the app once.** It stops with "exported native data missing"; this first
   start makes Android create the app's folder `Android/data/org.recvx.nativeport`
   with the app as owner:

   ```bash
   adb shell am start -n org.recvx.nativeport/.RecvxActivity
   ```

4. **Copy the data** into that folder (no storage permission needed). Do not create
   the folder with `adb` before step 3: the app could not read it ("Permission
   denied"); then uninstall, reinstall and start once.

   ```bash
   adb push data/. /sdcard/Android/data/org.recvx.nativeport/files/data/
   ```

   `files/MOVIE` and `archives/RDX_LNK` are not read by the runtime and can be left
   out (about 1.1 GB less). Mods go to `.../files/mods/`.

5. **Play:** start the app from the launcher. Console output: `adb logcat -s RECVX`.

Every build of the release is signed with its own debug key, so a newer APK only
installs after `adb uninstall org.recvx.nativeport`, which also deletes the data
on the device; repeat steps 3 and 4.

- Settings, save games, snapshots and `runtime-warnings.log` live in the app's
  internal storage (`files/RECVX`) and are removed when the app is uninstalled.
- **Rendering:** the Vulkan backend is the Android GPU default. It needs ordered
  fragment shader interlock (`VK_EXT_fragment_shader_interlock`) and
  `VK_KHR_shader_float_controls`, as on the desktop. Most Android GPU drivers do
  not expose interlock; there the game draws with the software rasterizer and
  logs why (`Renderer: software (...)`). There is no blend approximation. The
  emulator falls back to software.
- **Input:** keyboard and game controllers (USB/Bluetooth) with the same
  bindings as on the desktop. Touch controls do not exist yet. Landscape only.
- Runtime arguments for diagnostics:
  `adb shell am start -n org.recvx.nativeport/.RecvxActivity --esa args --renderer,software`.

#### Build the APK yourself

Needs JDK 17 or 21, Gradle, the Android command-line tools with NDK r28,
platform 36 and build-tools 36, and `glslc` (shaderc or the Vulkan SDK) for the
Vulkan shaders.

Linux (Debian/Ubuntu), with the [command-line tools](https://developer.android.com/studio#command-line-tools-only)
unpacked to `$ANDROID_HOME/cmdline-tools/latest`:

```bash
sudo apt install openjdk-21-jdk glslc ninja-build
export ANDROID_HOME=$HOME/Android/Sdk
yes | sdkmanager --licenses
sdkmanager "ndk;28.2.13676358" "platforms;android-36" "build-tools;36.0.0" platform-tools
```

macOS (Homebrew):

```bash
brew install openjdk@17 gradle shaderc
brew install --cask android-commandlinetools
export JAVA_HOME=/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home
yes | sdkmanager --licenses
sdkmanager "ndk;28.2.13676358" "platforms;android-36" "build-tools;36.0.0" platform-tools
```

Gradle 9.8 must be on `PATH`. The build finds the SDK through `ANDROID_HOME`/`ANDROID_SDK_ROOT`
or the default Homebrew and Android Studio locations, the NDK through
`ANDROID_NDK_HOME` or the newest r28 in the SDK, and Java through `JAVA_HOME`.
Then:

```bash
./build.sh --platform android
```

This writes `build/android-arm64/RECVX-Native.apk` (the runtime as `libmain.so`,
SDL2 and the C++ runtime; the native-port policy audit runs on the unstripped
`libmain.so` first). `--install` installs it on the connected device or emulator,
`--vulkan off` builds without Vulkan. Continue with step 3 above.

Build details: [Native Port → Android](docs/NATIVE_PORT.md#android-arm64-v8a-api-26).

### Linux

Install:

- **GCC or Clang** with C++17 support, **Make or Ninja**, **Python 3.12+** and
  **CMake 3.16+**.
- **pkg-config** and development packages for SDL2's host backends: **X11
  and/or Wayland** for windowing, and **ALSA and/or PulseAudio** for audio.
  The build downloads SDL2 itself. GPU rendering additionally needs an OpenGL
  4.1-capable driver (for example Mesa) and SDL's OpenGL build dependencies.
- **FFmpeg**, including `ffprobe`, on `PATH` when using `--iso`.
- **Optional Vulkan backend:** Vulkan headers, `glslc` (for example the
  `libvulkan-dev` and `glslc`/`shaderc` packages) and the Vulkan loader
  (`libvulkan.so.1`). Missing headers or `glslc` leave the backend out.
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

Linux selects OpenGL by default (`renderer = gpu`); OpenGL 4.1 Core and a display
are required. A missing context falls back to software with a diagnostic. The
backend can be left out with `./build.sh --opengl off`, or explicitly selected on
any desktop platform with `--renderer opengl`. Linux driver parity/performance
still need hardware validation.
Linux packaging is not yet supported, so do not pass `--package`. Run the built
runtime with the prepared data:

```bash
./build/native/recvx-runtime --data data
```

See the [build guide](docs/NATIVE_PORT.md) for further build and packaging options,
and the [launch guide](STARTEN.md) and [controls reference](docs/CONTROLS.md) for
playing.

### Web (Firefox, Chrome)

The game also runs in the browser: the same native engine compiled to
WebAssembly, served by a container on port **8722**. With prepared game data in
`data/`:

```bash
python3 tools/web_site.py --index-only      # once per data/ change
docker compose build
docker compose up -d
```

Then open **http://localhost:8722/** and click *Click to start*.

**Prebuilt image:** GitHub Actions publishes the web container for `linux/amd64`
and `linux/arm64` as [`ghcr.io/patt92/recvx-native-web`](https://github.com/Patt92/recvx-native/pkgs/container/recvx-native-web)
(`main` = latest `main` build, `sha-<commit>` per build, `<version>`/`latest` for
releases). It needs no local build; in a repository checkout:

```bash
RECVX_WEB_IMAGE=ghcr.io/patt92/recvx-native-web:main docker compose up -d --no-build
```

Or in any folder, with this `compose.yaml`:

```yaml
services:
  web:
    image: ghcr.io/patt92/recvx-native-web:main
    ports:
      - "8722:8722"
    volumes:
      - ./data:/srv/recvx/data:ro
      - ./mods:/srv/recvx/mods:ro
    restart: unless-stopped
```

```bash
docker compose up -d
```

**`data/` and `mods/` must exist in that folder** before `up` (`mods/` may be
empty): the container mounts both read-only. `data/` is the prepared game data
including its index `data/web-files.txt` from `python3 tools/web_site.py
--index-only` (run it in a checkout; `--data`/`--mods` point it at other
folders), again after every data or mod change.

- **Game data stays outside the image.** The container mounts `data/` (and
  `mods/`) read-only and the browser loads files on demand. Other folders or
  port: `RECVX_DATA_DIR`, `RECVX_MODS_DIR`, `RECVX_WEB_PORT`.
- **Controls:** keyboard and game controllers with the desktop bindings. There
  are no touch controls.
- **Saves:** save games, snapshots and settings live in the browser's private
  storage for this address (per browser; cleared together with the site data).
- **Rendering:** the original-faithful software rasterizer on worker threads,
  presented through WebGL 2. WebGPU/WebGL cannot reproduce the game's ordered
  per-pixel blending, so there is no approximated GPU path.
- **Access:** `http://localhost` or HTTPS only. Browsers block the required
  shared memory on plain HTTP from other hosts.
- **Verified** in Firefox 158 and Chromium 154 on macOS: title, new game, intro,
  free control, walking/running, inventory, sound, snapshots across reloads,
  LOAD GAME with the same room, position and inventory as the native runtime,
  and a steady 30 Hz game tick. Safari, mobile browsers and other machines are
  not verified.

Without Docker: `./build.sh --platform web` builds `build/web/site`.
Architecture, server requirements and the browser checks (`tests/web`):
[Web](docs/WEB.md).

### Docker

`docker compose -f compose.builder.yaml build` and
`docker compose -f compose.builder.yaml run --rm builder` build Linux and
Windows x64/ARM64 in a container, from a disc image in `./iso` into `./out`.
macOS and iOS still need a Mac. Details: [Docker builder](docs/DOCKER.md).

### GitHub Actions

[`.github/workflows/runtimes.yml`](.github/workflows/runtimes.yml) builds the runtimes without
game data on every push to `main`, on pull requests, on `v*` tags and on demand
(*Run workflow*). Each job runs the policy check and the regular `build.sh`/`build.ps1`;
no ISO, no `data/`, no tests that need disc files.

**Downloads:**

- [Releases](https://github.com/Patt92/recvx-native/releases): every push to `main` adds a
  prerelease **build-<version>** (older builds stay); a `v*` tag publishes a regular release. Both carry
  `recvx-windows-x64.zip`, `recvx-windows-arm64.zip`, `recvx-linux-x64.tar.gz`,
  `recvx-android-arm64.apk` and `SHA256SUMS`.
- Web container: the job *Web image* builds `docker/web.Dockerfile` for `linux/amd64` and
  `linux/arm64` and publishes it as
  [`ghcr.io/patt92/recvx-native-web`](https://github.com/Patt92/recvx-native/pkgs/container/recvx-native-web)
  (tags `main`, `sha-<commit>`, and `<version>`/`latest` for `v*` tags; pull requests only build).
  Usage: [Web](docs/WEB.md#quick-start-docker).
- [Actions artifacts](https://github.com/Patt92/recvx-native/actions/workflows/runtimes.yml):
  open a run and download the artifacts at the bottom of its summary page (kept 90 days,
  also for pull requests).

| Artifact | Contents |
|---|---|
| `recvx-windows-x64`, `recvx-windows-arm64` | `recvx-runtime.exe`, `SDL2.dll`, the exporters, `Extract-ISO.cmd`, `tools/`, an empty `iso\` folder; Direct3D 12 and FSR 4 built in (AMD's FSR DLLs are not included) |
| `recvx-linux-x64` | `bin/recvx-runtime`, the exporters, `lib/libSDL2*.so`, `extract-iso.sh`, `tools/`, an empty `iso/` folder; OpenGL and Vulkan |
| `recvx-android-arm64` | `RECVX-Native.apk`, signed with the debug key (sideload) |

**What else you need: the game data.** The downloads contain no game files; the Windows
and Linux packages prepare them from your own disc image, without a build:

1. Unpack the download. Its top folder is the **main folder**.
2. Put your disc image(s) into the folder **`iso`** inside the main folder
   (`iso\game.iso` / `iso/game.iso`; with both discs put both images there, they are merged).
3. Install **Python 3.12+** and **FFmpeg** (`ffmpeg` and `ffprobe` on `PATH`; on Windows they
   may instead lie in `build-tools\` inside the main folder). FFmpeg cooks the movies.
4. Run **`Extract-ISO.cmd`** (Windows, double-click) or **`./extract-iso.sh`** (Linux).
   It writes **`data`** and an empty **`mods`** folder directly into the main folder.
   This takes a few minutes and about 8 GB; the images in `iso` are only read and can be
   removed afterwards.
5. Start `recvx-runtime.exe` (Windows) or `./bin/recvx-runtime` (Linux). Without `--data` it
   finds `data` in the current folder, next to the runtime or one level above it.

```text
recvx-windows-x64\              recvx-linux-x64/
  recvx-runtime.exe               bin/recvx-runtime
  Extract-ISO.cmd                 extract-iso.sh
  iso\game.iso      <- you        iso/game.iso       <- you
  data\             <- extractor  data/              <- extractor
  mods\             <- extractor  mods/              <- extractor
```

Android has no extractor: prepare `data` with a Windows/Linux package or a source checkout
(`./build.sh --iso "/path/to/game.iso"`, see [Build and run](#build-and-run) or the
[Docker builder](docs/DOCKER.md)) and copy it to the device. The prepared `data` folder is the
same on every platform.

| Platform | Game data (`data`) | Mods | Saves, settings, snapshots |
|---|---|---|---|
| Windows | `data\` in the main folder (or `--data C:\path\to\data`) | `mods\` beside `data\` (or `--mods`) | `%APPDATA%\RECVX` |
| Linux | `data/` in the main folder (or `--data /path/to/data`) | `mods/` beside `data/` (or `--mods`) | `$XDG_DATA_HOME/RECVX`, else `~/.local/share/RECVX` |
| Android | `/sdcard/Android/data/org.recvx.nativeport/files/data` (`adb push`, see [Android](#install-and-play)) | `.../files/mods` | app-internal storage, removed on uninstall |
| Web (Firefox, Chrome) | `data/` on the server, mounted read-only into the container (`RECVX_DATA_DIR`), indexed by `tools/web_site.py --index-only`; see [Web](#web-firefox-chrome) | `mods/` on the server (`RECVX_MODS_DIR`) | the browser's private storage (OPFS) for this address, per browser; removed with the site data |

Linux additionally needs SDL2's runtime libraries for X11 or Wayland and ALSA or
PulseAudio, and an OpenGL 4.1 driver (for example Mesa); Windows needs a Direct3D 11
capable GPU, Direct3D 12 and Vulkan are optional.

## Further reading

- [Engine and build architecture](docs/NATIVE_PORT.md)
- [Assets, HD textures and mods](docs/NATIVE_ASSETS.md)
- [Audio and movies](docs/AUDIO.md)
- [Gameplay systems](docs/NATIVE_GAMEPLAY.md)
- [Player models and animation](docs/NATIVE_PLAYER.md)
- [Web (browser) build and hosting](docs/WEB.md)

## Credits and license

The original decompilation foundation is
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).
RECVX Native adds the native engine, platform backends, tooling and further
reverse-engineering work described above.

The project license is in [LICENSE](LICENSE). Dependency licenses accompany the
packages. Resident Evil and CODE: Veronica X belong to their respective rights
holders; this is an independent, unofficial project.

## Let your AI play the game

A language model of your choice plays the real game in the normal window while
you watch. It reads the engine's own state as numbers (no screenshots) and sends
one action per decision. The game logic is untouched.

1. Create `.env` in the project folder (untracked, never committed):

   | Variable | Meaning |
   |---|---|
   | `RECVX_LLM_BASE_URL` | OpenAI-compatible endpoint, e.g. `http://localhost:8080/v1` |
   | `RECVX_LLM_MODEL` | model name |
   | `RECVX_LLM_API_KEY` | API key, empty for a local server |
   | `RECVX_SKIP_VIDEOS` | optional, `1` = skip FMV videos automatically (in-engine scenes still play) |
   | `RECVX_COMBAT_RADIUS` | optional, metres for combat mode (default 25): enemies in sight that close make the model fight or leave first |
   | `RECVX_REFLEXES` | optional, `0` = the model fights alone (default: the driver shakes off grabs and shoots enemies in sight within `RECVX_ENGAGE` m, 20) |
   | `RECVX_LLM_CONTEXT_TOKENS` | optional request size cap in tokens (e.g. `15000`), oldest decisions dropped first; 0 = off |
   | `RECVX_LLM_EXTRA_BODY` | optional JSON added to every request; for models that think before answering: llama.cpp/vLLM `{"chat_template_kwargs":{"enable_thinking":false}}`, Ollama `/v1` `{"reasoning_effort":"none"}` |

2. Start `Start-AI-Play.command` (macOS) or `Start-AI-Play.cmd` (Windows).
3. Watch. Every decision is printed and logged to `build/play/driver.log`.

Works with llama.cpp, vLLM, LM Studio, Ollama, OpenAI and other compatible
servers. A coding agent such as Claude Code can also play directly through
`tools/agent_play/play.py` and `docs/AI_PLAYBOOK.md`.

Details (all options, how the driver stays fast, the control channel, limits):
[docs/AI_PLAY.md](docs/AI_PLAY.md).
