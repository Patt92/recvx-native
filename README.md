# RECVX Native

A native port of *Resident Evil CODE: Veronica X* (PS2) to **macOS arm64** and
**Windows x64**, built on top of the decompilation published at
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).

The goal is a self-contained native game with original behaviour: every
mechanism is implemented on the existing native owners. Original sources are a
semantics reference only; the original engine, generated function bodies and
any PS2/VU/GS emulation are off limits.

> **Status (20 October 2026): active port, not fully playable end to end.**
> The policy checker passes on the current tree and the full native suite is
> green (190/190). Rooms, events, enemies, items, effects and audio run on the
> native runtime; the Metal GPU rasteriser is the default macOS renderer with a
> software fallback. Enemy families EN02, EN06, EN09, EN19, EN21, EN22, EN26
> and EN27 are integrated and checked in native runtime or event runs. The Windows runtime
> and several enemy/effect branches remain open. A shared acceptance run of the
> current tree and a new starter package are not yet evidenced. The table below
> measures documented mechanisms only, never game completion.

## What sets this port apart

- **A native game, not a recompilation.** The native engine builds a standalone
  binary for each platform that loads rooms, runs events, animates models and
  mixes audio itself. The original engine is
  never compiled, linked or called, and no PS2 CPU, VU or GS hardware is
  reproduced.
- **Hand-written implementations, line by line.** Every ported mechanism cites
  the original source (`file.c:line`). The earlier machine-translation pipeline
  was removed together with its output; its semantic defects (random-number
  order, offset field order) are gone.
- **No approximation layer.** No heuristics, placeholders, room-specific fixes
  or reimagined logic — not even as a stopgap. Missing behaviour stays a named,
  visible porting gap.
- **Evidence instead of claims.** `tools/port_check.py` derives the scope below
  from a per-function ledger, `tools/check_no_original_core.py` gates every
  build, and 190 CTest cases, headless room sweeps, replay harnesses and
  differential runs back the mechanisms that are done.
- **Measured renderer work.** The Metal rasteriser reproduces the software path
  bit for bit under point sampling, shadows included, at about 0.3–0.5 ms CPU
  per room frame instead of 62–73 ms; `--renderer software` keeps the reference
  path available.
- **Original data, native I/O.** RDT rooms, models, textures, animations,
  messages, ADX music and voice, Sony sound banks and all 18 movies are read
  directly — no ISO or AFS access at runtime. An optional build step
  (`./build.sh --iso game.iso`) extracts a PS2 ISO9660 image into the loose
  `data/` tree; the runtime still opens only loose files. Drop-in HD textures
  and mods are supported.
- **Two platforms, one core.** macOS and Windows share the native engine; only
  the host layer is platform-specific.
- **Host conveniences kept honest.** Door skip, cinematic skip, options,
  controller mapping, snapshots and the debug menu live in the host layer with
  documented deviations; they never change original game semantics.

## Native components

- **Original data, native:** rooms, models, textures, animations, messages, ADX
  music and voice, Sony sound banks and all 18 movies — no ISO or AFS access at
  runtime.
- **Event VM:** the original scenario interpreter with tasks, conditions,
  cameras, dialogs and door requests. All seven D06 inventory commands are wired
  into the production event path, including script gifts and key consumption.
  Other event effects remain incomplete.
- **Inventory and status:** pickup prompt, sorting, equipment, inventory-side
  reload and ammo counting, combinations, item use, box transfer, quantity
  display, command window and health/poison ECG. This is not evidence of a
  complete playthrough.
- **Examination and files:** original action and transformation flows,
  visibility masks, offsets, camera distances and item-view scaling; file
  pickup, reader and folder with **24 files / 167 pages**.
- **Actor, rendering and audio:** native model, texture, skin/pose and drawing
  foundations, a standalone face/lip port, math and weapon flashes; BGM
  de-clicking on both room-load paths. Many effect procedures are hand-ported;
  further effect, actor, shadow and sound paths remain open.
- **Renderer:** native software rasteriser and a Metal backend with a parity
  test, selectable at runtime (`--renderer gpu|software`).

## Porting status

Progress is measured by actual packages and verification boundaries, not by
historical generator output. **Ported** describes the named area, not the whole
game or every dependent flow.

