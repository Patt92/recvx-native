# Flammen, Bewegungen, Security-Progression und Renderlast — 2026-10-06

Dieser Abschnitt ersetzt frühere Integrations-/Scanangaben weiter unten.
Logs und echte Ansichten: `build/validation/progression-20261006`.

- **Originaleffekte:** Generator übernimmt bhEff200/201/218 aus effsub5.c.
  EF_WRK, originale Atlasgruppen 58/59/70–79, Bildfolgen und Bodenanker laufen
  im gemeinsamen World-/Rendererpfad. Additive Polygone beachten die Tiefe,
  schreiben keine Tiefe und verdoppeln ihre gemeinsame Kante nicht.
  RM_0020 erzeugt elf Feuerquads und einen Funkenquad (24 Dreiecke).
  `cemetery-fire-runtime.png` zeigt die wirklichen Flammen nach dem originalen
  Friedhofereignis, nicht nur einen registrierten Effekt. Explosionsemitter,
  Rauch, andere Effekttypen und Linktyp 5 bleiben ungebunden; ihre Befehle
  bleiben als pending sichtbar. Die RNG-Startphase ist ein Host-LCG.
- **Animation:** en01-Raum-MTNs verbinden die originalen acht unteren mit den
  zehn oberen Kanälen (mtn_no+1). Fünf echte Raumressourcen geprüft.
  Common-Knotenänderungen enden erst beim nächsten tatsächlichen Schreiben
  durch die Motion (Position nur mit Key, Winkel auch bei leerem Key).
  Keine dauerhafte Null-Wurzel mehr; Claire/Rodrigo im Prolog wieder sichtbar.
  LoadWork selektiert NPC-Work ohne künstliche Steuerungssperre; SubCtr 0x80
  gibt die Animation an die originale InitType-Pose zurück. Keine vollständige KI.
- **Steve-Musik:** ADX-v3-Loopflag/Start/Ende werden gelesen. BGM1/0048 hat
  keinen Loop und endet nach rund 22,38 Sekunden, ohne künstliche Wiederholung.
  BGM1/0000 erhält sein Intro und die eingebettete Loop-Teilsequenz.
- **Metalldetektor:** bhCamSet2 schaltet die originale feste Kamera.
  bhCheckFloorP wird wie durch bhControlPlayer nur bei sp_flg Bit 0 ausgeführt.
  Zuvor startete die Bodenprüfung während der Ansage Event 11 erneut und
  verdrängte Event 2 im selben Tasklevel. Jetzt laufen VOICE1/0455, Kamera 9
  und der originale Abschluss. Nachricht 16 ist im Dump FF02/1/FFFF: bewusst
  leer, keine erfundene Untertitelzeile. `verify_progression.py` prüft Ansage,
  Kamera, Freigabe, Boxeingabe, Ablegen, erneutes Passieren und Zurückholen.
  Triggerpositionen sind diagnostisch gesetzt, kein Laufweg-Durchspielnachweis.
- **Security Box:** bhPlItemBlockCk, SecurityItemSet/Back und originales
  ItemBoxChange aus sub1.c (Quelle als 67,97 % matching kommentiert).
  Verwendet die originalen Inventarbereiche und Typ-/Schlüsselrestriktionen;
  Platzmangel rollt die Transaktion zurück. Tastatur/Controller nutzen den
  gemeinsamen modal_input-Hook. Das originale System-Boxflag öffnet das Menü.
  Linke Boxliste ist ausdrücklich eine Host-Ansicht; originale Fensteranimation
  und Spriteoberfläche sind noch nicht portiert. `security-box-final.png`
  wurde auf überlappende Texte geprüft.
- **Hunde:** Originale Bereichs-/Trap-Routinen aus zonzon.c/en04.c aktivieren
  Cerberus nach Floor/Gruppe, setzen originale Spawnposition/Richtung, Lauf-MTN
  und rm_flg. Zwei echte RM_0050-Traps und der wartende Hund geprüft.
  Verfolgung, Angriffe und die vollständige en04-Zustandsmaschine fehlen;
  eine aktivierte Laufanimation ist kein Beleg vollständiger Hund-KI.
