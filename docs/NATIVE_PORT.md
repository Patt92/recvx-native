# Build, packages, and architecture

RECVX Native runs host C/C++ code and reads prepared loose game files. It does
not emulate a PS2 CPU; the runtime never opens ISO images or AFS containers.
Original sources under `src/` remain semantic references for independent ports
into the native engine.

## Requirements

- Python 3.12+, CMake 3.16+ (both accessible from the terminal), and a C/C++17
  compiler.
- macOS: Apple Command Line Tools.
- Windows: Visual Studio with “Desktop development with C++”; native ARM64
  builds also need the ARM64 C/C++ build tools.
- Windows cross-builds from macOS/Linux: Ninja as well.
- iOS: Xcode with the iOS platform (simulator and device SDKs); a provisioning
  profile and its signing identity for device installs.
- Linux: GCC or Clang with C++17, Make or Ninja, and the SDL2 development
  dependencies for X11/Wayland and ALSA/PulseAudio; see the
  [README](../README.md#linux).
- `--iso`: FFmpeg and ffprobe on PATH. On Windows, they can instead be placed
  at `build-tools/ffmpeg.exe` and `build-tools/ffprobe.exe` in the repository.
  PATH takes precedence; the local copies are used when PATH has no matching
  tools. Put the required DLLs from the same FFmpeg package beside them. A
  static build needs no separate FFmpeg DLLs. No PATH change is needed.

The first build downloads pinned dependencies (SDL2 2.32.10 and, for
cross-builds, Zig 0.16.0; both checksum-verified). Neither the build nor the
game needs a network connection after that. Before running `./build.ps1` on
Windows, check `python --version` and `cmake --version` in the same PowerShell
session. `WinError 2` immediately after the policy check while starting CMake
means `cmake` is missing from PATH.

## Building and packaging

```sh
./build.sh --platform macos --verify-data --package
```

```sh
./build.sh --platform windows --verify-data --package
```

```powershell
./build.ps1 --platform windows --verify-data --package
```

Windows ARM64 can be cross-built on macOS/Linux with the same verified Zig
toolchain. SDL2 has no official ARM64 MinGW package, so this build compiles it
from the pinned source:

```sh
./build.sh --platform windows-arm64 --verify-data --package
```

On Windows ARM64, `./build.ps1` instead builds natively for ARM64 with the
Visual Studio generator. No cross-build is needed. The Visual Studio generator
does not produce `compile_commands.json`, so the policy gate checks the
`ClCompile` entries in the generated `.vcxproj` files. One limitation remains:
the Windows binary audit (`tools/windows_link_audit.py`) obtains the link
command through `ninja -t commands` and checks implicit inputs in Zig's cache.
It currently applies only to the Zig cross-build; a native MSVC build does not
yet pass this final gate.

The package is written to `build/packages/windows-arm64-v<version>`. As with
any Windows cross-build, it must be tested on Windows ARM64 or under suitable
emulation. Building alone does not establish runtime behavior.

### iOS (arm64 device and simulator)

iOS builds need a macOS host with Xcode and its iOS platform. The wrapper uses
the active developer directory, or else the first `/Applications/Xcode*.app`
with an iOS SDK; it does not change `xcode-select`. CMake runs with the same
single-config generator as on macOS (not the Xcode generator), so the policy
gate audits `compile_commands.json` and the final binary exactly as for the
other platforms. SDL2 is linked statically; Metal is the only GPU rasterizer
(no OpenGL on iOS). Only `recvx-runtime` is built as an app bundle; the export
tools and tests stay on the host. Deployment target: iOS 15.0.

```sh
./build.sh --platform ios-simulator --install
```

```sh
./build.sh --platform ios --ios-profile <profile.mobileprovision> --package
```

| Option | Effect |
|---|---|
| `--platform ios-simulator` | Build `build/ios-simulator/recvx-runtime.app`, signed ad hoc |
| `--platform ios` | Build `build/ios-device/recvx-runtime.app`; unsigned without a profile |
| `--ios-profile <file>` | Sign the device app with this provisioning profile and the matching keychain identity |
| `--ios-bundle-id <id>` | Bundle identifier; must be covered by the profile (default `org.recvx.native`) |
| `--ios-bundle-data` | Copy `--data` into the app as `data/` (7–8 GB; otherwise the app reads `Documents/data`) |
| `--install` | Install on the booted simulator, or with `--ios-device <id>` on a device (`xcrun devicectl`) |
| `--package` | Device only: write `build/packages/ios-arm64-v<version>/RECVX-Native.ipa` |

The app has no command line. Game data comes from the bundle when it contains
`data/native-data.version`, otherwise from `Documents/data` in the app
container (Finder/Files file sharing is enabled; for the simulator, copy it
into `xcrun simctl get_app_container booted org.recvx.native data`). Mods are
read from `Documents/mods`. INIs, snapshots and save games use the shared user
path resolution: `Library/Application Support/RECVX` inside the app container.

A provisioning profile for a device build can come from Xcode (automatic
signing of any project with the same bundle identifier and your team) or from
the Apple developer portal. Building, signing and simulator start do not
establish runtime behavior on iOS hardware. Open on iOS: on-screen touch
controls (input currently needs a game controller or keyboard), app lifecycle
during backgrounding, and memory limits on smaller devices.

| Option | Effect |
|---|---|
| *(none)* | Build only the runtime binary |
| `--test` | Build test targets and run CTest |
| `--verify-runtime` | Run the longer replay suite with original data |
| `--verify-data` | Verify the game-data manifest |
| `--data <path>` | Use a different data tree |
| `--package` | Create a launchable package under `build/packages/` |
| `--output <new-path>` | Choose another package destination; never overwrite an existing one |

macOS builds contain only arm64 code (deployment target 11.0, arm64 SDL2, no
Rosetta). They are signed locally ad hoc, not notarized. Windows builds target
x64 or ARM64; cross-builds need separate testing on Windows, ARM64 emulation,
or Wine.

Direct development commands (macOS; Linux CI uses `build/native`):

```sh
cmake -S . -B build/macos-arm64 -DRECVX_FETCH_SDL2=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
```

```sh
cmake --build build/macos-arm64 --parallel 8
```

```sh
ctest --test-dir build/macos-arm64 --output-on-failure
```

```sh
build/macos-arm64/recvx-runtime --data data
```

## Game data

`data/` is the complete local export: every disc file, separated AFS entries,
205 expanded rooms, decoded textures, and native movie packets.
`asset-manifest.json` records the size and SHA-256 of every file.

```sh
python3 tools/prepare_native_data.py --output data --verify
```

Optionally, the build creates the complete loose data tree directly from a PS2
ISO9660 image; a source checkout plus the ISO is sufficient:

```sh
./build.sh --iso /path/to/game.iso
```

After compilation, ISO extraction, `recvx-assets`, `recvx-world-assets` for
every room, `tools/cook_movies.py` (which needs FFmpeg/ffprobe at build time),
and manifest creation run in a staging directory. Only a complete run is moved
to `--data`. `--iso` can be repeated: identical files from both discs are
merged, conflicts are rejected. An existing complete export under `data/` is
left untouched; an existing incomplete directory is rejected. Use a fresh
`--data` directory to extract again. `python3 tools/iso9660.py game.iso --list`
or `--extract <directory>` checks the image without a build. The runtime still
reads only loose files.

`archived/` contains ISO images, source extraction, old toolchains, historical
work, and evidence. Normal builds and launches do not depend on it. `data/`,
`build/`, and `archived/` are not in the Git repository. The quarantined
`archived/FORBIDDEN_DONTUSE` tree is never a build or documentation source.

## Architecture

| Library / program | Role |
|---|---|
| `recvx_data` | Bounds-checked binary reading, room expansion, archive and export tools |
| `recvx_math` | Ninja mathematics and matrices |
| `recvx_scene` | Model/texture decoding, motion, skinning, software renderer |
| `recvx_game` | Event VM, inventory, messages, world, combat, enemies, audio, movies |
| `recvx_input` | Keyboard/controller input and menu/cinematic routing |
| `recvx-runtime` | SDL window, input, audio output, game loop |

### Original code and the port

Original sources and disassembly are semantic references only. The productive
runtime must use independently ported mechanisms on existing native owners;
it must never compile, link, or call original engine functions or generated
copies of their bodies. `tools/check_no_original_core.py` checks source, build,
and binary scope. `docs/port_ledger.tsv` records the original source location,
native evidence, and remaining gaps; `tools/port_check.py` reports its coverage.
Coverage percentages are not proof of equivalent gameplay.

### Timing

The original waits for two VSyncs per game step. The port runs a fixed 30 Hz
game clock (`native/timing.h`); movies, ADX fades, and the 48 kHz mixer (1,600
stereo frames per tick) follow the same clock. A frame is rendered only on a
new tick. The original's separate 60 Hz VSync time (`sys->time`) is retained
for event timers measured in VSync units.

### Rendering

A shared native geometry and command stage uses an internal 640×480 resolution;
the default window is 1280×960. `renderer = gpu` in `video.ini` selects Metal on
macOS, D3D11 on Windows, and OpenGL on Linux. `renderer = opengl` selects GL
explicitly, `renderer = vulkan` selects Vulkan; `software` selects the CPU
reference. `--renderer gpu|opengl|vulkan|software` applies only to the current run. Headless mode stays on
software unless explicitly changed.

OpenGL requires Core 4.1 and an SDL video driver with a display. It has its own
hidden context, rasterizes the same setup and shadow commands, and returns
color and 1/z to the existing image owner. Ordered, non-overlapping groups read
an earlier copied state; GL 4.5/ARB_texture_barrier can replace the copies for
color rasterization. Masks and composites remain ordered copy passes. There is
no fixed-function blending or new owner of game state. If the device is absent
or a frame fails, the runtime reports the reason and renders in software.

Vulkan (`native/gpu_raster_vulkan.cpp`, shaders in `gpu_raster_vulkan.glsl`) is
never a platform default. It requires `VK_EXT_fragment_shader_interlock` with
ordered pixel interlock, the counterpart of Metal framebuffer fetch and D3D11
rasterizer ordered views: color, 1/z, transported depth and shadow masks are
storage images read and written in primitive order. Devices without it get no
Vulkan backend (software path), not an approximate blend. Rendering is
offscreen and read back; presentation stays on SDL. The library is opened at run
time (`vulkan-1.dll`, `libvulkan.so.1`, or the Vulkan loader/MoltenVK on macOS;
`RECVX_VULKAN_LIBRARY` overrides the path). SPIR-V is compiled at build time with
`glslc -O0`, uses `precise` (NoContraction) and `SignedZeroInfNanPreserve`, which
keeps MoltenVK on Metal safe math. All instances share one device and draw one
frame at a time.

### MetalFX presentation stage (macOS)

`native/metalfx_present.h` presents the finished 640×480 frame through the
CAMetalLayer of the SDL window instead of the SDL texture copy. It is a host
display feature: the 640×480 image, the 30 Hz tick, inputs and events are
unchanged, screenshots and headless runs read the same image. Each stage is a
row in the settings screen (OPTION), a debug-menu action and a `video.ini` key:

| `video.ini` key | Values | Stage |
|---|---|---|
| `frame_rate` | `30`, `60`, `120`, `unlimited` | Presentation cadence for every backend (SDL copy path and MetalFX stage): the game tick stays 30 Hz, between two ticks the last drawn frame is presented again, so 60/120 show duplicates (no new motion) unless frame generation supplies the intermediate image. 60/120 keep vsync and are clamped to the display refresh rate divided by 30; `unlimited` turns vsync off and presents continuously (busy loop, higher CPU/GPU load). Frame generation needs at least two presents per tick and raises 30 to 60 by itself. |
| `upscaler` | `off`, `metalfx-spatial`, `metalfx-temporal` | `MTLFXSpatialScaler` or `MTLFXTemporalScaler` from 640×480 to the 4:3 fit of the drawable (Retina: 2560×1920 for the 1280×960 window). Temporal is limited to the device's scale range (2× on current devices); a spatial pass covers the rest. |
| `frame_generation` | `off`, `metalfx` | `MTLFXFrameInterpolator` (macOS 26 or newer): one intermediate frame per game tick between the previous and the current image. The runtime's cadence shows the intermediate image in the first half of the tick and the drawn image in the second half (`frame_rate` 60: two presents, 120: four). Presents are immediate (vsync through the layer); the earlier `presentDrawable:afterMinimumDuration:` scheduling let the display lag behind the tick and the loop stall. Resets on every cut. |
| `post_process` | `off`, `sharpen`, `crt` | Own compute kernel on the presented image: five-tap contrast-adaptive sharpening, or sharpening plus the 480 game scanlines and a subtle aperture mask. Also does the 4:3 letterboxing. |

Temporal scaling and frame interpolation read the 1/z depth of the geometry
pass and motion vectors from the camera reprojection of that depth with the
projection of the previous frame (view matrix and focal lengths recorded by the
renderer). Pixels that 2D drawers changed after the geometry pass (HUD, fades,
menus, text) are treated as unmoving foreground. Objects that move relative to
the camera carry no own motion vectors, so a moving actor can ghost in temporal
mode; spatial mode is the robust choice. No sub-pixel jitter is applied: the
2D layer is drawn at integer 640×480 positions and would shake against a
jittered 3D pass. A cut (room, camera cut, event camera key, screen owner such
as menu, map, terminal, movie, door transition) discards the MetalFX history,
so no intermediate frame is generated across a cut. Unavailable stages show
"(not available)" in the settings screen and the runtime falls back to the SDL
copy; errors in the stage disable it for the run with a message on stderr.
The MetalFX framework is weak-linked; macOS 13 is needed for the scalers.

Optional supersampling and host texture filters are set in `video.ini`. The
existing CPU/GPU filter differences have not been shown bit-identical. Point
sampling must preserve color including alpha and depth bit for bit. Linux and
Windows drivers require separate verification; a Mac build does not verify
them. `./build.sh --opengl off` or CMake `-DRECVX_ENABLE_OPENGL=OFF` omits GL;
Metal and D3D11 remain available. Vulkan needs `vulkan/vulkan.h` and `glslc` at
build time (Vulkan SDK, Homebrew `vulkan-headers`/`shaderc`, distribution
packages); without them CMake warns and omits it. `./build.sh --vulkan off` or
`-DRECVX_ENABLE_VULKAN=OFF` omits it explicitly. The Windows crossbuild has no
Vulkan headers by default, so packages built that way contain no Vulkan backend.

## Diagnostics

| Option | Purpose |
|---|---|
| `--in-game` | Skip the main menu |
| `--preview` | Room/model inspector with a free camera |
| `--room`, `--player`, `--motion`, `--frame`, `--inventory` | Inspection |
| `--headless --replay <file> --frames <n>` | Deterministic run without a window |
| `--state <new.json>`, `--screenshot <new.bmp>` | Write state or image |
| `--snapshots <directory>` | Snapshot directory (F5/F9) |
| `--enemy-ai on\|off`, `--enemy-grab on`, `--enemy-attacks on\|off` | Enemy switches (`--enemy-attacks off`: host switch, no enemy/enemy-effect damage, poison, grab/throw or kill on the player; default on) |

Headless runs execute every simulation step and render only the requested last
frame. Output files are never overwritten. Diagnostic positioning does not
prove a natural playthrough. Replays start with `0 new_game`.

### Render warnings (host diagnostic, not original behaviour)

When a custom effect entry references a texture that is missing or a frame
outside its atlas, the runtime omits exactly that draw entry instead of ending
the game. Only this pre-checked texture error (`EffectTextureError`) is caught;
simulation errors still stop the runtime. The simulation, the effect work and
the blend state that the entry would have left are unchanged.

- A small red `!` appears in the top-right corner and stays until the program
  ends, also across room changes.
- Each distinct warning is printed once to stderr and appended to
  `runtime-warnings.log` in the user-data directory
  (`<unix seconds> frame=<n> <room>: <detail>`), naming effect id, texture
  category, frame and atlas size.

The original game has no such fallback. A warning is a port defect to fix, not
an accepted state.