The percentages below are taken from the [port status](docs/PORT_STATUS.md).
That report computes them with `tools/port_check.py` from the
[function ledger](docs/port_ledger.tsv) and the event commands bound in the
native event path. They describe only the explicitly evidenced scope, never
overall game progress. **Weighted** counts partially evidenced mechanisms as
0.5; **fully evidenced** counts only completed ones.

<!-- port-status:begin -->
<!-- Progress table and figures synced directly from docs/PORT_STATUS.md. -->

Explicit native evidence scope: **88.1 %** weighted, **76.8 %** fully evidenced (499/650 mechanisms; not overall game completion).

Event commands (bhScenarioJmpT): 211/223 bound, 12 consumed only, 0 missing.

| Area | Mechanisms | Evidence scope | Fully evidenced | Partial | Open |
|---|---:|---:|---:|---:|---:|
| Enemies/AI | 72/103 | 85.0 % | 69.9 % | 31 | 0 |
| Effects | 220/250 | 93.8 % | 88.0 % | 29 | 1 |
| Events/script | 63/79 | 89.9 % | 79.7 % | 16 | 0 |
| Player/combat | 44/62 | 85.5 % | 71.0 % | 18 | 0 |
| Room/camera/door | 14/35 | 70.0 % | 40.0 % | 21 | 0 |
| Menu/items/text | 43/54 | 87.0 % | 79.6 % | 8 | 3 |
| Models/animation | 10/15 | 83.3 % | 66.7 % | 5 | 0 |
| Audio | 5/21 | 61.9 % | 23.8 % | 16 | 0 |
| Flow/saving | 28/31 | 95.2 % | 90.3 % | 3 | 0 |
| Platform (host-replaced) | 16/31 | 75.8 % | 51.6 % | 15 | 0 |

Documented evidence scope only, not overall game completion.
<!-- port-status:end -->

<!-- native-progress:begin -->

| Area | State | Evidence / remaining limit |
|---|---|---|
| Policy / build | Source policy PASS; shared acceptance open | The current tree passes `check_no_original_core.py`; the full CTest suite is 190/190 (2026-10-20); the development app was refreshed through `build.sh`. No new packaged release is evidenced. |
| Inventory / box / status (D01/D02/D06–D09) | Ported, partial flows verified | Pickup, use, combination, transfer, item commands and ECG have positive partial evidence. A complete playthrough is not evidenced. |
| Examination / files / map (D03/D10) | Ported, runtime parts verified | Item view, file flows, and MAP geometry, markers and sprites were verified in dedicated build runtimes; no current starter/package release yet. |
| Player / input / combat (A/B) | Partially ported | Movement, collision, push, stairs and target selection have their own evidence; weapon numbers, HandChange, light clipping, effect 199 and the optional third-person camera were added most recently. General player states and further weapons remain open. |
| Enemies / actors (C/F) | Several families active; full port open | EN02 Gulp Worm, EN06, EN09 Bandersnatch, EN19 Tyrant, EN21, EN22, EN26 and EN27 are integrated and checked in native runtime or event runs. EN05, EN12, EN13/13B, EN23, EN56 and further services remain open. |
| Effects / light / audio (E) | Many hand ports; full paths open | The regex translator was removed and the procedures are hand-written, including `bhEff006` and the original empty procedures 4/28/129/174/197. `bhEff_Sub367` stays a documented gap: the original has no producer for its `exp0` helper or `op->func`. Remaining IDs, particle/model paths, light/sound services and rendering limits remain open. |
| Shadow / camera / doors (F/G/P) | Partially ported | Door sequences, clip calls, camera offsets and follow targets have targeted evidence. The shadow renderer, the complete camera path and the original ice-room presentation remain open. |
| System / save | Finalized; 3 branches open | Ledger 28/31 (2026-10-20). Closed: both draw passes (mirror effect pass), EOR fog/background colour, main-loop pad gating, room-load handover. The host task machine, pause overlay, pad keytype/analog tables and load-screen `typ_flg` are host-replaced and scored as such. Open: save-name UB, the deliberate door skip, map draw order/pad binding. |
| Windows x64 | No current evidence | Existing Windows binaries come from older builds; the current tree has not been run on Windows. |
| Renderer | GPU default on macOS | Metal backend with the `gpu_raster_parity` test (point sampling, shadows), software fallback, `--renderer` switch. A D3D11 backend remains open. |

<!-- native-progress:end -->

