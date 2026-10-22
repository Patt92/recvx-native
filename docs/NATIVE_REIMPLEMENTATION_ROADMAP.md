# Roadmap: independent native implementations, original behavior

Status: 2026-10-09, updated after restoring the build, runtime link, and room
loading. Technically stable, but gameplay was incomplete. This is a historical
work plan, not completion evidence. Task IDs remain stable; preserve existing
native work rather than porting it again. Current `AGENTS.md` and user
decisions supersede this plan.

The 2026-10-10 orchestrator handoff summarized later package states, boundaries,
and owners; that handoff note has been removed from the tree. Compare historical
removal gaps below against current reports and code before assigning them again.
The third agent's task grew to all enemies C01–C08, beyond C01 trap activation.

On 2026-10-10, P04 began installing clip ranges at evidenced camera calls
rather than every rendered frame. Flag/Rcase retention, fixed-camera keys, and
return defaults were checked in twelve runtime cases. Eff137, the full camera
state machine, and ice-room material sorting remained open; a hardware core
was not an answer. B04 proceeded separately. The next conflict-free entry for
the third assignment was C01 from [historical enemy assignment](../archived/work/history/2026-10-21-enemy-assignment/THIRD_AGENT_C01_TASK.md): trap activation
in `enemy_spawn.*` as part of the full enemy package, with shared connection
files changed only by agreed patches. Evidence:
`archived/work/native_port_P04_camera_invocations.result.md`.

## Recorded state: data ports, removal, and gameplay kept separate

This comparison used active files and CMake, not quarantine or old success
claims. **Disabled does not mean ported.** Commented-out neutral/no-op
returns are open gameplay gaps, even when they end a task.

