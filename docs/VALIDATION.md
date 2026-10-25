# Verification

Verification demonstrates specific original mechanisms and their dependencies.
A passing test, screenshot, or replay does not prove a complete port or full
playthrough. The porting verification method is in [QUALITY.md](../QUALITY.md).

There are three levels:

1. **CTest groups:** unit tests and tests against original data.
2. **Verification scripts** `tools/verify_*.py`: actual runtime, headless
   replays with original rooms and scripts.
3. **Room scan:** starts every room and reports load and script errors.

## CTest

```sh
./build.sh --test
```

```sh
# macOS: ./build.sh creates build/macos-arm64 (Linux/CI uses build/native)
ctest --test-dir build/macos-arm64 --output-on-failure
```

A focused run builds test targets and runs only matching names (an empty
selection is an error):

```sh
./build.sh --test-regex '^(gpu_raster.*|scene_renderer|texture_filtering|third_person_camera_mod)$'
```

`gpu_raster_parity` checks the platform backend, `gpu_raster_opengl_parity`
checks explicit GL, `gpu_raster_opengl_copy_parity` checks its forced
GL 4.1 copy path, `gpu_raster_vulkan_parity` checks Vulkan (including a
second instance on the shared device), and on Windows `gpu_raster_d3d12_parity`
and `gpu_raster_d3d12_warp_parity` (`RECVX_D3D12_DRIVER=warp`) check the
Direct3D 12 backend on the driver and on WARP. With point sampling, they compare packed color including
alpha and floating-point depth bit for bit against the reference, covering
shadow phases, SSAA, NPOT/mutated textures, and context changes. Every
expected GPU pass must actually finish on the GPU; a software fallback is
not PASS. Without a device/display, they exit 77 (SKIP); shader failures are
failures, not skips. A hidden GL context still needs a display server. GPU
tests in a sandbox without a display prove neither shader compilation nor
image parity.

`recvx-gpu-compare` accepts `--backend opengl|vulkan|d3d12` at the end, then optionally
`--force-copy`; point differences or software fallback produce failure.
Filtered host quality is evaluated separately, without assuming CPU bit
identity. Measurements include CPU and wall time; no real GPU run means no
performance evidence.

Tests with original data expect `data/`. Do not run tests sharing fixtures
(e.g. snapshots) in parallel across build directories.

| Area | Groups |
|---|---|
| Data and foundations | `expand`, `archive_room`, `math`, `matrix`, `texture_filtering`, `hd_texture_content_key` |
| Rendering | `scene_renderer`, `original_chunk_blend`, `original_view_clip`, `original_face_geometry`, `active_camera_return` |
| Player and movement | `player_motion_skin`, `wall_collision`, `player_push`, `core_continuity`, `script_motion` |
| Combat and enemies | `combat_weapons_ammunition`, `ammunition_pickup_zero`, `enemy_damage_original_en01`, `effect_collision_debris`, `effect_pool_lifetime` |
| Events and progression | `native-gameplay-bindings`, `event_lifecycle_control_release`, `engine_progression_resources`, `door_transition`, `gameover_original`, `computer_terminal` |
| Inventory and items | `inventory_events_assets`, `inventory_combine_inspect`, `original_inventory_table_coverage`, `original_file_reader`, `status_condition_pulse` |
| Audio and movies | `audio_movie`, `bgm_room_transition` |
| Input, menus, host | `keyboard_controller_config`, `main_menu_settings_video_skip`, `debug_menu_hotkey_registry`, `snapshot_savefile_roundtrip`, `snapshot_room_sound`, `third_person_camera_mod`, `third_person_aim_camera` (headless: aiming/shooting keep the host chase camera, event camera/cinematic/stairs hand it back) |

[INVENTORY-COVERAGE.md](INVENTORY-COVERAGE.md) is generated with original data:

```sh
build/macos-arm64/recvx-inventory-engine-tests data docs/INVENTORY-COVERAGE.md
```

## Verification scripts

Scripts take `--runtime <binary>` and `--data <directory>`; some also take
`--output <directory>`. They run headlessly. Diagnostic setups (position,
snapshot) are labeled; room scripts and resources remain unchanged.

