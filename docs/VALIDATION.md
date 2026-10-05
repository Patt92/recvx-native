# Item camera-space and spatial event integration — 2026-10-05

This supersedes the previous integration snapshot below.

- The prior handgun-ammunition image was the actual host display, not a debug
  orientation. `DrawSubItem` uses an identity camera-space basis; the host had
  incorrectly added the room camera's Y/Z inversion and viewed the back.
  `SoftwareRenderer::draw_view` now shares clipping/rasterization with the room
  renderer while accepting the original item-view basis and 500-pixel focal
  distance (PS2 screen aspect 1.174). Framing still fits the decoded visible mesh;
  original ModelScaleSet/hoseipos framing is not completely ported.
- All 156 default rotation matrices are compared independently against the
  original DSP_WORK X/Y/Z construction. Euler decomposition at Y=±90° uses a
  stable equivalent pose. Action spin is a separate X/Y/Z matrix multiplication,
  not addition of Euler angles. All eleven menu transformations retain the
  original ChangeZoomOut / DrawSubItem subcsr 1/3 result poses through messages.
- `bhAreaSearchObj` 0x72: original player/enemy/object/item work lookup, two
  separate sign masks, inclusive minima/exclusive maxima; missing lookup blocks
  with retained PC. `bhGrdPosSet` 0xb9 mutates the shared ATR_WORK wall positions
  used by walking, ground probes, shot rays and the camera mod. Parser alignment,
  all four object kinds, signed bounds and live collision changes are tested.
- Room scan: **205/205 load; 72 rooms report blocked commands**, down from 81
  using the same 600-tick scan. Later commands can become visible after a blocker
  is removed. This is not proof of completed room/game progression.
- Host, macOS universal arm64 and Intel/Rosetta, Windows/Wine, ASan/UBSan:
  19 test groups passed, including the parallel HD-content-key test. Five
  original-data suites (inventory engine, game/events/assets, input, combat,
  audio) passed on all targets. Original frontend, door and cemetery/Steve
  runtime replays passed on Host/Mac/sanitizers/Windows-Wine.
- Both v0.5 starter packages updated and launched with their own data; macOS
  arm64/Intel and Windows/Wine passed. Real knife-pickup modal was rendered from
  the package; ammunition examination images are pixel-identical on Mac/Wine.
  Mac GUI item examination passed through the normal inventory path (no debug
  menu). App signature verified; user INIs preserved. Previous binaries:
  `archived/releases/item-view-before-20261005-230857`.
- Logs and render evidence: `build/validation/item-view-20261005/`.
  Physical controllers and physical Windows hardware remain untested.
- Claudes HD-code remains separate uncommitted work. It is compiled into these
  working-tree builds and passes the lookup test; the 3.4-GB replacement pack
  is not included/activated in the starter packages. HD matching tools and
  their reported matching coverage are not revalidated by this integration.

The previous limits remain: enemy AI, helicopter/particle effects, face/lip
morphs, MIDI sequencer/ADSR/reverb, HD UI, item box, map and save/load.

# Current integration validation — 2026-10-05

This section supersedes older snapshots below. Current sources include Claude's
27 room-loader corrections and Codex's shared timing/audio/actor/inventory work.

- Host, macOS universal arm64, Intel/Rosetta, Windows x64/Wine and ASan/UBSan:
  18/18 native test groups passed. Seven original-data suites (inventory engine,
  inventory UI, input, events/assets, combat, 105 door cases and audio) passed
  on arm64, Intel/Rosetta, Windows/Wine and ASan/UBSan.
- Runtime frontend/film/hint/lighter/Rodrigo, physical walking door round trips,
  OK door skip, original knife pickup/equip/attack, debug actions and both
  cemetery/Steve task groups passed. The cemetery is walked; Steve's trigger is
  positioned diagnostically. Windows uses Wine, not physical Windows hardware.
- 205/205 exported rooms load in the 600-tick scan; 81 still report blocked
  original event commands. This does not establish full game progression.
- One audio second produces exactly 48,000 stereo frames over 30 engine ticks;
  film durations and fades use the same clock. This fixes the doubled host rate.
- Original MASK-derived node mapping and segmented en01 body motion regression
  tests passed. Render inspection: Rodrigo's hands at his wrists; cemetery
  zombies have complete body poses; pickup default angles are oblique and the
  dialog does not overlap the item name.
- Render evidence: `build/validation/core30-visual/`; complete target logs:
  `build/validation/core30-full/`; room scan: `/tmp/recvx-core-roomscan.log`.

- Existing macOS universal and Windows x64 v0.5 packages updated. Root and
  package starters passed with their own loose data; macOS Intel package also
  started. Mac GUI launch ran 90 presentation frames with default window/UI.
  macOS app signature passed deep/strict verification. Existing controls/debug/
  third-person INIs retained. Backups: archived/releases/core30-before-20261005-
  224356 and 224819. Final launch logs: build/validation/core30-launch-final.
  source-snapshot.json in each package records the integrated source hashes.