- **CPU:** Texturpixel und Mipcache werden zwischen Framekopien geteilt;
  Änderungen lösen Copy-on-write und Cacheinvalidierung aus (auch Änderungen
  außerhalb der früheren Fingerprint-Stichprobe). Default höchstens vier Worker.
  GUI rastert/lädt nur bei neuem 30-Hz-Tick oder Eingabe, Präsentation bleibt
  VSync. Gleicher RM_0050-Lauf mit Mods, 180 Präsentationen: vorher 19,92 s
  User-CPU, jetzt 6,70 s (−66 %), 74 Neuzeichnungen; Laufzeit 2,93 → 3,34 s,
  RSS 271 → 275 MB. Kurze Messung auf diesem Host, kein allgemeiner M4-Benchmark.
  Software-Rendering/HD/Supersampling kosten weiterhin Rechenzeit.

Arbeitsbaum-Prüfung: **22 CTest-Gruppen**, sechs Originaldaten-Suiten
(Inventar-Engine, Game, Input, Combat, Audio, Progression), Frontend-/Tür-/
Cemetery-/Steve-/Security-/Snapshot-Replays auf Host, Mac arm64, Intel/Rosetta,
Windows/Wine und ASan/UBSan. Intel zusätzlich alle Unit-/Datentests; keine
physische Windows-Hardware oder Controller geprüft.
Raumscan: **205/205 laden; 38 blockieren** (vorher 37). Vergleich mit dem alten
Paket zeigt ausschließlich RM_9301 neu: Event 10 erreicht jetzt bhGameOverSet
0xc6, dessen Oberfläche nicht implementiert ist. Kein stilles Überspringen.
600 Ticks/Spawn 0 ohne Eingaben beweisen keine vollständige Progression.

Renderer/Filter/Threads werden wegen der gemeinsam geprüften CPU-/Effektarbeit
gezielt in diesen Engine-Commit integriert. Claudes HD-/Titel-/Equip-/Snapshot-/
Grafiksettings-/3rd-Person-Erweiterungen bleiben im Arbeitsbaum und den gemeinsam
geprüften Builds erhalten; das Paketmanifest nennt ihre zusätzlichen Quellen.

# Original event word state and game clock — 2026-10-06

This supersedes the earlier scan and package counts below.

- `event.c::bhCmpW` 0x07 reads signed player/enemy HP, live or latched
  `mes_sel`, and `stv_tm`. All six operators and unsigned 16-bit operands
  are tested independently. `bhSvW` 0x09 updates actual shared work state;
  fields 11–15 assign negative HP to enemies 0–4 as the original does.
  Missing enemy work blocks at the original PC. Real RM_0000 actor work
  confirms assignments and conditions reach the same entity, not a byte register.
- `bhRoomCaseNo` consumes two bytes and passes the variant through DoorCall.
  Message marker 0xfe09 sets the default question choice; page changes reset
  selection. Confirmed status-menu answers keep the original 100/101 offset,
  including after the question closes. This does not complete the original
  status-screen transition animation or item-box interface.
- `sync.c::bhControlVSync` game time is room-independent and advances two
  VSync units per 30-Hz game tick, gated by event flag 3, capped at 216000000.
  Title/debug pause do not advance it. `tools/verify_event_state.py` uses real
  exported room geometry with an isolated event-table fixture to verify this
  runtime contract; it is not a campaign script or playthrough test.
- Host, Mac arm64, Intel/Rosetta, Windows/Wine and ASan/UBSan passed **20 test
  groups**, five original-data suites, and original frontend/doors/events
  replays. All five architectures passed the extra clock/pause runtime check.
  No physical Windows hardware or controller was used.
- Same 600-tick scan: **205/205 rooms load; 37 report blocked commands**,
  previously 38. Later unbound commands remain visible; this scan does not
  prove full progression or enemy AI. Logs: `build/validation/event-conditions-20261006`.
