# Work index

This index replaces the chronicle in the mandatory agent files. Not a new
gameplay acceptance: before editing/shipping, reconcile claims from old reports with
current code, working tree and installed runtime.

## Active rules and documentation

- Binding policy: [AGENTS.md](AGENTS.md); verification method: [QUALITY.md](QUALITY.md).
- Verified function evidence: [Ledger](docs/port_ledger.tsv);
  [coverage report](docs/PORT_STATUS.md) is no proof of semantics/playthrough.
- Current functional ported/open state: [Gameplay](docs/NATIVE_GAMEPLAY.md),
  [Player](docs/NATIVE_PLAYER.md), [Audio](docs/AUDIO.md),
  [Assets](docs/NATIVE_ASSETS.md), [Validation](docs/VALIDATION.md).
- `work/` contains only ongoing work (enemy task board `work/enemies/`, briefings,
  inventory contract, performance plan, quarantine manifest). Finished results, applied
  patches and the instruction history are under `archived/work/` since 2026-10-21;
  references in ledger, docs, code and tests have been updated. Since 2026-10-21 `archived/` is
  no longer versioned (local only, `.gitignore`); a fresh clone does not have
  these reports.

## Working tree and delivery

**MetalFX presentation stage (2026-10-25, working tree):** `native/metalfx_present.mm` shows the finished
640x480 image via the CAMetalLayer of the SDL window: MetalFX spatial/temporal upscaler, frame interpolation
(macOS 26+, intermediate frame per game tick), post-processing (sharpen/CRT); three new rows in the settings screen,
debug menu actions and `video.ini` keys (`upscaler`, `frame_generation`, `post_process`, default off).
Motion vectors from camera reprojection of depth, 2D pixels as static foreground, no jitter; moving
actors without their own vectors (documented limit). User finding 2026-10-25 (M4 Pro, macOS 27): stage runs,
frame interpolation dropped step by step after about one minute from 60 to below 30 frames (Metal presentation times
via afterMinimumDuration lagged behind, nextDrawable blocked the loop). Rework: immediate presentation, cadence in
the main loop (`video.ini frame_rate` 30/60/120/unlimited for all backends, tick stays 30 Hz, intermediate frame in
the first half of the tick), debug menu names shortened to the value column. GPU path **not** runnable in the sandbox
(no Metal device), only built and covered by settings tests. User check 2026-10-25 with their own build
(M4 Pro, macOS 27): upscaler, frame interpolation (stable at 60 frames) and post-processing run without issues.
Commit 09b26a7. Afterwards (working tree): sharpening as its own stage (`sharpness` off/light/medium/strong), CRT filter
in two modes (`crt-light` as before, `crt-full` with Gaussian beam profile, slot mask, bloom, brightness compensation,
all in linear light). User finding: full overexposed, ghosting with frame generation, light a good compromise.
Rework: mask only half compensated, beam width at white without clipping, bloom before beam/mask and decaying with its own
brightness, light also gamma-correct (wide beam, weak mask); 2D layer as uiTexture to the
interpolator (overlaid undistorted instead of blended). User finding afterwards: still overexposed/ghosting, plus
stutter at 120 (four VSync waits per tick on the main thread plus draw pass > 33 ms). Rework: presentation
and cadence on the stage's own thread (main thread only hands over an image copy), beam compensation halved.
No Metal compiler in the sandbox: shaders compiled at runtime, errors as "MetalFX presentation disabled:
presentation shader" on stderr; visual check at the window open.