- A final input regression checks that automatic inventory opening emits SE 3;
  keyboard/SDL virtual controller paths passed on all targets after that change.

Limits: enemy AI/attacks, full face/lip morphs, original explosion/particle
rendering (including helicopter), ambient MIDI/sequencer/envelopes/reverb,
three absent door-program requests, HD UI, item box/map/save/load remain open.
The display defaults to 1280×960, integer-scaled from 640×480; this is not
high-resolution native rendering. Physical controller/audio comparison remains
unverified. Native tests use original data and SDL virtual controllers.

# Local validation of native 0.5

Validated on 2026-10-05. This records checked behavior, not completion of the port.

| Check | Result |
|---|---|
| macOS universal build | arm64 and x86_64 slices present |
| Windows build | PE32+ x64 executable and bundled SDL2 |
| CTest | 14/14 on macOS; 14/14 under Wine |
| ASan + UBSan | 14/14 and original-data opening/controller/inventory regressions passed |
| Intel/Rosetta | original-data opening/UI and SDL virtual-controller regressions passed |
| Disc data manifest | 12,701 files; 6,574,841,350 bytes; both packaged trees verified |
| Room scene decoder | 205/205, 1,008,630 stage triangles, 470 active camera targets |
| Five playable bodies | 272 supported body clips; all 7,901 frames sampled |
| Opening without skipping | 15,000 simulated ticks, original movie completed, cell hint/camera 37 reached |
| Original skip/lighter path | hint confirmation, inventory equip, floor condition, Rodrigo voices, restored position/control |
| Post-event movement | 120 running ticks moved Claire through the same native movement/collision path |
| Dialogue rendering | no panel; white core and black edge; uncovered pixels preserve the scene |
| Side pack | original GM flag switches 8/10 normal cells and cursor bounds; frame row changes |
| Native skin normals | weighted influence rotation tested separately from positions |
| Packaged launch | own packaged data; Windows replay under Wine; bounded macOS SDL window/audio start |
| Configurable input | keyboard/gamepad rebind, axis inversion, deadzone, trigger, short presses, repeat, focus and hot-plug/removal passed with SDL virtual GameControllers |
| Gamepad opening flow | original cell skip/dialogue/inventory/lighter/Rodrigo/control-return path passed on arm64, Rosetta and Windows/Wine |
| Main menu | original ADV title; idle leaves event VM, film and game audio inactive; new game starts original opening |
| Settings screen | keyboard/controller pages, live rebind, Confirm/Cancel conflict swap, persistent atomic INI save, default restore, stick thresholds and Escape capture cancel checked |
| Running film skip | original active film skipped by keyboard Space and SDL virtual A; movie/PCM stop, cell hint/camera 37, fast inventory equip, Rodrigo VOICE1/0016 and control return verified |
| Automatic inventory after hint | original opening/late film skip; OK alone opens inventory; held forward input leaves actor position unchanged |
| Physical controller | no mapped controller or joystick detected locally; physical Windows XInput hardware not tested |
| Backward movement | original clip 16/key 25, advancing pose, signed curve, unchanged facing, return to idle and controller backstep checked |
| Original doors | 105 MRY parameter cases rendered, completed and skipped; original hinge/camera/fade procedures compiled natively |
| Room round trip | cell/hall/cell walked without teleport; natural and OK-skipped sequences give identical spawn, retain items/equipment/HP and do not replay opening; arm64, Rosetta, ASan/UBSan and Windows/Wine |
| Confirm during doors | keyboard Enter and SDL virtual A, held-key suppression and inventory blocking tested; binding remains configurable |
| Lighter resources | item ID 55 reads ITEM1/0054.bin and original PVP textures; rotated 3D view differs; old diagnostic screenshots archived |
| Solid wall collision | ATR types 0..5; room-height fallback, original player dimensions, floor attributes, thin walls both directions, signed diagonal walls, circular contacts, sliding and corners checked |

The original-data opening regression passes with 109 known unbound commands.
Particle effects, lighter/flame object, full MASK/face/lip processing, camera
interpolation, ambient/door SE routing, full enemy AI/projectile combat and complete
puzzle/progression/save systems remain pending. A successful room decode does
not verify that room's gameplay. Wine validation is not a physical Windows test.

The original ISO, previous Git and obsolete PS2 compiler/CRI/Katana distributions
are preserved under ignored archived/. The new source Git excludes private data,
archives and all build/package outputs. No remote was configured or pushed.

Direct root launchers `Start.command` and `Start.cmd` expose the existing packaged
builds. `STARTEN.md` describes launch, first equipment flow and door controls.
Door sound banks, special door lighting and rumble are not yet implemented.
The inventory regression also checks retained poison/HP/side-pack state across
VM reconstruction; door filters support signed below-ground floor IDs.

The main menu and settings use original title art and font atlases. They provide
a native development menu; the complete original title/difficulty/load system is
not yet ported. `tools/verify_frontend.py` verifies real-runtime default idle,
settings, active movie and the complete late-skip/rapid lighter flow.

## Gameplay corrections, 2026-10-05