| Area / tasks | Recorded state | Consequence at this checkpoint |
|---|---|---|
| P01 item data/header | `native/item_tables.c/.h` replaced direct `item.c` and native use of `item.h`. Its own header uses `ninja_matrix.h` instead of missing PS2 Ninja headers. All twelve tables matched original initializers after stripping comments/whitespace. | Source-level data/header replacement done; do not recreate as `item_data.*`. Add value/layout regressions on both platforms; no mechanism/runtime acceptance. |
| P02 room index | `native/room_table.c/.h` and `RoomArchive::name` replaced direct `ps2_dvd_image.c`. All 205 RDX names matched in original order. | Source replacement done. Do not invent a disc/ISO subsystem; add index/archive regression. |
| A01–A05 combat/targeting | `targeting.cpp` and `player_combat_core.c` disabled: no automatic targeting, active combat/shot/knife, or arm control. | Implement mechanisms independently; removing includes/second works alone is insufficient. Preserve WeaponFlash and pure weapon data. |
| B00–B04 movement | `player_push_core.c`, `player_stairs.c`, and `player_hair.c` disabled: no wall/floor resolution, push/stair/ladder action, or hair simulation there. | Shared geometry/collision first, then full state flows. A neutral position return is not a collision port. |
| B05/D01 item box | B05 lid control independently native: opening angle, one sound, subsequent menu pass, and return. Existing D01 transfer remained connected. | Five original-room World tests and runtime open/cancel/reopen with unchanged inventory. General object/player owners remain separate tasks. |
| C01/C05 spawn/subplayer | `activate_enemy_spawn` always returned false; `recvx_is_subplayer` always returned 0; init/tick did nothing. | Fully port trap activation and real subplayer ID/state binding. Normal room placement is a different mechanism. |
| C03/C04 EN01/EN04/EN00 | `en01_damage.c` had also been disabled: only host values were stored and reflected through `EN01_STORE`. AI, capsule hit, damage, death, grab/bite, and explosive barrel were missing. | Do not claim functioning zombies/dogs. Establish a shared native Enemy state source instead of pass-through, and port all flows including EN00. |
| D02/D03/D05/D06 inventory | Pickup/use/box, file folder, Game Over, and event inventory mutations were disabled. The event VM logged missing mutations as pending; other APIs were mostly neutral. Native table/matrix/draw helpers remained. | Implement pickup, use, sorting, reload, inspection, reader, death/retry, and event mutations. Do not change tests to accept neutral answers. |
| D07 combinations | `combinations.inc` and its original bridge were no longer linked. `Inventory::combine` returned failure without changing slots; the native table-search helper remained. | Removal done; six combination mechanisms missing. Implement original effects on the same Inventory; a neutral return is not completion. |
| D08 ECG | `status_pulse.cpp` had no `Pulse*` bodies. `tick` produced no quads, `heal_mode` remained 0; Condition API and renderer interface remained. | Removal done; independently implement curve, condition/heal/poison/fade progression, and quad output. |
| P05/D09 item UI | Data from a mixed 1,160-line include lived in `native/ui_tables_data.inc`. `BulletSet`, `TrigerSet`, and `original_command_parts` were missing: no quantity display and an empty drawn item-command window. | Preserve data split; do not extract again. D09 implements digits/color bank and original command display; existing menu input does not prove rendering. |
| D04 computer | `computer_core.c` returned an empty frame and immediate `finished`; terminal scripts, keyboard, scanner/password flags, and SFX were missing. | Port the full flow; immediate task release is not successful terminal behavior. |
| E01–E03 effects | Ground/wall/line queries were empty or returned -1000; particles were not spawned; sprite/explosion/lighter control was disabled. Native field transfer/slot selection/draw output partly remained. | Port shared ATR queries, effect data/atlas binding, and full state machines; preserve existing native output. |
| P04/G01 clip/door | Clip queries used original stage tables and first-hit rules. Event misses depended on display flags (2/20000 or 1/99); monitor query bypassed room tables (1/20000). Game Over retained explicitly installed 2/20000; a camera flag alone was not a clip call. `door_core.c` had parameter sizes/procedures and reported completion bit 0x40 only at sequence end. | Clip queries and focused runtime transitions checked. Monitor calls including unported Eff137, full camera state machine, and visible ice-room depth sorting remained open. No PS2 ICE/OT core. |
| F01–F05 Actor/Face/Morph | Native Actor/Basic/Skin/Pose layer existed. Morph events wrote fields, but vertex interpolation/draw remained open; active Lip stream source was separate from the newly selected stream on `WorldEntity`. | Preserve decoders and Lip cursor semantics; complete productive owner/vertex/face/shadow bindings. Old core morph results do not count. |
| Math/WeaponFlash/BGM | Native Math and six WeaponFlash types existed. BGM disc rule was bound to both room-load paths. | Do not restart. Type-3 wall binding, point filter, and Enemy SFX order remained acceptance/integration work. |

**Technical baseline restored:** Claude reported successful Configure, Build,
and link of `recvx-runtime`, 205/205 rooms loading, and no blocked event
commands. For this update the policy checker also ran against source,
`build/gapcheck-claude`, and its runtime binary: **PASS**. The room run was
not repeated here. No original C file was a compilation source; reference
DEPENDS were allowed. D07–D09 consumer removal was done and should not be
repeated. None of this proves playability, enemy acceptance, or Windows
runtime behavior.

CTest was **34/41** in Claude's report, with seven cases still in progress.
That was an interim state, not the final result. For each failure, separate a
real regression, missing mechanism, and bad test; do not test away missing
behavior with neutral expectations.

The `common_effect_textures_offset == size_t(-1)` guard in `world.cpp` prevents
access through an invalid file offset and must remain. Shared effect textures
were still open (E03): neither offset 0 nor empty/wrong textures may replace
them. The integrator needs separate regressions for missing and valid blocks.

**R02 remained open with narrower scope:** CMake still called
`generate_native_tables.py` for `adx_tables.inc`. It also emitted three
prohibited function outputs, which CMake subsequently removed with `rm -f`.
Not linking them did not make it a data-only generator. ADX data had to be
provided without function output; deletion afterward could replace neither
generator checks nor output audit. The then-current static PASS missed this
mixed-generator remainder.