| Script | Checks |
|---|---|
| `verify_frontend.py` | Main menu, movie skip, lighter flow |
| `verify_events.py` | Graveyard/courtyard tasks and control handoff |
| `verify_event_state.py` | VSync time units with menu and debug pause |
| `verify_event_freeze.py` | Stairs, trap event, and door after release |
| `verify_progression.py` | Metal detector and box scripts |
| `verify_detector.py` | Alarm, switch question, metal-free passage |
| `verify_scanner.py` | 3D scanner terminal and lid animation |
| `verify_continuity.py` | Security room, stairs, return paths |
| `verify_doors.py` | Door routes, skip, inventory preservation |
| `verify_room_spawn.py` | Order during room loading |
| `verify_bgm_rooms.py` | Music on exit, normal and skipped doors |
| `verify_audio_state.py` | Event_init → RoomSoundCase → sound bank |
| `verify_combat.py` | Knife pickup, equipment, combat |
| `verify_enemy_ai.py` | Zombie pursuit, grab/bite, stick wiggle, death → Game Over (RM_0080) |
| `verify_ammunition.py` | Ammunition pickup with an empty handgun |
| `verify_pickups.py` | Map and wide-weapon pickups |
| `verify_pickup_slide.py` | Pickup question ignores input while sliding in |
| `verify_item_catalog.py` | Manual → `EraseItem` → ammunition, debug files |
| `verify_file_pickup.py` | Scripted file pickup and reading completion |
| `verify_file_folder.py` | File folder through FILE and the manual |
| `verify_push.py` | Crate pushing |
| `verify_crate_edge.py` | Crate edges and descent |
| `verify_gameover.py` | `GameOverSet` and Retry |
| `verify_snapshot.py` | Snapshot save and load in a new runtime |
| `verify_debug.py` | Debug menu and hotkeys |
| `verify_rooms.py` | Room scan (below) |

Example:

```sh
python3 tools/verify_frontend.py --runtime build/macos-arm64/recvx-runtime --data data
```

## Room scan

```sh
python3 tools/verify_rooms.py --runtime build/macos-arm64/recvx-runtime --data data
```

Starts all 205 rooms without input. Load errors fail the check; blocked event
commands are reported as progress information.

## Other checks

- **Sanitizers:** separate `build/native-sanitize` directory with ASan/UBSan
  (no LeakSanitizer on macOS).
- **Windows:** cross-build under `build/windows-x64`; Wine tests with
  `WINEPREFIX=build/wine-native`. These do not test physical Windows hardware
  or controllers.
- **Packages:** run replays against package runtime and data. User INIs and
  snapshots must remain unchanged.

Logs and images from individual checks are local under `build/validation/`.

## Decision on undefined original behavior

- **ATR cylinder cap with downward lines** (user decision, 2026-10-11):
  `bhCollisionCheckLineMain` (`hitchkl.c:350–365`, 0x26A418..0x26A4CC)
  writes one cap point per loop iteration and immediately tests an octagon.
  Iterations 0..6 therefore still contain the last side face (area[1..3])
  and unwritten stack values (area[4..7]). Iteration 0 cannot hit because
  area[0] == area[1] gives a zero normal. The chosen rule is that unwritten
  points never enclose the intersection. Only the complete octagon in
  iteration 7 decides; then `p2->y = area[0].y`. This matches the original
  wherever its result is independent of stack content. It can differ only
  for a point outside the octagon where stack content could cause a phantom
  hit. `native_line_reflection` checks center, inside/outside, and corner.
  Previously any such query aborted the runtime, e.g. the EN07 sightline
  `bhEne07_SearchPlayer` in RM_2010.

## Live muzzle-flash/0x49 effects (2026-10-11)

Details: `archived/work/native_port_live_effects.result.md`. Build
`build/live-effects-20261011`, policy PASS, 13 CTest groups, `verify_combat`
(build and package), pixel acceptance at frames 3502/3562 versus a control
build with only flash draw disabled. arm64 package SHA256 a0adeb29…1a9c,
signed. Windows cross-built, not executed.

## Shared effect pool / ascending pass (evening 2026-10-11)

