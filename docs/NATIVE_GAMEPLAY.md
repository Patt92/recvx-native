# Native gameplay port status

This page records which original mechanisms were ported in each game system
and which were still missing when each section was written. Source references
refer to `src/ps2/veronica/prog/`. Domain percentages are in
[PORT_STATUS.md](PORT_STATUS.md), and verification tools are in
[VALIDATION.md](VALIDATION.md).

“Ported” means an independent native implementation preserves the original
tables, flags, and call order. Original function bodies and generated copies
must never be built or linked. “Open” means the mechanism is missing or only
partially connected. Missing services are reported, not silently counted as
complete.

Older lists below, outside the collision section, still include evidence from
before removal of the original-core path. They are historical records, not
current gameplay acceptance. Check the current gaps in
[NATIVE_REIMPLEMENTATION_ROADMAP.md](NATIVE_REIMPLEMENTATION_ROADMAP.md).

## Startup and progression

**Recorded as ported**

- Title screen from ADV resources; New Game plays ADV/0003 as in `adv.c`
  `TitleCall` and starts the event VM.
- The prologue follows original room scripts: movie, cell scene, lighter hint,
  automatic status-screen opening, equipment, Rodrigo scene, and release of
  player control.
- Cinematic skip follows `bhSysCallGame`/`bhSysCall` for all CineSet modes,
  gated by original status bits `0x1C040008`.
- Event countdown `bhEvtTimerSet` with all three `bhSetEventTimer` modes and
  expiry branches from `player.c`; Retry/Game Over through `bhGameOverSet`.
- Disc-set selection follows `sys->ss_flg & 1`.

**Historical gaps**

- Original saving/loading (typewriter, memory card), difficulty selection, and
  complete title transitions. Saving and LOAD GAME have since been ported with
  a host user-data file in place of a memory card; see current status.
- Full `bhSysCallMonitor` state machine for `mn_md`; room loading and changes
  run natively in Runtime/World.
- Host snapshots (F5/F9) are development tools, not original saving.

## Events and scripts

**Recorded as ported**

- Scenario interpreter with Event_init, tasks, conditions, loops
  (`FOR`/`WHILE`/`NEXT` with a shared stack), waits, and the 10-bit flag mask
  of `bhCk`/`bhSet`.
- At this checkpoint, 147 of 223 event commands were bound: cameras
  (including event-camera moves through `njOverhauserSpline` from `cut.c`),
  dialogue, doors, floor triggers, item commands (`bhPlItemLost`,
  `bhItemGetGet`, `bhArmsItemSet`, etc.), object links, model-node control,
  `bhMotionPauseSet`, lights, sound, and BGM.
- Unknown commands block their task at the original address and are
  reported. Known but unbound commands are skipped at the correct length and
  logged as *pending*.
- All 205 rooms load. `tools/verify_rooms.py` starts each room and reports
  blocked commands.

**Historical gaps**

- At this checkpoint, 76 event commands: 46 consumed at the correct length
  and 30 without a parser. For current counts, run
  `python3 tools/port_check.py --domain Events`.
- `Common_controll` (`event.c`) only partially bound.
- Face/lip morphs (0x3c–0x3f, 0x90, 0x9a), camera pause, shadows.

## Player, movement, and collision

**Existing native mechanisms and partial ports**

- Movement with original clips and speed curves at 30 Hz, backward step, and
  180° turn.
- Independent ATR ground query following `bhGetGroundPosition`: boxes,
  cylinders, signed triangles, all four ramp directions, and planes. Room and
  mobile tables are checked in original order; a hit retains its ATR table and
  index identity. The existing `RoomWorld::combat_ground` path uses this
  service.
- Independent wall-contact query following `bhCheckWallType`: all eight ATR
  types, radius/height overlap, and original query flags. It detects contact
  but does not resolve player movement.