**Verification:** test runs differ per build. As of 20 October 2026 the full
suite is 190/190; `gpu_raster_parity` requires a Metal device and is reported as
skipped where none is available. Earlier build reports are kept with their red
cases and baselines in the individual reports (effect hand port 82/83, EN09
86/86, enemy audit 85/86). None of these numbers is a joint acceptance of the
current tree. The policy source check passes; current runtime/package and
Windows acceptance are missing.

Selected evidence: [EN09/Bandersnatch](work/native_port_EN09_world.result.md),
[enemy audit](work/native_port_enemy_audit_fixes.result.md),
[effect procedures](work/native_port_effsub0_handport.result.md),
[GPU rasteriser](work/performance_gpu_raster_d2_20261020.result.md),
[MAP renderer](work/native_port_D10_map_render.result.md),
[input lock RM_1020](work/native_port_discard_question.result.md).
The generated ledger table above describes documented mechanisms, not game
completeness.

## Quick start

Local builds keep working as long as the mandatory policy check passes. The
launcher scripts also check the policy and need a matching native binary plus
local game data. A refreshed package for the newest changes is not yet
evidenced; do not confuse older packages with the current source and build
state.

- **macOS:** `Start.command`
- **Windows:** `Start.cmd`

Controls, keys and game data: [STARTEN.md](STARTEN.md).

Build it yourself (Python 3.12+, CMake, C/C++ compiler):

```sh
./build.sh --platform macos --verify-data --package
```

```sh
./build.sh --platform windows --verify-data --package
```

Without an extracted disc tree, let the build read a PS2 ISO once and write the
loose `data/` tree itself:

```sh
./build.sh --iso /path/to/game.iso --package
```

`--iso` is repeatable (both discs are merged) and only runs when `data/` is not
yet prepared; the game still opens loose files only.

`--test` also builds and runs the regression suite. Details:
[docs/NATIVE_PORT.md](docs/NATIVE_PORT.md).

Game data is not distributed with the source code; a source-only clone does not
run without a local `data/` folder.

## Principles

- **Original semantics, own native implementation.** Original code, original
  data and disassembly are the reference. No generated function bodies, no
  original engine, no second game world.
- **No substitute logic.** No heuristics, no room special cases, no
  "approximately matching" placeholders. If a mechanism is missing, it stays
  visible as a gap.
- **Platform code stays platform code.** Host adaptations touch interfaces and
  data formats only, never game decisions.

The binding verification method is described in [QUALITY.md](QUALITY.md).

## Documentation

| Page | Content |
|---|---|
| [STARTEN.md](STARTEN.md) | Launch, controls, diagnostics |
| [docs/NATIVE_PORT.md](docs/NATIVE_PORT.md) | Build, packages, architecture |
| [docs/NATIVE_GAMEPLAY.md](docs/NATIVE_GAMEPLAY.md) | Porting state of the game systems and open gaps |
| [docs/NATIVE_PLAYER.md](docs/NATIVE_PLAYER.md) | Player model, skinning, animation |
| [docs/PS2_HARDWARE.md](docs/PS2_HARDWARE.md) | Historical hardware and disassembly reference, not runtime architecture |
| [docs/AUDIO.md](docs/AUDIO.md) | Music, sound banks, movies |
| [docs/NATIVE_ASSETS.md](docs/NATIVE_ASSETS.md) | Assets, mods, HD textures |
| [docs/CONTROLS.md](docs/CONTROLS.md) | Keyboard and controller mapping |
| [docs/DEBUG_MENU.md](docs/DEBUG_MENU.md) | Debug menu |
| [docs/VALIDATION.md](docs/VALIDATION.md) | Test groups and verification tools |
| [docs/INVENTORY-COVERAGE.md](docs/INVENTORY-COVERAGE.md) | Historical item/recipe coverage, not current port evidence |

## Project layout

| Path | Content |
|---|---|
| `native/` | Native engine and host layer |
| `src/`, `include/` | Retained original decompilation |
| `tools/` | Data tools, extraction, build, policy and verification tools |
| `tests/` | Regression tests and replays |
| `docs/` | Documentation |
| `mods/` | Optional drop-in resources |
| `data/`, `build/` | Local game data and build artefacts |
| `archived/FORBIDDEN_DONTUSE/` | Locked, recoverable quarantine; do not read or use |

## License

The original project license is in [LICENSE](LICENSE). Dependency licenses are
shipped with the packages.
