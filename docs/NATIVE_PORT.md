# Native port 0.5

This is an in-progress Windows/macOS port of the local PS2 decompilation. It runs
host C/C++ code and reads loose cooked assets. It does not execute an emulated
PS2 CPU, open the ISO, or open AFS containers at gameplay runtime. It is not yet
a complete playable game. The original sources remain available for tracing
native behavior back to the game.

## Build and package

Use Python 3.12+, CMake and a C/C++ compiler. macOS needs Apple Command Line Tools;
Windows needs Visual Studio C++ (CMake can select the generator). Cross-building
Windows from macOS/Linux also needs Ninja. The initial dependency download needs
network access; subsequent builds can use the cached dependencies.

```sh
./build.sh --platform macos --verify-data --package
./build.sh --platform windows --verify-data --package
```

```powershell
./build.ps1 --platform windows --verify-data --package
```

`tools/build_native.py` configures, builds and runs unit tests on the host. When
`data/native-data.version` exists, it also runs the real-data opening regression.
Windows cross-builds report that tests must run separately on Windows or Wine.
`--no-test` explicitly skips host tests. `--data <path>` selects another complete
cooked tree. `--output <new-directory>` changes the package destination; package
creation refuses to overwrite a previous release.

macOS outputs a universal arm64/x86_64 binary (deployment target 11.0). Windows
outputs x64. SDL2 2.32.10 and the cross-build Zig 0.16.0 are pinned by checksums.
The app includes SDL2 and the full `data/` tree. Local ad-hoc signing is used on
macOS; this is not an Apple-notarized release. Windows hardware testing remains
necessary even when Wine checks pass.

Direct development commands:

```sh
cmake -S . -B build/native -DRECVX_FETCH_SDL2=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel 8
ctest --test-dir build/native --output-on-failure
build/native/recvx-game-tests data
build/native/recvx-runtime --data data
```

## Data and history

`data/` is the productive complete export. It contains all dumped files, separately
split AFS entries, 205 expanded rooms, decoded textures and 18 native film packets.
`asset-manifest.json` contains file sizes and SHA256. Validate it with:

```sh
python3 tools/prepare_native_data.py --output data --verify
```

`archived/` holds the original ISO, source extraction, obsolete PS2 toolchain,
old binaries, superseded exports, historical research and previous `.git`.
`archived/relocations.json` records moves. No normal build, packaging or launch
command depends on it. The new Git history excludes `archived/`, `data/` and
`build/`. Preserve local `data/` when transferring the project to another machine.
A source-only clone does not contain the game assets.

## Architecture

- `recvx_data`: bounded binary input, room expansion and archive/export tools.
- `recvx_math`: original selected Ninja math plus native matrices.
- `recvx_scene`: original model/texture decoding, motion/skin and CPU rendering.
- `recvx_game`: original item-combination C, inventory, messages, event VM,
  first-room actor/camera/light bindings, ADX mixer and native movie playback.
- `recvx_input`: configurable keyboard/SDL GameController actions and shared
  cinematic/dialogue/inventory routing; see [CONTROLS.md](CONTROLS.md).
- `recvx-runtime`: SDL window/input/audio and fixed 60 Hz simulation.

The GUI and deterministic headless replays use the same movement and event path.
Headless rendering draws only the final requested frame; every simulation tick
still executes. Menus, dialogue, hidden/script-controlled actors and original
cinematic flags gate movement and inventory access.

## Verification

Nine CTest groups cover compressed input, archives/rooms, Ninja math/matrices,
scene rendering, player motion/skin, inventory/events, audio/movies and
keyboard/controller input. Real-data
`recvx-game-tests data` uses the original RM_0000 scripts to skip the intro,
acknowledge the in-cell hint, equip the lighter, satisfy its real floor trigger,
run the Rodrigo event and assert the original return position/control state. It
also checks a wall and the original empty-slot atlas pixel. It does not teleport
Claire or dispatch the lighter event directly.
`recvx-input-tests data` checks the same opening with SDL's virtual GameController,
including remapping, menu repeats, focus changes and removal while moving.

```sh
build/native/recvx-runtime --data data --headless --replay tests/native/replays/lighter.txt --frames 3000 --state build/native/lighter-state.json
build/native/recvx-runtime --data data --headless --replay tests/native/replays/lighter-walk.txt --frames 3121 --state build/native/walk-state.json
```

`tests/native/replays/opening-skip.txt` uses the game's skip flag. An unskipped
15,000-tick run also reaches the cell hint after the original opening film.
Unit success and zero unknown-opcode errors do not mean all opcode effects are
implemented: known unbound commands are recorded as pending, as described in
NATIVE_GAMEPLAY.md. Asset manifests establish completeness of copied files,
not completeness of the ported game logic.

Detailed local results: [VALIDATION.md](VALIDATION.md).

## Diagnostics

`--preview` selects the room/model inspector. `--room`, `--player`, `--motion`,
`--frame`, `--inventory`, camera/wireframe options and replay `position` are
inspection controls. Diagnostic teleports are not evidence of authentic event
progression. `--screenshot <new.bmp>` and `--state <new.json>` write review artifacts.
Output files are never overwritten. CI builds host binaries and runs synthetic
unit tests without private disc assets; local real-data regressions run separately.