- Independent wall reflection following `bhSetWallRefAngle` for all eight ATR
  types, and line queries following `bhCollisionCheckLine/2/3`: original
  filters, side order, segment shortening, and unnormalized polygon normals.
  Cylinder sides are octagons, not analytic circular rays. The downward
  cylinder-cap test remains explicitly unported: the original reads
  uninitialized polygon points there. Such queries report the gap without
  writing partial results to the caller. Existing World line services use the
  same room/mobile geometry; a real room test also covers changes made by
  event command 0x0a.
- Interaction distances and direction masks from `bhCheckExmAtari`.
- Independent player-wall resolution following `bhCheckWallEx` in the free
  movement path: a smoothed forward probe followed by the body, with original
  radius, heights, flags, previous-position rules, and rounding. Room/mobile
  ATR and temporary step edges are tested in original order. Event-driven
  wall changes affect the same owner. `bhCheckFloorP` uses the safe probe
  point for directional triggers and sets ground/water/footstep fields. The
  independent direct `bhCheckWall` query also includes its distinct ramp and
  Y-rounding branch.
- EE libc random sequence (`state * 0x41C64E6D + 0x3039`).

**Open**

- General `bhControlPlayer` work with all state flags and modes.
- Complete player state machine, push/stair/ladder actions, and their mobile
  geometry producers. Free player-wall resolution is connected, but not every
  scripted, combat, or enemy movement path.
- Semantics of the downward cylinder-cap test and the real `bhCheckL2Wall`
  binding (a different capsule/inset contract from the line query). Existing
  analytic combat-ray/enemy-movement paths do not port or replace them.
- Complete player-ground work and every effect/enemy consumer. A valid ground
  query alone creates no particles or effects.
- Wall-contact effect 29, ponytail, and lighter flame with room lighting.
- Vibration and arm IK (`bhArmIkMdk`).

## Combat

**Recorded as ported**

- Knife, handgun, M93R, custom handgun, Magnum, M-100P, dual MPs, assault
  rifle, and sniper rifle with original models, clips, ammunition rules,
  reloading, and weapon sounds.
- Target selection from `pwksub.c`/`playpch.c` (`bhSearchNearEnemy`,
  `bhSearchNextEnemy`, `bhCalcLockEneYR`, dual-weapon split).
- Muzzle flash (`bhEff003`) and casings (`bhEff000`) with floor/wall bounce.
- Grenade launcher (weapons 14–17: explosive, acid, incendiary, BOW gas
  rounds): launch (`player.c:4990`), `bhEff130` projectile flight,
  wall/enemy hits (`bhCheckFlyAtari`), impacts, and `bhSetBowDamage`.

**Open at this checkpoint**

- Shotgun, crossbow, weapons 18/20, scope. Unreleased weapons report their
  named gap when aiming (`Combat gaps`).
- Grenade launcher BOW-gas screen tint (`sys->ef_ntx2d`), water impact
  (`bhCheckL2Water`), and reload with ammunition switching unverified.
- Other muzzle variants, smoke, weapon lights, and joint capsules for weapons
  with `WpnTab.flg & 8`.

## Enemies

**EN21 (young Albinoid): implemented, connected, and checked in an original
room at this checkpoint.** Its native state machine runs in WorldEntity
dispatch, including routes, game mode, player grab, and audio. Sparks/trails
(399/396) use the model texture list set by the enemy instead of a shared
replacement atlas. Shared room routes load without a zombie motion library.
`tests/native/en21_world_test.cpp` checks RM_3120/RM_3140 initialization (types
0/1), movement, pursuit through grabbing, damage 9, sparks 399, trails 396,
death with a blood pool, and the distribution of lockout time/death motion.
Open: natural script release in RM_3120 (the test releases with 0x24), a real
weapon hit instead of hit fields, and observation of voice/SE/`plp->dpx`.

**Recorded as ported**

- Zombies (`en01.c`): perception, route finding, pursuit, foot anchoring, hit
  reactions, knockdown, death, blood, and enemy voices.