**RM_7120 cameras 2..6 (2026-10-25, working tree):** user finding stutter in camera 4/5. Measured headless (software,
`--ss 1`, 60 frames): camera 1 1.3 ms draw pass, camera 4 6.1 ms, of which 4.8 ms bhDrawTrsEffect2D on the main thread:
the glow plate bhEff122 (texture group 41) fills the whole image in these cameras, the screen path computed every pixel
individually. `draw_trs2d_effects` now distributes the rows across workers like the band stage (same pixel math, image
byte-identical, hash aad435c7): 0.72 ms. User check afterwards: only a slight stutter on close inspection, only
with frame generation; change stays (saves ms). The headless run measures no GPU/MetalFX share of the app; the
lines "Draw:", "GPU:" and "MetalFX presentation:" on app exit are open for the remaining cause. Headless report now shows the draw phases
(content, compose, ntx2d, trs2d, fnc) like the windowed run.
[Docs](docs/NATIVE_PORT.md#metalfx-presentation-stage-macos). **No commit.**

**bhEffTypeSet 0xAD (2026-10-22, working tree):** `world.cpp` writes `op->type = v1` as event.c:7345 without
checking, also on texture holders (id 30..99, bhEffDmy) and unported ids; previously the command stayed
unbound there. Test in `event_lifecycle` (RM_3010 id 70); red with the old condition (counter-check), green with fix
(`event_lifecycle|effect_bg|game`). bhEffBG 93..99 and effect 26 from bhCheckEvtTimer are also in the working tree.
Open from the effect audit: holder release after the first pass (bhEffDmy), 0x98 on holder 66 (RM_3091). **No commit.**

**Android build path (2026-10-27, working tree, started):** `./build.sh --platform android [--install]`:
NDK r28 arm64-v8a API 26, runtime as `libmain.so` (`SDL_main`), SDL shared, Gradle shell `android/`
(SDLActivity from pinned SDL source), APK `build/android-arm64/RECVX-Native.apk` (debug key). Policy PASS on
unstripped libmain.so. Data loose in `Android/data/org.recvx.nativeport/files/data`, user data internal
`files/RECVX`, output to logcat `RECVX`. **Emulator (API 35 arm64, not hardware):** title, keyboard input,
intro FMV, RM_0000 load, pause/resume of the surface observed. **Vulkan:** backend built for Android and GPU
default there (NDK headers, host glslc, SPIR-V byte-identical to the macOS build, libvulkan.so at run time); the
emulator (gfxstream on Apple M4 Pro) lacks fragmentShaderPixelInterlock + float_controls, so it falls back to
software with logged reason. Vulkan raster on a real Android GPU **not** verified (needs an interlock driver). No
touch controls, no release signing/`--package`, no real device/controller/performance check.
[Build docs](docs/NATIVE_PORT.md#android-arm64-v8a-api-26).

**tvOS build path (2026-10-25, working tree):** `./build.sh --platform tvos|tvos-simulator` via the iOS path
(CMAKE_SYSTEM_NAME tvOS, static SDL, Metal, bundle with baked-in compressed data, since tvOS has no
Documents folder). User data in `Library/Caches/RECVX` (not persistent, documented), input via
Bluetooth controller, MetalFX stage stays macOS. Simulator (`build/tvos-simulator`, ad hoc signed, platform 8)
and unsigned device (`build/tvos-device`, platform 3) compile, link, PASS, 3.1 GB data each in the bundle;
SDL source reused from the macOS build (download blocked in the sandbox, `RECVX_SDL2_SOURCE`). CMake sets
no `TVOS` variable for tvOS, CMakeLists.txt adds it. `setMathMode:` now also guarded for tvOS 18.
Start on simulator/device **not** verified: CoreSimulatorService not reachable from the sandbox, no profile.

**iOS build path (2026-10-22, working tree):** `./build.sh --platform ios|ios-simulator` (Ninja/Makefiles,
policy audit unchanged, SDL static, Metal, bundle icons, signing via profile). Simulator and unsigned
device build compile, link, PASS. **Run on the device (2026-10-26, user acceptance):** the iOS version
runs as is on the iPhone (iOS 27) with a Bluetooth controller. `--package` without a profile now writes
`RECVX-Native-unsigned.ipa` for sideloading (AltStore/SideStore, zsign with own certificate: paid 1 year,
free 7 days with re-signing); touch controls open.
[Build docs](docs/NATIVE_PORT.md#ios-arm64-device-and-simulator).

Shared working tree with ongoing work by others; check the current `git status --short`
and the binary state actually launched. Do not adopt blanket approvals
or file ownership assignments from historical hand-overs.

**Crates after elevator ride (working tree/arm64 build):** snapshot 2 RM_7160 reproduced:
the rotated native Event-N/Game-N+1 tick started with Event instead of Game after loading;
the elevator script lowered crates to y=-5 before their first bhObj001 initialization.
Upper ATR floor stayed 0 instead of 1. Now first game pass before the following event tick
(main.c:195-201, game.c:48-51), both runtime loops. Targeted test
`tests/native/lift_crate_floor_check.py --runtime build/macos-arm64/recvx-runtime --data data --snapshot snapshots/snapshot-2.rcvxsnap`:
edge holds at x70.25 instead of walking on air to x44.17; material 3 instead of 0; descent y0.
11 adjacent CTests green. `verify_stairs.py` blocked by stale JSON field
`stairs.active` (KeyError); no overall stairs proof. Evidence `build/validation/crate-floor-root/`.
RM_0110 counter-check: previous shot replay red with first game pass (1 shot,
0 hits), green without it (1/1). Diagnostic shot position after the natural scene
stabilized in `rm0110_event_zombie_check.py`; all 4×362/8×370, hit/death,
scene/attack/re-entry checks kept. Green against both start orders;
12 adjacent CTests and snapshot 2 crate climb up/down green.
Evidence `build/validation/first-pass-rm0110/`. No target-switch/playthrough proof
from the diagnostic shot probe. Not installed in the launcher app; no commit.

**Porting round after the audit (2026-10-21, working tree):** StartVibrationEx(0, 8) on push/turn
(psh/rpsh) bound to the vibration unit, flr_no for Common/Player 7, bhCheckSubTask gate plp->flg 0x6
(status key/script request). Crate climbing (bhObj001) and bhControlPlayer :1540-1572 shown to be
bound; audio bank 3 message in the scan is the direct start without bhRoomSoundCase (not a port error). Snapshot format 2
stores the room sound variant (test `snapshot_room_sound`, format 1 still readable). [Report](archived/work/port-batch-20261021.result.md). **No commit.**

**EventVm audit and remaining opcodes (2026-10-21, working tree, 212/212 green):** all 223 script commands
bound (0x82 bhSetDebugLoopEx, 0xCF bhExGameItemInit, 0xD3 bhRankingCall; dm0 slots stop as in
event.c:437 with a console message instead of a host error). Static script walker
`tools/walk_event_scripts.py`: Player_controll 132/140/141/143/145 and 0x82 used in no room.
Ledger legacy entries from the generator era (bhItemGetGet/Ex, bhPlItemLost/Ex, bhArmsItemSet,
bhPlayerPoisonCk, bhNext) re-verified against the original (02d12c1). Afterwards: Exm gate as player.c:1678
(mode0 1, pass gate, without st_flg 0x4), bhCheckCut(1) eye branch (cut.c:24-45), Common selector 0
only on the player (walker). Open: ranking.c ControlRanking.
[Report](archived/work/eventvm-audit-20261021.md). **No commit** for the second part.

**Audio and IOP model (working tree):** native/audio_iop.* models the TSNDDRV
command thread, TQ queue, MODMIDI sequencer, MODHSYN (2x24 voices, stealing, key-on stack) and the 48
SPU2 voices; the mixer now only carries EE handles and ADX/host PCM. Audio tests switched to the original
IOP latency (Req at sdSysServer, sound in ATick, stop two frames later). Host policy: a replaced
bank stops its port. COMMON bank0, SoundMode/OPTION/persistence, movie ADX release,
cancel gates/screen/audio fade, quantized ADX levels, block scale/loop playtime and
HD multi-layer/velocity selection corrected. Targeted 5/5 tests; five reverse mutations are
detected by the effect tests. RM_7250 plays COMMON/0002 in the enemy run; OPTION and movie start/skip
checked in the native runtime. **No audio 100 % proof:** float ADX, remaining synthesis/SQ
branches, transmission and output clock open. Windows x64 cross build and policy binary check PASS; four audio tests and
OPTION/room/movie checks under Wine PASS. Frontend neighbor suite under Wine still red: identical error `hint confirmation failed`
also in the separately built HEAD 47b4a22 (no expectation weakened).
[Mechanisms](docs/AUDIO.md), [remaining task](work/audio-port.md), [verification artifacts](build/validation/audio-port/).
Build macOS arm64; installed app/package not replaced. 2026-10-22 re-checked against the original
(PlayStartMovieEx/PlayMovieMain, MovieTypeDef, SetSoundModeEx, bhClearEffect, bhSetEffectTb UVs,
pxb before link point, PS2_Render_Tex_Sub) and committed on main; two tests adapted to the new OPTION row
and the co-drawn room geometry respectively (counter-check red), full suite 223/223.

**MIDI levels and MidiHandle[2] (2026-10-21, working tree):** sequence notes compute the MODHSYN register
(MODMIDI CC7 x port volume, HsMsg pan per 240 Hz ATick, pitch bend now evaluated, channel state per port
across sequences); background slot 2, system MIDI and door share the port rotation 2->8->9; background timer,
background stop and room fade-in run as MidiInfo fades (host ramps and finish_door_sound removed);
door skip stops only door lists. Voice pool 24/core measured (max. 24, RM_0100) and added as a message,
stealing/ATick batching open. Tests audio_levels/audio_gaps/audio_movie/core_continuity adapted. **No commit.**

**SE shot levels TSNDDRV/MODHSYN (2026-10-21, working tree, `./build.sh --test` 205/205):** own
disassembly of TSNDDRV/MODHSYN/MODMIDI.IRX (read reference only). SE shots now compute the
SPU2 register VOLL/VOLR (`sony_voice_levels`): note level incl. sample +0x10 (251 samples with 100
were missing), CC7/CC10, SdrSeReq/SdrSeChg rules (invalid volume, Chg pan 64, Chg bend 8192).
New test `audio_levels` (5 counter-checks red). Ledger: Get3DSoundParameter, CallPlayerFootStepSeEx,
CallPlayerWeaponSeEx, CallYakkyouSe done (ARMS SPQ gate: natively synchronous transport). Open:
CallNativeEventSe/MIDI levels (MODMIDI CC7, HsMsg pan, channel start) and voice pool, see docs/AUDIO.md.

**Audio gaps sdfunc.c (2026-10-21, working tree):** 11 partial rows re-checked against the original and
set to done: `ss_flg 0x4000000` has no original writer (PlayBgmEx2/ExecAdxFadeManager
always open), `sdMidiSetFxLev` is empty on PS2 (CallDoorSe FxLevel without effect, -2 delays = level 0/
center/pitch 0), Set3dSoundFlag/SetUserSoundVolume 6/7/8, object SE (type always 0), enemy SE manager
and 60 Hz fades, VolDownTbl 8..15 (caller frame, no literal reaches it), PlayVoiceEx2 pan
(pure state). New test `audio_gaps` (5 counter-checks red). Only the closed
IOP level/pan/pitch synthesis remains open (Get3DSoundParameter, Weapon/Yakkyou/Footstep, CallNativeEventSe
voice pool). **No commit.**

**RM_1090 computer, wrong password (2026-10-21, working tree):** task 8 (bhSysCallEvent) keeps running in the
original during the terminal (only task 7 stopped, system.c:2121); natively the
event tick was paused, event 4 displaced the sleeping event 5, the monitor camera stood still.
Fix runtime.cpp, test `rm1090_terminal_wrong_password`. **Wesker crash** („player motion 9
missing“): motion 9 has 20 channels on 22 nodes; Motion.c:131 writes only obj_num nodes,
`has_motion` now `<= bones`. Test `player_short_motion`. **No commit.**

**User findings 2026-10-21 (dev app, 203/203 green):** (1) sky black when opening the inventory: njSetBackColor(0)
came already on Tab instead of in the monitor after fading out (system.c:1987); (2) Calico muzzle flash with two targets:
PlyPchMain arm nodes were missing in the committed owP tree (both co-committed in de1fac7); (3) RM_3091 worm sound:
SE voices ignored VAG loop markers (70400 cut off after 1 s) – mix now follows the markers, plus AllStopEnemySe
(system.c:602/617); (4) stair action kept running past the pass gate player.c:1365 when an event takes control
(RM_3100 suspicion, scene not reproduced). Autoaim on dead enemies not reproduced. **No commit.**

**Enemy row audit and EN21 acceptance (2026-10-21, working tree):** EN21 in the real room
(RM_3120/RM_3140) covered with `enemy_en21_world`, six ledger rows done. Stale were
EN26 head burst, EN29 blood, EN19 shadow/hit assignment/camera shake, EN12 effects;
newly ported `bhSetGunSplash` (combat.cpp, combat and effect pass) and drawing of the
EN01 children with their own model row (cap/worm/bom/scope, 30 rooms). Tests `gun_splash`,
`enemy_en01_child_draw`, `rm5840_tyrant_gun`. Reports `archived/work/enemy_rows_audit.result.md`,
`archived/work/en21_acceptance.result.md`. **No commit.**

**RM_0090 safety box with raised lid (2026-10-20, working tree, 197/197 green):**
room objects adopted every Common 24/34 motion in the next tick; in the original only
bhSetMotion (v0 0/2/5/6, bhSetObjMotion) writes objP. Trigger: scd1 sets the work of level 15 to
obwp[14], the window zombie Common 34 (room motion 2) then hits the box. Test `object_motion_commit`.
Additionally the headless run draws every tick (rand() in bhEff106), `verify_enemy_ai` green again.

**Review of the last 10 commits (2026-10-20, working tree, 196/196 green):** GPU batches across
shadow phase boundaries (Metal/D3D11), double bhStandPlayerMotion when closing the crate,
claw blend state, trivial checks in the stand test, ISO raw images/PVD position/names fixed.
Open: `verify_enemy_ai` red since 2de4dad (zombie timeline, original reference needed).
[Report](build/validation/review-last10-20261020/README.md). **No commit.**

**CTest `script_motion` green (2026-10-14, `build/en09-claude-20261014`, 86/86).**
The cause was bookkeeping, not rendering: `pose_frame()` now returns for native
enemy works the frame that bhSetMotion sampled before advancing
(`sampled_frame`, set in the service `set_motion`); bhMotionPauseSet (0x2b) writes
mode3 also to the native work and exp1. Test expectations that contradicted the
original are corrected with source evidence: Common 23 is not a pause (event.c:11971,
only flg 0x118), bhMotionPauseSet does not stop an EN01 work (bhEne01_SetMtn runs
independently of mode3, en01.c:2528); the figure is reactivated after the drag scene via original command
0x24, the zombie in RM_0070 is only controlled via Common 34 after bhEne01_Init.
Rebuild older test binaries (e.g. `build/macos-arm64`, `box0090`) before use.

`Start.command` prefers the development app under `build/start/macos-arm64`,
then the v0.5 package, finally the build binary. The documentation split changes
no runtime, no package, no configuration and no snapshot.
Do not derive a current delivery from an old „in the package“ paragraph.

**Performance/energy:** the software rasterizer is ≈ 99 % of the CPU load (≈ 62–70 ms CPU per
room frame at „High“, simulation ≈ 0.26 ms/tick). Plan and measurement basis:
[Plan](work/performance_optimization_plan_20261020.md),
[Phase A + cuts](archived/work/performance_phaseA_scalpel_20261020.result.md)
(`bilinear` inline −8 % bit-identical, one draw pass per tick, `--render-threads` for
`powermetrics` A/B at the launcher). Worker count decided via `powermetrics` (automatic stays).
[GPU feasibility](archived/work/performance_gpu_feasibility_20261020.result.md) passed: the same
setup list on Metal instead of 62–73 ms CPU. **[GPU rasterizer in the game](archived/work/performance_gpu_raster_d2_20261020.result.md)**
(macOS, dev app): `video.ini renderer = gpu` (default), `--renderer gpu|software`; point sampling
bit-identical incl. shadows (test `gpu_raster_parity`), CPU per frame ≈ 0.3–0.5 ms. Open: wattage measurement
at the launcher. Shadow masks also run on the GPU (one command buffer per frame).
Windows: D3D11 backend written via Rasterizer Ordered Views, cross build green, **still unverified on
hardware**; GPUs without ROV keep the software path.
**OpenGL (2026-10-21, working tree):** optional Core 4.1 backend, Linux default;
`--renderer opengl`/INI/debug selection, Metal/D3D defaults kept. Mac arm64 GL on/off and
Windows x64 cross build green; 11 targeted tests passed, 3 GPU checks skipped because of
sandbox without display. Dev app updated, title/room only checked in the
software fallback. GL shader/pixel parity and Linux hardware open.
[Report](archived/work/opengl_backend.result.md).
**Vulkan (2026-10-25, working tree):** optional backend via ordered fragment interlock,
never default; `--renderer vulkan`/INI/debug selection. macOS (MoltenVK) bit-identical incl. shadows,
rooms 100 % (point), runtime image == Metal. Windows/Linux hardware open, no package replacement.
[Report](work/vulkan_raster_backend.result.md).

**Direct3D 12 (2026-10-25, working tree):** optional Windows backend, same HLSL as D3D11
(`native/gpu_raster_hlsl.h`), ROV, UAV barrier per draw call, fence before readback;
`--renderer d3d12`/INI/debug selection, `RECVX_D3D12_DRIVER=warp`. Cross build x64 checked, **parity on
Windows hardware/WARP open** (`gpu_raster_d3d12_parity`). [Report](work/d3d12_raster_backend.result.md).

**FSR 4 (2026-10-25, working tree):** Windows presentation stage `native/fsr_present_d3d12.cpp` behind
the MetalFX interface: own D3D12 device, DXGI flip swapchain on the SDL HWND, camera
motion vectors, reactive mask of the 2D layer, FSR 4 FP8/INT8 (`upscaler = fsr4|fsr4-int8`) with
FSR 3.1 fallback of the upscaler provider, sharpen/CRT as HLSL port. ffx-api headers (MIT, SDK 2.1.0)
under `native/third_party/FidelityFX-SDK/include`, DLLs never in the repo. Cross build x64 and macOS tests
green; **not run on a Windows GPU**. Open: object motion vectors, jitter, frame generation,
Vulkan (Linux/macOS). Details [NATIVE_PORT](docs/NATIVE_PORT.md#fsr-presentation-stage-windows),
[Report](work/fsr_present_d3d12.result.md).

**Map screen:** MAP draws MRY geometry, markers and sprites in its own
build runtime; fade return and re-entry checked specifically. Constant material
color path and type 41 normals corrected. **No launcher/package replacement yet.**
Evidence and remaining limits: [D10 map renderer](archived/work/native_port_D10_map_render.result.md).

**Pause key (host):** Escape never quits the runtime. In menus/dialogs it cancels,
in free play it opens and closes the pause (`system.c:668/691`, the same
key toggles on and off); the gate stays `st_flg 0x1C040208`. Quit is on F10,
and a key that is Cancel does not count as Quit (also after rebinding; old
`controls.ini` with `quit = Escape, F10` keep F10). Evidence:
[keyboard_controller_config with negative control](tests/native/input_test.cpp)
and `main_menu_settings_video_skip` in the own build runtime,
[Controls](docs/CONTROLS.md). **No launcher/package replacement yet.**

**bhLoadWork case 0 (player work):** `event.c:bhLoadWork`/`bhLoadWorkEx` case 0
now write `mode0 7`, `mode2 0`, mtn_no low byte 42, `stflg |= 0x10000`,
`stflg &= ~0x80` and `flg |= 0x10000`; `bhLoadWork2` case 0 only `mode0 7` and
`flg`/`stflg 0x10000` (each with original source location). With that the
player pad gate `player.c:1804` takes effect and the ported release in
`system.c:736-744` clears real writers. The rmthp gap datP NULL
(`Motion.c:29`) is handled for the player work as for entities (no
step, no frm_no advance, the last written objP/owp is
drawn). Set and release happen in one tick; end frame before/after
identical, evidence: [measurements](build/validation/loadwork-case0/README.md),
`event_lifecycle_control_release` and `script_motion` (Ctest 124/124).
**No launcher/package replacement yet.**

**ID card RM_1020 (fix A):** the freeze after the discard question is fixed. The player pass
runs despite st_flg 0x4 under the original conditions of bhCPM0_action, and bhControlPlayerPad
releases the bit (player.c:1838). Checked in the own build runtime
`build/discard-claude-20261014`. B is open: the original pad gating instead of the host gate.
**No launcher/package replacement yet.**
[Report](archived/work/native_port_discard_question.result.md).

**Effect procedures (translator replaced, further groups):** all 66 formerly
regex-translated procedures as well as 68 never translated ones (effsub0, effsub1, effsub1b,
effsub5 and effsub6, most recently the eye lights 145/146, the gas wall 127, the snowfall
219 with the snow library from effsub5 and the afterimages 135/136) are ported independently in
`native/effect_sprite_procedures.cpp`; 18 procedures of these files and the
families from effsub2/3/4 remain open; `native/effect_procedures.cpp`, `native/effect_core.*` and
`tools/generate_effect_procedures.py` are removed. In addition
`njGetPlaneNormal`/`njGetPlaneNormal2`, `njProjectScreen` and the screen state
(`njSetScreen`/`njSetAspect`/`njSetPerspective`/`njViewScreenMatrix`) are ported in
`native/ninja_matrix.c` and carry the backface gate `flg 0x20000` of the
effect draw path. Random and offset errors of the
generated code are documented in [QUALITY.md](QUALITY.md). Evidence, open list of the never
translated procedures with the scan of all 205 room files:
[Result](archived/work/native_port_effsub0_handport.result.md), differential runs
`build/validation/effproc-handport-20261014/`, groups 7/8
`build/validation/effproc-backface-20261014/`, group 9 `build/validation/effproc-snow-20261014/`, group 10 (incl. `bhSetEffectLink` on
room load) `build/validation/effproc-afterimage-20261014/`.
Afterimage re-check: verified DifAmb lighting computation, committed player/extra model capture
and skin FM; **88/88** tests. Diagnostic A/B with original room geometry, no natural
visual acceptance. [Evidence](archived/work/native_port_effsub0_handport.result.md).
**Start.command dev app updated by `./build.sh`; no package replaced.**

**Enemy AI EN21:** albinoid juvenile attached in the native WorldEntity/dispatch path,
with route/play mode, player grab, audio and effects 396/399 on the
model texture list. Shared route initialization without zombie resource and
player grab side effects added. Own arm64 build `build/en21-bind-claude-20261014`.
**Tests deferred at the user's request; no gameplay acceptance or launcher replacement.**
[Scope and later acceptance](archived/work/native_port_EN21_world.result.md).

**Bandersnatch (EN09):** standstill fixed — identity 9 was missing in the dispatch, the
state machine was only a skeleton. AI, motion/foot pinning, arms, grab, damage, death
and smoke ported; RM_1060 A/B and `tests/native/en09_world_test.cpp` green.
Own build `build/en09-claude-20261014`. **No launcher/package replacement yet.**
[Report](archived/work/native_port_EN09_world.result.md).

**Open points, as of 2026-10-14 afternoon:** ID card variant B (original pad lock instead of
st_flg 0x4 host lock), UFO crane bhEff134 + EN56 services + display, sparks 308, blood spurt 296/297,
EN16 follow-up effects, rain (lines cleared per frame), RM_1100 ENDIF without IF done; EN12: services and fire effects 262-265/274-276 bound (Alexia fights in RM_9321); open
EN12 MV04 (original UB with standing player, user decision), effects 256/257/266/269.

**Enemy gap scan (2026-10-14 evening):** instead of adopting the ledger texts,
`build/validation/enemy_scan_20261014/scan_enemy_gaps.py` reports the actual `Enemy gaps:` of all
205 rooms (600 frames, without input). Before: 51 rooms with gaps, after four fixes **38**:
`bhEne01_FlyingCap` `owP[0].flg |= 3` (7 rooms, service had long been bound),
`bhCheckWallRefAngle` as `wall_ref_angle` with both original gates (7 rooms),
`eff[sdw].mdlver` (4 rooms) and `bhEne03_Collision` in EN08 (3 rooms, the service was also
bound). 31/31 enemy/effect tests green. The old ledger row for EN04
(„MV-Zustandsmaschine fehlt") was stale: mv00..mv12, ng/dg/dd and the brain dispatch are there,
RM_1000 reports no gap. The remaining gaps are **subsystems and enemy families**, not
unbound individual services: `bhEne02` Gulp Worm (6, structure determined: 18 child works, mobile
ATR entry, EXP0 layout), `bhEne19` (4), `bhEff108`/`bhEff219` (4), `bhEne03`/`bhEne29` (3),
`bhEff355` (3), EN53 `child_ambient` (3).
[Report](archived/work/native_port_enemy_gap_scan.result.md). **No commit/push, no package replacement.**

**EN02 Gulp Worm (2026-10-14 night):** fully ported (`native/enemies/en02.c`, dispatch
`jump[2]`, 18 child works on slot 31, mobile ATR, sand sprites 253/254, effect voice 127).
RM_5830 headless: attack chain runs, player HP 160→0; CTest `enemy_en02_world` green, enemy/
effect suite 32/32. Scan now **35 rooms** with gaps. Verified original UB: `ang` in
`SetSandEffectMain` case 8 is `$s3` of the caller of `bhControlEnemy` (disassembly), reported natively.
Open: script opcodes 0x98/0xC3 (world.cpp being worked on by others), sprite 255, RM_3091/7220 held by script.
[Report](archived/work/native_port_EN02_world.result.md). **No commit/push, no package replacement.**

**Effect 355 (2026-10-14 night):** `bhEff_Sub355` ported (scope marker of the EN01 child
bhEne01Scope; global texture group 4 from SYSTEM/0001.bin, 16-pass alpha). **Correction:** the earlier
report `eff[-1]` in RM_5550/5630/8000 was a misdiagnosis — `spawn_effect` returns -1 for
**unported** effects, and the caller then reports the original condition; the pool was never
full. Both reports (`bhEff355`, `eff[-1]`) disappeared with the port. Scan now **32 rooms**
with gaps. [Report](archived/work/native_port_enemy_gap_scan.result.md). **No commit/push, no package replacement.**

**Effect 219 (2026-10-14 night):** wait condition on `camera_ready` removed — false gap, because
`room.c:33 bhInitCamera` zeroes `cam` on every room load (camera.c:8) and `game.c:50 bhControlEffect`
runs before `:58 bhControlCamera`; in the first effect pass the camera is therefore 0, like the native
defaults. Affected were 4 rooms (RM_5800/5840/6100/9000); the counter „rooms with gaps" stays
at 32 because these rooms have further gaps — the **gap entries** dropped 58 → 48 since run5.
**No commit/push, no package replacement.**

**EN11/EN16 binding (2026-10-14 night):** RM_9100 reported three EN11 gaps with one cause —
`en11_services` was NULL from the second enemy pass on, because the provider was only bound in `init_enemy_works`
and released again at the end of each pass in `control_enemy_works` (EN21/EN09/EN05
bind correctly there per pass). Both blocks are now in the pass scope. RM_9100 gap-free in the scan
(3 → 0), scan **31 rooms**. Test `enemy_en11_world`. **No commit/push, no package replacement.**

**Player owP in the effect pass (2026-10-15, subagent, checked):** the test report „owP vor
bhCalcModel" was a fixture error (no `commit_player_joints` as in runtime.cpp:450/493 =
player.c:840/1688); EN11 fixture corrected, test `player_owp_order` covers both. **Open as a
real gap:** the effect pass natively reads the player tree of the previous frame (runtime commits
after `world->tick()`), the original that of the same frame (game.c:49 before :50) — one frame latency
for 261/270 hits, player links, afterimage 136, E12 fire. Fix = player pass hook in
`RoomWorld::tick` (world.cpp:1777-1778), not surgical, open.
[Report](archived/work/native_port_player_owp_effect_pass.result.md). **No commit/push.**

**RM_1090 grandfather clock (2026-10-15):** the viewing hole was 22.5° next to 12 o'clock — Common_controll 9 wrote
for the native clock work into an unread override, now like 8/18 onto `enemy_obj`
(event.c:12136). Escape confirmed script selections with selection 0 („Links“); Cancel no longer
confirms a selection, the room script reads it itself (bhPadCheck, event 13). The reset freeze
after L/R was the dev app from 10.10. without 0xb1 (HEAD has the port). Room data: plates
1/3/4/5/6/7/8/emblem, L=+180°, R=−45°; L,R,LR,RRR shows 3,1,7,1 — „1974“ cannot be displayed.
Load: only the desk camera (glass roof, full-screen alpha) costs ~2× CPU, not an error.
Proof `tools/verify_clock_puzzle.py` (build and dev app green);
[Report](archived/work/rm1090_clock/README.md). `./build.sh` ran. **No commit/push.**

**Semantics audit flags/conditions (2026-10-15):** fixed in `world.cpp`: bhLoadWork2 case 2
(bytes 3..5, case 1 keeps model_cno), Common 15/16/28-33/44/45 onto `enemy_obj` or work
px/ax respectively, Common 40 frm_no (+exp1), Common 50 work type, sub 0x8f/0x92 only ct2 low byte, 0x93 ct2
untouched, 0x43 stflg 0x1000000, 0xd2 sx=sxb. Dialogs: Cancel answers no selection
(also GetItem/inspection), only closes the finished last page; timer pages (FF02 n) accept
no key, FE08 turns the page after expiry (only RM_7240); no system sound on text close
except the GetItem answer (SE 3); inspection selection without up/down. `bhDrawEneObject`:
bhCalcModel for en_obj (RM_9290 EN25). Refuted: IF yield, Sleep/Next, skip set,
height check/break in bhCheckExmAtari, metcp type 3/4, pad order, ZXY/BREAK in
bhCalcTree. Afterwards: the scheduler only runs with sp_flg 0x10 (event.c:13024); the rooms set
or clear it themselves in scd1 (bhSet 6,27), after a door `runtime.cpp` sets, like
bhStartDoorDemo case 1, sp_flg -1 (room.c:1006; RM_7050 event 8 did not continue otherwise).
Event_exec reassigns slot 15 when slots 0..14 are full. Empty FFFF pages wait as in the original
for OK/Cancel (1507 empty room texts, e.g. RM_0000 event 32 for the voice). Synthetic
event tables in verify_doors/gameover/view_clip set sp_flg 0x10 in scd1; verify_doors
green, gameover/view_clip could not run because of the auto mode block. Room sweep (205 rooms,
600 frames) before/after: only RM_6100/RM_7050 (gate) or open empty texts respectively.
`MessageDialog` is now bhControlMessage per pass: build-up via mes_ct (item names
count like bhDispItemName, FF04/FE00 immediately, OK held +4), timer only after build-up,
FE08/FE09 as in message.c. Closing with OK/Cancel only with st_flg 0x4000|0x1000 of the previous
pass (game.c:264); selection/page-turn OK and left/right are processed by EventVm::tick before
the script (status menu: immediately after its tick); sound 2/3 from bhControlMessage/GetItem.
Suite 98/100 unchanged, sweep unchanged. **Unverified (auto mode block):**
verify_clock_puzzle, verify_doors, verify_gameover, verify_view_clip after this rework.
Afterwards: bhIfelFlg/G_Sp as in event.c (global pointer +4/-4 instead of a stack; every 0 return in IF
jumps, also Sleep/EvtNext/EvtEnd/EWHILE end, which natively returned 2; RM_0090 Ev33 waited
natively). Opcode mapping corrected: 0xd1 = bhEffSSizeSet (sx=sxb), 0xd2 = bhEffLinkOffsetSet
(lox/loy/loz; was wrongly size). Newly bound: 0x2f bhInitSetKage (bhSetShadow once per
work, 51 rooms), 0x7e bhKageSwSet (stflg 0x8; obwp/itwp cases remain a gap). Sweep:
no new gaps. Item glint ported (objitm.c:404-450 after bhControlEffect: pause
60..89 from EE rand, 8 frames mdflg 0x80; bhDrawObjItm replaces light 1 with
bhGetLightVector(57344,ayp,0) with 0.1*aspd); items load aspd (short), 0x42 bhItmAspdSet
bound. 144 items in 69 rooms; RM_1090 then picks a different random event (EE rand
consumed as in the original). Light state per draw group like bhAllDrawModel/bhDrawObjItm:
object lists ob_nlg/ob_hlg/ob_spc per bhControlObjItm pass (sp_flg 0x4, pt_flg 1/2/4,
cb_flg 0x40000000 = only 4 objects); player, enemies and objects id >= 1000 (347 in 70 rooms)
with bhSetHalfLight (light 1 and point lights 0.5, amb_chr); ob_spc draw_tp 0 with rom->lgtp[aspd]
as light 1, first ob_spc in RM_1130/3060/7060/7100/9110 undrawn; en_obj (bhDrawEneObject)
with amb_rom and full light. Player shadow (player.c:791/905: bhSetShadow(NULL,plp,1,
4.5,4,3.5) per room, bhEff001 reads plp flg/stflg/owP). 0x6c bhObjAlphaSet with
bhControlAlphaFadeObject (start alpha = plist byte 7 of the node, persistent per node; RM_7050,
RM_8030). 0x23 was already ported (only entries behind rom->etcp stay reported).
Single-light path (ps2_NinjaCnk.c:252-276): draw_tp 2/3 and with gm_flg 0x80 every model
(game over camera, gameover.c:323) with the easy state of the group (bhSetLight/HalfLight,
bhDrawSpObject lgtp 0.2, bhSetEasyDirLight (0,0.3,0.7) in camera space, eye camera 0.6 per
enemy). Bound: 0x88 bhEffUVSet, 0x80 bhInitPonySet (plp->flg2 0x2 resets the ponytail in
bhObjClpn case 1), 0x2d/0x30 bhInitMotionPause(Ex), sub 148 bhEne11_LightControl
(EN11 services outside the enemy pass), pl_sleep_cnt (RM_9302). RM_9100 event 0 now runs
through. A02 `native/player_event.c`: pl_evt.c bhCPM0_event/pl_smove00..08 (test player_event);
Player_controll 131 sets plp mode0 7/mode1/mode2 (mode3 rule event.c:9405-9420), the runtime
runs the pass via the single player owner (also during a text), 128/139 release.
Without input only RM_7010/RM_3220 are reached (pl_smove01 case 0). Player shadow test and EN02 test
adapted to the new owner; in addition the tests made red by the dialog rework (only built with
`./build.sh --test`) adapted to bhControlMessage. Suite 99/101 (47/69 others').
RM_7010 event 37 DoorCall spawn 1 to RM_7030 also blocks on HEAD (baseline verified).
0xce bhSyukanModeSet sets/clears the gm_flg bits as in event.c:8873 (reaches RM_5500 with 1).
Eye camera in RoomWorld::tick_camera: bhInitPlEyeCamera (gm_flg 0x2000; owP[5], pe_ax/pe_pers from
combat core or 0xce respectively, without hidden_objects mask, lgtp[4..] flg 2 off, rom->fog_* ROM 0x108..0x110,
njClipZ 1.0/1.1), bhSetPlEyeCamera under gm_flg 0x40 (head +1.5/-1, wax, zoom st_flg 0x800000 in
steps of 364), return via bhControlPlEyeCamera mode0 1/2 with CAM_WRK.spd. Probe RM_0010:
eye pose and return to the cut camera. Combat core provides plp->wax/mode1/at_flg. Unverified
in game (scope replay). Open there: cam.flg 0x4 entry from other cameras (event/fixed).
Reported: mdflg 0x400 npGetMatColor, lgtp[0] type 100/101 in bhSetEasyDirLight, 0x91 only for
unported effect procedure.
**No commit/push.**

**EN19 Tyrant T-078 ported (2026-10-15, subagent, checked by the main agent):** `jump[19]`, CMake,
test `enemy_en19_world`; 2353 lines `en19.c`, tables exactly as in the original (50/21/17/28/26 …), all
21 Mv routines. In RM_5840 he runs (motion 14/frame 28, player HP 160→120); RM_1122/3230/5010 are
held by script. Spot checks (`head_turn` line-identical, `bhCtrPly_mode0[5] == bhCPM0_enedam`) passed.
**Open:** the remaining gaps of the 4 rooms are unported `effsub3.c` effects (303–312, 349) and depend on the
missing subsystem **`sys->efm`** (global effect models from `bhInitEffect`, natively 0 hits).
Effect *entries* in the scan rose from 45 → 52 because of this (4× „bhEne19" became 10 named effect gaps).
**No commit/push.**

**Effect 256/257 acid + EN14 acid services (2026-10-15):** `bhEff_E03_Acid`/`bhEff_E00_Acid` ported
by hand (`effect_sprite_procedures.cpp`); for that, new on the owner: `pxb/pyb/pzb` on `EffectSprite`
(effect.c:794), hook `player_watr` (plp->watr, player.c:1758), `effect_detail::collision_line`
(bhCollisionCheckLine2 shortens the position to the hit). EN14 `acid_alloc`/`acid_setup` bound in
`control_enemy_works`. Test `effect_e03_acid`, 36/36 green; scan `run10` **31 rooms / 50
entries** (52 → 50: `bhEne14_Acid` gone from RM_5810/RM_9302, otherwise identical). `./build.sh` ran
(Start.command app updated). Open: 269 `bhEff_E14_Fire` (fire child, EN14 type ≠ 0).
[Report](archived/work/native_port_enemy_gap_scan.result.md) section 9. **No commit/push.**

**Effect 267/269 + EN14 services (2026-10-15):** `bhEff_E13_Fluid` (spawner is EN30, group 81
enabled) and `bhEff_E14_Fire` (child of 256 type 6; `fire_light` hook generalized for color/range)
ported by hand; EN14 `joint_flags`/`node_angles`/`motion_angles`/`joint_matrix_set`/
`floor_enemy`/`game_mode`/`find_beam`/`beam_line` (O_WRK +0x10/+0x64 per `RECVX_EN16_EFF`) bound.
Test `effect_e14_fire`, 37/37 green; RM_9302/RM_5810 show 256/257/267 visibly. Scan `run11` **31 rooms /
46 entries** (50 → 46; newly visible `bhEff250`/`bhEff301` in RM_9302 because the fight now gets further).
Open: `bhEff130` (player beam, 1120 lines) — `find_beam` natively never finds a beam. `./build.sh`
ran. [Report](archived/work/native_port_enemy_gap_scan.result.md) section 10. **No commit/push.**

**Effect 250 + `model_texp_index` + morph draw path (2026-10-15):** `bhEff_E00_Mince` ported; the
spawner needed `RECVX_ENEMY_EXT.model_texp_index` (blood texture list), now bound — RM_5810/RM_9302
only report `bhEff301` now. Found along the way: `actor.cpp` threw „morph object has no vertex stream" for
EN14 MV02 (object 1 evalflags 0xE), which `npCalcMorphing` skips; source access now behind
`morph_blend`. HEAD only renders there because EN14 MV02 was not reached before (worktree measurement). Scans see
only the end frame — end frame sweeps needed after enemy ports. Scan `run12`/`run13` **31 rooms / 44 entries**,
37/37 tests, `./build.sh` ran. Last EN14 room gap `bhEff301` depends on the missing 2D effect list
`sys->ef_trs2d` (subsystem). [Report](archived/work/native_port_enemy_gap_scan.result.md) section 11.
**No commit/push.**

**EN05/EN13/13B/EN23/bhStandPlayerMotion (2026-10-14 evening):** Hunter/Sweeper (EN05 +
bhEne05s), Alexia II (EN13/13B), giant spider (EN23 + bhEne03s/EN24 children) ported and
in the dispatch; bhStandPlayerMotion at crate/map/status/computer/Player_controll 52/bhEff134.
Runtime computes plp before Event_init (player.c:840) — closes the first-pass gaps
`plp->mlwP->owP[n]`. CTest 89/89 green (`enemy_en05_world`, `enemy_en13_world`,
`enemy_en23_world`). Open: effects 238/256/267, EN14 gaps, plp->txp[2], no
window acceptance, Windows not built. Only the development Start.command updated.
[Report](archived/work/native_port_en05_en13_en23_stand.result.md).

**Enemy audit findings (2026-10-14):** `bhCheckWall` for enemies, `bhControlMask` in the
enemy pass, tree on room build-up (room.c:438), model-less works, EN11 services,
effects 261/270 with `bhCollisionCheckL2MDL` (plp mdl[3]) and corner alpha bound; CTest
`enemy_beam_effects`. Zombie `natural` is original behavior (RM_0080 pos_no 0 →
Sub_controll 0x8f type 8), verifier corrected. Open: EN56/bhEff134, EN16 follow-up effects
(EN05/EN13/EN23 and EN12 fire effects see above).
[Report](archived/work/native_port_enemy_audit_fixes.result.md).

**Typewriter / save games (2026-10-14 evening):** save and load screen per
`ps2_SaveScreen.c`/`ps2_LoadScreen.c` on the native owner `native/typewriter.*`/`ui.cpp`:
15 slots in the 5-row window (ITEM1/0144), confirmations 482/483/459, empty
load slot 458, typing animation at the original tick rate (McSaveFile.c:571–584), success 485, errors 486/461,
special save confirmation 253 with ev_flg 56. The memory card selection (SelectCard, port 0/1) is
removed as console behavior — the host has exactly one storage location, the screen starts
directly in the slot list; Cancel there leads to the leave state. Panel foundation
mcDispWindowFoundtion corrected (arguments right/bottom; fixed GS alpha 96/255 converted into the host color value,
previously drawn invisibly). Corrected against the original: mcWriteStartSaveFile
**clears** ev_flg 56 (McSaveFile.c:168; previously set and thus written into the save game),
no invented inventory rollback on write error (instead ev_flg 56, :206),
save completion re-checked on 2026-10-18: continue point only after the typing animation
(SaveScreen.c:1408), not on file write; one-time completion message from Typewriter.
The swapped flag 56 operations corrected: write error sets, special save clears.
`verify_typewriter_save.py` checks RM_0010 with isolated user files: last typing pass
still old continue point, success new point; real host write error, repeated error,
cancel confirmation and saving again in another slot without further ribbon/counter consumption.
Card management stays removed. CTest 122/122 green; the same check run via `Start.command`
green, success/error/confirmation images viewed. Evidence: `build/validation/typewriter-completion/`.
**Reported bug „save menu can only be left by saving" (2026-10-18):** in the host Escape is
both `cancel` and `pause`; the pause gate `pause_allowed` did not check the running typewriter
task and swallowed the key before it reached the slot list (Tab is not an
original key there). The original blocks exactly this case: `system.c:668` additionally requires
`(ts_flg & 0x3DE00) == 0x3DE00`, and bit 15 is the task bit of the typewriter (`main.c:195`,
task 15). Now bound; pause in free control unchanged (measured: with Escape 61 passes
and player stands, without Escape 356 passes and z −35.9 → 52.5). Check with real SDL keys:
`tools/verify_typewriter_save.py --sdl-cancel` (leave slot list and save confirmation, no
ribbon/counter consumption, no save file; baseline binary red), also via `Start.command`.
Open: load completion (`typ_flg 0x10`), Type_Space in rooms without a RoomNameSwitch entry
(original reads index −1); no interactive window acceptance or Windows run.
**Start.command dev app updated by `./build.sh --test`, arm64/signature checked;
no release package replaced, no commit/push.**

Reported bugs typewriter/pickup/scanner (working tree, 2026-10-15): panel foundation
with original factor — `njDrawPolygon` (ps2_NaDraw.c:8-58) writes vertex alpha `col>>25` (12 for
0x18808080), context 1 with the ALPHA register of the preceding font (`bhDispFont` 8/6,
message.c:374); the previous 96/128 came from PS2_swaps context 2. Typing line:
room name `CountMes-(14-ply_id)` (McSaveFile.c:707; previously `n-15`, count value 0 = whole text
in the first frame) and drawing with the counter value before the increment (McSaveFile.c:569-586,
`Typewriter::drawn_count`). Scanner crash: `sample_motion_pose` no longer aborts the game when there are more
motion channels than model nodes (original writes past `objP`,
Motion.c:131); surplus channels are consumed as in `native/motion_set.cpp:63-67` and
reported (`bind_pose_gap`, for static objects with work/identity/motion in `object_pose`).
Cause **not reproduced**: user state save-2 (RM_0010) + debug teleport RM_0090 +
item 56/59 + panel/material, headless with `--validate-poses`, also with the 20:41 app,
without errors; in RM_0090 only event 22 (object 8, MTN 6) and the zombie events 60-98 bind
room motions. Missing click after pickup: **not fixed** — sub1.c:3070 is the
opening click (ItemTaskCheck start), GetItem plays SE 3 only on the answer (:3879/:3928),
the final message 158 closes silently (message.c), then SE 9 (SpriteUV :2940); same natively.
`values[21]=identity.variant` on room load stays (system.c:1623: file follows from rcase).
`native-gameplay-bindings`: sys->mes_ip bound, effect gap now id 258 (E03_Shadow;
256 is ported). `verify_scanner` check frames 230/1000 (message build-up per pass):
green. CTest 100/101; `effect_room_sprites` equally red on HEAD worktree.
`verify_typewriter_save` equally red on HEAD worktree (title LOAD replay without card step).
Dev app updated via `./build.sh` (23:10); no package replaced.
Enemy package 2026-10-15 (working tree): `rySetShadow`/`bhEff304` bound (modifier shadow via
`sys->efm[1]`; three joints, position/ay/sx/sz/visibility gate per effsub3.c:1683) and EN29 tentacle
ported (`native/enemies/en29.c`+`en29.h`: Mode0/BrainType, `En29ActTbl[40]` in the EA_WORK layout,
Mv00-04/20 with 74498/74499, TargetAnalyze, DmgCheck, AttackHitCheck against `plp->watr`,
PlySetDamage/PlyMoveMain/PlyAction*, SetDmgEffect, five damage capsule children as a ring).
Tests `effect_ry_shadow` (RM_5840) and `enemy_en29_world` (RM_9000, attack chain up to
PlyMoveMain); CTest 102/103 (47 others'). Gap measurement all 205 rooms: 31/44 -> **26/37**
(`build/validation/enemy_scan_20261014/run15`). Ledger and
`archived/work/native_port_enemy_gap_scan.result.md` (section 12) updated.
**Next item:** EN17 (Monster Steve; contract en17.h and en54/en55 are ready),
EN17 Monster Steve including two chain works (arms) is ported (`native/enemies/en17.c`);
with that the armored knights EN54/EN55 also run (both now in build and dispatch). Test
`enemy_en17_world` (RM_9280); CTest 103/104 (47 others'). Measurement: 26/37 -> **25/34**
(`run16`); `bhEne17`/`54`/`55` gone from RM_9280. Open and named: particles `bhEff_SetPtcl*`,
afterimage 364, `rom->lgtp[2]`, player blood. [Report](archived/work/native_port_enemy_gap_scan.result.md)
section 13. **Next item:** the `sys->wt_*` water depth field (`bhObj005`) for `bhEff108`
(4 rooms).
Water surface `bhObj005` (`native/world_water.cpp`: height map `sys->wt_*`, vertex y/normals
per the ELF-checked asm blocks and `VU0_WAVE_INIT/CALC`, 4x4 tile UV via `npSetOffsetUV2`,
decay) and `bhEff108` (water rings) are ported; draw path via
`ModelGeometryOptions.local_vertices/local_normals/uv_*`. Finding: `sys->st_flg 0x40` was natively
never set by anyone, all water-bound paths (zombie water check, splashes, water steps) were silent in the
13 water rooms; EN01 `bhCheckWater` now bound. Test `object_water_wave`
(RM_3060/3210/7060/9130); CTest 104/105 (47 others'). Measurement: 25/34 -> **32/42** (`run17`):
`bhEff108` (4) gone, new is the one verified original UB "neighbor cell before `wt_wvp`" (edge vertices
x/z = 0) in 12 rooms. [Report](archived/work/native_port_enemy_gap_scan.result.md) section 14.
**Next item:** `bhEne03` (RM_6090/7080/7250), then EN53 `child_ambient`. **No commit/push.**
Black widow `bhEne03` (`native/enemies/en03.c`, about 4100 lines) is ported and in the dispatch
(`jump[3]`): init including eight `bhEne03s` leg children and fifteen EN24 children, hidden
leader work, shadow 258; brain BR00-02, move MV00-19, nage NG00/01, damage DG00-12, die
DD00-05, CollisionWalls/BoxEdge2/Line/Line2, DiveSpace, DamageInit, CheckJumpSpace, PlayerControl,
CallSE, Acid, HitMark. Findings: `CheckClimbWall`/`CheckClimbDownWall`/`AvoidWall` are parameterless
in the original and return 0 — the branches `mode2 6/7/8` are thus unreachable (original behavior).
An intentionally empty MDL record **inside** a model set is in the original an ML_WORK with
`obj_num 0`; `load_models`/`create_enemy_child` now distinguish it from a failed
read (`model_failed`). `plp->dvx/dvy/dvz` has no consuming player owner (service
reports it); `EXP0_INT(pno,980)` writes past the child block in the original (not reproduced),
`EXP0_BYTE(...,12/14/15)` is kept as `recvx_en03s_leg_mark`. Test `enemy_en03_world`
(RM_6090; init, children, leader, MV00 branch after 96 passes, joint matrix deviates from the bind pose,
GoAHead moves, counter-check without enemy pass); CTest 105/106 (47 others'). Measurement: 32/42 -> **30/40**
(`run18`); `bhEne03` (3) gone, new is the single one-time player lookup report after the grab ends in
RM_7250. [Report](archived/work/native_port_enemy_gap_scan.result.md) section 15.
Nosferatu `bhEne15` (`native/enemies/en15.c`, `jump[15]`) is ported: movement, three slashes
(wing box `njCollisionCheckBC2`, capsule), splashes, throw, damage/core hit, death, all eight
player guidances and the poison 397 (`bhEff_E15_Poison`). Mechanism-wide findings:
`bhSetMotion(plp)` from the enemy pass (`player_set_motion_now`), return path for `plp->spd`
(`take_player_speed`), poison bits `stflg 0x80000/0x200000` now reach the inventory (so the
acid 256 also poisons as in the original), effect sets carry `exp0 = epw`. Test
`enemy_en15_world` (RM_5800: throw chain, fall over the platform edge, poison); CTest 106/107
(47 others'). Measurement: 30/40 -> **30/38** (`run19`), `bhEne15` (2) gone.
[Report](archived/work/native_port_enemy_gap_scan.result.md) section 16.
EN53 `child_ambient` is bound to the existing model owner: one live stream per `mbp[k]`,
patch only on ambient green, ordered material propagation and multi-light evaluation.
`enemy_en53_world` checks three original rooms, pulse, diagnostic death, patch limits and the
productive draw/light path; nine targeted tests green. Start.command dev app rebuilt,
arm64/signature checked; RM_5800/6040/6100 at 1/8/30/60/600 frames without enemy gaps and
pose abort. No new full scan; combat/window acceptance open.
[EN53 report](work/enemies/EN053.result.md), scan report section 17.
`bhSetRapEff 310` done (2026-10-16): the whole effect setup of the Tyrant runs natively on the
existing `eff[512]` set. Newly ported by hand: 310 `bhEff30a` camera shake, 349 callback,
308 `bhSetEffSpark` (own table `Eff308Prm[2]`, `gnd_hgh` from `bhGetGroundPosition`) in
`native/effect_rap_camera.cpp`; 307 claw surface + 312 mirrored claw in
`native/effect_rap_claw.cpp`; 309/311 blood jets in `native/effect_rap_blood.cpp`; 305 dust +
306 smoke in `native/effect_eff30x.cpp`. Camera/light via the hooks `camera_offset_set/add` and
`spark_light`, range overruns remain named gaps. Tests `effect_rap_camera`,
`effect_rap_claw`, `effect_rap_blood`, `effect_eff30x` and `enemy_en19_world` green
(RM_5840: `310=1 349=0 312=3`, type 2 has no 349). Headless 600 frames: RM_5010 **+310/349**,
RM_5840 **+305/307/309/310/312**, camera pose and Tyrant state identical before/after, no
effect gap. Tables verified byte by byte against the ELF (among others `Tex5Buv[12]` reads jump table words =
denormals ~3.4e-39, natively `{0,0,0,0}`). Open: `bhEff298`/`bhEff303` unported, `bhEne19`
Mode0 index 4 (NULL in the original), no natural playthrough proof (player dies during the run).
[EN19 report](work/enemies/EN019.result.md). **No commit/push.**

**`effect_room_sprites` green again (2026-10-16), two pre-existing findings clarified:** (1) since
`7d1f366` `step_eff219` intentionally no longer reports the camera gap, because `room.c:33`
`bhInitCamera` zeroes the whole `cam` object on room load and `game.c:50` runs before `:58` – the
box sits at the origin in the first pass; three stale checks updated. (2) The afterimage error
visible afterwards comes from `f7f6c67` and is narrowed down by **measurement** (`7b81215` green,
`7d1f366` + 219 correction green, only `f7f6c67` red): the five afterimage alphas are correct
(2704 triangles each, material phase 6), but `f7f6c67` gives object list works
(`kind==2 && object_list>=2`) `light_category = 1` – **verified correct**, because `objitm.c:518-529`
draws `ob_hlg`/`ob_spc` after `bhSetHalfLight()` with `amb_chr`. The test was too coarse; the
alpha check now runs on `material_phase == 6`, plus the same triangle count per stage and an
explicit check that phase 3/4 is at half light. Counter-check with alpha writing
disabled fails. **Full CTest 112/112 green.**
Scan report section 18. **No commit/push.**

**`bhEff298`/`bhEff303` ported (2026-10-16), the EN19 effect chain is thus complete:**
298 `bhEff_E00_AcidGenerator` (effsub2.c:623) in `native/effect_sprite_procedures.cpp`, 303
blood stain (effsub3.c:1507) in `native/effect_rap_blood.cpp`; `enemy_en19_world` allows **no**
effect gap anymore. Two ownership questions clarified along the way: `sys->bl_ct` now lives at
`effect_blood_counter()` (one counter for the particle path and bhEff301/303, snapshot
preserved), and `RoomWorld::EnemyCallbackFrame` binds the world pointer and
EN19 services in the effect pass, because `bhEff349` executes enemy code there (`bhEne19_TyBloodSet` calls
`bhGetGroundPosition`/`objP[18]`/`bhSetEffectTb`) — **without the frame the world test crashes**.
Verified peculiarities: 303 first counts `bl_ct` and then uses it (reverse of bhClrEff_RY/301),
`sy` grows toward `szb` instead of `syb`, `flg = 0x200001` is an assignment; 298 also stops the wait counter
on the spawner's `stflg 0x1000000`. Full CTest **112/112** green,
four counter-checks take effect; RM_5010/5840/3230 headless 600 frames each with identical set IDs,
camera pose, HP and motion (no natural run reaches the two sets).
Measured open: RM_5010/3230 keep the Tyrant in `mode0 5`, RM_5840 is type 2 without 349,
RM_1122 not in combat; the 303 proof runs via the marked `status` bit, and the
spawner branch of 298 needs a hit with weapon 15 (`comb_wep` from en03_lib.c:1314).
Scan report section 19, [EN19 report](work/enemies/EN019.result.md). **No commit/push.**

**Ledger cleanup (2026-10-16):** `docs/port_ledger.tsv` had ten duplicate keys, and
`tools/port_check.py` silently evaluated the last row of each. Three distorted the report:
`rySetShadow` (stale `missing` covered the verified port), `bhEff219` (`partial`
covered `done`) and `bhDrawTrsEffect3D` (a misfiled `eff[-1]` note raised the
draw pass to `done`; now `partial` again, the note is at `bhSetEffect`). All ten
merged, and `port_check.py` now rejects duplicate keys. `rySetShadow`/`bhEff304`
checked against effsub3.c:1640/1683 (counter-check sign `ay` takes effect) -> `done`. `bhEff219`
image acceptance in the snow courtyard RM_5840 (natural run, only snowflake source) -> `done`; in RM_9090
the room script switches all three boxes off via `0x43`, the empty image there is original behavior
(runtime trace `build/validation/snow219_20261016/`). Effects now 176/190, no open item.
CTest 112/112. **No commit/push.**

**Grenade launcher and the 298 spawner branch (2026-10-16):** weapon 15 is the acid grenade launcher
(sub1.c:4489-4492). The spawner branch was not reachable because `CombatController` silently rejected weapons
14..17 (`return false` without a message). Now: weapons not enabled report
`Combat gaps` (also in the runtime output); the launch player.c:4990-5120 is ported in the core
(previously a named gap), the projectile `bhEff130` (effsub1.c:9001, all types) in
`native/effect_eff130.cpp`, `bhCheckFlyAtari`/`bhSetBowDamage` in `native/weapon_fly.c`; weapons
14..17 enabled. `enemy_en19_world` covers the **whole chain via a real hit**:
acid shot -> projectile -> `bhCheckFlyAtari` (wpnr_no 15) -> `comb_wep 15` -> 298 -> 257
(Tyrant HP 900 -> 860). Clarified along the way: `bhEff019`/`bhEff130` write `rom->lgtp[0]` via a
light owner (`room_light0`, bhEff019 now `done`); player combat wrongly attached effects from
`bhSetEffectTb(..., NULL, ...)` (pwksub.c:2810/2919) to the player — `link_player`
now decides as in the enemy path. Tests `effect_eff130` (new), `enemy_en19_world`,
`effect_room_sprites`; counter-checks take effect; after a forced rebuild **CTest 113/113**,
`verify_combat` green, RM_5010/5840 identical before/after. Open: `sys->ef_ntx2d`,
`bhCheckL2Water` (both since ported, see below), weapons 18/20, muzzle smoke not accepted visually.
**No commit/push.**

**Two findings by others checked and fixed (2026-10-16):** (1) `bhLoadWork` 0x65 took byte 3 as the node for all kinds;
the original sets it to 0 for player/item/effect (event.c:12733/12785/12799).
Real consequence in RM_4010/4011/8010: `65 03 03 01` + sub-command 18 rotated item 3 around node 1 instead of around the
root (different pivot). Corrected in `native/world.cpp`; `script_motion` plays the real
command sequences of the three rooms (measured red before) plus counter-cases player/effect/object. `bhLoadWorkEx`
0xB5 (same rule, bhEtask) is still not ported and now tracked as `missing`. (2) The
computer figure window re-sampled a motion pose; the original draws the existing local
objP tree after npRetSkinConvert and zeroes objP[1].ang permanently (sp_evt.c:1478-1500). New
`PlayerAsset::sample_tree` (direct tree draw path with npRetSkinConvert translations),
`computer_character` reads enemy_obj/evalflags/live_stream or committed pose + node_overrides respectively and
writes mtn_no/frm_no/px..az to the enemy work. Affects only script 3 (albinoid database); scanners 4-6
do not use the window. Tests actor/computer_terminal/script_motion, counter-checks red, **CTest 113/113**,
`verify_computer` green. **No commit/push.**

**`bhLoadWorkEx` 0xB5 ported (2026-10-16):** the same selection branch as 0x65, target `bhEtask[0]`
(task level 0) instead of the running task; node rule identical. Test `script_motion` (target context
measurably separated, counter-check red). Case 0 still shares the known gap with `bhLoadWork`.
**Build trap found:** `./build.sh` without `--test` configures `BUILD_TESTING=OFF`; afterwards all
test targets in the Makefile are empty (`make: Nothing to be done`, rc 0) and a test run hits the old binary.
A previous "green" run of 0xB5 was invalid because of this; rebuilt after `cmake -DBUILD_TESTING=ON`
and verified with `strings` on the binary. Before every test run after build.sh: check the cache or
`./build.sh --test`. **No commit/push.**

**Grenade launcher remainder (2026-10-16):** (1) **No reloading in the original** — `bhSearchBullet` (sub1.c:8907)
only loads combi rows with flag `0xF000`; all grenade rows carry `0x4/0x5`, only bow ammunition `0xF005`.
The "reloading with ammunition switch" suspected here before is not a port item; the port checks the same
gate. Pinned in `ammunition` (empty launcher does not load/fire, crossbow loads). (2) **`sys->ef_ntx2d`
ported:** `bhEff130` type 3 enqueues the screen set per pass (`ntx2d_queued`), `RoomWorld::draw_ntx2d_effects`
= effect.c:905 `bhDrawNtxEffect2D` -> `njDrawPolygon` (alpha `col>>25`/128 under 8/6) draws it in the
runtime draw pass before HUD and fade. Tests `effect_eff130` (entry only from ct0 > 7, colors exact) and
`effect_room_sprites ntx2d_room` (RM_5840, pixel proof across the whole canvas, gates pt_flg/stflg);
two counter-checks red. `./build.sh --test`: **CTest 113/113**, dev app 16:58. **No commit/push.**

**Water impact ported (2026-10-16):** `bhCheckL2Water` (hitchk.c:5561) = `RoomWorld::l2_water` on
the same surfaces/in the same order as `bhCheckWater`; `bhSetWaterSplash4` (pwksub.c:2643) =
`effect_detail::water_splash4` (in the effect pass via `submit_template`, otherwise directly). Bound and thereby
closed: combat core shot (weapon.c:872-958, id 15 + Splash4, weapon 11 with 32 sets) and knife
(:1035-1069, two id 11 sets), `bhEff130`, EN01 `water_line` (4 places), T3 `water_line`. Also fixed:
T3 passed an **end point** instead of the vector (0,20,0) (zonzon.c:921); the combat core drew `rand()` from the
libc instead of the EE generator (also player.c:4950). `RECVX_CB_EFFECT.sys_ef_keep_ax`: water branches
write `sys->ef`, ax stays the template value. Tests `object_water_wave` (RM_3060/3210/7060/9130 real
surfaces) and `combat_player_state test_water_impact`; four counter-checks red. `./build.sh --test`:
**CTest 113/113**, `verify_combat` green, dev app 18:56. Not verified: weapon 11 (32 sets) only in code,
no image/playthrough proof in the water room. Finding: script command 0x7c only changes the event surface list,
not `floor_sounds_` (source of bhCheckWater/L2Water) — possible second owner, unverified.
**No commit/push.**

**Floor surface table rom->flrp unified (2026-10-16):** the 0x7c finding was real. `RoomWorld` held
three copies (`floor_sounds_`, `enemy_areas_`, `effect_water_room_`); 0x7c (bhFlrAtariSet2) reached none,
0x0c did not reach the effect/enemy floor copy (bhCheckFloorEnemy, effect floor). Now one table
`room_floors_`, `enemy_areas()` as a view, 0x7c forwarded to the world. Real case **RM_0070**: the
init script sets the floor sound of surface 2 to 2 at flag 1/0x74 — previously sound 1 stayed natively.
Likewise RM_4020 (surface 2 -> sound 3, not checked individually). Test `world_floor_table` (new), two
counter-checks red. `./build.sh --test`: **CTest 114/114**, dev app updated. Still separate: the
event view `EventVm::floors_` (both writers now mirror), `activate_enemy_spawn` ignores sys->mflrp
(unverified). **No commit/push.**

**Weapons 13/18/20 and scope (2026-10-16):** finding: `WpnTab.flg 0x20` sends weapon **13** (previously tracked as
"supported") and 18 to mode2 71 `bhCPM2_act_scp` (playpch2.c:13) — the core only had a
gap there, so weapon 13 never fired natively. Ported: first-person view (gm 0x80000/st 0x800000, pt_flg 0x1 off),
shot (13: gun atari from owP[5]; 18: rocket bhEff130 type 4, ef_flg 0x2), 30-step recoil, zoom
cam.ppers 3640 on pad 0x80, aiming via pad_dx/pad_dy, release after ct0 > 5. Weapon 20: bhSetGunFire sparks
bhEff027 (pwksub.c:3126). CombatController enables 18/20. Also fixed: sys->pad_dx/pad_dy now one
owner (runtime, only with sp_flg 0x20 like bhSysCallPad); pt_flg and sys->ef_flg bound in combat (previously
a gap or always 0 respectively, the rocket reload gate player.c for 18 never took effect). Tests `combat_player_state`
test_scope/test_linear_launcher, three counter-checks red; `./build.sh --test` **CTest 114/114**, `verify_combat
--unit/--runtime` green. Not verified: no image of the first-person view in the runtime, no natural acquisition
of 13/18/20 played through. Open/finding: the combat pad maps "forward" only to 0x20, the original
sets 0x1|0x20 (pad.c pad_tab: D-pad up = bit 0 and bit 5) — affects bhControlPlayerPad
(player.c:1818); analog stick pad_ax/pad_ay missing in the host. **No commit/push.**

**plp->stflg unified in combat (2026-10-16):** found during the pad check: the combat view held
plp->stflg as its own copy. The aim bit 0x400 (player.c:1789), which en05/en09/en17 read, never reached the
enemies; the item crate gate 0x10000 did not block aiming. Now the CombatController reads/writes
event bank 14 around every core call. Combat pad: D-pad up/down = 0x21/0x42 like pad_tab (effect in the host
not observable, see ledger). Test `combat_player_work` (new), counter-check red; `./build.sh --test`
**CTest 115/115**, `verify_combat --runtime` green. OPEN: plp->flg (bank 13) is still private in the
combat view. **No commit/push.**

**Weapon 11 and plp->flg check (2026-10-16):** `bhSetGunFire` weapon 11 (pwksub.c:2981) ported: five
bhEff014 and eight bhEff002 sets via sys->ef with the template inheritance of the original (type 3 takes the position
of set 4). CombatController enables 11. Test `test_weapon11_fire`, two counter-checks red; `./build.sh --test`
**CTest 115/115**, `verify_combat` green. plp->flg: checked and **deliberately not** bound to bank 13 — no
external reader of a combat flg bit except the pass gate 0x10000, and a binding would leave 0x10000 set after a menu/event
(core is discarded) and block the movement path. Prerequisite would be to keep the core during
locks like the original keeps the player work (ledger `player.c:BH_PWORK.flg:Kampf`). **No commit/push.**

**Bow gun (weapons 10/19) ported (2026-10-16):** `bhEff013` (effsub1.c:3352) in `native/effect_eff013.cpp`
with flight, homing, wall/enemy/water hits, sticking (121 passes) and explosive arrow explosion; drawn
via bhDrawPolEffect with `efm[13].objP[mdlver]` (fourth model block from SYSTEM 0001). Closed along the way:
**`bhCheckCapCol2Capsule`** (weapon.c:1496, CPCL joint capsules) was only a gap in the core — **so weapon 13
could not hit any enemy either**. `bhLinkBlood` is empty in the original, EN01 wrongly reported it as a
gap. New: `recvx_combat_check_gun_atari`/`recvx_combat_search_near_enemy` for effects, bhEffDmy 139..142,
runtime attaching of an effect to an enemy work. CombatController enables 10/19. Test
`effect_eff013_world` (RM_0080, three cases, three counter-checks red); `./build.sh --test` **CTest 116/116**,
`verify_combat` green, dev app 20:29. Not verified: image, water room, natural acquisition. **No commit/push.**

**Semantic alignment of hits (2026-10-16):** `bhCheckGunAtari` (weapon.c:473-870) and `bhCheckKnifeAtari`
(:1002-1146) line by line against the core: filter, cone test, joint capsule/capsule branch, sight wall, nearest
hit, penetration branch (takes px/nr of the previously nearest one, as in the original), joint choice, damage levels and the
joint coordinate return match. The only remaining gap there: wall destruction attr 0x1000000
(:552-566, bhSetExplosion/obwp[prm3].mdflg/ahtp->flg) is unbound in player combat — finding: only `bhObj012`
(objitm.c:2069) creates such walls, and **no room carries a type 12 object** (all RDX checked); bhObj012 is
not ported natively. **No commit/push.**

**Player motion blending (SetMtn*Hokan) ported (2026-10-16):** plp->mlwP->objP is now persistent
(`RoomWorld::player_obj_`); action/event, combat, damage, grab and push write the node part of
bhSetMotion with the state before advancing via the existing `set_motion`, all sampling sites put objP under
their edits. Motion changes blend as in the original (log: run->stand over 5 passes, draw/holster
over 4/12); the displayed pose is the frame before advancing (previously one ahead). Combat gap `SetMtn*Hokan`
and push bit HOKAN dropped. Test `player_motion_blend` (new, two counter-checks red). `./build.sh --test`
**CTest 117/117**, `verify_combat` green (same 6 shots/4 hits; lock-on angle 3521 -> 3366, because the
core now reads the objP pose), `verify_push`, `verify_stairs`, `verify_lighter` green. Red, but measured identically red
with blending disabled: `verify_events` (window sequence), `verify_enemy_ai` (dog
death bite at frame 3000); `verify_crate_edge` expects, stale, opcode 0x8b as open (bound since 7023885).
Open: stairs, game over, script-driven player motion do not carry objP yet. Dev app 21:10. **No commit/push.**

**2026-10-16, three reported bugs (RM_1050 / inventory), causes verified and fixed:**
(1) *Ant jewel, blue surface:* `RoomWorld::set_fixed_cut` looked up the near limit with the previous room cut
(cut 1: 20/20000) instead of with `cam.ncut = cno` (cut.c:486 before bhChangeViewClipRM :537); the display case was
in front of the near limit, only the background color remained. Now cut 5 -> 2/20000, the image shows the display case.
(2) *Steve switch without images:* the message bits of `sys->st_flg` (0x1000 page finished, 0x4000, 0x8000, 0x800,
0x400, 0x80000) lived only in `MessageDialog`; the script (event 34, @2ba2) waits via bhFlagCk type 5 for
0x1000 and its clearing when turning the page of message 24 and only then sets bhCamSet 4 (switch panel with the
six weapon images). `EventVm` now mirrors `MessageDialog::status()` into flags[5] after every pass; camera 4
and the images appear. Found along the way: the monitor (bhEff111, render texture from cut 6, only RM_1050)
is not bound and stays black (ledger `effsub1.c:bhEff111`, missing).
(3) *Inventory text before the UI:* in the original GetItem only runs in the standing pickup screen (sub1.c:3452,
StatusMain case 8 after SpriteH without statusflg 0x40); `begin_pickup` decided immediately, question 157 stood at
its fixed position (mes_spos[1] 44/342) above the incoming sprites. The decision now waits for the
status task and the end of the slide-in. The item name itself slides in with cen_pos[6] (unchanged).
Tests: `native_ground_world` (clip), `event_lifecycle` (RM_1050 event 34 up to bhCamSet 4),
`native-gameplay-bindings` (RM_0000 knife); all three red with counter-check. Headless images: display case, switch panel.

**2026-10-16, RM_0080 roller door zombies (user finding "moving dummies"):** the switch (interaction 8, after
the lock `ev_flg 110` is resolved) starts events 5-11/40/41; 7 and 8 are the two zombies: Common 25 walk,
`Common_controll 53` (@132a/@1360), then `Sub_controll 0x8f` mode 1. The native VM did not know command 53,
the task stood still ("unimplemented Common_controll command 53"), the zombies stayed in mode0 5. The cause
is the parsing: the 0x69 length table was a partial state. Now `EventVm::common_controll_length` with all
49 original commands from event.c:9963-12710 (pointer advance per case, selector dependence 24/25/34/35,
axis dependence 10/22); natively bound 0, 53, 10/22, 43, 35 selector 1; named gaps 51/52
(`sys->evt_posno`, opcodes 0xb3/0xb4 without owner) and 35 selector 0/player. Test `event_lifecycle`
(table against the original advances; RM_0080 roller door until both zombies run in the brain; counter-check red).
Headless proof (`--enemy-ai on`, snapshot with ev_flg 110): frame 300 roller door opens, 500 both walk
toward the player (mtn 125/117), 800 grab (mode 1/0/5/1). CTest 117/117.

**2026-10-17, nine user findings (diagnosis by subagents, fixes checked):** (1) idle->walk:
pad_old after the pass (pad.c:146), `tools/verify_idle_walk.py`. (2) RM_4050 bag: `bhCheckSubPack`.
(3) RM_3091 worm: `njPtclDrawPolygon` in screen points. (4) RM_4020 crane shadow: `bhObj008`.
(5) Ladder in the first transition frame: door model only without status 0x40 (door.c:395). (6) RM_1080
Luger door camera: gm_flg mask on room load (system.c:185); Luger door without insert question and RM_3030
card reader open are faithful to the original. (7) RM_3140 camera event: `bhEff137` (monitor display bhDraw137
open). (8) RM_1050 monitor: `bhEff111` + render texture (headless image shows Steve). (9) Steve: bhSetDispObj
mdflg 0x1, bhEneRenderSet on the work, subplayer motion changes per original. Details in the ledger. CTest 117/117,
all counter-checks red.

**2026-10-17, addendum:** bhDraw137 (RM_3140 monitor display) ported; the shared 2D rasterizer of the
ef_fnc displays computes texels normalized to 256 (GS ST, ps2_NaDraw2D.c) -- this also corrects the crane display
bhDraw134 (group 44 is 128x128, previously only a section). gm_flg 0x20 (cut change) now has writers
and the bhCheckCut clearing; the RM_1050 monitor refreshes on cut changes. Only the
PS2 framebuffer copy (PS2_Render_Tex_Sub) remains open, which cannot be determined without GS emulation.

**2026-10-17, further open items (working tree built 09:01, CTest 117/117; dev app still 02:34, not
delivered):** (1) opcodes 0xb3 `bhEtcAtariEnePosSet` (event.c:7794) and 0xb4 (event.c:7865) with
`sys->evt_posno` (snapshot, room change) and rom->posp; Common 51/52 bound. Test `event_lifecycle`
(RM_0110). (2) Script-driven player motion: `pl_smove08` (Common 24/25/34 selector 0) carries objP via
`set_motion`; test `player_motion_blend`. (3) `verify_events` green: cb_flg 0x4000 is consumed by GetItem
itself (sub1.c:3830-3857), replay tick for message 158. (4) `verify_enemy_ai`: the expectation mode0 6 at the
dog death had no original source (EN07 only sets mode0 4, bites clamp hp to 1; death comes from the
jump attack, damage mode 2, hp 6 -> 0 -> -6 at frame ~2420/~2497). Ported: player.c:1405 (hp < 0 ->
mode0 3, mnwP = mnwPb) and `bhCPM0_die` (fall motion 25/27 with objP via set_motion, ground position, voice 1025,
game over start at the frame change via ts_flg 0x4000, lying motion 26/28; continues running in gov_md1 0).
Two host errors along the way: the death reporter read plp->flg 0x2 in every mode (only bhCPM0_enedie/mode0 6 reads it),
and ts_flg 0x4000 was missing in the native room load. The RM_0050 part of the checker was also red with the baseline binary
(fixed lure position -142, rm_flg 9 randomizes the trap) and now follows the roll.
`verify_enemy_ai` green; verify_events/event_freeze/stairs/push/lighter/continuity/doors/idle_walk green;
CTest 117/117; counter-checks red (no transition -> no game over; old flag -> enemy_work red). Dev app 09:53.

**2026-10-17, stairs on objP:** the six stair routines now carry the bhSetMotion node part (blending,
hokan_count deduction) on plp->mlwP->objP, also in the final frame; bhCalcFixOffset reads objP of this
pass against owP of the last bhCalcModel (previously unblended reconstruction). hokan goes back to act_std.
Gap bit HOKAN dropped (0x280 -> 0x200). `verify_stairs` with POSETRACE check green, end positions as before;
`core_continuity` with node part/owP fixture (counter-check red). CTest 117/117, neighbor runs green. Dev app.

**2026-10-17, death scene (gov_md1 10) on objP:** bhSetMotion node part per pass at the player owner,
frm_no also advances during blending (previously held), entry with mnwP = mnwPb, bhDrawGameOver draws
the objP tree. Test `gameover_original` with SYSTEM/0010 (counter-check red), CTest 117/117, all check runs green,
dev app 10:23. Open: runtime measurement of the self-destruct scene (no trigger reachable headless).

**2026-10-17, task 7 in gov_md1 0:** the game image keeps running in the 70 frames before the game over mask
(main.c:197, only task 8 blocked via ts_flg 0x100), the game over pass follows at the end of the frame. Along the way: NG00 mode3 7
is an empty slot in the original (not a gap), bhCPM0_enedie clears ts_flg 0x4000. `verify_enemy_ai` checks
the dog movement in md1 0 (counter-check red); all check runs green, CTest 117/117, dev app 10:56.

**2026-10-17, grab death RM_0080 checked:** player mode0 6 / mode3 7 with motion 412 is PlyDG00 case 7, zombie 1
in MV16 -- faithful to the original. The error was the native game over start: in mode0 6 already at hp < 0 instead of with plp->flg 0x2
at the end of the death motion (player.c:6711, en01.c:7748); now start at frame ~1072 instead of ~1008. `verify_enemy_ai`
death-1050 (counter-check baseline red), all check runs green, CTest 117/117, dev app 13:19.

**2026-10-17, stairs mtn_attr:** plp->mtn_attr has no reader in the original (only Motion.c:76 writes it); the
node part carries it along, the gap bit 0x200 is dropped, the stair actions no longer report open services.
CTest 117/117, dev app 14:00.

**2026-10-17, self-destruct scene measured:** `tools/verify_self_destruct.py` (diagnostic snapshot RM_4040 with
ev_flg 257/35 set, 259/36 clear; examining entry 1 triggers scd1 @0452: EvtStart 5 + 0xcb mode 1; the
door in event 5 leads to RM_3230, whose escape scene is skipped via SELECT, then RM_3091). Measured: evt_tim
16202 at frame 1100, expiry at frame 9200 (ev_flg 67 clear, gm_flg 0x400, ts_flg 0x4000 clear, sp_flg 0x20 -> no more
player pass, player.c:1365), md1 0 71 passes, md1 10 from frame 9273 with exactly one node part per pass: motion 4
(20 frames) without blending, fifth explosion in pass 96 -> motion 25 from frm_no 0 with hokan 8..1 with advancing frm_no,
wrap after 40 frames -> motion 26 from pass 137, md1 1 after pass 150; with ply_hp 100 motion 7 (second PlMtnAct row).
Three native errors fixed: the counter evt_tmd/evt_tim was lost on room change (system.c:178 leaves it in place;
`inherit_progress`), bhCheckEvtTimer did not require ts_flg 0x4000 (player.c:1272), the death scene took wpntp/dmlvl 0/0
from begin() instead of exp0 on entry (gameover.c:289) and did not write plp->px/py/pz/ay (:303-308). `--state` now carries
`event_deadline_mode/ticks`. Tests game_test/gameover_original (counter-checks red), CTest 117/117,
verify_self_destruct/enemy_ai/stairs/events green. Dev app 14:43.

**2026-10-17, RM_3230 escape scene:** not an engine error. The scene runs through natively as soon as the enemy pass runs:
event 2 (rel 0a22) holds EN19 (kind 1 index 1) via Common_controll 24 (event.c:11299-11500: mnwP = sys->rmthp, mtn_no 1,
mtn_add 0x10000, mode0 5) 569 frames at frm_no 0, then tasks 0/1/2/4/5 wait with bhFrameCheck (event.c:3237,
command 0x38) for frm_no 35/80/463; the advance is provided by bhEne19_Event (en19.c, bhEne19_Mode0[5] -> bhSetMotion) in the
enemy pass bhControlEnemy, which only runs with sys->sp_flg 0x2 (eneset.c:352, natively enemy_dispatch.c:144). The old
finding (frm_no 0, all tasks in 0x38) is the headless/replay default `--enemy-ai off` (runtime.cpp, world_enemies.cpp
masks sp_flg 0x2), which `verify_self_destruct.py` did not override. Measured with `--enemy-ai on --enemy-grab on`:
room entry RM_3230 frame 450, loop until 1018, waits 35/80/463 end at 1053/1098/1481 (script arithmetic
450+569+463), EN19 frm_no 82 at frame 1100 and 464 at 1500, EEVT of tasks 1/2/4 at 1482, 5/7 at 1572, door from 1573,
RM_3091 at frame 1869 without input (with door skip confirm 1580). Checker without SELECT row, with scene measurement and
`--enemy-ai on`; expiry now at frame 9956, md1 10 from 10029, motion 4/25/26 as before (counter-check without `--enemy-ai on`
red: frm_no 0). Native sources unchanged; `./build.sh --test` CTest 117/117, verify_self_destruct/enemy_ai/events/
stairs green, dev app 14:59. **2026-10-17, bhSetExplosionEffect:** weapon.c:1812 (23 kind 1 size 6 at pos - 4 * cam.v, two 24 with
random sequence sx/ay/ax, offset (0,0,-2) rotated) was already ported in `native/effect_sprites.cpp`, but without a
world owner: `RoomWorld::explode_effect` (cam.v from effect_context, under cam.flg 0x40 from the directly
written position) creates the sets, `GameOver::death_blast` binds gameover.c:539 (gov_etb, six calls at
gov_ct 40/54/68/82/96/110, test gameover_original). Plus the scene itself: gameover.c:278 bhClearEffect
(`RoomWorld::clear_effects`), :296-297 pt_flg 0x15, :301-302 sp_flg 0x6C (task 7 runs: `world->tick()` in md1 10
before task 14), :313-327 camera cam.flg 0x40 0/10/35 pers 11832 (`set_monitor_camera`, effect_context reads it),
game.c:318 effect drawing into the image of the death scene (`death_scene_append`, scene of the room owner adopted),
:313 txp[0] player textures (`death_body_textures`), :567 pt_flg 0. Measurement `verify_self_destruct`: eff[512] empty at
gov_ct 10 (previously room effect 122 occupied), at gov_ct 100 sets 23/24/25 = 4/18/86, no bhEff23/24/25 or
bhSetExplosionEffect gap in the log, end frames with 2582/30020 warm pixels at gov_ct 45/100 (baseline
0/0 with the binary 14:42). Counter-check binding removed: red at the effect sets, back to green. `./build.sh --test`
CTest 117/117, verify_self_destruct/enemy_ai/events/stairs/combat green, dev app 15:33. No new effect procedure
needed (23/24/25 were done). **2026-10-17, death scene player body/fog:** in md1 10 the body stood at the camera position instead of at the origin:
native/gameover.cpp shifted the body corners like the cmat net by (0,10,35), but game.c:357 bhPutModel draws the
bhCalcModel tree (MdlPut.c:240-264, px/py/pz 0, ay 35499) in world space under cam.mtx (camera.c:28-37, cmat * T(0,-10,-35));
offset removed. Measurement 640x480 at gov_ct 36: before fragment x 48..134 / rows 0..31 (clipped at the top), after
x 278..356 / rows 141..394 (feet 394 against 383.5 for y 0, head 141 = 16.3 units). Fog: no fog in the original
(gameover.c:242 njFogDisable, :323 gm_flg 0x200000 -> sync.c:143 bhControlEOR njFogDisable per frame); natively the
EOR fog part now runs along in md1 10 (`world->end_of_render()`), fog.enabled false at gov_ct 10 (previously true). Tests
gameover_original (body pixel difference with/without PlayerAsset; counter-check offset back red), CTest 117/117,
verify_self_destruct/enemy_ai/events/stairs/combat green, dev app 15:52. **2026-10-17, event explosion and RM_3230 event 3:** script command 0xc8 = event.c:8722 bhEffBakuDrmSet (bhScenarioJmpT[200],
sign byte + 3 u16/100) was unimplemented in the native VM (task blocked with "unimplemented opcode 0xc8"); now
native/events.cpp case 0xc8 (8 bytes) + RoomWorld::bind op 0xc8 -> explode_effect (weapon.c:1812, cam.v from effect_context).
Users per disassembler across all 205 room scripts (length table completed, no aborts): RM_5010 event 8 @09ea
(catapult lever with Tyrant in TY_BR0_PINCH/hp < 0, scd1 @026c = ELSE of IF rm_flg 0x4, en19.c:1068) and RM_9370 event 5
(10 calls). Headless RM_5010 (snapshot ev_flg 269, 3700 frames until ct_pinch 0, two lever pulls): before task 8 @0x9ea
blocked and scene frozen, after @0x9fb at frame 5360 with 2x23/3x24/14x25 in the pool, scene ends at 5600, only
gap the pre-existing bhEasySESet owP[0]. RM_3230 event 3: original behavior, no fix. Event 1 pauses enep[2]
(bhSubpl id 96, subpl.c:357 mv00_subpl0_1 without mode3 4 branch) via bhMotionPauseSet 0 as soon as frm_no >= 123; bhEwhile
(event.c:9147) returns 0 at loop end, the next enemy pass (bhSysCallGame before bhSysCallEvent, main.c:195)
still advances by 0x8000 to 123.5, after that the work stands; 124<<16 is never reached, the room change ends the task.
Measured natively: 123.0 at frame 509, 123.5/loop left at 510, mode3 4 from 511, task 3 at 0afd until RM_3091.
Tests event_lifecycle (RM_5010 event 8 with 23/24, counter-check without binding pending/empty; RM_3230 hold point 123.5),
CTest 117/117, verify_self_destruct/enemy_ai/events/stairs/event_freeze green, dev app 16:58. **2026-10-17, event VM 0x86/Obj_controll:** 0x86 = event.c:6117 bhEasySESet
(16 bytes, kind 0 plp/1 ene[enep.wrk_no]/2 obwp/3 itwp, node v7) calls sdfunc.c:1279 PlayGameSe4Event; type 7 =
CallNativeEventSe mode 1 never reads pPos (sdfunc.c:1861). Natively type 7 also required an owP position and discarded the call;
RM_5010 event 8 @0908 names obwp[0] (weapon, room.c:210 room objects only from 4, natively no weapon owP) -> gap
`bhEasySESet owP[0]` and missing sound. Now type 7 without position resolution (native/world.cpp). 2002 calls in 205 rooms,
kind 2 only with type 7, obwp[0] only RM_3070 @06fc/RM_5010 @0908. Headless RM_5010 (setup as for 0xc8): before 1 gap,
bank 2 sequence in event 8 seq2/3/7/11/8; after 0 gaps, seq2/3/6/7/11/8, 0 blocked. 0x66 = event.c:9708
bhObjCtr/Obj_controll: only cases 1 (2 bytes) and 20 (4 bytes), default without advance and without effect, return 1 -
the command byte runs as the next opcode (event.c:13047). `EventVm::obj_controll_length` + default `advance(1)`
instead of an exception. The only non-1/20 case of all rooms: RM_3020 event 6 command 12 -> 0x0c bhFlrAtariSet @040f ->
bhEnd @0413 (task stays there as in the original); no script in RM_3020 starts event 6. Tests event_lifecycle
(Obj_controll length table, RM_3020 event 6 explicitly without error at 0x413, RM_5010 event 8 without
bhEasySESet gap with bank 2 seq6 exactly once; both counter-checks red/green), CTest 117/117,
verify_events/event_freeze/enemy_ai/self_destruct/stairs/doors green, dev app 17:37.
**2026-10-17, RM_9370 event 5 / light of the death scene:** event 5 has no trigger of its own: scd0 @0054 clears
ev_flg 458, scd1 @0430 starts with 458 clear with all scene tasks @044c level 5 event 5 (entry from RM_9091 event 2
@036e, door 09/25); it waits with bhCamCheck (event.c:4881) for the cameras of event 2 and with bhFrameCheck for
enep[0/1] (bhSubpl 92/97, --enemy-ai on). Headless it already ran through (10x 0xc8, bhEvtEnd @23f2), but left all 22
0x6d = event.c:5028 bhCyodanSet (hits, bhSetEffParticle effsub3.c:911) pending; now RoomWorld::bind op 0x6d ->
EffectParticles::spawn. After: 10x 0xc8, 22x 0x6d, end ~frame 4760, blocked 0, no gap, live 23/24/25 (new
state field effects[].flg) frame 3620 7/12/110, 3800 0, 4700 4/10/48. Test event_lifecycle RM_9370 (natural
scd1 start, exactly 10 requests/10 fireballs 23 kind 1, no pending commands of event 5, particles > 0; counter-checks
without 0xc8 binding or without 0x6d binding respectively red/green); test scaffold now loads disc change/room bank from RoomIdentity like
runtime.cpp. verify_events now checks 0x6d as bound. Death scene: md1 10 sets gm_flg 0x80 (gameover.c:323), hence
bhSetHalfLight = bhSetEasyDirLight(0.6) (light.c:992/279, MdlPut.c:33); the body now goes via death_scene_append
through RoomWorld::append (category 1, no second light path) instead of the renderer host shading; body gov_ct 36
mean RGB 21.0/14.2/17.0 -> 16.8/11.5/14.1, light gray 0.2..0.657. Net: njDrawTriangle3D without light (corner color -1),
now world_lit without host shading (md1 2 mean R 18.1 -> 24.9). Test gameover_original (RM_3091 owner; counter-check
red/green). CTest 117/117, verify_events/event_freeze/enemy_ai/self_destruct/stairs/doors green, dev app 18:22.
**2026-10-17, bhEffClearEvt/bhEff085:** 0xca = event.c:8801 bhEffClearEvt (2 bytes) -> effect.c:159 bhClearEventEffect,
a selective release (not bhClearEffect): bhClrEff_YT (effect.c:166: 5/11/113/114/136/159/225/398, plus 22/24/25/27 with
helper set exp0), bhClrEff_RY (effsub3.c:117: 301, particle root 302 with exp0/exp1), bhClrEff_YG (effsub2.c:4793:
250/253/256/257/272/296-298, 254/260 with exp0), each only flg & 1. Now RoomWorld::bind op 0xca -> RoomWorld::clear_event_effects
(+ EffectParticles::clear_event). 51 calls in 27 rooms. Test event_lifecycle RM_9360 event 14 (entry with ev_flg 458,
setup explosion + hits): around 0xca 3x24/31x25/2x302 and 29 room effects 159 released, 33 helper sets released, 23 and everything else
unchanged; counter-check red/green. Headless RM_9370 -> RM_9360 (7000 frames): pending 1 -> 0. 0x91 bhEffModeSet was already
bound and only failed because of the missing procedure: bhEff085 (effsub6.c:18) now hand-ported - never draws, mode1 1 swaps
ene[rom->effp[type].lkno].mlwP->texP->textures[3].texaddr with sys->ef_tn[85] (backup in exp0, all works of the same
et_lp list), mode1 2 restores. In RM_9370 0x91 2 and 0 (event 0 @07ee/@07f2) run in the same scheduler pass before the
next bhControlEffect, hence the swap stays in place as in the original. Tests effect_eff085 (new) and event_lifecycle RM_9370
(1 @pass 3646, 2/0 @4551, texture swap shared with ef_tn[85]); counter-check red/green. Headless RM_9370 4700 frames: pending 7 -> 4
(only 0x86). CTest 118/118, verify_events/event_freeze/enemy_ai/self_destruct/stairs/doors green, dev app 19:02.
**2026-10-17, 0x86 room sound bank:** bhEasySESet (event.c:6117) always returns 1 after PlayGameSe4Event (event.c:6205);
type 7 -> CallNativeEventSe (sdfunc.c:1830) -> ExPlayMidi. MODHSYN.IRX (read only): program change .text 0x75c keeps no
program for entry 0xffffffff / number > last index / 0 splits, note-on 0x5f90 then starts no voice; the split loop
(0x6098) compares key limits &0x7f, uncovered key = no voice. The sequence keeps running. Not a format error of the base:
MULTSPQ2/0086 bank 2 programs 23/24/27..30 are empty entries, program 11 has only key 60 (seq 11 plays 60 and 61). Natively
decode_system_sound threw for MIDI notes and aborted the whole sequence; now a silent note (native/audio.cpp), and op 0x86 always
counts as executed, an unserved call is audio_unavailable 'bhEasySESet type T slot S SeNo N' (native/world.cpp).
Measurement of all 205 room scripts against the room package case 0: 9 type 7 calls affected (RM_3020/3130/6080x2/7100/9370x4), after 0;
RM_3150 @0cae/@0cc8 (seq 116/117) lie beyond the table of package case 0 (the room sets case 1 via 0xb6, package 3151 is missing
in the IDX; bank choice there open) and would now be a gap instead of pending. Tests audio_movie and
event_lifecycle (RM_9370 event 11), counter-checks red/green. Headless RM_9370 4700 frames: pending 4 -> 0, no gap.
**2026-10-17, MIDI multiple splits:** MODHSYN.IRX note-on (0x5f90, split loop 0x6098..0x64a0) starts a voice of its own for every split that
covers the key (no abort after a hit; voice 0x6264, key-on 0x63d0). Natively now `decode_system_note`
(native/audio.cpp/.h): one clip per split with its own volume/pan, in the sequencer one note layer per voice. Measurement: 80 package
sequences, 1757 notes with >1 voice (voices 13347 -> 15104); 7 type 7 calls (RM_0100/1100/1130x2/1140/2000/4050), voices 88 -> 104.
RM_1100 seq 0: right channel previously silent (RMS R 0 -> 831, L 833 unchanged). Test audio_movie (sum of the individual splits), counter-check
red/green; CTest 118/118, verify_audio_state/bgm_rooms/events/event_freeze/enemy_ai/doors green, dev app 19:46. Open: voice pool/stealing (24 per core) natively unlimited.
**2026-10-17, four user findings (dev app 21:19, CTest 121/121):** (1) barrel explosion without damage: the enemy part of
bhCheckBombAtari (weapon.c:1420) ran via the decommissioned EN01 facade with an empty recvx_en01_bomb; now on the real
enemy work (`RoomWorld::bomb_enemy_damage`) with watr capsule, bhCheckC2WallN 0x400 (new `check_c2_wall_n`, capsule radius 0.1
instead of a ray), joint choice and dam[]/comb_flg. Test `native_barrel_blast` (RM_0080 via the real hit path).
(2) RM_1130 water splashes: the draw pass took the start size of the room table instead of the one updated by bhEff249
(effect.c:1374 draws op->sx/sy/sz); frame 350 before: full-screen surfaces, now small droplet clouds (counter-check red).
(3) Typewriter: the typing sounds (0x80000242-244) have bit 31 and go via ExPlayMidi to MIDI slot 2
(sdfunc.c:1344-1385); `play_system` took them as a COMMON SE program and discarded them. Measured RM_0010: 24 sequences 2/66..68.
(4) Overlapping text in the pickup screen: it inherited the state of the previous status screen (FILE/Examine), and the
command description 180 - maincsr drew over the question; sub1.c:3308 starts it with StatusInit. Test `pickup_status_reset`.

**2026-10-17 late, RM_4020 grab arm and RM_1050 Steve (dev app 23:32, CTest 122/122):** (5) grab arm: the same lkono regression
as with the water splashes (loader set lkono 0 instead of efp->lkono at lkflg 0, room.c:877); bhEff134 checks the grab distance
against sys->obwp[op->lkono] (effsub1b.c:453-471) and read obwp[0] instead of the crate obj[4]. With the fix from 21:18 the run measures
clamps px 13/pz 10, descent py -40.5, nn 1.97 < 4 -> ev_flg 59, EN56 mode0 2; missed grab px 27 without flag. Test `crane_ufo_event`
(counter-check red), replay build/validation/crane-rm4020-20261017. (6) Steve hang: opcode 0xb0 bhHEffectSet2 (event.c:7472) was
not implemented; event 12 stopped at @0x144a and never reached CineSet 1 (@0x14b0) - hence the cinematic ran until SELECT.
The e.frame/frm_no hypothesis is refuted. 0xb0 writes sys->ef (22 bytes) and occupies bhEff211 (monitor noise) with lkono v4.
(7) Monitor black from a distance: njDrawTextureSub3DH writes XYZF2.F = 128 fixed (ps2_NaDraw.c:443, FGE on), the port fogged
the Thl quad depending on distance (from 85 units only fog color remained in cut 1). Now SceneTriangle::fog_fixed, measured
cut 1: 497 px, mean RGB 95/64/64 with a gray test image (= Cs*128 + ocol*127 >> 8). Both in `event_lifecycle_control_release`.
Runtime end frame cut 1 before RGB 54/6/6, after 42/13/12. Counter-checks: 0xb0 removed -> red (unimplemented @0x144a), F=128 sabotaged -> red (128/128/128).
Open: headless replay of the Steve event via the dialog path aborted (selection changes via right/left, not down); bhEff110 (full-screen view) unported.

**2026-10-18, game over retry (dev app 01:18, CTest 122/122):** user finding „after retry I die again right at the respawn point, the second retry works".
Cause: `ts_flg` bit 0x4000 is the one-time entry ticket of the game over start (player.c:6709-6714 requires `plp->flg 0x2` AND the bit and
consumes it; gameover.c:516/593 only returns it before `bhPopGameData`). The port cleared the bit but did not check it, and because
`plp->flg 0x2` stays set after death (latch `player_dead_`, world.h:394, only drops at hp > 0), every pass of the 70 frames of
`bhMainGameOver` md1 0 requested the start again -> a second game over immediately after `bhPopGameData`. Measured RM_0080 grab death: frame 1800 hp -20/md1 2,
after retry 1 hp 160/position clean, but md1 0->1->2 again; only retry 2 free. The same gap in the script command `bhGameOverSet` (event.c:8681-8689).
Both fixed in `native/runtime.cpp`. Counter-check on the sabotaged O3 binary against the same archives: red; `verify_gameover` and
`verify_self_destruct` still PASS. New checker `tests/native/gameover_retry_death_check.py`.
Side finding about the retry point checked and **refuted**: `bhInitPlayer` (player.c:848, bhPushGameData) only runs in mn_md0 1 (game start/load,
system.c:1397); room changes go via mn_md0 4 with `bhResetPlayer` (system.c:1776) without push. Further pushes: character change (system.c:1824),
saving (ps2_SaveScreen.c:1408), script 0xbd (event.c:8176); ss_flg 0x400 (system.c:1901) has no setter in the sources. The port models
exactly these places (runtime.cpp `!retry_point || player_change_push`, 0xbd, typewriter).

**2026-10-18, stairs (dev app 01:41, CTest 123/123):** (1) camera stutters on long stairs (RM_3080): `tick_camera` always followed
`plp->px/py/pz`, which in stair actions only catches up at the motion wrap (player.c:3258-3271); the original targets `owP[0]` under `plp->stflg 0x30` and
in mode0 7 (cut.c:1197-1217). Before 42/80 frames standstill and 3.3-unit jumps, now 0.07..0.34 per frame. Test
`camera_follow_target` (counter-check red), `verify_stairs` green. Open: free control cut.c:1207-1209 (EXP_WORK plx/plz) without owner.
(2) RM_1000 stair entry "only at a 90-degree angle": not reproduced; type 1 has no angle check in the original (hitchk.c:4677),
port float-exact identical, entry window 271 degrees at the natural standing position.

**2026-10-18, RM_3130 crash and camera entry (dev app 02:01, CTest 124/124):** (1) RM_3130 "Native runtime error: vector" after the
leave-room event: event 2 sets mtn_no 5 on the subplayer work via Common 24 (sel. 1), but the room only has 3 room motions;
`room_motions_.at(5)` threw. Original: `sys->rmthp` are 512 zeroed MN_WORK (room.c:700-706), `bhSetMotion` does nothing with datP NULL
(Motion.c:29). Test `room_motion_empty_rmthp_slot`. Reproduced via it_flg 232 + floor trigger 0, not in natural gameplay.
(2) RM_1000 door left: camera swung in from outside. According to the table spawn 2 lies just outside every cut surface,
`follow()` then left the raw CAM_WRK position in place; cut.c:663-710 computes the follow pose without a range limit. Before 24 units
of swing, now none. `tests/native/room_entry_camera_check.py` (counter-check red).
(3) Third person "switches to a fixed point at corners and on room entry": the cause is the avoidance logic of the host mod
(native/third_person.cpp `consider`): wall directly behind the player -> arm swings up to 180 degrees forward (frontal view), back after a few
steps. Verified RM_1000 spawn 1 (gate 2 units behind Claire, corridor narrow on both sides): frame 2/20 frontal, from 45 behind. The mod does
not switch off there (room scan: switch-off only at st_flg 0x4 stairs/steps and event cameras). Three attempts (swing <= 90 degrees, look in
walking direction, raise before retracting) do not solve the bottleneck: there is no space behind and to the side there, Claire drops out of the image.
Reset to HEAD; experimental state $TMPDIR/tpd/third_person.experiment.cpp. **Deferred as a gap by the user on 2026-10-18.**
**2026-10-20 fixed (only working tree/build, not delivered):** avoidance swing removed; arm stays behind the player, is
shortened and raised at walls, shoulder offset shrinks along, player/hair/weapon fade out when the eye is close (alpha down to 0.2).
Room entry scan (every 6th room, spawn 0/1, 30 ticks): camera in front of Claire 10 -> 0, behind 8 -> 52 of 53 free cases.
Test `third_person_camera_mod` (wall behind spawn; counter-check against old state red), CTest 173/173.

**2026-10-18, camera filter (dev app 09:46, CTest 125/125):** room effect gaps re-measured: of 89 room table IDs without a hand port
77 are `bhEffDmy` in the original (ids 30-84, delete themselves) or id >= 400 (bhControlEffect sets flg 0, effect.c:846); real gaps 12.
Largest: `bhEffFil` 90..92 (effsub1.c:28), the full-screen filter that 140 camera cuts in 61 rooms switch on via CAM_WRK flg 0x8000
(cut.c:412-421). Now ported (vignette, runtime RM_0070 image edge 14.9 -> 10.0). Test `effect_camera_filter` (counter-check red).
Rest: 110 (3 rooms), bhEffBG 93-97 (1-3 rooms each), 273 E14_Mucus, 112, 133, 138.

**2026-10-18 11:09, start crash fixed (dev app 11:09, CTest 125/125):** "Native runtime error: active camera outside table" immediately
at start. Caused by the camera entry fix 02:01: `follow()` now computed the follow pose also without a cut hit, but at the title
there is no room and no camera table. All checks ran with `--in-game` and skipped the title path. Now only with an
existing camera; `tests/native/room_entry_camera_check.py` additionally starts without a room (counter-check red with exactly this message).
Checked on the installed app binary: title, New Game up to RM_0000, LOAD GAME from the title, room entry RM_1000.

**2026-10-18 12:3x, flame streaks graveyard (dev app, CTest 126/126):** user finding RM_0020 cinematic scene: flying flame strokes around the
burning zombie and its head. Source clearly the joint flames `bhEff_Sub352` (without these sprites no strokes). Cause: the
renderer tiled every texture (REPEAT); bilinear fetched 8 flame texels of the opposite edge at the 48 px cell borders of the effect atlas. The original
sets GS CLAMP_1 from the TIM2 header: `Ps2TexLoad` (ps2_dummy.c:1650) passes ClampFlag, `Ps2CheckTextureAlpha` (:1749-1844) sets for
ClutChange 0 `0xFF000FF5` (WMS/WMT CLAMP), only Gindex 20500/15205..15209 `0xFF000FF0` (REPEAT); otherwise the file value stays (0xCC). Now
`SceneTexture::clamp_u/clamp_v` in the single TIM2 decoder (scene.cpp), in the renderer (filtered and unfiltered) and preserved with TGA/HD/
mod replacement. Evidence: frame 650 before/after 366 points difference, exactly the three strokes; 37 rooms frame 45 otherwise at most 22 points.
Test `gs_texture_clamp` (synthetic + real RM_6050 texture Gindex 20500), counter-check red three times (renderer, Gindex exception, ClutChange).
On the installed app binary: frame 650 identical to the check image, title start, New Game up to RM_0000, LOAD GAME. **Open:** the
texture cache path `Ps2DrawOTagSub` (ps2_dummy.c:1420) sets CLAMP/CLAMP fixed, also for the six REPEAT Gindices (RM_6050/5600/7000/3020),
in case these are drawn via the OT - not assigned. Also open from the same finding: gray bar at the bottom (cinematic frames ~410-426,
camera 12) and black flicker (frames 358-378). Caution counter-check: `make` compares seconds; restoring + touch in the same second
as the sabotage build left the sabotaged object in place (delivered like that once, rebuilt immediately) - wait at least 1 s before touch.

**2026-10-18, graveyard crate scene (event camera 12), flicker and gray bar - no port error found:** measurement per frame
(probe runtime in $TMPDIR, product sources untouched). (1) Black flicker frames ~383-406: in a 2-frame rhythm the whole image is ~30 %
darker. Effect 119 alone (smoke, blend 11/3 = Cd*(1-As)) causes it; hidden, the brightness stays even. Spawner
`bhEff103` kind 2 (effsub1.c:6080) sets a smoke cloud 119 kind 1 every second pass; the newest one sits 14.5 units in front of the
camera, corners +-1*sx (effect.c:506-520, sx 33) - it covers the whole image. 119 (effsub1.c:7890), 103, corners, bhDrawTrsEffect3D
(effect.c:1287), njDrawTextureSub3D discard (ps2_NaDraw.c:300-358, _Clip_Screen ps2_Vu1Strip.c:1202) and depth test match the
port. Not further decidable without an original recording. (2) Gray bar at the bottom (frames ~407-428): background color 0x181E2D
(rom->bak_col; bhChangeBackColor ps2_event.c:8 has no entry for stage 0 room 2). The floor is missing there because bhChangeViewClip
(event.c:13205, ViewClipSt0) sets near -9.0 for room 2 evc 12; the camera stands ~8 above the floor. Same natively. Side findings (ledger):
bhChangeClipVolume/bhChangeClipVolumeRM not ported (other stages), PRIM reading in effect_particles.cpp probably swapped.

**2026-10-18 afternoon (subagents, every change re-read against the original, dev app via `./build.sh --test`, CTest 127/127):**
(1) Save screen "white overlay": `draw_typewriter` did not clear the canvas; the original clears every frame to black
(bup_00.c:299-304, ps2_dummy.c:89). The room image stayed in the black-keyed holes of the background and the window
0x18808080 blended again frame by frame (gray ~118 instead of 12). Invisible headless (only the last frame drawn). Test
`typewriter_slot_list_draw` (counter-check red). (2) `bhChangeClipVolume`/`...RM` ported (view_clip.cpp, renderer strip rule);
measured byte-identical images in RM_0070/0090/0150/0020, only no longer discarded packets outside the image. Open: ClipDispW of the
negative rows (only DoubleMain reader), door/game over camera keep 320/240. Tests `view_clip`, `native_ground_world` (counter-check red).
(3) RM_0100 crate jump jolt: dnd sets plp->flr_no before py (player.c:3601); the host took the floor from py and wrote back the
flag state from the start of the action, so the final frame built the edge wall of the crate and pushed Claire 5.0-5.9 units away.
Now `stairs.floor` during stair actions and re-reading the flags before every tick. `tests/native/box_jump_landing_check.py`
(all three sides 0.000, counter-check red 5.045), `tools/verify_stairs.py` (RM_2050 expectation corrected), verify_push green.
(4) GS PRIM from bit 47 (ELF-verified): blood pool now a triangle strip instead of eight rectangles, particle sprites as GS sprites, n/2 sprites
per njPtclDrawSprite and width /1.174 (ps2_NinjaPtcl.c:100-163). Tests `original_effect_particles`, `effect_room_sprites` (red against HEAD).
On the installed app binary: title start, RM_1000 entry, crate jump, stairs, pushing.
Punch-through side finding followed up: bhEff023 mdlver 0 now draws without blending with alpha hole test and depth;
RGB filtering independent of alpha, also in the optional host mip filter. Counter-checks for list, coverage and RGB red,
CTest 129/129 green; crate jump and title/room entry now regularly in CTest. `Start.command` title/New Game
and LOAD GAME checked on the app binary. Details and limits: [check report](archived/work/2026-10-18-punch-through.md).
Integrated afterwards (subagents, every diff re-read against the original, own counter-check per port red, CTest 139/139,
dev app, `Start.command` title/New Game, LOAD GAME, crate jump, stairs, pushing on the app binary):
(a) bhChangeBackColor/Evt (`native/back_color.*`, 132 table rows, EOR timer sync.c:114, 0xab immediately into rom->bak_col;
ts_flg now starts with 0x20000 as in system.c:453). RM_7070 camera 3: table color 0x205070 through the grating. partial: global
njSetBackColor of other tasks, target alpha/SSAA. (b) Fog color without R/B swap (canvas is ARGB; old AABBGGRR comments wrong;
`ui.cpp` direct_message still has the same swap, open). (c) UV scrolls bhObj010, bhEff120 (ELF: -floor(az/32)),
bhEffectSensyaSet 0x96 and OT CLAMP only on the verified bhPutModel route (tank tread tex 9). (d) bhEff110 full-screen monitor and
bhEff133 thermometer (RM_1050/4030/8050, RM_7050). (e) Depth GEQUAL: a later surface of equal depth wins, edge rule for all,
material alpha 0 discarded; 10-room comparison only edge pixels or +-1 respectively. (f) bhEff138/bhDraw138 oil puzzle RM_8001 (test `native_oil_puzzle_world`, counter-check ev64 red; bhDraw138 partial: shared OT
of other drawers). (g) pt_flg draw gates room 0x20/objects 0x4/items 0x8/linked>=1210 0x1 (game.c:328, objitm.c:503/550/639), test
`native_presentation_gates`. (h) UI color channels (direct_message, FE01..05 message.c:161/401), test `ui_color_channels`.
(i) one owner for Db.clear across door/game over/map/typewriter/movie/terminal/status (back_color.*). CTest 142/142,
dev app, verify_doors green. Open: water surface bhDrawGeneralPurposeWater without draw path, bhEff112 (mirror pass runs),
filtered alpha test quantization, GS 16-bit Z.
Addendum: (j) mirror pass/bhEff112/njMirror (RM_2040, test `mirror_pass`; open: mirrored bhDrawEffect game.c:91).
(k) Crate equipment slot ArmsSet center 9 (sub1.c:4975-5105, test `item_box_equipment_slot`). (l) Muzzle light type 100/101
(light.c:673-699, test `native_lighter_effect`). (m) EN01 blood pool bhSetBloodPoolLnk (en01.c:9990/9994/10122/10149, en01b.c:763,
tests `enemy_en01_blood_pool`, `zombie_blood_pool`). (n) GetItem screen without status pad (sub1.c:3447-3460, test `pickup_gap_pad`).
RM_0050 sliding back after the zombie event is original behavior (event.c:12034 Common 7 without spx, player.c:2127).
(o) Ponytail as obwp[2] on the link work (event.c:2177/2501, objitm.c:310), test `player_hair_link`. (p) RM_3060 water SE ListNo char
(types.h:1754), test `rm3060_water_action_se`. (q) RM_1140 EOR after the event pass, test `rm1140_lever_back_color`. (r) Combat wall capsule
bhCheckC2WallN (pwksub.c:2172), test `combat_target_rules`; hits/auto-aim otherwise original. (s) EN26 Anatomist zombie RM_0110
(en26.c), test `rm0110_event_zombie`. (t) Muzzle flash call clip (ps2_NaDraw.c:327), blood pool lkwkp/pt_flg gate (effsub3.c:389).
(u) bhDrawGeneralPurposeWater (objitm.c:471, test `object_water_wave`). (v) bhEff362/370/371-373 (effsub4.c). (w) Light types 7-12
and gm_flg 0x80 return (light.c:367/486-657, test `light_types_7_12`). (x) bhStartDoorDemo fade and door task modes
(room.c:949-1009, system.c:939-1061, verify_doors). (y) Mirrored bhDrawEffect (game.c:87/427, effect.c:1225, test `mirror_pass`).
(z) Frame order (game.c:48-59/289-296): "camera before player" was a false finding (tick_camera = bhCheckCut, cut.c:12). Ported: enemies ->
player -> effect/ObjItm/light, head/bhCalcModel only with sp_flg 0x1 (player.c:1365), bhCheckCut after drawing, camera snapshot
game.c:58 for drawing/effects. Tests player_head_pass_gate, effect_after_player, event_camera_first_frame, camera_control_order.
(aa) bhSetDispObj player (op 0x20): plp->mdflg 0x1 now as player_model_hidden_ (model only, does not block the pass) instead of player_hidden;
pl_sleep_cnt special case moved from op 0x20 to bhInitModelSet (op 0x24). Test event_lifecycle_control_release (counter-check red).
(ab) bhInitCamSet (0x5b) now calls bhControlLight (event.c:4745): light pass split out as RoomWorld::control_lights(), in
tick_after_player (game.c:59) and in the 0x5b branch of the runtime after control_camera. Test light_types_7_12 (RM_1020 type 9: pass + extra call
= 2 steps; counter-check red "outside the pass"). (ac) Light link death light.c:719-748: for each light with flg 0x1|0x2 the target work
(lkflg 1 plp, 2 ene, 3 obwp, 4 itwp; 5 eff without owner, >5 lkflg/lkno = 0) is checked for flg 0x1, otherwise flg &= ~0x3 permanently. Finding:
the work flg bit 0x1 comes from the ETTY flg and has no writer in the original for ene/obwp/itwp/plp; all 33 linked lights of the
room data (lk 2: 19, lk 3: 14) point to active works, the rule is unreachable with game data and only verified via the check seam
set_work_flags (test light_types_7_12 RM_3230 light 5 -> obwp[7], counter-check red "link death") (ad) water light draw_tp 0 (RM_9110) verified without code change: bhDrawGeneralPurposeWater ->
bhDrawSpObject sets light 1 = lgtp[aspd] itself (objitm.c:483/696-703), point lights bhSetHalfLight 0.5 (game.c:352), amb_chr (game.c:344);
bhDrawEnemy/bhPutModel(plp)/bhDrawEffect write no EasyMulti colors - identical to the native ob_spc glint set. CTest 158/158, counter-check per fix
red/green, dev app room entry/lever/RM_0110/blood pool/crate/LOAD GAME/door green.
(ae) Mirror bhDrawMdfEffect (game.c:427 -> effect.c:866/1076, second modifier stage under cam.mtx*njMirror): SceneShadow::mirror,
renderer stage between mirror room (phase 2) and mirror effects (phase 3) with mirror_view; world.cpp marks the modifiers created in the mirror call
instead of discarding them. Finding: the pass choice remains the screen sign of njCnkCoP3 (ps2_NinjaCnk.c:1684,
lCnkModClipFace), game.c:425 njControl3D(0x100) no longer carries 0x10000 before the mirror bhDrawEffect -> a mirrored closed
volume swaps front/back faces and yields the same (empty) mask as a pre-mirrored volume in the normal pass (original
semantics, no correction invented). Test shadow_modifier_pixels: equality mirror modifier == pre-mirrored volume, M*M*T visible
only via mirror_view, mirror room receives / mirror effects afterwards, rejection without njMirror matrix; three counter-checks red (view instead of
mirror_view, stage before the mirror room, stage after the mirror effects). CTest 158/158.
(af) bhDraw134 second run in the mirror closed without code change: the mirror pass depends on effect 112 (room tables only RM_2040/9230,
bhEff112 only stage 2), bhDraw134 on effect 134 (only RM_4020/8020); no room carries both, scripts spawn neither 112 nor 134
(bhHEffectSet2/bhSetEffect of all 205 room disassemblies). The ef_fnc second run (effect.c:929 under game.c:427) is thus unreachable for 134.
Open: image acceptance water rooms/head burst/mirror room RM_2040 (visual check in game).
(ag) Room scan 205/205 with --enemy-ai on (600 frames, dev binary 2026-10-19): 0 runtime errors. The older scan without enemy AI reported 94
unbindable commands (Common 24/25/34 on uninitialized works) - artifact of the AI switch-off, 0 with AI. Remaining reports: bhEffModeSet
0x91 on texture holders (RM_7140, now bound, test event_lifecycle, counter-check red); 0x23 behind rom->etcp (RM_3100/3140/7030, original UB);
bhSetMotion frm_no 9226 on a 2-frame room motion (RM_3120/3220, hidden enemy 2, original UB, ledger); bhObj005 neighbor cell (12 rooms, UB);
unported: bhEff_Sub350 (RM_1120/5580), bhEff301 (RM_5810/9302), bhEne06 (RM_6030), bhEne22 (RM_7100), plp mnwPb[mtn_no] (RM_7250).
(ah) bhEff_Sub350/351 ported (poison spit of the EN01 poison zombies, en01.c:11190 bhEne01_PoisonEffect; effsub4.c:319/538): trajectory, wait time,
floor impact (bhCollisionCheckLine2 0x400, position +0.5), player hit against plp->watr (damage 10, mode0 2/mode2 1/mode3 0, flg 0x4, mode1 from
bhDGCdirCheck) with three 351 at plp->djnt_no (new effect hook player_hit_check_parts = zonzon.c:1376 bhEne_HitCheckParts on the player tree),
SE 8961 via bhEne01_SePlay(lkwkp) with flg 0x10000 gate, splash 351 (wait time, unlink, fade out, 14 frames). Before, the spit did
nothing natively (gap 'bhEff350'). Test effect_eff350 (counter-checks red), headless RM_1120/5580 with enemy AI without gap, 350/351 alive.
(ai) bhEff301 ported (blood stain from zonzon1.c:301 bhEne_SetBloodstain, four times per enemy hit; effsub3.c:404-808): scatter/ray,
floor (case 4-6), wall (8-10), type 6 set (16-17), water (12-14, bhCheckWater with h via new service set_effect_water_area), lens
(bhCheckCamWall2D effsub3.c:1611 against cam.mtx, case 24/25 as ef_trs2d, gm_flg 0x20 and cam.flg 0x10 = ci->flg 0x200), ATR flg 0x10,
sys->bl_ct staggering, gm_flg 0x2000000. EffectContext now carries cam.mtx (camera_view_matrix) and cam.flg 0x10. Previously natively
no blood stains appeared on walls/floor (gap 'bhEff301'). Test effect_eff301 (counter-checks red), headless RM_5810/9302 with enemy AI without
gap. CTest 160/160, dev app via `./build.sh --test`. Remaining from the AI scan: bhEne06 (RM_6030), bhEne22 (RM_7100), plp mnwPb[mtn_no]
(RM_7250). **No commit/push.**
(aj) bhEne22 ported (Albinoid adult, RM_7100; native/enemies/en22.c, jump[22], services RECVX_EN22_SERVICES): init/brain/MV00-06/DG/DD,
player damage, collision, water effects, electric shock (354), spark attack mtn 9 with bhEne22_ChgDengekiColor/GetDengekiColorAddr on the
writable streams mdl[1..6].objP[1], floor light 353, lightning models 364 and rom->lgtp[2] (SetLight/CtrLight, Light.ct1). New as well
bhEff_Sub353/354/364 (effsub4.c:837/958/1774; 354 as bhEff_3DSpriteDraw billboard with enemy texture) and RoomWorld::water_splash2
(pwksub.c:2572, now also bound for EN01). **Open:** bhEff_ModelDraw (effsub4.c:3306) does not yet draw the 364 lightning models
(njCnkSimpleMultiDrawModel color path without host path, named gap in the draw pass). Test enemy_en22_world (five counter-checks red),
headless RM_7100 with enemy AI without a new gap, CTest 161/161, dev app via `./build.sh --test`. **No commit/push.**
(ak) bhEne06 ported (moth, RM_6030; native/enemies/en06.c, jump[6], fragments bhEne06s on jump[31] via SetCallFunc, hidden
leader for the player grab): flight, leaning on walls, grab with player motion 20 from the moth library, moth egg (id 27 work + plp->stflg
mark), hit/crash/shredding with twelve falling fragments on the owP matrices of the moth. New bhEff_E06_Rinpun (260) with
particle drawing from mdl[1].texP. Test enemy_en06_world runs via the real room script (event 16 releases moth 3 at flag 4/0),
six counter-checks red. **Open:** (1) dust player contact: the original averages with never-set FPU registers $f20-$f22 (ELF verified),
named gap; (2) EN27 (moth egg) is unbound on the world side and reports its services; bhCheckMothEgg is missing in the player owner.
CTest 162/162, dev app via `./build.sh --test`, title start checked. **No commit/push.**
(al) RM_7250 spider grab: the cause was a second writer of plp->mnwP — the EN03 leader hooks in the spider library via a
service and afterwards writes back its old player snapshot, which set mnwP back to mnwPb (grab motion 62/63 was missing
there). enemy_player_set now adopts the library fields only if the view changed them itself. Check rm7250_spider_grab.
(am) EN27 moth egg attached: egg on plp->mdl[6]/[7] with player motions 93..97, root at player joint 3 (new lk_player for
lkwkp = plp, gates read plp->stflg), bhCheckMothEgg before the first enemy pass, plp->stflg & 0x78280000 across the room change (marks
were lost so far), player row without room texture fallback. Test enemy_en27_world (init, drawing, hatching, grab, damage/poison,
release, fall), counter-checks red; the EN06 test now checks the laid egg without a gap. Fresh room scan with enemy AI (205 rooms):
only named original undefined behaviors remain (bhObj005, bhCheckRoute, bhSetMotion channels/RM_3120 Common 34 frm_no 0x240A,
en02 ang, ef_tn before render cut). CTest 164/164. **No commit/push.**
(an) bhEff_ModelDraw (effsub4.c:3306) drawn for the albinoid lightning models (364): njCnkSimpleMultiDrawModel is the Ninja mode
of the enemy drawing (MdlPut.c:182), the single lights set before (mode 2) have no effect; light category 4 like bhEff30cDrw,
material colors from the stream written by bhEne22_ChgDengekiColor. EN22 test checks image and color (two counter-checks red).
Mirror pass remains a named gap. CTest 164/164. **No commit/push.**
(ao) Five effect procedures already requested natively but unported are now ported: 356 zombie neck bleeding after head burst (callbacks in
EN01 from the effect pass; EnemyCallbackFrame binds the T3 supplement and EN01 context for that), 363 flesh chunks of the leg/worm explosion,
252 flame at the enemy joint (bhEne_SetFireEffect), 259 Hunter blood, 26 screen streaks on explosion death. Tests effect_en01_exp,
effect_e00_fire (counter-checks red). progression_test tracked 26 as unbound; guard now on 6 (bhEff006, really unported).
Next: EN17 (Steve) has unbound effect services (SetPtcl 365/366, axe afterimage 364, lgtp[2], SetBlood2(plp)). CTest 166/166.
**No commit/push.**
(ap) EN17 (Monster Steve): all previously unbound effect services attached. Newly ported are the spawners bhEff_SetPtcl/SetPtcl1
(effsub4.c:19/35) as well as bhEff_SetPtcl2/SetPtcl2V (:117/:199) as RoomWorld::set_ptcl/set_ptcl1/set_ptcl2/set_ptcl2v with their
bhEff_AllocOwork helper work sets (EffectPtcl365Work/EffectPtcl366Work) and the procedures bhEff_Sub365 (:1812) and bhEff_Sub366
(:1884) including the draw part bhEff_LineDraw (:3090, untextured triangles with their own corner colors). Also bound are
bhEne17_AfterimageAxEffect (effect 364, axe afterimage on epw->mdl[5]), bhEne17_SetLight (rom->lgtp[2]), bhEne_SetBlood2(plp) and
mlwP->objP[0].pos[0/2] = 0 on the enemy as well as on the player tree. For the afterimage bhEff_ModelDraw now has its flr_no branch:
npSetAllMatColor (njplus.c:1375) colors the materials of the drawn object, so that the seven afterimages of a frame keep their
own alpha levels; for this the port colors a copy of the model stream instead of the shared stream (equivalent, because
only this effect draws mdl[5] and every drawer colors it itself beforehand). Three deviations in native/enemies/en17.c corrected along the way:
plp->djnt_no = 2 with plp->dpx..dpz = cap.c1 on hit and plp->djnt_no = 3 before bhEne_SetBlood2 were missing entirely (en17.c:1594/1653),
bhEne_SetVibration(1) is StartVibrationEx(1, 0xA) and stood as (1, 11), and a duplicate assignment of cap.c1 with the wrong index
is removed. **The display of effect 365 remains open and named:** bhEff_PtclSpriteDraw passes n = 1 to njPtclDrawSprite, and
Ps2AddPrim3D emits exactly n vertices per GIF tag with a newly set PRIM — a GS sprite needs two. The set therefore runs,
but puts no surface into the image; without hardware evidence nothing is invented here. Test enemy_en17_world extended by particles, afterimage, light,
player blood, joint and vibration; the previously allowed gap names bhEff369/370/301 have been dropped without replacement,
because these procedures have long been ported and no gap occurs at all on the paths run — the test now allows
not a single one. Fourteen counter-checks red. 205/205 rooms load, the enemy AI sweep over all rooms still reports only the six known
original undefined behaviors. CTest 166/166. **No commit/push.**
(aq) Magazine on reload (bhEff007) ported and two errors in the draw path sys->ef_pol fixed. bhEff007 (effsub1.c:2335) was
not ported, although the combat core requests it on every reload (pwksub.c:3372 bhSetMagazine, natively cb_set_magazine): the set
was never created and the port reported bhEff007. Now the magazine falls from the wrist, bounces with CallPlayerWeaponSeEx(259),
rotates while falling and comes to rest half a unit lower; in water it slows down and sets splash 15.
The magazine IN the weapon now also has its owner: bhEff007 writes, with mdlver != 0, via RoomWorld to the
weapon asset owner CombatController; WeaponAsset::append only omits the right node model 2, not children
or the left model. player.c:6075 clears the flag at act_ct1 ONLY for weapon 12; the existing clear request
is consumed once. Weapon 13 stays hidden as in the original. A combat abort keeps the flag, a new
weapon asset resets it (dread.c:200-210). Test effect_magazine with real reload paths 12/13 and diagnostically
empty inventory, draw counts, abort/reload; six further counter-checks red. Material writers of the
hidden node model are also dropped, positions of the remaining writers stay correct.
[Check report](build/validation/magazine-owner-20261019/README.md). Finally ./build.sh --test: 169/169 green,
dev app backed up/updated/signed; Start.command title and reload checked (motion 116, effect 7, no
blocked commands/open world bindings), arm64/signature/policy green. Runtime images viewed, but camera
behind the figure: no separate visual acceptance of the magazine change. No commit/push.
On attaching, two deviations in bhDrawPolEffect (effect.c:949) stood out, both verified via disassembly: (1) the texture is
sys->ef_tlist.textures[tex_id + ani_ct] WITHOUT sys->ef_tn (ELF 0x0021D3F8 against 0x0021DEBC in bhDrawTrsEffect3D) — the port looked up the
category, which made the cartridge case carry the shared image 19 instead of 5 and the marking 25 instead of 8; (2) only
objP[op->mdlver] is drawn, the port drew the whole efm[0] and thus both case objects at once. Both are corrected, the four
model blocks from bhInitEffect (efm[0]/[1]/[7]/[13]) are available individually for that. New test effect_magazine, plus the block
pol_effect_draw in effect_room_sprites; eleven counter-checks red. In addition: the gap list in the EN17 test allowed bhEff369/370/301,
although these procedures are ported and no gap occurs at all on the paths run — the test now allows none,
ledger and entry (ap) are corrected accordingly. 205/205 rooms load, enemy AI sweep unchanged at the six known
original undefined behaviors. CTest 167/167. **No commit/push.**
(ar) The two sand script commands of the Gulp Worm ported: 0x98 bhEffectSandSet (event.c:4110) and 0xC3
bhEffectSandSetMatsumoto (:8577) were missing in the event VM, although their targets bhEne02_SetSandEffect and
bhEne02_SetSandEffectEV already existed natively (the EN02 row named both as open). 0x98 is 12 bytes long:
sign bits for three u16 amounts in hundredths, a position kind (world position or offset, which only adds x and z to the
enemy position) and a type byte, after which the script pointer advances by two — checked via disassembly 0x0015CAF0,
because a wrong length would shift the whole script stream. 0xC3 takes the work slot of
sys->ene, not the room index. Because the EN02 services are only bound natively during the enemy pass, the
script pass queues the fully resolved request and the enemy pass of the same frame executes it before the enemies —
the same order as in the original. Unknown targets stay unbound instead of guessed. With that 197 of 223
script commands are bound (previously 195). Test enemy_en02_world extended by three blocks; seven counter-checks red.
205/205 rooms load, enemy AI sweep unchanged. CTest 167/167. **No commit/push.**
(as) Look commands of the player head ported and the situation for the remaining script commands clarified. 0x40
bhLookGsetPlStart (event.c:3488) and 0x41 bhLookGsetPlStop (:3539) write the head state plp->exp1, which the port
already carries with RECVX_HEAD_STATE: start sets the two look flags, picks the target kind stored point and
enters the point from the script, stop only clears the two flags. With that 199 of 223 script commands are
bound. Important finding along the way: an unknown script command throws in the event VM and stops the room, so the
remaining seven are possible hard stops as soon as a script branch reaches them — in 205 rooms x 600
frames of init run none is reached. Their lengths and effects are now recorded in the ledger row
event.c:bhScenarioJmpT:offen, so that they can be followed up individually; the most expensive is 0x1E
bhAdxTimeCk, a condition that needs the playback time of an ADX stream and has no owner for it natively.
Also checked: of the 17 commands reported as 'only consumed', all are in truth bound to the room owner
(the object bindings via a table that the counter does not see). New world block in the test player_head
(which now gets the data directory); seven counter-checks red. 205/205 rooms load, enemy AI sweep unchanged.
CTest 167/167. **No commit/push.**
(at) Six of the seven remaining script commands no longer stop the room, and three previously ineffective ones are
bound. Their effect has no owner
natively (player as child of an object binding, the two hand object works, sys->efid, the event task works,
the debug camera, the Battle Game ranking), but their length is fixed in the original: 0x6F 12, 0x82 4, 0x97 8,
0xAA 10, 0xAF 4, 0xD3 2 bytes. They are now skipped exactly that far and reported as an open binding, instead of
ending the run with `unimplemented opcode`. What remains is 0x1E bhAdxTimeCk as the only hard stop — that is
a condition that delivers the playback time of an ADX stream to an IF; skipping it would mean inventing
the branch. New test event_opcode_lengths: it puts each of the ten commands handled in this state at the beginning
of an event script, places a bhSet behind it and checks that exactly this bhSet is executed — a wrong
length would shift the whole stream behind it. Eight counter-checks with wrong length red, plus a counter-check of the
fixture itself (a bhSet shifted by one byte must not run). Also bound: 0x3A bhMutekiSetPl (player invulnerable, plp->flg 0x4 with mirroring
to player_flags), 0x2E bhMotionPauseSetPly (sp_flg 0x1, the gate of the player pass) and 0xA1 bhTrapDamageSet
(plp->stflg 0x1000, set with the value 0 in the original). All three only write flag bits that the event VM itself
carries — so far they were read and handed to the room owner, which did not bind them, so they went nowhere.
Plus 0x46 bhSetNextRoomBgm and 0x47 bhSetNextRoomBgSe, which write the music and room sound presets for the next room. Along the way a finding that refutes my assumption from the last report: the target structure NextSoundInfo (sdfunc.c:90) has no reader at all in the original — across all 154 sources it appears only in these two commands and in its definition. So they have no audible effect in the original either; therefore no wrong music can play after a door. The port still carries the state, so that the commands are bound and a later reader finds the right values. With that 204 of 223 script commands are bound. 205/205 rooms load, 0 blocked commands, enemy AI sweep
unchanged. CTest 168/168. **No commit/push.**
(au) No script command stops a room anymore. 0x1E bhAdxTimeCk (event.c:1523) was the last one without a native case.
It is a condition on the playback time of an ADX stream; modes 0..3 return 0 IF the condition holds,
modes 4 and 5 return 1, and a mode above 5 returns the raw time (no default in the original). The
time source is new: AudioMixer::adx_play_time converts the mixer's channel position with the same formula as
adxwrap.c:650 (SampleCount / SamplingRate * 100). Channel::clip_rate survives the natural clip end, so that a
stream that ran to the end keeps its end time — otherwise a wait loop in the script hangs on a time of 0.
SIDE FINDING, found myself while re-reading: the execution loop of the event VM continued on return value == 1,
but the original on EVERY value other than 0 (event.c:13046). Observable for commands that can return something other
than 0/1 — bhAdxTimeCk with mode above 5 and bhCkFlg with `mode ^ flag`. Corrected; CTest still
green, so no shipped script was affected so far. Test event_opcode_lengths now covers sixteen commands;
six further counter-checks red. With that: 205 of 223 script commands bound, 18 skipped with correct length
and reported, none unknown anymore. 205/205 rooms load, 0 blocked commands, enemy AI sweep unchanged.
CTest 168/168. **No commit/push.**
(av) Hide masks of the event camera at the end of the track corrected. cut.c:1803 bhSetEventHideObjLgt writes the
object and light mask of a key ONCE; it stays until the next call. It is called when the
camera is set, in the init pass and after every key change — NOT at the key change that leads into the final hold
(cut.c:2124 breaks off there with `break` before the call). In the hold phase the camera therefore stands on the last
key, but the masks still on the second to last. The port read the masks per frame from the current key and showed at the end
of every cutscene the occlusion of the LAST key. RoomWorld::event_camera_mask_key_ now holds the key
with which the original last wrote them. Own scan over all room data: 383 of 1362 ending
camera tracks in 82 rooms have different masks there; in RM_0000 track 33 the second to last key hides a whole
object group, the last one nothing — these objects became visible at the end of the sequence. New test event_camera_hide
(RM_0000 track 33); three counter-checks red. The finding comes from one of the five parallel investigations and was
re-checked line by line in the original and with an own room data scan before implementation.
The interaction with bhControlLight is also closed: set_event_camera_masks writes the light mask
once into Light::flags bit 0x2 at the original call sites; light pass and drawing read this state.
Type 100/101 stay off after going out until the next mask call, without clock/mode reset; type 13
and link death lose active bit 0x1, which the mask does not restore (light.c:669/:680/:696/:749).
Extended test event_camera_hide: diagnostically changed light fields/key masks in RM_0000, four types,
light contribution on the room triangle, last light pass, repetition, key change, final hold and FixedCut.
Three further counter-checks red; [check logs](build/validation/event-camera-light-20261019/).
Earlier state: 205/205 rooms load, 0 blocked commands, enemy AI sweep unchanged, CTest 169/169.
Currently CTest 169/169, policy/ledger green; dev app backed up, updated and signed via ./build.sh --test.
Start.command checked: title and RM_0000 with enemy AI 60 frames each headless, exit 0, no blocked commands/
open world bindings; arm64/signature checked. [Report](build/validation/event-camera-light-20261019/README.md).
No visual acceptance. **No commit/push.**
(aw) EN01 njRandom sign adopted into main from the worktree session "Check EN01 njRandom sign" and
re-checked: macros.h:105 with RAND_MAX+1 = -2^31 and disassembly 0x1882C8/0x1885E8 (rand -> negu -> cvt.s.w ->
div.s by 0xCF000000) yield a POSITIVE value in [0,1]; en01_core.c computed it negative, so size and
speed of the ExpLeg/ExpWorm fragments were below the original range. All other enemy ports
were already positive. New test enemy_en01_exp_effect (428 checks), counter-check with old expression red.
The worktree itself is discarded. **No commit/push.**
(ax) 0xAF bhPlyHandChange (event.c:7378) bound instead of only reported: two room objects hang on the hands
(lkono 9/13, flg 0x80, stflg 0x1000000) and the weapon models obwp[0]/[1] disappear from the
draw list via mdflg 0x1 (new owner CombatController, reset by gameover.c:304 and when loading a weapon).
Putting down leaves the object at the last hand position. Data: RM_9110, RM_9140, RM_9260. Open and named: the
parking of a drawn lighter, because plp->wpnr_no is natively not separated from the inventory slot. Test
player_hand_change with real RM_9110 bytes, eight counter-checks red. **No commit/push.**
(ay) Weapon in cutscenes and during enemy grabs corrected: under script control the port drew the
model of weapon 0 (empty hands) instead of the equipped weapon — substitute logic from 2026-10-05, already noted back then
without original evidence. The original changes the weapon model only via bhReadWeaponData (equip,
bhArmsItemChange, game start) and draws it under pt_flg 0x1 (objitm.c:364/:648); this gate was also missing
natively. Test player_hand_change extended, two counter-checks red. Open: 0x27 bhArmsItemChange natively
mapped wrongly (clears the inventory slot), needs its own plp->wpnr_no owner. **No commit/push.**
(az) Drawn weapon plp->wpnr_no separated from the equipped slot pip[0] (Inventory::wpnr_no). Previously
the port derived the weapon from the slot. Now WeaponSet/GetItem/ItemBoxChange write both, bhArmsItemSet 0xC1
and bhSv 22 only the slot, bhArmsItemChange 0x27 / bhSv 18 / 0xAF only the weapon; player change and save game
carry it via ply_wno. Fixed with that: 0x27 with 0 cleared the inventory slot, other values had no effect; and
the lighter parking from 0xAF is ported (0xAF now complete). Limit: bhSv 18 loads no new
weapon model in the original, natively it does (never reached in the room scan). Four tests set only pip[0] in their setup
and are supplemented or corrected to the original respectively. CTest 172/172, five weapon-relevant runtime checks
green, eight counter-checks red. [Logs](build/validation/wpnr-no-20261020/). **No commit/push.**
(ba) 0x97 bhEffectKokuenSet corrected and 0xAA bhEffUVSet2 ported. 0x97 was unchecked in the working tree and
set the smoke at the world position of the task work; the original takes the local position of objP[0]. 0xAA writes
UVs as 28-byte H vertices to op->pv; for effects without H vertices this reaches into tv. The only
data location RM_9180 (full-screen filter bhEffFil): there the original thereby changes tv[1].u - now natively as well.
Test event_kokuen, six counter-checks red. Still skipped with that: 0x6F, 0x82, 0xD3. **No commit/push.**
(bb) 0x6F bhObjLinkSetObjPly ported: the script hangs the player onto an object node, bhCalcModel(plp)
then builds her root from obwp[lkwkp].owP[lkono] * T(lox,loy,loz) * RXYZ(ax,ay,az) (MdlPut.c:242). Natively
PlayerObjectLink + PlayerAsset::sample_root in RoomWorld::sample_player. Only data locations RM_4011 (object 6,
node 3, script-controlled). Open and named: binding in the free player pass (not present in the data).
Test player_object_link with real bytes, six counter-checks red. Only 0x82 (debug camera, unused in
the data) and 0xD3 (Battle Game ranking) remain skipped. **No commit/push.**
(bc) Open light gaps and parallel tasks (2026-10-20). (1) Screen clipping of the point lights (light.c:789-808)
ported: without an event camera and with cutp[cam.ncut].cam[0].flg 0x10 a point light outside the 640x480 window
with margin 2*(u8)ci->frame (CAM_WRK byte 2, SceneCamera::key_frame) is dropped without occupying one of the three slots; 14 cuts in
11 rooms carry the flag. The monitor image draws under gm_flg 0x80 (screen.c:947) with the single light and is not
affected. Link death lkflg 1..4/default was already in control_lights, the ledger row did not say so. Test light_clip
(RM_0000 bytes, own projection computation), five counter-checks red. (2) bhAllEasyDrawModel (game.c:391-403) calls no
bhDrawEneObject: the monitor pass now leaves out en_obj works (flg 0x80000000); no data case (en_obj only RM_7120/9290,
monitors only RM_1050/7030), no test. (3) In parallel by subagents, re-checked: bhEff199 (effsub0.c:3478) as
native/effect_eff199.cpp with test effect_eff199, partial (Ps2_ptcl_spr_flag, control pass drawing); unreachable in data/scripts.
Effect 298 bhEff_E00_AcidGenerator has been ported since 2026-10-16 — the note "bhEff298 unported" above
(EN19 entry) is stale, as is "EN14 gaps": inventory showed all EN14 services bound, EN14 only in RM_5810/9302
(type 0), new test enemy_en14_world; its first expectations contradicted the original (en14.c:297 clears flg 0x10,
bhSetMotion runs in the same pass) and were corrected. (4) bhDrawObject/bhDrawObjItm mdflg 0x10 (ETTY flg 0x80000000,
objitm.c:164/:228) ported: under st_flg 0x2 these objects/items draw without fog (:664-676/:565-567; not ob_spc);
23 objects and 5 items in 23 rooms carry the bit (among others RM_0090 object 20, RM_0000 item 0). mdflg 0x8 njControl3D(0x2500)
has no effect (ps2_NinjaCnk.c:205-215 does not read 0x2000/0x400). Test object_cut_restriction section 3, two counter-checks
red. [Logs](build/validation/light-clip-20261020/), [eff199](build/validation/eff199-20261020/),
[Fog](build/validation/object-fog-20261020/). **No commit/push.**

(bd) Weapon model load request separated from plp->wpnr_no (2026-10-20): bhSv 18/0xAF only change the
player value; only mn_mode0 3 (WeaponSet/GetItem/ItemBoxChange/0x27) and game start/player change/load
replace the weapon work. A 0x27 with an identical number reloads and zeroes mdflg, bhSv 18 does not.
ply_wno is written along immediately at bhSv 18/0x27. Test player_weapon_number extended, three counter-checks
red; intermediate state CTest 177/177. [Evidence](build/validation/weapon-reload-20261020/). **No commit/push.**
(be) Three light gaps in parallel (three subagents, integrated and re-checked by the main process, 2026-10-20). (1) Effect
models 364 (Steve afterimages, albinoid lightning) now also draw in the second bhDrawEffect of the mirror pass, with the mdflg
of the effect as the gate; fixed at the same time: they were not lit at all so far (light run ended before ef_fnc). (2) bhSetEasyDirLight
type 100/101 advances in the bhControlLight call under gm_flg 0x80; the draw pass calls and the separate easy/simple set
remain a named gap. (3) Light link lkflg 5 onto eff[efid] ported (unused in the room data); side effect: bhDrawPolEffect
drew with an identity matrix when the parent matrix was missing (substitute removed, now a gap). CTest 180/180, 12 counter-checks red,
launcher: title, RM_7100/9280/1020/0000 without blocked commands and without new gaps, arm64/signature ok. Reports
work/{light_effect_link,easy_dir_light,effect_model_mirror}.result.md, logs build/validation/light-link-easydir-20261020/ and
build/validation/effect-model-mirror/. No visual acceptance. **No commit/push.**
(bf) Two waves with five subagents each (2026-10-20), each integration re-checked against the original by the main process,
CTest 188/188, dev app delivered, title/RM_0000/0080/0090/2050/3130/5810/7030/9280 without blocked commands and without new
gaps (RM_7030 has 3 open world bindings, measured identically on the HEAD state), arm64/signature ok. Wave 1: verify_combat
expectation corrected (Steve shoots three times in RM_0030 via plp->wpnr_no, five ARMS sounds are faithful to the original);
head search bhSearchNearEnemy2 from ene[]/owP[lok_jno]; 15 effect procedures (among others 273 Alexia mucus threads RM_5810/9302 with
its own world test, 357, 398). Wave 2: script commands 0x6E bhHEffectSet and 0x4A bhYakkyouSet bound; camera shake (EN17
zeroed ofx/ofz, room change left the shake in place); item light (glint gate, monitor stage 7 blue, light 1 after-effect);
enemy joints of the mask figures id 91..99 logical instead of physical (effect links lkono 28..30, EN16 hits, dam[27..30]);
body wall pass with plp->pxb; Common_controll 10 no longer writes pxb. RM_0090 box: no regression found
(17.10./18.10./HEAD byte-identical); verified candidate: examining and immediately cancelling leaves the flap of box 5 open,
because event 19 is displaced during its 30 frames by the floor 4 restart as soon as st_flg 0x8 drops - natively after
~19 frames; the hold duration of the original is not measured (status task passes sub1.c:3103/system.c:599-611 open).
The lid is written by no script. Test rm0090_security_box_nodes. Counter-checks build/validation/wave1-20261020/,
wave2-20261020/. No visual acceptance. **No commit/push.**

**2026-10-19, grab-and-turn statue (bhObj007 + bhCPM2_act_rpsh) ported (dev app via `./build.sh --test`, CTest 158/158):**
User finding "I can't turn the statue" (reported as RM_1050). Cause: the grab-and-turn object is **unported** — bhObj007
(objitm.c:1648, bhJumpObject[7], 100%-matching) and the associated player action bhCPM2_act_rpsh (player.c:4336, mode2 22). The object pass
only dispatched bhObj005/008/010, the player pass did not know mode2 22. RDT scan of all 205 rooms: the only type 7 object is **RM_0150 object 5**
(ident 46, iron statue; event 16 raises it to py 10) — **RM_1050** (stage 1 room 05, Steve/monitor, 18 objects all type 0) **has no statue**.
Port: `control_turn_objects`/`control_turn_object` (bhObj007, objitm.c:1648-1845 with original lines) + `tick_turn` (rpsh), mode2 22 branch in
runtime.cpp analogous to bhCPM2_act_psh, pad bit correction 0x21/0x42 (pad.c analog forward/backward instead of only 0x20/0x40). Test `turn_object`
(RM_0150, grab -> turn until |ay|<182 -> ev_flg 2 -> release, counter-check red). StartVibrationEx as a named host gap.
**Re-check of the subagent diff (2026-10-19, every line re-read against objitm.c:1648-1845 / player.c:4324-4490), four deviations fixed:**
(1) The statue only turned in `angles[1]`, but the draw root reads `transform.yaw` (RoomWorld::append) -> only the final jump to 0 was visible;
now `transform.yaw = angles[1]` per turn frame (test checks the co-rotation, counter-check red "draw root does not follow op->ay").
(2) `ABS((short)plp->ayp - ayn)` with `short ayn`: int difference of two shorts without re-truncation; the port truncated the difference
to int16 (one frame difference at the wrap boundary). (3) Head latch `head_push_owned` was reset between the push and turn branch,
EXP1_I(0) release thus ran every frame instead of only in case 0 (idempotent, but not original); latch now shared. (4) `plp->px = PEXP0_F(72) = ...`
(spx/spz = collision_safe_) in case 1 (:1716) and 2/3 (:1786/:1826) was missing -> after letting go the settle step of act_std would have
pulled back to the old safe position. Assumption remains: plp->ar = 3.5 (PlyInfo[*][0]); en19.c:501 (ar_bak) not at the same time. CTest 158/158.
**Followed up (same cause chain, dev app via `./build.sh --test`, CTest 158/158):** Player_controll 128 (event.c:9640) was incomplete in the port
— world.cpp op 0x64 0x80 treated 128 like 139 and left out `mtn_add=65536`, `flg|=0x10` (body wall pass, player.c:1600),
`mnwP=plmthp`, `mode0=1`, `stflg&=~0x10000`, `flg&=~0x210000`. Consequence: after the release cinematic the player stayed stuck in mode0 7 / flg
0x10000 and could not walk to the statue (all rooms with player release cinematics). Now fully ported; test
`event_lifecycle_control_release` (bhLoadWork 0 -> Player_controll 128 -> mode0 1, flg/stflg 0x10000 gone, flg 0x10 on; counter-check red
"mode0 stays 7"). **No commit/push.**

**To verify: RM_0090 Security Box deformed** (user finding 2026-10-18): after picking up the eagle medal at the *other* box the
front box (obj[5], camera 9) is deformed on the way back (flap open + lid raised), without having been used; when examining
the flap does not move. According to the user not yet present on 2026-10-17 -> regression from the changes since yesterday. User suspicion:
3D scanner/milling machine (event 23) or zombie window spawn. Not reproduced headless: all 99 events and 20 interactions of RM_0090
individually (with enemy AI) leave obj[5] unchanged; room scripts only write node 1 (event 31 +75 degrees relative, 7/19 absolute 0),
never the lid. Next step: F5 snapshot from the user in the broken state, then read out the node angles directly; alternatively a diff
against the state of 2026-10-17 (HEAD worktree) in the same sequence.

**RM_9370 room effects 35/50/54/70-76/80/400/402/405/426 (checked 2026-10-19, no gap):** these are texture holders, not procedures:
room.c:306 loads the atlas (sys->ef_tn[id]) for table entries with id 30..99 and >= 400; in bhJumpEffect (effect.c:33) 35/50/54/70-76/80
point to bhEffDmy, 400+ lie behind bhJumpEffect4[50]. All 16 holders of this room are contained in native ported_effect_atlas (effect_sprites.cpp:1070);
the procedures that use them are tracked separately. Still open: sys->obwp[0..3] (weapon right/left, ponytail) not resolvable via
kind 2 (weapon without its own owP tree, ponytail only as afterimage model) - types 1/3/4/5 of 0x86 with kind 2
index 0..3 (not used in the data); kind 1 node positions of 0x86 not measured per room. **No commit/push.**

**Effect 006 + empty procedures 4/28/129/174/197 (2026-10-20, workers T01/T02, checked):** `bhEff006`
(effsub1.c:2214) ported by hand (ef_opq at mdlver 0, otherwise ef_trs; alpha decay type != 0; height end
mtx[13] < -100 via `mtx_world_y` like 352). The empty original procedures are bound as `step_eff_empty`
(slot stays occupied, undrawn; not bhEffDmy). Spawners: only script commands 0x2C/0x6E/0xB0;
a byte superset scan of all event ranges of the 205 rooms finds none of them (the only hit RM_6070
+1306 lies in the operand of a 0x7c). **367 remains a gap** (no exp0/func spawner in the original).
Test `effect_eff006`; `engine_progression_resources` now uses 367 as the unbound example.
`./build.sh --test` 190/190 (gpu_raster_parity skipped), dev app updated. Open side finding:
`initialize_effect_sprite` does not set the UV defaults of bhSetEffectTb (effect.c:522-532). **No commit/push.**

**Flow/saving finalized (2026-10-20):** ledger audit of the area. Six entries set to done, because the
points open back then are verified: `bhAllDrawModel`/`bhEtcMirrorDrawModel` (mirror effect pass under gm_flg 0x4000,
mirror_pass/effect_model_mirror/claw_model_light), `bhAllEasyDrawModel` (monitor pass, event_lifecycle),
`bhControlEOR` (BackColor natively via back_color.cpp; the GS palette upload is the console fade display, natively
host fade), `bhSysCallMain` (pad gating; `ss_flg 0x40000000` has no writer, `tk_flg 0x1000` is the
host task automaton) and `bhSysCallMonitor` (room load/task release, A/B measurement). Five further entries
are rated as **host-replaced** (user decision 2026-10-20): `pad.c:bhSetPad` (SDL input instead of keytype/analog tables),
`ps2_LoadScreen.c:ExecuteStateLoadScreenLoadExit` (typewriter slot list instead of typ_flg/tk_flg), `system.c:bhCheckSubTask`
and `system.c:bhSysCall` (host menu accesses instead of the tk_flg task automaton) as well as `system.c:bhSysCallEvent:Pausetor`
(host pause instead of ssv_md0 10). Area 22/36 -> **28/31** (95.2 % weighted, 90.3 % fully verified); overall
88.1 %/76.8 %. Only `ps2_McSaveFile.c:mcSetStringSaveFile` (original UB saveroom 0),
`system.c:bhSysCallDoordemo` (user decision door skip) and `system.c:bhSysCallMap` (draw order/pad binding) remain open.
**No commit/push.**

## Targeted access to earlier findings

Complete [section index](archived/work/history/2026-10-13-instructions/README.md)
of the unchanged earlier instruction files. Do not load completely; when
needed, read the relevant section. The following links are **historical
findings**, not a current open/done list:

| Topic | Historical entry point |
|---|---|
| Door skip / targeted door sound stop | [CLAUDE before split:26](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:26); active decision in AGENTS.md |
| SELECT skip / stand-in Claire | [CLAUDE before split:947](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:947) |
| Main menu / host deviations | [CLAUDE before split:1039](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1039) |
| Graveyard / script enemy motion | [CLAUDE before split:1094](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1094) |
| Event camera / pause | [CLAUDE before split:1131](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1131) |
| Dialog pad edge | [CLAUDE before split:1213](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1213) |
| Object cut gate / glow | [CLAUDE before split:1285](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1285) |
| Body pass / stairs / dog push | [CLAUDE before split:1346](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1346), [order:1506](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1506) |
| Renderer workers / fog cost | [CLAUDE before split:1421](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1421) |
| Map: input lock and draw contract at the time | [CLAUDE before split:1539](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1539); do not roll back later map renderer work based on this old open report |
| Audio / enemies / shadows / inventory | [AGENTS section index](archived/work/history/2026-10-13-instructions/README.md); read the matching `work/*.result.md` selectively |

## Maintenance

Only update current, task-relevant short status entries with an evidence/result link.
Move finished detailed logs to `archived/work/` or `build/validation/` respectively,
do not copy them into this table or the mandatory files. Topic pages remain
undated mechanism descriptions. Historical core/generator approvals are
rejected and never override the current original-core/emulation policy.
- **Data profiles per platform (2026-10-22, working tree):** `tools/data_profiles.py` defines what is shipped from `data/`: `desktop` everything, `ios` (iPhone/iPad) without `files/MOVIE` (PSS, the runtime only reads `movies/*.rvv`) and `archives/RDX_LNK` (unread next to `rooms/`), `tvos` additionally without the mod folder (`RECVX_ENABLE_MODS=OFF` → `RECVX_NO_MODS`, `--mods` rejected). Saves about 1.15 GB. Title start and RM_1000 headless with the slim tree rc=0. tvOS build target and WebGL2 profile are still missing; plus `prepare_native_data.py --compressed` (TGA as RLE, movies optionally downscaled) and RLE loader in `read_tga`; HD mod keys from the unpacked bytes, so that `data_compressed` renders image-identically to `data` (RM_1000 with mods byte-identical). Committed 2026-10-22.
