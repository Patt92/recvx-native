# Web (browser)

The web build runs the native engine in the browser: the same C/C++ runtime as
the desktop and mobile builds, compiled to WebAssembly. It plays in current
**Firefox** and **Chrome/Chromium** and is served by a small nginx container on
port **8722**. There is no emulator, no PS2 code and no translated executable;
the native-port policy check audits `recvx.wasm` like every other runtime.

## Quick start (Docker)

You need prepared game data in `data/` (see [README → Build and run](../README.md#build-and-run);
`--iso` creates it from your disc image). Then:

```sh
python3 tools/web_site.py --index-only      # once per data/ change: writes data/web-files.txt
docker compose --profile web up -d --build web
```

Open **http://localhost:8722/** and click *Click to start*. Stop it with
`docker compose --profile web down`.

- The image (~100 MB) contains only the web runtime and nginx. **Game data is
  never copied into it**: `data/` (and `mods/`, if present) are mounted
  read-only. Other folders: `RECVX_DATA_DIR=/path/to/data RECVX_MODS_DIR=/path/to/mods`;
  another port: `RECVX_WEB_PORT=9000`.
- The image builds the runtime itself from the repository (pinned
  `emscripten/emsdk:6.0.13`), so Docker alone is enough; no local Emscripten.
- Remote access: browsers only allow the required features (shared memory,
  private storage) on `http://localhost` or **HTTPS**. To play from another
  machine, put the container behind an HTTPS reverse proxy that keeps the
  `Cross-Origin-Opener-Policy`/`Cross-Origin-Embedder-Policy` headers.

## Playing

- **Keyboard and game controllers** with the same bindings as on the desktop
  (`docs/CONTROLS.md`): arrows/WASD/IJKL move, Enter confirms, Escape cancels,
  Tab opens the inventory, Space skips videos, P is SELECT (cinematic skip).
  Controllers use the browser Gamepad API (standard mapping, XInput and
  DualShock/DualSense pads). Touch controls do not exist.
- The first click starts game and sound together (browser autoplay rules).
  *Fullscreen* (bottom right) switches to fullscreen; the image keeps 4:3.
- **Saves**: the typewriter save games, snapshots (F5/F9), INIs and
  `runtime-warnings.log` are stored in the browser's private storage (OPFS) for
  this address, per browser. They survive reloads and restarts, are removed when
  the site data is cleared, and are not shared between browsers or machines.
- Game data is loaded on demand in 16 MB blocks over HTTP ranges and cached by
  the browser. The first visit to a room or movie waits for its data; on a local
  network this is a fraction of a second.

## Architecture

- **Toolchain**: Emscripten 6.0.13 (pinned emsdk; `./build.sh --platform web`
  installs it into `build/toolchains/emsdk` on first use), wasm32 with pthreads,
  WebAssembly SIMD and native WebAssembly exceptions. SDL 2.32.10 is the
  Emscripten port of the same pinned version (WebGL 2, WebAudio, Gamepad API).
- **Threads**: the runtime (`recvx_runtime_main`, unchanged) runs on a pthread
  (`native/web_main.cpp`) with its blocking main loop and its raster workers.
  The browser main thread owns SDL: every SDL call of the runtime is linked to
  `native/web_sdl_proxy.cpp` (`-Wl,--wrap`), which runs it synchronously on the
  main thread with the same arguments and results. Only `SDL_WaitEventTimeout`
  waits on the game thread (polling the main thread), so the browser main thread
  never blocks. No game logic is involved.
- **Rendering**: the original-faithful software band stage (the reference of all
  GPU backends) rasterizes on worker threads; the 640×480 image is presented
  through SDL's WebGL 2 renderer. A WebGPU or WebGL rasterizer is **not**
  possible without approximation: the GPU backends need ordered per-pixel
  read-modify-write (fragment shader interlock, rasterizer ordered views or
  framebuffer fetch), and neither WebGPU nor WebGL 2 offers it. It stays an open
  item, not an approximated blend.
- **Files**: `/data` and `/mods` use the WasmFS fetch backend (HTTP range
  requests). It cannot list directories, so `tools/web_site.py` writes
  `web-files.txt` (and `mods-files.txt`) and the runtime creates the tree from it
  before it starts. `web/wasmfs_fetch_fix.js` replaces the block reader of
  Emscripten 6.0.13, which returned zeros past 16 MB when a server omitted
  `Accept-Ranges` on range responses (found with `movies/0007.rvv`; verified
  byte-identical against the file after the fix). `/user` uses the OPFS backend.
- **Server**: `web/nginx.conf` sends cross-origin isolation headers (required
  for shared memory), `Accept-Ranges` on every data response, no directory
  listings, and never compresses game data.

## Building without Docker

```sh
./build.sh --platform web        # build/web/site (+ data/web-files.txt)
```

Serve `build/web/site` with `web/nginx.conf` (or any server with the same
headers and range support) and mount `data/` at `/data/` and `mods/` at `/mods/`.

## Checks

`tests/web` drives real browsers (Chromium and the installed Firefox, through
Playwright) against a running server. Each check starts its own test browser
with its own profile and closes only that one; a running Firefox or Chrome of
the user is never touched.

```sh
cd tests/web && npm install && node run.js            # both browsers, http://127.0.0.1:8722/
node run.js --browser firefox --only walk,load
```

| Check | What it proves |
|---|---|
| `walk` | New game → intro (video and cinematic skip) → free control in RM_0000; walking (motion 0) and running (motion 4) from held keys |
| `audio` | WebAudio context running; non-zero output level from menu sounds and the intro |
| `perf` | 20 s of free play; simulation rate of the runtime's own statistics within 29–30.5 Hz (original 30 Hz) |
| `persist` | F5 snapshot written to OPFS, page reloaded in the same profile, F9 loads it |
| `load_live` | Test save (copy of `saves/save-7.rcvxsav`) in OPFS, LOAD GAME by keyboard, room RM_3110 loads |
| `load` | The same load as `tools/verify_typewriter_save.py` (`--replay`, `--headless`, 700 ticks): room, position, yaw, HP, weapon and inventory equal the native runtime |

`build.sh --platform web` cannot run CTest or prepare data; prepare data with
the host target.

## Open

- WebGPU/WebGL rasterizer (see Architecture); software rasterizing costs about
  15–18 ms per frame on an Apple M4 Pro, inside the 33 ms tick.
- Safari is not a target of this build.
- Touch controls; mobile browsers are not verified.
- Saves are per browser (OPFS); there is no import/export of save files yet.
- Not verified: other machines/GPUs, real game controllers in the browser
  (the Gamepad API path was checked with a synthetic standard gamepad), long
  play sessions beyond the first rooms.