Details: `archived/work/native_port_live_effects.result.md`, addendum. Sprites,
muzzle flashes, particle roots/auxiliary work, and blood pools share one
`eff[512]` (`effsub3.c:67/92/145`). The effect pass ascends and commits
spawn output after each procedure (`effect.c:742`). Tick gate:
`(sp_flg & 8) || (ef_flg & 1)` (`effect.c:750`). Draw gate:
`pt_flg & 0x10` (`game.c:339`) plus mirror condition
`gm_flg & 0x4000 && mdflg & 0x40` (`effect.c:1301`). Command 0x49 resolves
the recipient joint before sound. 62/62 CTest sequentially;
`verify_combat --runtime-only` against build and installed runtime; pixel
acceptance at 3502/3562, 570 pixels each in (163,244)–(213,266) with
identical state, reproduced through `Start.command`. A new productive
regression (RM_0090 `bhEff125→126` in one pass) was red against the saved
previous state. `native_lighter_effect` was not weakened: three `bhEff182`
children now draw in the same frame; the test finds the flame through its
atlas entry and passes in both states. Delivered starter arm64 SHA256
f77140f4…fa23, signed/verified; backup
`archived/releases/live-effects-shared-pool-before-20261011-203905`
(c9a8de35…b190). Windows x64 cross-built (SHA256 d86a54fc…1917),
`windows_link_audit` passed, not executed. Open: no native writer for
`ef_flg`/`pt_flg`; no effect procedure sets `mdflg`; particle producers
were reachable only through the quarantined en01 core (no productive blood
spawn checked); `CallPlayerWeaponSeEx` slot/position remained a reported gap.

## Enemy attacks: dogs and player motion during grabs (evening 2026-10-12)

Details: addendum to `archived/work/native_port_enemy_ai.result.md`. Build
`build/enemy-ai-claude-20261011`, policy PASS. `tools/verify_enemy_ai.py`
PASS (`build/validation/enemy-ai-20261012/verify-final`): natural zombies in
RM_0080, RM_0060/RM_0031/RM_3070; natural dogs in RM_1000
(grab/release/Game Over); trap in RM_0050 via diagnostic snapshot. 76/77
CTest serially (red: `shadow_modifier_pixels`, concurrent untracked shadow
work, identical baseline). Installed starter binary arm64 SHA256
44bb6154…1743, signed/verified; smoke via `Start.command` (headless and
standard SDL dummy). Backup
`archived/releases/enemy-ai-dogs-before-20261012-204915`. Windows x64
cross-built (64db1186…0c06), build-directory policy PASS, not executed.
Logs: `build/validation/enemy-ai-20261012/dogs`.

## Texture alpha domain, transparency plan step 1 (night 2026-10-13)

One host texture-alpha domain (0..255, factor alpha/255 = original As/128):
decoder without `Ps2AlphaIsHalf` halving, `write_tga` without ×2
(lossless export/import), renderer with one divisor for every blend mode, and
removal of four handwritten ×2 doublings in UI/computer/world. Details:
`archived/work/native_port_alpha_domain.result.md`. CTest 79/81; red:
`script_motion` (pre-existing, confirmed with the 22:00 binary in
`build/mapinput-claude-20261013` with identical message) and
`original_map_render` (new incomplete parallel map work: 11 fills instead of
1, zero objects, zero light; none on the changed path). New
`chunk_blend_test` case (TGA alpha 0x80 → half blend) with a control copy of
the renderer using the old divisor. `verify_detector`, `verify_lighter`,
`verify_scanner`, `verify_computer`, `verify_frontend`, `verify_progression`,
`verify_pickups`, `verify_events`, `verify_doors`, `verify_stairs`,
`verify_combat`, `verify_snapshot`, `verify_continuity`, and `verify_fog`
PASS. Room scan 205/205 loaded with zero blocked; policy PASS. Dev app updated
through `tools/build_native.py` (`build/macos-arm64`, arm64 only, signed and
verified, SHA256 a55478ed…23d6). Smoke with app options including `--mods`
(HD pack) through SDL dummy: 30 presents, no error. Image comparison RM_0090
camera 1/2 with and without HD pack: same change (776 and 242 pixels with
Delta ≥ 8). Backup `archived/releases/alpha-domain-before-20261013-224118`;
the dev app retains its prior version under
`build/start/macos-arm64/.previous-*`. This build included concurrent
map/shadow/fog work. No package, commit, or push.