- Grabbing/biting with player damage and stick wiggling (`EatCheck`,
  `bhEne01_Nage`, `PlyDG00/01`) run in normal play (on by default;
  `debug.ini enemy_grab = off`, the debug menu, or `--enemy-grab off` disable
  them). As of 2026-10-12, the chain included player search
  (`bhSearchPlayer/2`), area/ground effect surfaces
  (`bhEne_EnemyAtariCheck`, `bhCheckFloorEffect`), route zones
  (`bhCheckRouteID/bhCheckRoute` from `rom->rutp/ruttp`), walking motion
  changes (`bhGetFrameNum`, `bhCalcFixOffset`, `bhEne_GetTranslateMtn/2`,
  `bhAddSpeed`), stair surfaces (`bhKaidanAtrCheck`), the player capsule
  (`plp->watr`, `npCollisionCheckSC`), and enemy-player contact
  (`bhCheckPlayer`; condition `plp->flg & 8` had been inverted). Bites deduct
  inventory HP directly; HP < 0 enters native Game Over. Evidence:
  `tools/verify_enemy_ai.py` (RM_0080: natural spawn, diagnostic setup, stick
  wiggling, death).
- Barrels (`bhEne00`) with explosion and chain reaction.
- Dogs activated by original area/trap rules.

**Historical gaps**

- Other enemy families (en02 onward) and full dog AI were still missing at
  this checkpoint. Later ports must be checked individually in current code.
- Zombie stairs, head/limb loss, fire, crawler branches, and mutual pushing.
- Still reported on the attack path: blood sprite `bhEff005` (particles run),
  controller vibration (`StartVibrationEx`), `bhSetUseKaidanFlag` for
  `sys->metcp` crates, and the `nlxb` branch of `bhCheckPlayer` when the
  player is dying. A recorded original defect was modeled:
  `bhEne01_ChgWalkMtn` shifts raw `bhGetFrameNum` by 16 bits, so the starting
  frame can reach `frm_num`; this pass retains the previous pose and reports
  it.

## Inventory and items

**Recorded as ported**

- Status screen with original sprites, slide-in/out (`SpriteH`, `SpriteUV`,
  `StatusCancel`), shutters, EXIT/FILE/MAP/ITEM command bar (`MainCommand`),
  and cursor logic (`CursorMove`).
- `itemdata[156]` with types, quantities, and combinations; all 117 directed
  recipes through `combidata`/`Combi_*`.
- Context-menu sprites and visibility from `ItemCommand`/`TrigerSet`.
- Examination with initial angles from `dsptbl`, model swaps, case opening,
  and name changes (`ItemModelCheck`, `MdlAction00–02`).
- Inventory mutations through `GetItem`, `ItemSearch`, `EraseItem`, and
  `ItemSort`, including equipment-index correction.
- Health/ECG display (`PulseInit`, `PulseMain`, etc.) with original draw
  priority.
- File folder (`SearchTag`, `SelectFile`, `SelectTag`, `FileSyu`) and reading
  view; file numbers through `FileNumberSwitch`.
- Item box opened by `bhCheckExmAtari` (type 4, box bit), with
  `bhObjItmBox` lid animation; exchange, stacking, restrictions
  (`ItemBoxChange`), slide transitions, and EXIT button. The lid controller
  is independently native: 41 opening steps, menu request on the next pass,
  one sound, and return on the same object. Pause/player flags and reopening
  were checked; general reserved/linked object owners remain separately open.
- `ItemUse` for all item types; healing and antidote through `Use_00`
  (including spray count and poison status bits).
- Floor/script pickup with Yes/No, full-bag handling, and completion through
  `ItemTaskCheck` (clears GetItem/GetFile requests).
- Menu sounds through `CallSystemSe`: confirm 3, cancel 0, cursor 2, error 1.

**Open**

- Full `StatusMain` and `ItemCommand` state machines; some input and
  transition phases still use host control.
- `ItemCombination` menu flow and animation (the recipes themselves follow
  the original).