Some ammunition and StatusPulse tests were changed to documented gaps (no
pickup/reload, no ECG quads). Those were removal/gap contracts, not the
original gameplay regressions. Their passing status was not port coverage or
1:1 acceptance. R03 was to restore positive effects against independent
native ports.

## Binding assignment for every agent at that time

Build a standalone native game on macOS arm64 and Windows x64, with original
behavior and all enemies in productive World. No PS2/CPU/VU/GS recreation,
original core, or replacement game logic.

No task may read, search, copy, import, restore, or use
`archived/FORBIDDEN_DONTUSE`, including through old archives, patches,
bundles, tasks, or build caches. Derive semantics from permitted original
sources, original data, and disassembly; never compile these references.
Implement independently on native owners, not as renamed or newly generated
original function bodies.

Preserve algorithms, order, constants, flags, tables, transitions, and side
effects. No room/enemy special cases, heuristics, silent no-ops, fake returns,
or unevidenced “fixes” to the original.

Run the policy checker before work and every build, and report findings.
Technical buildability was restored but did not establish gameplay. R02 still
had to remove unnecessary function output and the corresponding checker gap.
A PASS did not erase that evidenced remainder. Disabled areas remained
missing game functions; do not weaken the checker or positive effect claims.
An isolated native component test is not productive game evidence.

## Ownership, parallel work, and shared acceptance

- One integrator owned `CMakeLists.txt`, `native/world.cpp/.h`,
  `native/runtime.cpp`, `native/events.cpp`, global work/asset interfaces,
  packaging, and shared docs. Other agents supplied small native modules and
  connection patches as separate handoffs rather than editing shared files
  concurrently.
- P00 identified the existing native owner of each state and froze minimal
  interfaces. No parallel Player, Enemy, Room, or Effect work. Views/references
  into existing storage were allowed; mirrored worlds and copy-in/copy-out
  dispatchers were not.
- Each subtask needed original source/disassembly evidence and state/output
  comparison in `work/native_port_<ID>.result.md`, plus a focused test.
  Shared files were changed only by the integrator.
- Implement and integrate all modules of a package before a shared regression
  block. Fix common causes globally rather than adding per-enemy/room cases.
- CMake/compiler/link/binary checks had to pass fully before productive
  runtime acceptance or package release. No old core as comparison runtime;
  original data and disassembly were the references.

## Phase 0 — integration remainders, owners, and geometry

R00/R01/R02/R03/P00 belonged to the integrator. P01/P02/P05 were no longer
new implementation tasks. Source ports and data comparisons existed; anchored
data/layout regressions were still missing. The macOS runtime link was again
evidenced and no longer a removal task.