Added real-data regressions for automatic hint → inventory ownership, A/D turn
directions, continuous D-pad item inspection, voice dismissal, animated health
pulse, knife/ammunition Yes/No, full-inventory rejection and persistent pickup
flags. RM_0020's shared NPC texture banks now render without a texture-range
exception; the actual first-stairs door transition from original hall spawn 1
loads RM_0020 and its initial bytecode has no blocked commands. All eleven COMMON.MLT
programs decode to non-silent PCM. Original title/menu atlas sprites are used.
Hands/accessory libraries, ambient MIDI routing, full pickup animation/layout and
complete scene fidelity remain open; these tests do not certify those systems.

Verification for this revision: 12/12 groups on macOS universal and Windows/Wine;
12/12 under AddressSanitizer/UndefinedBehaviorSanitizer. Original-data gameplay,
controller and sound-bank checks passed on arm64, Rosetta and Windows/Wine.

## Room audio and attachment extension, 2026-10-05

Real-data tests now decode all five floor programs, select cell/hall sound packs,
stop room channels on reload, reject missing/invalid programs and verify healthy
walk/run/backstep contacts with silence between contacts and during idle. Walked
cell/hall round trips require original sound dispatches in both rooms; the automatic
hint/inventory test rejects footsteps while gameplay input is blocked.

Attachment tests use original room item geometry and check updated player-bone
translation, inherited visibility, release and Rodrigo model-node hide/restore.
Pose-only matrices are compared against geometry-sampling matrices. These tests
do not establish complete hand/accessory rendering, ambient MIDI or physical
controller/audio-device fidelity.

## Inventory combinations and item checks, 2026-10-05

The new inventory_combine_inspect group verifies all eight herb recipes in both
orders, invalid recipes, cancel/result cursor, full capacity, red-herb rejection,
healing, partial powder conversion and full-magazine dialogue/rollback. Four
original cases/briefcase variants use real SYSMES.ALD questions and contents,
missing-lockpick checks, wrong facing, Yes/No/Cancel, replacement at full capacity
and retained lockpick. Real ITEM1 packets render closed/opening/content views;
PS2 hinge geometry moves without losing model parts. Contents IDs are read from
idsettbl, including handgun parts 63→19 and magnum case 129→30→13.

Keyboard and SDL virtual GameController regressions exercise Combine and case
opening through GameplayInput, hold suppression, No/Cancel, error sounds and
invalid key Use keeping inventory ownership. A bounded event fixture checks
original floor parameters, UseItemCheck/ItemLost/flag commitment, 0x7c parameter
changes, TG-01/ammunition rejection messages and retained empty extinguisher. This is not verification of all
world puzzles, physical gamepads, original zoom/catch animation, hidden contents,
file registration or complex multi-item extraction.

Reproduce the additional real-data check with:

```sh
build/native/recvx-inventory-tests "$PWD/data"
# Optional rendered diagnostic fixtures; the output folder must contain no old BMPs.
build/native/recvx-inventory-tests "$PWD/data" "$PWD/build/validation/inventory"
```

The regular build script runs this data regression after CTest.

## Knife, handgun, aim and pickup (2026-10-05)

The combat_weapons_ammunition group checks original constants, target height and
wall occlusion. The real-data invocation additionally loads original weapon
models/MTNs, ready/aim/attack/reload clips, sound banks, magazine depletion/empty
shots, held-fire animation cadence, modal blocking, real RM_0020 zombie damage
and death, NPC immunity and frame-gated knife hits without ammunition usage.
The input regression exercises keyboard R/Ctrl/Enter and SDL virtual RB/A,
original knife/ammo pickups, suppression of pickup while aiming, and migration
of older customized INI files. verify_combat.py checks the actual runtime through
the opening, original pickup/equipment, knife attack/sound and return to idle.

```sh
build/macos-universal/recvx-combat-tests "$PWD/data"
python3 tools/verify_combat.py --runtime build/macos-universal/recvx-runtime --data "$PWD/data"
```

The new original-data combat and input checks passed on arm64, Intel/Rosetta,
ASan/UBSan and Windows x64 under Wine. Full enemy AI and original damage/death
animations remain unported; corpse rendering is a documented native fallback.

## Debug menu integration (2026-10-05)

Native host, macOS universal and ASan/UBSan builds: 17 CTest groups passed.
Windows/Wine: 15 CTest groups passed; Wine service output handles prevented the
CTest run from completing after child processes exited. The remaining scene and
actor checks passed separately with output redirected to log files.

Debug-specific unit and original-data runtime checks passed on macOS arm64,
Intel/Rosetta and Windows/Wine; runtime checks also passed with ASan/UBSan.
Verified: F2/default file, F6 remap, repeat guards, software/bundle configuration
paths, action registration/errors, input handover, 180 paused ticks, all five
original character bodies, equipment/items/HP/bag/wireframe and room/spawn changes.
Both existing package starters passed debug actions using their own packaged
data and debug.ini; macOS bundle signature verification passed. Existing user
controls.ini files were preserved. See DEBUG_MENU.md and tools/verify_debug.py.