- Full analog-pad binding of the map screen (`pad_ax/ay/al/ar`) and shared
  system-task/draw order beyond the checked MAP path.

### Map screen

**Recorded as ported:** `MapScreen` owns the `map.c` state machine,
flag/floor selection, zoom/neighbor states, and original draw list.
`MapRenderer` decodes the MRY index and floor models through the existing
MDL/PVP path and draws the grid, frame, sprites, markers, floor objects, and
compass. `pri` and model geometry share the depth buffer. Map CNKs use the
constant-material color path selected by `njControl3D(0x810)`, not the normal
lighting shader; type-41 normals remain in the shared asset decoder. Sprite
color and texture modulation retain the original factor domain based on 128.

While the map owns the screen, **task 7** is stopped: entering from the game
sets `ts_flg 0x180` (`system.c:777`); entering from the status menu already
stopped task 7 and left `sp_flg 0x30` (`system.c:608`), so `bhControlPlayer`
(`player.c:1365`, `sp_flg & 0x1`) does not run. The host guards world, camera,
and player passes under the same condition; holding a direction no longer
moves the actor while the map is open.

`ScreenFade` reproduces setup, progression, and completion from `screen.c`.
MAP return waits for the live `cb_flg & 0x2`, even if the fade starts in the
same control pass. Status menu → map → status menu → game and reentry were
checked in the native runtime path.

**Open:** analog map input, a global fade/task owner for all other game tasks,
full original primitive/material draw order, and alpha/texture behavior beyond
this native mapping. This is not blanket proof of a complete 1:1 map screen.

`recvx-inventory-engine-tests` generates the full item, examination, and
recipe coverage report [INVENTORY-COVERAGE.md](INVENTORY-COVERAGE.md).

## Rooms, cameras, and doors

**Recorded as ported**

- Room names/identity from `system.c` (decimal), spawns, camera cuts with
  floor comparison (`bhCheckCut`), clip planes (`bhChangeViewClip`).
- Native clip queries with original event/room tables, first-hit rules,
  display-flag-dependent event defaults, and monitor-table bypass. Game Over
  retains its explicitly installed range independently of the camera flag.
- Door sequences from MRY packets through independent native `FadeProc1`,
  `ViewProc1–7`, and `DoorProc1–7`; the host door-skip convenience uses Confirm.
  Room scripts finish before `bhStartDoorDemo`.
- Inventory, flags, HP, and play time survive room changes.
- 3D scanner terminal (`sp_evt.c`) with script interpreter and on-screen
  keyboard.

**Open**

- Special door lighting and vibration.
- 3D actor in the terminal (`bhDrawSpEvtComCharacter`).
- Eff137 as another monitor producer, complete camera-state transitions, and
  visible acceptance of ice-room depth sorting; no PS2 ordering-table clone.

## Models and effects

**Recorded as ported**

- Skinning following `npCalcSkin`/`npCalcSkinFM`, MASK node layout, composite
  upper/lower body zombie motions.
- `eff[512]` slots with `bhSetEffectTb`; selected effects: flame and sparks
  (Eff200/201/218), hits (5), blood particles (`bhEff302`), blood pools
  (`bhEff300`), explosion (16/23/24), lighter (9), casings (0).

**Open**

- General effect lifecycle (`bhInitEffect`, `bhControlEffect`, draw queue) and
  most effect IDs.
- `bhControlLight`, exact EasyLight shading, face morphs.

## Audio

+See [AUDIO.md](AUDIO.md). At this checkpoint, ADX music and voice, Sony sound
+banks, SQ sequences, footstep/door/weapon/enemy/event sounds, and movies ran
+natively. Pitch, envelopes, reverb, and bit-exact SPU mixing were still listed
+as gaps; consult the audio page for later work.
+EOF
+python3 - <<'PY'
+from pathlib import Path
+p=Path('docs/NATIVE_GAMEPLAY.md')
+p.write_text(''.join(line[1:] if line.startswith('+') else line for line in p.read_text().splitlines(keepends=True)))
+PY