- The 4-byte asset error reported in the older renderer handoff did not
  reproduce in baseline commit 1978aa0: original door/lighter replay passed
  before this change. No speculative audio/resource repair was added.
- Claude's HD/filter/supersampling/parallel-renderer work is preserved and
  included in tested working-tree binaries; those sources stay outside this
  engine commit. Source manifests distinguish committed and working-tree code.

Both v0.5 packages updated; root/package starters passed with their own data
on Mac arm64/Intel and Windows/Wine, including the full opening/lighter replay.
The normal Mac SDL inventory/examine path ran for 90 frames; screenshot:
`build/validation/event-conditions-20261006/package-gui.png`.
Mac app signature passed deep/strict verification and user INIs were preserved.
Backup: `archived/releases/event-conditions-before-20261006-083827`.
The packages include Claude's tested parallel-renderer sources; HD-pack files
remain unbundled. Binary/source SHA-256 manifests record the integration.

# World controls and scheduler integration — 2026-10-05

This supersedes the older scan/build counts below.

- Original `event.c::Common_controll` model-node changes (17–19, 36, 38),
  model-node position/angle interpolation (28–33, 44–45), key capture (48/49),
  low-byte type assignment (50) and `Obj_controll` 1/20 are bound. Overrides feed
  the same weighted vertices, normals and matrices. Object/item attachments
  preserve full parent-node orientation, including static MDL hierarchies;
  player link lookup resolves original MASK script indices. Complete enemy AI
  and every possible linked actor animation are not established by these checks.
- `Event_init` resets interpolation keys without discarding work selection or
  movement increments. Common 44/45 consume five bytes, including their trailing
  padding; 38 consumes ten. Tests verify subsequent opcode alignment.
- `Event_T_timer` advances once per active scheduler pass, including multiple
  tasks and an in-pass clear. `bhTFrameCheck`, `bhEventTimerClr`, `bhEvtKill`,
  `bhEneLifeSetM`, `bhLightParameterSet`, `bhGrdPosMoveCSet/Start` are bound.
  Spline countdown keys change the actual walls used by movement and ray checks.
- Enemy SE uses six independent voices and original MULTSPQ bank 3; actual room
  0002 sound PCM, stop isolation and room reload are checked. Spatial pan/volume,
  Sony sequencing, ADSR and reverb are not fully implemented by this change.
- Pony reset, shadow switch, vertex-morph setup, effect UV/local offsets and
  object-SE deletion now parse exact operands but retain `pending_bindings`;
  parser progress is not evidence these effects are rendered or sounded.
- **20/20** unit groups passed on Host, Mac arm64, Intel/Rosetta, Windows/Wine,
  ASan/UBSan. Original-data suites inventory-engine, game, input, combat and
  audio passed on each; frontend/doors/cemetery/Steve replays passed. Logs:
  `build/validation/world-control-20261005`. No physical controller/Windows PC.
- **205/205 initial rooms load; 38 report blocked commands**, previously 72.
  Newly reached RM_3180 DoorCall requests RM_3100 spawn 2, whereas this dump has
  only two POS entries. The shared destination preflight reports that condition
  in the event task and preserves the current room; no guessed spawn is used.
  This exact failure path was checked on all target architectures and sanitizer.
  The room scanner distinguishes initial load failures from runtime failures
  after a successful load. No complete-playthrough claim.
- Claude's concurrent HD/filter/supersampling changes are compiled and tested,
  preserved in the working tree, and excluded from the engine commit.


Both v0.5 package binaries were updated and their project/package starters checked
with their own data, preserving user INIs. Mac arm64 and Intel/Rosetta launch;
Windows/Wine root and package launch; full opening/lighter replay passed.
Mac codesign deep/strict validation passed. Backup: archived/releases/world-control-before-20261005-233728.
Package cemetery replay renders the original NPC meshes/motions and restores
control; this is not evidence of complete zombie AI. Handgun ammo view inspected
at room 0010; normal Mac SDL GUI inventory/examine ran for 90 frames.
Manifests retain source hashes and list Claude's concurrent compiled sources.

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
