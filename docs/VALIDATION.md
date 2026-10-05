# Local validation of native 0.5

Validated on 2026-10-05. This records checked behavior, not completion of the port.

| Check | Result |
|---|---|
| macOS universal build | arm64 and x86_64 slices present |
| Windows build | PE32+ x64 executable and bundled SDL2 |
| CTest | 13/13 on macOS; 13/13 under Wine |
| ASan + UBSan | 13/13 and original-data opening/controller/inventory regressions passed |
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
interpolation, ambient/weapon/door SE routing, combat and complete
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
