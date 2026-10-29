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
- Android: JDK 17/21, Gradle, Android SDK with NDK r28; see
  [Android](#android-arm64-v8a-api-26).
- Web: Docker alone (image builds itself), or the pinned Emscripten 6.0.13
  (`./build.sh --platform web` installs it); see [Web](WEB.md).
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
| `--install` | Install on the booted simulator, or with `--ios-device <id>` on a device (`xcrun devicectl`; needs a signed build) |
| `--package` | Device only: write `build/packages/ios-arm64-v<version>/RECVX-Native.ipa` (signed, with `--ios-profile`) or `RECVX-Native-unsigned.ipa` (without) |

Status (2026-10-26): the device build runs on an iPhone with iOS 27 and a
Bluetooth game controller as it is; verified by the project owner on hardware.

**Unsigned IPA and sideloading.** The IPA is `Payload/recvx-runtime.app`
zipped with `ditto` (symlinks and permissions preserved). An unsigned IPA
cannot be installed directly; it is signed with a certificate bound to an
Apple ID first:

- AltStore / SideStore / Sideloadly: import the IPA, the tool signs it with the
  user's Apple ID and installs it. Free Apple ID: 7-day certificate, 3 active
  apps, refreshed by the tool (SideStore refreshes on-device); paid Apple
  Developer Program: one-year certificate.
- zsign: `zsign -k <cert.p12> -p <password> -m <profile.mobileprovision>
  -o RECVX-Native.ipa RECVX-Native-unsigned.ipa`; install the result with
  Xcode (Devices and Simulators), Apple Configurator or `xcrun devicectl device
  install app --device <id> RECVX-Native.ipa`. The profile must cover the
  bundle identifier. A free developer certificate expires after 7 days (re-sign
  and reinstall; `Documents` with saves and data survives a reinstall of the
  same bundle identifier), a paid one after a year.
- `--ios-profile` in the build does the same signing on the Mac with the
  keychain identity of the profile and writes the signed IPA.

The signature is the only difference between the routes; the app, its data
layout and the `UIFileSharingEnabled`/`LSSupportsOpeningDocumentsInPlace` file
sharing are identical.

### tvOS (Apple TV, arm64 device and simulator)

tvOS shares the iOS build path (`--platform tvos` / `tvos-simulator`, build
directories `build/tvos-device` and `build/tvos-simulator`, same options
`--ios-profile`, `--ios-bundle-id`, `--install`, `--ios-device`, `--package`
for the IPA under `build/packages/tvos-arm64-v<version>`). Differences:

- Game data is always bundled (`--ios-bundle-data` is implied) from the
  `tvos` profile in `tools/data_profiles.py`: `data_compressed/` (created on
  first use), no `files/MOVIE` and `archives/RDX_LNK`, mods baked in. tvOS apps
  have no Documents folder and no file sharing.
- User data (INIs, snapshots, saves) goes to `Library/Caches/RECVX` in the app
  container (`native/user_paths.cpp`): Application Support is limited to
  500 KB on tvOS and Caches may be purged by the system, so save games are not
  durable there. A durable store (iCloud key-value or CloudKit) is not ported.
- Input is a Bluetooth game controller (`GCSupportedGameControllers`:
  ExtendedGamepad; the Info.plist marks controller interaction as supported).
  The Siri Remote is not bound; SDL maps its menu button to the app-switch
  gesture. There is no keyboard or mouse path.
- Metal is the only rasterizer; the MetalFX presentation stage is macOS-only
  (`CMakeLists.txt`: `APPLE AND NOT IOS AND NOT TVOS`), so tvOS presents
  through the SDL copy path. No CoreMotion framework on tvOS.
- Icon: flat `AppIcon400x240(@2x).png` from `tools/build_app_icons.py`; the
  layered App Store icon needs an Xcode asset catalog and is not produced.
- SDL2 source: the pinned 2.32.10 tree of another build directory is reused
  when present (`RECVX_SDL2_SOURCE` names one explicitly), so the build works
  without network access.

```sh
./build.sh --platform tvos-simulator --install
```

```sh
./build.sh --platform tvos --ios-profile <profile.mobileprovision> --package
```

### Android (arm64-v8a, API 26+)

`--platform android` builds only the runtime as `libmain.so` with the Android
NDK (r28, `c++_shared`) and wraps it with Gradle (AGP 9.4, JDK 17/21) in
`android/` into `build/android-arm64/RECVX-Native.apk`. SDL's own
`SDLActivity` (Java sources from the same pinned SDL 2.32.10 tree, not copied)
loads `libc++_shared`, `libSDL2`, `libmain` and calls `SDL_main`
(`native/runtime.cpp`). The policy audit runs on the unstripped `libmain.so`
with the NDK `llvm-nm` before packaging.

Requirements (macOS host): `brew install openjdk@17 gradle shaderc` and
`brew install --cask android-commandlinetools`, then `sdkmanager "ndk;28.2.13676358"
"platforms;android-36" "build-tools;36.0.0" platform-tools`. The SDK is found
through `ANDROID_HOME`/`ANDROID_SDK_ROOT` or the Homebrew/Android Studio
default locations; the NDK through `ANDROID_NDK_HOME` or the newest r28 in the
SDK; Java through `JAVA_HOME` or Homebrew `openjdk@21`/`openjdk@17`.

- Game data is **not** in the APK. The runtime reads `data/` loose from the
  app-specific external directory, `mods/` next to it (no storage permission):
  `adb push <data>/... /sdcard/Android/data/org.recvx.nativeport/files/data/`.
  The `android` profile in `tools/data_profiles.py` names the unread sources
  (`files/MOVIE`, `archives/RDX_LNK`) that need not be copied.
- User data (INIs, snapshots, saves, `runtime-warnings.log`) lives in the
  internal app storage `files/RECVX` (`native/user_paths.cpp`, reported by the
  runtime from `SDL_AndroidGetInternalStoragePath`); removed on uninstall.
- stdout/stderr of the runtime go to logcat with tag `RECVX`.
- Rendering: the Vulkan raster backend (`native/gpu_raster_vulkan.cpp`) is
  built and is the Android GPU default (`renderer = gpu` or `vulkan`). Headers
  come from the NDK sysroot, SPIR-V from the same host `glslc` as the desktop
  builds (Vulkan SDK or Homebrew `shaderc`; byte-identical shader modules, the
  older NDK `glslc` is deliberately not used), and the system `libvulkan.so` is
  opened at run time. It keeps its hard requirement of ordered
  `VK_EXT_fragment_shader_interlock` plus `VK_KHR_shader_float_controls`;
  devices without them use the software rasterizer instead of an approximated
  blend, and the runtime logs the reason (`Renderer: software (...)`). Most
  Android GPU drivers do not expose the interlock extension, so the software path
  is the common case. `--vulkan off` builds without it. There is no desktop
  OpenGL 4.1 or Metal. Presentation goes through the SDL renderer (OpenGL ES 2).
  Landscape only (`sensorLandscape`).
- Diagnostic command line: `adb shell am start -n
  org.recvx.nativeport/.RecvxActivity --esa args --renderer,software` passes
  runtime arguments (comma-separated); the launcher starts without arguments.
- Input: keyboard and SDL game controllers. On-screen touch controls are not
  implemented.
- The APK is signed with the local debug key (sideload); release signing and
  `--package` are open.

```sh
./build.sh --platform android --install
```

```sh
adb shell am start -n org.recvx.nativeport/.RecvxActivity
```

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

Direct3D 12 (`native/gpu_raster_d3d12.cpp`, Windows, `renderer = d3d12` /
`--renderer d3d12`, debug-menu index 4) is never a platform default either. It
compiles the same HLSL as the D3D11 backend (`native/gpu_raster_hlsl.h`, shader
model 5.1, `D3DCOMPILE_IEEE_STRICTNESS`) and requires
`D3D12_FEATURE_D3D12_OPTIONS.ROVsSupported`; without it the backend is absent
(software path). Each frame is one direct command list: texture uploads through
staging buffers, UAV clears, the three draw phases, mask and composite passes,
the resolve compute pass, a copy into read-back buffers and a fence the caller
waits on. Because D3D12 does not serialise UAV access between draw calls the way
D3D11 does, a UAV barrier follows every draw, matching the Vulkan backend's
fragment barriers; the ROV ordering then holds within each call. The shadow mask
is `R32_UINT` (typed UAV loads guaranteed on every D3D12 device; values 0..255
unchanged). Texture copies are credited to the texture cache only after the
frame executed, so a frame that fails after recording leaves no stale upload.
`d3d12.dll`, `dxgi.dll` and `d3dcompiler_47.dll` are loaded at run time;
`RECVX_D3D12_DRIVER=warp` selects the WARP software adapter and
`RECVX_D3D12_DEBUG=1` enables the debug layer (messages go to the debugger
output). Each instance owns its device. Purpose: a DirectX 12 device and command
stream for later presentation stages (FSR, below). Status: cross-build verified
on macOS; parity on Windows hardware and WARP is still open (`gpu_raster_d3d12_parity`,
`gpu_raster_d3d12_warp_parity`).

### MetalFX presentation stage (macOS)

`native/metalfx_present.h` presents the finished 640×480 frame through the
CAMetalLayer of the SDL window instead of the SDL texture copy. It is a host
display feature, independent of the rasterizer backend: the 640×480 image, the
30 Hz tick, inputs and events are unchanged, screenshots and headless runs read
the same image. Each stage is a row in the settings screen (OPTION), a
debug-menu action and a `video.ini` key:

| `video.ini` key | Values | Stage |
|---|---|---|
| `frame_rate` | `30`, `60`, `120`, `unlimited` | Presentation cadence for every backend (SDL copy path and MetalFX stage): the game tick stays 30 Hz, between two ticks the last drawn frame is presented again, so 60/120 show duplicates (no new motion) unless frame generation supplies the intermediate image. 60/120 keep vsync and are clamped to the display refresh rate divided by 30; `unlimited` turns vsync off and presents continuously (higher GPU load). On the SDL copy path the repeats run on the main thread and a slot already missed is skipped, not caught up; the MetalFX stage repeats on its own thread. Frame generation needs at least two presents per tick and raises 30 to 60 by itself. |
| `upscaler` | `off`, `metalfx-spatial`, `metalfx-temporal`, `fsr4`, `fsr4-int8` | macOS: `MTLFXSpatialScaler` or `MTLFXTemporalScaler` from 640×480 to the 4:3 fit of the drawable (Retina: 2560×1920 for the 1280×960 window). Temporal is limited to the device's scale range (2× on current devices); a spatial pass covers the rest. Windows: AMD FSR 4 with the FP8 or INT8 model through the ffx-api DLLs (see the FSR stage below); the MetalFX values read "(not available)" there and the FSR values on macOS. |
| `frame_generation` | `off`, `metalfx` | `MTLFXFrameInterpolator` (macOS 26 or newer): one intermediate frame per game tick between the previous and the current image. The stage's own presentation thread runs the cadence: intermediate image in the first half of the tick, drawn image in the second half (`frame_rate` 60: two presents, 120: four), each present immediate with vsync through the layer. The main thread only hands over a copy of the frame and never blocks on vsync; a first version presented from the main thread, where four vsync waits per tick plus the draw pass exceeded the 33 ms tick and the game stuttered at 120, and before that `presentDrawable:afterMinimumDuration:` let the display lag behind the tick. The 2D layer (pixels the HUD, text, fades and menus changed after the geometry pass) is handed to the interpolator as its UI texture, so it is overlaid unwarped instead of blended with the moving background; moving actors without own motion vectors can still ghost. Presents are immediate (vsync through the layer); the earlier `presentDrawable:afterMinimumDuration:` scheduling let the display lag behind the tick and the loop stall. Resets on every cut. |
| `sharpness` | `off`, `light`, `medium`, `strong` | Five-tap contrast-adaptive sharpening of the presented image (amount 0.3 / 0.6 / 1.0), independent of the CRT filter. |
| `post_process` | `off`, `crt-light`, `crt-full` | Own compute kernels on the presented image in linear light, also doing the 4:3 letterboxing. Both modes: Gaussian beam profile per game line whose width grows with brightness (so the profile compensation reaches 1 at white and highlights are not clipped), mask compensated only by half, soft knee from 0.75. `crt-light`: wide beam, faint per-pixel channel mask, no bloom. `crt-full`: narrower beam, slot mask (phosphor triads, one to several output pixels per phosphor by output density, every second line group shifted half a triad), halation from a bright pass at quarter resolution with a separable blur, added before beam and mask and fading with the pixel's own brightness. A first version compensated beam and mask fully and added bloom on top, which overexposed bright areas (user report 2026-10-25). Old values `sharpen` and `crt` map to `sharpness = medium` and `crt-light`. |
| `widescreen` | `off`, `on` | Host mod, not original behaviour (`native/widescreen.h`). Like the GameCube (GCDE08 `04488170 3F619368`, BHD_ASPECT_X 1.174 × 0.75) and PS2 (SLUS-20184, projection word 1.0 → 0.75) patches the game keeps drawing 640×480, but every 3D view of the main image projects x with 3/4 of the original aspect (`NaViewScreenMatrix`, renderer, effects, map, inventory models, `pwksub` screen check) and the presentation stretches the frame to 16:9 (SDL logical size 1280×720, MetalFX/FSR fit). Vertical view unchanged (Hor+); the ClipVolume strip test widens with it so geometry at the new edges is kept. HUD, menus and text are stretched as on the consoles; movies stay 4:3 with side bars; in-world render textures (`draw_screen`) keep 4:3. Also `--widescreen` / `--no-widescreen` for one run. |

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

The 2D effect screen path (`RoomWorld::draw_trs2d_effects`, bhDrawTrsEffect2D) splits the rows of each textured
screen quad across worker threads like the renderer's band stage; the per-pixel arithmetic is unchanged and the image is
identical for any worker count (`--render-threads`). Large plates such as bhEff122 in RM_7120 cost about 5 ms per tick
on the main thread before. Headless runs print the draw phases (`Draw: content (compose, ntx2d, trs2d, fnc); setup, gpu,
bands, shadow, lines`) like the windowed exit report.

Optional supersampling and host texture filters are set in `video.ini`. The
existing CPU/GPU filter differences have not been shown bit-identical. Point
sampling must preserve color including alpha and depth bit for bit. Linux and
Windows drivers require separate verification; a Mac build does not verify
them. `./build.sh --opengl off` or CMake `-DRECVX_ENABLE_OPENGL=OFF` omits GL;
Metal and D3D11 remain available. Vulkan needs `vulkan/vulkan.h` and `glslc` at
build time (Vulkan SDK, Homebrew `vulkan-headers`/`shaderc`, distribution
packages); without them CMake warns and omits it. `./build.sh --vulkan off` or
`-DRECVX_ENABLE_VULKAN=OFF` omits it explicitly. Direct3D 12 needs only the
Windows SDK or mingw `d3d12.h`/`dxgi1_4.h` (both cross-build and MSVC have
them); `./build.sh --d3d12 off` or `-DRECVX_ENABLE_D3D12=OFF` omits it. The Windows crossbuild has no
Vulkan headers by default, so packages built that way contain no Vulkan backend.
The loader is opened at run time and is not linked. `RECVX_VULKAN_LIBRARY`
selects one explicit library; otherwise the loader is searched in the app's
`Frameworks` folder, the system search path and, on macOS, Homebrew. MoltenVK is
not bundled by the packaging scripts, so a standalone macOS package needs the
loader/MoltenVK supplied by the user or embedded separately. MoltenVK 1.4.2 is
the validated macOS loader; Windows and Linux hardware are still unverified.

### FSR presentation stage (Windows)

`native/fsr_present_d3d12.cpp` is the Windows counterpart of the MetalFX stage
behind the same `MetalFxPresenter` interface: a Direct3D 12 device of its own, a
DXGI flip-model swap chain (three buffers, tearing for `unlimited` when the
system supports it) on the HWND of the SDL window, and the same presentation
thread model (the main thread hands over a copy of the frame; the thread runs
the `frame_rate` cadence with repeats). Like MetalFX it is independent of the
raster backend. Chain per frame: upload of colour, scene image and 1/z →
`motion_cs` (camera-reprojected motion vectors, inverted finite depth from the
njClipZ near/far, reactive mask = pixels the 2D drawers changed) →
`ffxDispatch` upscale → `present_cs` (sharpen, CRT, 4:3 fit, HLSL port of
`present_k`) into the back buffer → `Present`.

FSR comes exclusively from AMD's signed ffx-api DLLs. The repository vendors the
MIT headers only (`native/third_party/FidelityFX-SDK/include`: `ffx_api.h`,
`ffx_api_types.h`, `ffx_api_loader.h`, `ffx_upscale.h`, `dx12/ffx_api_dx12.h`,
SDK 2.1.0, upscaler API 4.0.3); the DLLs are ignored by Git. At run time the
stage loads `amd_fidelityfx_upscaler_dx12.dll` (FP8 model, or the INT8 build
from the `FSR4 INT8` subfolder for `fsr4-int8`) and then the loader
`amd_fidelityfx_loader_dx12.dll` (SDK 2.x) or `amd_fidelityfx_dx12.dll` (SDK 1.x
name) from `RECVX_FSR_SDK`, the runtime's folder, `<runtime>/FidelityFX-SDK`,
or `native/third_party/FidelityFX-SDK` of the development tree. The loader
resolves the upscaler DLL by name, which is why the chosen model DLL is loaded
by path first; switching FP8 ↔ INT8 therefore takes effect after a restart and
the status line says so. Version selection: `ffxQuery GET_VERSIONS` lists the
providers of the upscaler DLL (FSR 4, FSR 3.1.x, FSR 2.x); the stage creates
the context with the first entry and falls back to the next when
`ffxCreateContext` refuses (FSR 4 needs a GPU/driver with the ML path, e.g.
Radeon RX 9000). The provider in use is reported through
`FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION`.

Context flags: `FFX_UPSCALE_ENABLE_DEPTH_INVERTED` and
`FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE` (the 640×480 image is sRGB).
Dispatch: `jitterOffset` 0 (no subpixel jitter: the 2D layer is drawn at
integer positions), `motionVectorScale` 1 (vectors in render pixels, current →
previous, FSR convention), `renderSize` 640×480, `upscaleSize` = 4:3 fit,
`frameTimeDelta` from the game tick, `preExposure` 1, `reset` on every cut
(`scene_key`), camera near/far/FOV from the pass projection. No exposure,
transparency or composition textures. Debug/warning messages of the DLL go to
stderr. The stage is built with `RECVX_ENABLE_FSR` (`build.sh --fsr off` to
omit); the DLLs present in the development tree are copied next to the runtime
by the build and into Windows packages with `FIDELITYFX-LICENSE.md`.

**Open items (FSR on Windows):**

1. Validation on hardware: the stage is cross-built and the API use follows the
   SDK 2.1.0 headers and the FSR 4 integration guide, but it has not run on a
   Windows GPU yet (image, reset behaviour on cuts, FP8 vs INT8, FSR-3.1
   fallback on non-RDNA4 GPUs, tearing path).
2. Per-object motion vectors: the engine only reprojects the camera, so moving
   characters and effects are treated as static scenery; smearing on fast
   motion is expected until the renderer exports object motion.
3. Subpixel jitter for the 3D pass (FSR's intended TAA input) without shaking
   the integer-positioned 2D layer — needs a split render of 3D and 2D.
4. Frame generation (`amd_fidelityfx_framegeneration_dx12.dll`, FSR 3.1/4
   frame interpolation with its own swap chain): FSR expects ≥ 60 fps input at
   30 Hz game tick, and the 2D layer must go in as a UI composition texture.
   Not offered yet; `frame_generation = metalfx` stays macOS-only.
5. Mipmap bias for the texture filters at render resolution (the guide
   recommends `log2(640/fit) − 1`), only meaningful once 3 exists.

**Vulkan (Linux, macOS over MoltenVK): TODO.** FSR 3.1 supports Vulkan, FSR 4
does not. The SDK's Vulkan path (`ffx_api_vk.h` plus the FSR 3.1 upscaler
sources and SPIR-V) is not part of this checkout; once it is, the same stage
design applies with a Vulkan swap chain on the SDL window and the Vulkan raster
backend's device. OpenGL has no FSR path (no SDK backend, no SDL GL/Vulkan
interop). On macOS, MetalFX remains the supported upscaler.

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