## Original engine blend state, transparency plan step 2 (2026-10-13)

`njColorBlendingMode`/`njColorBlendingModeSys` as a state machine in
`native/chunk_blend.cpp` (nj and GS save state separate; indices 0/1 write
0x01/0x44 without table access; a pair with index < 2 leaves the register
unchanged; invalid pairs report `blend_unavailable` in `--state` instead of
aborting). Bound to effect-sprite, particle, and muzzle-flash paths; ID
allowlist and `ninja_blend_register` removed. Details:
`archived/work/native_port_blend_state.result.md`. CTest 79/81 (red:
pre-existing `script_motion`, concurrent `original_map_render`).
`verify_lighter`, `verify_combat`, `verify_detector`, `verify_scanner`,
`verify_doors`, `verify_continuity`, and `verify_events` PASS;
`verify_enemy_ai` was red identically against the previous binary
(RM_0080 “no bite,” another session). RM_0090 camera 1/2 and the lighter
scene matched the 22:41 runtime pixel for pixel (0/307200 changed, delta
0). The mechanism was replaced; visible effect gaps remained in missing
effect procedures (category and pair). No package, dev app, or commit.

## Effect script commands: blood pool 0x56/0x58, dev delivery step 2 (2026-10-13)

The dev app in `build/start/macos-arm64` was replaced with step 2 and signed
(hash 2766db10…, backup
`archived/releases/blend-state-before-20261013-233239` = a55478ed…). Runtime
suites against that exact binary passed; `blend_unavailable` was empty in
eight real rooms. CTest 80/82 (red: pre-existing `script_motion` and concurrent
`original_map_render`; `verify_map` failed identically with the prior binary).
Command 0x56 `bhEffBloodPoolSet` was then bound (length 6), and 0x58
`bhEffBloodPoolSet2` changed from “effect spawn pending” to the original
binding (`native/world.cpp`, tables `evnt_BldTbl`/`evnt_BldTbl2` with
field-identical `RECVX_ENEMY_BP_WORK`). The pool mechanism itself
(`effsub3.c:145/234`) was already ported. Evidence:
`tests/native/gameplay_bindings_test.cpp` (binding true, `blood_particles`
+1 per command, `epw->djnt_no`) and RM_3140: three open bindings including
0x58 before, two afterward. A scan (`tools/scan_event_rooms.py`, 205/205)
still found unimplemented 0x2c `bhEffectSet`, 0x4e `bhEffBloodSet`, 0x70/0x71
`bhEffPush/bhEffPop`, 0x96/0x97/0x98, 0xaa, 0xb0, 0xc3, and 0xc8. No effect
script command ran in the scanner replay; RM_0090 milling effects belonged
to the terminal task/room object, not `sys->ef_*`. Command 0x4e
`bhEffBloodSet` (length 14) was also bound: `RoomWorld::script_blood_set`
writes `epw->djnt_no` and calls the ported `zonzon.c:466 bhEne_SetBlood2`
core. The T3 service and world pointer bind for that call
(`EnemyExtScope` as in `control_enemy_works`); otherwise the library reports
“[zonzon.c:493 effect.c bhSetEffectTb]” as a gap (measured). CTest remained
80/82. Details: `archived/work/native_port_effect_script.result.md`.

Remaining effect script surface at that time: 0x2c `bhEffectSet` allocates
the first free slot in shared `eff[512]` (`RoomWorld::allocate_effect`) and
names a missing procedure instead of aborting the event task; 0x70/0x71
`bhEffPush/bhEffPop` save or replace the whole set (`sprites_` plus particle
payload, `EffectParticles::save/restore`) as in `bhPushEffectWork`/
`bhPopEffectWork`. Evidence: `tests/native/gameplay_bindings_test.cpp`. Dev
app replaced/signed (70cf7f60…); all 12 verification suites against this
exact binary passed. CTest was red only for `script_motion` (pre-existing,
other owner) and `engine_progression_resources` (ItemBox list scroll in a
parallel box session). A 23:57 dev build included that incomplete work and
made `verify_progression` red; controls with and without these commands
passed once adjacent files advanced. No package or commit.