| ID | Small assignment / write scope | Dependency / completion criterion |
|---|---|---|
| R00 | Known function includes, including combinations, ECG, and UI, were no longer linked; no direct original C source remained. Integrator checked final diff and data/layout/function classification. | Source-level consumer removal done; do not repeat. ADX mixed generation was R02; missing gameplay remained D07–D09. Do not count code removal as port success. |
| R01 | Integrator maintained a list of missing functions in disabled APIs along actual call paths. Gameplay/acceptance tests could not count neutral/completion answers as ported effects; separate gap tests belonged in R03. | Parallel to R00/P00. Combat, collision hit, trap spawn, subplayer, terminal/door completion, item box, inspection, and effect requests remained visibly incomplete. Checker PASS alone did not release the game. |
| R02 | First small remainder: provide ADX data through an independent data-only generator or verified native data module. CMake must stop running the mixed generator and deleting function outputs. Add mixed-generator/body-output detection to the policy checker. | D07–D09 consumers already disabled; do not remove again. Preserve exact original ADX values; do not ban pure Combat/Chunk/UI data. Synthetic negative tests for “generate, then delete” and partial bodies; positive data-only output. No exception or generator execution for classification. |
| R03 | Integrator labeled tests as data/removal/gap contracts or actual mechanism/runtime evidence; recorded lost effect regressions per task and reran them on native owners after package implementation. | Parallel with P00; shared tests after integration. No return to core/generator tests. Gap tests may exist, but “zero quads/no hit/no mutation” cannot count as 1:1 success. Derive old gameplay cases from permitted original references and test history, never quarantine. |
| P00 | Integrator documented existing Player/WorldEntity/Inventory/Effect/Pose owners and defined small state views and mutation APIs. No game logic. | Starting point: field mapping, order, and owner for every later package; no second works. |
| P01 | Existing `native/item_tables.c/.h` and consumers retained. Add regressions for twelve tables, packed values, and native ITEM_WORK/DSP_WORK/NJS_POINT3 sizes, alignment, and offsets. | Source comparison passed; do not replace native header with `item.h` or missing PS2 Ninja headers. Check macOS-arm64/Windows-x64 layouts; item mechanics remain D02/D03/D07. |
| P02 | Existing `native/room_table.c/.h` and `RoomArchive` retained. Add regressions for 205 names/indices, range errors, and actual room mapping. | Ordered source comparison passed. Do not claim a disc/file mechanism port or invent an ISO system. |
| P03 | Native geometry primitives for later packages: needed box/capsule/line/triangle operations in `native/geometry_*.*` first. | P00. Original and edge cases; no invented collision. `ps2_NaColi.c` is not compiled. |
| P04 | Native clip queries: 113 event/51 room rows, first-hit, Rcase asymmetry, both display defaults, and monitor-table bypass. Existing runtime display flags connected; clip limits and visible near/far effect positively checked. Game Over retained explicit 2/20000 exception. | No full camera/gameplay completion: monitor calls with unported Eff137 (E03), other camera transitions, and ice-room depth sorting not fully accepted. No PS2 hardware flags or OT engine. |
| P05 | Retain existing UI data `native/ui_tables_data.inc`; check PARTS/NJS_TEXANIM/font/position initializers and native declarations against original references; add data regression. | No re-extraction or functions in this file. Values/order/dimensions are data acceptance, not BulletSet/command/ECG acceptance. |

## Next low-conflict work wave at that time

The separate Claude follow-up dated 2026-10-09 has been checked and removed.
R02 (native ADX data and mixed-generator checks), the inventory mechanisms,
seven event mutations, quantity/command rendering and ECG are implemented.
The item-box width-change cursor remainder was closed in `f18acea`.
Current limits remain in [Native Gameplay](NATIVE_GAMEPLAY.md#inventory-and-items)
and the [port ledger](port_ledger.tsv); this historical wave is not a fresh task list.
The review is recorded in `archived/work/next-tasks-audit.result.md`.

1. Integrator: agree R02 as Claude's first narrow task; continue
   R00/R01/R03/P00 and P01/P02/P05 data regressions. Set owners/interfaces;
   no second Player/Enemy/Effect world.
2. Geometry agent: P03 → E01 (shared ATR ground/wall/line/hit identity) →
   B00. Port primitives/services once; movement, targeting, enemies,
   particles, and type-3 flash consume the same source.
3. Actor agent: F01/F02 and shared C02 asset/pose contract; reuse decoders.
   Then F03/F04/F05 under shared contracts.
4. Progression agent: P04 and G01 in separate existing files. Real clip/door
   parameters and transitions, no immediate success returns.
5. Claude: D02a/D02b → D02c/D02d → D07 → D01 → D06 connection handoff →
   D09 → D08. Do not split Inventory across agents. All mutations use the
   same `Inventory`; then run shared tests and root-cause fixes. D03/D04/D05
   are separate later packages.

After P00, A, C, and D could prepare independently; integrate only against
approved contracts. D07/D08/D09 were implementation tasks for missing
mechanisms, not another removal pass. They could use separate files after
P00: D07 `inventory.cpp`, D08 `status_pulse.cpp/.h`, D09 `ui.cpp`. D01/D02
could not edit the same Inventory concurrently with D07. Additional agents
could take D04/D05 after explicit file ownership separation. R00/P00/G02 and
shared World/Runtime files remained with the integrator. This division was a
plan; it did not start tasks or change active ownership.

## Phase 1 — parallel package implementations

One agent per package, smaller subtasks in the stated order. Packages could
run in parallel after P00 when dependencies were satisfied.

### A — Combat (owner: `native/combat*`, `native/targeting*`)

| ID | Task | Dependency / evidence |
|---|---|---|
| A01 | Implement missing target search/switch on existing WorldEntity/Player views, including wall visibility and arm angles. | P00/P03/E01; exact gate/distance/angle/dual-target tables. `Targeting::select/arms` were neutral. |
| A02 | Independently port Ready/Release/Reload/Bullet count and inventory transitions. | A01/P01; flag/ammunition/frame flow without separate COMBAT_SYS/BH_PWORK world. |
| A03 | Move Pistol/Burst/Dual state machines to existing CombatController state source. | A02; evidence motion/latch/pose consequences and side effects. |
| A04 | Connect knife/hit assignment, Joint/CPCL data, and damage reporting to actual native Enemy works. | A03/P03/C02; no second target/damage list. |
| A05 | Other original weapons in table order: scope, shotgun, crossbow, grenades, launcher. Implement all, then test together. | A04/E01–E03; do not stub light/SFX/blood/explosion. |

The inactive `player_combat_core.c` contract had to bind directly to the same
native Combat state source or be replaced together with callers. Do not
quarantine the new implementation wholesale: move only actually replaced
old artifacts recoverably. Do not copy/rename a function or reactivate the
old function generator.

### B — Player/object movement (owner: `native/player_*`, `native/object_itmbox.cpp`)

| ID | Task | Dependency / evidence |
|---|---|---|
| B00 | Port player wall/floor/Dansa resolution and dynamic ETC/FLR binding from disabled push bridge onto existing Player/RoomWorld data. | P00/P03/E01. Flags, ATR types, `grand`, ROM-before-mobile order, actual hit identity. Check `native/collision.cpp` against original semantics; no substep/polygon heuristic substitute. |
| B01 | Implement missing stair/ladder/step actions, motion, floor changes, foot anchoring, SFX, and release on native Player state. | P00/B00; replace immediate completion in `recvx_stairs_tick`. Include and substitute Player work were removed already. |
| B02 | Implement push contact/object initialization and mobile room collision directly on WorldEntity objects. | P00/P03/B00; original order/tables, no PUSH_SYS/PUSH_ROOM copy. |
| B03 | Independently implement push state machine, motion root, Move/Release path. | B00/B02; bind inactive `player_push_core.c` call contract to the same native source or replace in order. No second position bookkeeping as game logic. |
| B04 | Apply hair/linked-object transforms to the actual native Actor pose. | P00/F01; no separate Player/allocator dummy. |
| B05 | Lid controller ported: 41 steps, strict threshold, separate menu pass, SFX, player flags, and return in existing object/inventory path. | Positive component/World/runtime checks; no original bodies. This did not finish general object/player dispatch or all linked/reserved owners. |

### C — Enemies and subplayers (owner: new `native/enemies/enemy_*` modules)

**Required full scope (user, 2026-10-10): every original enemy.** The third
agent assignment covered C01–C08 and all C07+ IDs; C01 trap activation for
dogs/zombies was only part. Derive complete ID/variant list from original
dispatcher and room data, agree shared native owner contracts, port all
enemies/assets first, then test together and fix common causes. First
productive acceptance remained EN07/RM_0130 but did not replace the other
enemies. Spawn, link, or load scan alone did not finish the job. Updated
assignment: [historical enemy assignment](../archived/work/history/2026-10-21-enemy-assignment/THIRD_AGENT_C01_TASK.md).

| ID | Task | Dependency / evidence |
|---|---|---|
| C01 | Inventory original room/event spawn data and init order for all IDs; implement missing ATR trap activation, floor/direction selection, and EN04 contact cases on existing RoomWorld spawn path. | P00, contact cases P03/E01/B00; `activate_enemy_spawn` then always returned false. Prove normal room placement separately; no artificial spawn injection. |
| C02 | Shared native Enemy state/EXP/child/motion/damage fields only on existing Entity owner. | P00, agreed F01 contract; state and asset teams could work in parallel. Native arena/lifecycle; no mirrored BH_PWORK/global world. |
| C03 | Independently port missing EN01 Init/Move/script/Damage/Death/child/grab/bite. | C01/C02/A04; includes already removed. Replace `EN01_STORE` pass-through with actual native Entity progression; all branches before grouped tests. |
| C04 | Port EN04 Init/Move/Attack/Damage/Death onto the same native state source. | C01/C02/A04 plus shared C03 helper contracts; no unnecessary wait for the full EN01 port. No fixed EXP substitute or second dispatch owner. |
| C05 | Independently port actual subplayer ID table and Init/Move/script/motion/release. | C02/F01; `recvx_is_subplayer` then recognized no ID, Init/Tick empty. EventVm and Draw read the same work; no guessed ID ranges. |
| C06 | Fully port EN07: evidenced original spawn, init, animation, AI, attack/grab/damage/death, SFX, draw. | C01/C02/E/F/G; required productive RM_0130 acceptance (five original ID-7 entries), no direct-dispatch test as substitute. |
| C07+ | Remaining missing IDs in disjoint small groups; delegate only after shared interfaces. Each group handles all original cases, then grouped test. | C02/F/E/G; derive grouping from original dispatch/dependency data, not guessed enemy names. |
| C08 | Independently port EN00 explosive barrel: state, mobile wall, hit/explosion/disappearance. | C02/A04/E01/E03; then part of removed EN01 generator bridge. Do not lose this enemy-package case. |

### D — Inventory/menu/computer (owner: `native/inventory*`, `inspection*`, `file_folder*`, `computer*`, `gameover*`)

| ID | Task | Dependency / evidence |
|---|---|---|
| D01 | Port missing item-box inventory/selection/transfer states independently on the same Inventory. | P00/P01; `box_engine.inc` removed, transfer neutral. Connect World object B05 separately. |
| D02 | Port missing pickup/capacity/stack/ItemSort/equip/search/erase/reload/CountBullet and ItemUse00/other use cases. | P00/P01 and A02 contract; shared inventory helpers first, D01 consumes them. Includes removed; quantity/flag/slot effects missing. Combinations are separate D07. |
| D03 | Implement missing inspection state machine, questions, item conversion, zoom/action pose, hidden nodes, name/file mapping, and file-folder selection/rotation/reader transition. | D02; both includes removed. Retain and semantically check native table/matrix/folder-geometry helpers. |
| D04 | Port terminal interpreter with eight scripts, window state machine, soft keyboard, text/quads, original flags, SFX, and cancel path onto native dialog/event state. | P00 and existing Inventory/Message contract; need not await all D03. No own SYS/ROM wrapper; replace immediate `finished` in `computer_core.c`. |
| D05 | Port missing death scene, 16×16 grid/texture data, timing/fade, Retry, Release, and title return into native flow. | P00, E03 texture contract; `gameover.inc` removed. `begin`/`draw` empty, `active` false; do not claim successful return. |
| D06 | Prepare seven missing event inventory mutations for the integrator, bound only to the same Inventory. | D01/D02; include removed, VM reported pending. Original slot ranges, first hit, equip/ammunition flags; no shadow inventory. |
| D07 | Independently implement six missing combination mechanisms directly on Inventory slots, item data, and Message state; check/retain native table-search helper. | P00/P01, D02 sorting/equip contract. Original bridge removed, `combine()` then always failed. Exact quantity/product/remainder/flag/slot/message effects; valid combination must act. No copied bodies. |
| D08 | Implement missing condition/heal/poison ECG state machine and curve/fade/UV/color output on existing `StatusPulse`. | P00, D02 healing/poison contract. No bodies linked; then no quads and heal_mode 0. Original state fields/flag bits/frame sequence and four quads per relevant output; no substitute curve or thread-local copy-in/copy-out mirror. |
| D09 | Implement missing quantity digits/color bank and Triger/ItemCommand selection, visibility, height, and labels directly on native UI/Inventory state. | P00/P01/P05, A02/D02 contracts. Data already in `ui_tables_data.inc`; function bodies removed. Quantity and command window missing, input menu still present. Original item/cursor/weapon cases, no partial-body copy. |

D02 was divided into dependent subtasks without a second Inventory owner:

| Sub-ID | Scope | Boundary / connection |
|---|---|---|
| D02a | Pickup question, answers/modes, capacity, wide items, stack maxima, gift/script and immediate-use cases. | `pickup`/inventory mutation; use through shared D02d contract, not duplicated as a special case. |
| D02b | Original sorting, search/deletion, equipment index, and selected-slot updates. | Shared helpers for pickup, combination, box, and events; check host helpers semantically before reuse. |
| D02c | Inventory-side reload and ammunition count, including packed flags, signed counters, and slot sequence. | A02 uses a small contract on the same Inventory; no new combat state machine inside inventory. |
| D02d | Every ItemUse/Use_00 case, healing/poison/spray, triggers, message, closing, and pulse request. | Use existing Player/Event/Status state; D08 consumes the same healing/poison effect. |

D06 covers `bhPlItemLost`, `bhItemGetGet`, `bhPlItemLostEx`,
`bhArmsItemSet`, `bhItemGetGetEx`, `bhPlItemChangeM`, and
`bhPlItemTamaSet`. Claude provided native mutation contracts and evidenced
connection changes; the integrator owned `events.cpp` and connected them
without shadow inventory.

### E — Effects/sound/light (owner: `native/effect_*`, small native service modules)

| ID | Task | Dependency / evidence |
|---|---|---|
| E01 | Implement missing shared ATR wall/ground/line/reflection mechanisms, including real `bhCheckL2Wall` semantics and hit-entry identity. | P00/P03; `effect_collision.inc` removed. ROM/mobile order, types 0..7/ramps and flags; no simplified ray heuristic. |
| E02 | Implement missing particle emitters, step progression, random, movement, color/atlas, line/billboard output, and blood pool on existing native Effect state. | P00/E01; `effect_particles.inc` removed. Identify existing owner in P00; do not assume or invent another O_WRK/Effect pool. |
| E03 | **Native base connected:** `ef_info`, four model-block steps, 40 shared textures, Eff009 with both UV banks/types, player/object link, and lighter light. Other sprite/custom/explosion/blood cases remained open. | Positive `native_lighter_effect` and real opening/inventory `verify_lighter.py`. Arm pose `bhArmIkMdk` and full `pt_flg`/model draw gates remained F01/P00; no claim of complete lighter pose. Light animation 1..6 native; full light system still E04. See `archived/work/native_port_E03_lighter.result.md`. |
| E04 | Bind shared original Enemy/Player SFX/voice/light order to AudioMixer/OriginalLight. | C02/E03; do not hide missing calls with null/no-op returns. |
| E05 | Close weapon type-3 wall binding and native per-draw sampler selection. | E01; retain WeaponFlash, no GS/GIF transport layer. |

### F — Actor/asset/draw (owner: `native/actor*`, `native/face*`; renderer after integration)

| ID | Task | Dependency / evidence |
|---|---|---|
| F01 | Port complete MDL/MTN/Skin/Basic/Child asset data onto existing native Actor/asset source. | P00; directly bind slots, owner, motion directory, mutable data. |
| F02 | Advance original motion/interpolation/flag/node/vertex side effects in the same native Actor. | F01; no second pose or separately running original work. |
| F03 | Native Morph Push/Calc/Draw/Pop for Player, Enemy, Subplayer, and Object owners. | F02; no vertex drift, original object selection and event/AI flags; no quarantine copying. |
| F04 | Complete Face/Lip and owner bindings independently; active LIP stream unchanged until original activation. | F02; original data/disassembly, cursor/signed-MASK/lifecycle evidence. |
| F05 | Fully port Shadow in existing native draw path. | F02; original geometry/clip/blend algorithm, not GS/VU simulation. |

F04 was not another removal of `face.*`: the existing port was policy
compliant. Two evidenced deviations belonged to the Actor/Face owner:
**F04a**, transfer angles of `toothsrc`/`tangorg`/`eyesrc` to actual pose
nodes `objP[7/9/11/13]` (`face.c:232–239`); **F04b**, derive eye angles
from gaze direction transformed by `eyemat` (`_fmCnkCalcEye`). No second
pose, duplicate rotation, or original core. Active Lip cursor, signed MASK
data, and pose/vertex order remained shared regressions; Face/Lip was not
part of Claude's Inventory assignment.

### G — Room/door/camera (owner: `native/door_core*`, `native/doors*`, new room services)

| ID | Task | Dependency / evidence |
|---|---|---|
| G01 | Independently port MPAC parameter layout/size, linked View validation, FadeProc, ViewProc1..7, DoorProc1..7, timing, and SFX. | Retain `DoorState`, agree call contract with P00. `door_core.c` was disabled, not complete. Check original/disassembly; completion bit 0x40 only at evidenced sequence end. Audit existing door/inventory order separately. |
| G02 | Demonstrate and integrate room-load/event-start/player-init/enemy-init/SFX order at productive RoomWorld entry. | C01/F01/E04; integrator owns shared files. |
| G03 | Bind camera/clip/light work to existing native data source and close original transitions. | P04/G02; no global mirrored camera world. |

## Phase 2 — integration and shared acceptance

1. R00 had removed orphaned generator requirements. After full replacement,
   remove remaining active old paths from the build and quarantine them
   recoverably. Keep only verified data/layout generators. Existing API names
   may remain for compatible callers (except prohibited original dispatcher/
   core APIs), but implementations must be independent and neither reach the
   original core nor duplicate work.
2. Policy checker: static source/CMake, fresh compile database, transitive
   link inputs, and symbol audit of each delivered binary. Do not use old
   build artifacts or archived engines for missing symbols. R01 also requires
   positive effects: a game function is not ported because an include vanished
   or a task ended immediately. A green room sweep over omitted combat,
   terminal, door, or effect flows is not original behavior. R03 separates
   green gap contracts from restored effect regressions. Combinations, ECG,
   quantity display, and command window must show original effects; unchanged
   slots or blank output do not suffice.
3. Run component tests, then productive RM_0130 from original room loading:
   spawn time, complete visible model, advancing original pose, AI, and SFX
   together. Then check attack/grab/damage/death.
4. Check Rodrigo/event/Lip/combat/door regressions and all rooms/IDs as a
   group; fix global causes. A room without a process error proves neither
   complete enemy actions nor playability.
5. Fully link Windows x64, then check actual runtime flows and gameplay.
   Use an audited Ninja build with complete `compile_commands.json` and
   COFF-capable `llvm-nm`; missing audit evidence is not a reason for an
   exception. Update macOS package/`Start.command` only with a checked
   binary; do not release while policy is red. Never auto-restore quarantined
   starters: create a newly checked package and preserve data/settings.

## Copyable historical subtask

> Take only task `<ID>` from this roadmap and its write scope. Goal: an
> independently native macOS-arm64/Windows-x64 game with original behavior,
> no original core or PS2 recreation. Read AGENTS.md and run the policy
> checker. Never read or use quarantine. Derive mechanism, order, flags,
> tables, and side effects from permitted original sources/data/disassembly
> and implement independently on existing native owners. No shadow works,
> copied/generated bodies, heuristics, or special-case fixes. If an interface
> is missing, report its concrete contract to the integrator; do not create
> a second world. Implement every case in the subtask, then grouped tests.
> Deliver source evidence, tests, open bindings, and a small integration
> patch. No commit/push or disturbance of others' changes. Do not claim
> runtime acceptance from an isolated component path.

For areas marked **disabled** above, the task was to implement the missing
mechanism, not replace a still-running generator wrapper. Old `.inc` names in
task details described historical origins only; they were not permission to
read/import those files or proof the consumer was still linked. A neutral
return was never a completion criterion.
