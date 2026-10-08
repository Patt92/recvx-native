Progressionsupdate 2026-10-06: Originale Flammen/Funken (Eff200/201/218),
segmentierte en01-Raumbewegungen und der ursprüngliche Abschluss der
Metalldetektor-Ansage sind gebunden. Security Box: Inventaritem auswählen,
OK, Boxplatz auswählen, OK zum Tauschen; Zurück kehrt zur Inventarwahl zurück,
erneutes Zurück/Start schließt. Austausch/Stacking/Restriktionen verwenden
ItemBoxChange, die linke Liste ist eine vorläufige Host-Oberfläche.
Hunde werden über originale Bereichs-/Trapregeln aktiviert; ihre vollständige
Verfolgungs-/Angriffs-KI ist noch nicht vorhanden. Explosionsemitter/Rauch sind
nicht vollständig portiert. Steve-Musik endet gemäß ihrer ADX-Ressource.
Texturen/Filtercaches werden geteilt; das Bild wird nur bei Änderung neu gerendert.
Scan 205/205 lädt, 38 blockieren; Belege/Grenzen: [VALIDATION.md](VALIDATION.md).

Event-Zustandsupdate 2026-10-06: Originale Wortbedingungen für HP,
Dialogauswahl und Spielzeit greifen auf den gemeinsamen Zustand zu.
Standardantworten und Menüresultate folgen Message-Control; Raumvarianten
gehen in Türanforderungen ein. Spielzeit folgt dem originalen VSync-Zähler
und bleibt über Raumwechsel erhalten. Tests/Beleggrenzen: [VALIDATION.md](VALIDATION.md).
Aktueller Scan: 205 Räume laden, 37 mit blockierten Befehlen.

World-/Scheduler-Update 2026-10-05: Modellknotensteuerung und Interpolation,
ObjCtr, Event-Zeitgeber, bewegte Kollisionsflächen, Lichtparameter und sechs
getrennte Gegner-SE-Stimmen sind jetzt im gemeinsamen Engine-Pfad gebunden.
20 Testgruppen und Originaldaten-/Event-Replays auf allen Zielarchitekturen
bestanden. Details und Beleggrenzen: [VALIDATION.md](VALIDATION.md).

Item-/Event-Update 2026-10-05: Der gemeinsame Item-Blickraum entspricht jetzt
DrawSubItem ohne Raumkamera-Y/Z-Umkehr; ursprüngliche Start- und Resultatposen
sind geprüft. bhAreaSearchObj und bhGrdPosSet sind gebunden. Im 600-Tick-Scan
laden 205/205 Räume; 72 melden noch blockierte Befehle. Details und Grenzen:
[VALIDATION.md](VALIDATION.md). Beide v0.5-Starterpakete sind aktualisiert.

# Native gameplay 0.5

The runtime starts at a native main menu using the original ADV/0002.bin title
background, menu lettering and copyright sprites. New Game starts the event VM; Settings provides persistent live
keyboard/controller rebinding, deadzone and menu-threshold controls. Save/load,
difficulty selection and the complete original title transitions remain pending.
`--in-game` bypasses the main menu for diagnostics.

The opening now follows the original RM_0000 event scripts rather than a manually
placed Claire and a proximity hint. The normal initial spawn remains in the cell.
The intro hides the playable actor and restricts controls. Original dialogue,
ADX music/voices and the first film execute through the VM. The original skip
branch remains available through Space. Active films also accept Confirm/Cancel
or Skip and show the configured keyboard/controller buttons. Stopping an active
film does not leave a cinematic skip flag for the subsequent lighter scene.

## Opening and lighter

After the intro (or its original skip), event 16 selects event camera 37 and
message 4: "If I were equipped with a lighter, I could see outside...". Rodrigo
is rendered in the dark beyond the bars; the hint is not shown with Claire
standing outside. Acknowledge it with the configured OK button (Enter/A by default). Completing
this specific hint queues automatic inventory opening after the cinematic handover.
The request is consumed before locomotion, so held movement cannot move Claire
between the hint and the inventory. No Tab/Y press is required. Two separate OK
presses select/equip the lighter. The decoded bhSubStatus command also requests
the shared inventory UI. The automatic hint handover is an explicit native
behavior; the raw RM_0000 event-16 bytes do not themselves contain bhSubStatus. Original face-reset (0x90) and
event-effect-clear (0xca) commands are decoded with their original two-byte
lengths and retained as pending visual bindings.

The original floor trigger at the initial spawn sets the VM floor-contact bit.
Together with the equipped-item condition it starts the lighter/Rodrigo event.
Original actor visibility, room motion keys, position/angle commands, camera
selections, linked targets, light enables, ambient changes, fades, voice commands
and waits run through the event interpreter. The real-data regression reaches
original player position (30.6, 0, 33.9), restored movement/control and VOICE1/0016.
No diagnostic position command is needed.

The scene uses original actors and assets, with partial native lighting/camera
behavior. It is not pixel-identical to PS2 output: particle effects, the virtual
lighter/flame object, facial/lip animation, camera key interpolation, specular,
fog, hands/attachments and translucent materials still need complete bindings. The pre-film intro
can therefore have missing visuals. Lighting uses original vertex normals,
camera masks and light ranges with approximate triangle shading/attenuation.

## Inventory and status

Original atlases and glyphs render the inventory. Empty slots use the original
blue empty-cell graphic instead of a black rectangle. The original SidePackSet
frame adjustment and cursor bounds expose eight normal cells at new game and
ten only after the original GM side-pack flag is set. Item selection,
equip/unequip, two-slot items, quantity/ammunition, health, original C combination
recipes and item-model examination are implemented. Examination rotates original
models. A basic file viewer uses the original 24 files, 167 pages and wallpaper resources.
The full original menu transitions, map/save UI, book/tag animations and original
item-box stacking/animations remain pending. The condition monitor runs the
original sub1.c PulseInit, PulseMain, PulseAnim, PulseFadeAnim, PulseHealAnim,
PulsePoisonHealAnim and Pulse00 (`native/status_pulse.*`): PulseInit on every
opening, one PulseMain per 30-Hz tick in `StatusMenu::tick`, Use_00's heal and
poison-cure sweeps after herbs/spray, and the original input lock while that
sweep runs. Captured njDrawTextureH quads are drawn as texel*base+offset.
Menu sounds follow the original CallSystemSe numbers: open/decide 3,
cancel/close 0, cursor 2 only on actual movement, invalid combination 1. Original command-frame sprites, their
settled position and item-dependent height replace the custom rectangle. Holding
left/right rotates examined models every simulation tick instead of waiting for
menu-key repetition. Combat-driven damage display remains pending.

### Combining and opening items

Choose **Combine**, select the second inventory item, then confirm. All eight
herb recipes use the original combidata/Combi routines in either selection order.
The source stays highlighted; failed combinations retain both items and the
selection with the original COMMON error sound. A fully loaded weapon shows
original message 156. Partial bowgun powder conversion now retains both the
explosive arrows and the remaining ordinary arrows, including the result cursor.

Locked cases open through **Examine**, facing the lock, then Confirm. The original
c_a/itemflg/mestbl tables select the question, check for lockpick ID 50 and load
SYSMES.ALD pages/Yes/No. Missing lockpick, No and Cancel leave the case intact.
A confirmed opening animates the original ITEM1 lid, then replaces the case
in its existing slot; no spare slot is required and the lockpick remains.

| Source ID | Original contents | Contents message |
|---|---|---|
| 63 | Handgun Parts (19) | 162 |
| 128 | Bow Gun Powder (35) | 164 |
| 129 | Magnum Bullets model (30, normalized to ammo 13 on exit) | 163 |
| 83, Briefcase button | TG-01 (85), no lockpick needed | 165 |

Inspection rotates vertically and horizontally. Menu messages are owned by the
inventory and do not advance or stop room dialogue/voice. Use validates original
floor-trigger item parameters before returning control to the room; invalid
keys/ammo retain the menu and show original messages 160/161/174/175/176/195/197.
The original UseItemCheck, ItemLost and flag scripts handle successful use and
consumption. Charged extinguisher use retains the empty extinguisher; the original
Antarctica rejection checks its remaining charge. bhFlrAtariSet2 (0x7c) now updates those trigger parameters natively.

The generator compiles ItemModelCheck, MessageWait, MdlAction00–02, hide masks,
FlagErase, name changes and both 32-tick zoom phases from the original source.
Case IDs select composite content models 19/35/30 with their original five/six/four
node hierarchies. All eleven table-driven transformations are exercised through
the native menu. Silver Dragonfly 119 → 118 removes its wings, retains model 119
and hides the original wing nodes. Security File 106 → 105 is a separate action,
starting directly without a fabricated confirmation. Gold Dragonfly body IDs
79/122/123/124 combine with Wing Object 120 through Combi_05 to reach 66.

NameChangeSet preserves the examination bit for item 72, the empty extinguisher
name and manual name. Herb mixing asks original question 155. Shotgun Single/Burst
changes original event flag 74; shotgun combat itself is still pending. TG-01
registers file 2 in the original acquired-file word 383.

[INVENTORY-COVERAGE.md](INVENTORY-COVERAGE.md) enumerates all 156 item IDs,
117 directed recipes, 1,872 differential combination cases and remaining limits.
This coverage does not certify all puzzles or game progression.

## Movement and collision

I/K or W/S walks forward/back, J/L or A/D turns and Shift runs. A mapped controller
uses its left stick/D-pad, with B/right shoulder/right trigger to run. Bindings,
deadzone and menu repetition are configurable in controls.ini; see [CONTROLS.md](CONTROLS.md).
Body clips 0/4/16 and original
translation speed curves 0/3/6 drive locomotion at 30 Hz. Motion is inhibited
while a menu/dialog/cinematic or script owns Claire. Backward movement uses the
original backstep clip from key 25 and the signed backward curve, without turning
the actor around. ATR wall types 0..5 use original box/cylinder/triangle geometry,
script enable flags, vertical extents and floor-specific attributes. Height zero
inherits the original room height. Claire's 3.5 radius/16.5 height, rounded box
corners and wall sliding are applied with bounded substeps to prevent tunneling.
Ground queries now include type-6 ramps and type-7 floor surfaces, including
negative floor thickness, signed triangular bounds and stacked floors. Native
movement probes near the hip and settles the post-stairs spawn onto y=0, allowing
zero-height event areas to match. Full original gravity, ramp contact/wall
response, ledges and the physical/player state machine remain pending.
Type-1 room objects use the extracted `bhObj001` with its original mobile
ATR walls, work indices, side probes and eight aspd dimensions. Player contact
uses original `bhCheckWallEx`; box movement uses original `bhCheckWall2Box`.
The original `bhCPM2_act_psh` owns alignment, MTN49 entry, MTN50 loop, translation
curve 9, and reverse MTN49 exit at -32768. Motion.c supplies the four-frame
Hokan transitions and the sampled pose before clock advancement. The runtime
runs the push action before normal combat/locomotion and releases control at
original state completion. No per-room pushing rule or direct contact displacement.
Targeted tests include all four directions, all eight footprints, blockers,
a second box, release during entry, Hokan sampling and actual RM_2070 runtime
input/poses/sound. Test placement is diagnostic; no progress flags are injected.
Controller rumble output and unrelated collision effect/stair-limit services
are not implemented by this bridge; unresolved collision service calls are
reported in `push.unsupported`, and are zero in the verified push replay.
Room cameras follow original cut areas after scripted repositioning.

## Events, dialogue and sound

The VM initializes SCD0 and its scheduled tasks in the original order, executes
conditions/flags/loops/waits and evaluates cinematic skip, floor, equipment,
message end, motion-frame, voice-playback and film-playback conditions. Common5
is six bytes: its former four-byte decode corrupted following commands and was
a cause of the incorrect opening. Message FF02 nonzero is a timer; zero waits
for confirmation. Dialogue is white text with a black antialiased edge over the scene, with no
background panel. Empty timed messages draw nothing.

BGM1 and VOICE1 ADX playback is native. Film sound is native original stereo PCM.
System-bank menu, inventory, choice and script sounds play natively. Completing
a dialogue also stops its active voice so its original playback wait can finish.
Healthy walking/running/backstep contacts play the original room bank-2 sounds,
selected by ankle position and ATR floor material. Event SeOn/SeOff now dispatch the original bank-2 programs on separate event
voices; UI, feet, weapon and film PCM channels remain independent. Repeated
queries of an unchanged contact frame do not emit another step. Original pitch,
envelopes, environmental MIDI and the full EasySE/MULTSPQ system remain pending;
these checks do not establish the reported running-audio timing on hardware. See AUDIO.md for formats and limits.

Known commands without a real native world binding are recorded in the pending
trace and advanced; they are not claimed as implemented effects. The opening
still records over one hundred such dispatches, including effects, object-to-object linking,
item speed/motion and some actor subcommands. Unknown commands instead block
the task at its original PC, report an error and fail headless validation.
Type-4 item interactions now use the original Take/Yes/No, capacity and acquired
messages from SYSMES.ALD. FF03's dynamic-item operand and FF04 choices are parsed
without truncating the text. Yes acquires original RDT item IDs and quantities;
No or a full inventory preserves the item. Original ItmCk/ItmSetCk commit global
pickup flags, hide the model and prevent reappearance after room reload. The
first-room knife and handgun ammunition are checked with original resources.
The shared input adapter opens a modal status menu with the found item's original
model and question. The item remains visible through the acquisition/full-capacity
message, and acknowledgement returns control to the room. Menu messages use
original mes_spos mode 1 (44,342), replacing the item-name line; room dialogue
uses mode 0 (68,374). The automatic bend/pickup animation and exact original
status transitions remain pending.

Shared NPC texture banks follow bhSetEneMdl's cache identity (enemy/model/texture
version), instead of consuming a new bank for every actor. This fixes the texture
range crash in RM_0020 after the first stairs. The shared player animation path now samples room clips during scripted motions.
Common 24/25/34 select room or built-in actor libraries with the original 16.16
speed table, start frame, pause and clip wrapping. Enemy emtp libraries are shared
by identity and decoded on demand. Original FOR countdowns now feed root position
and angle interpolation. Node interpolation, segmented enemy motions and complete
actor driver state machines remain pending.

The original cemetery trigger is reached by walking from the actual post-stairs
spawn through the original walls, and its task group restores player control.
The Steve scene also reaches its original final position/control state in a
separate diagnostic trigger test. BGColorSet's six-byte command and delayed colour
application are bound; CyodanSet's 22-byte particle command is parsed and retained
as an explicit pending effect. These tests establish task progress, not complete
cutscene rendering, enemy AI or every campaign event. See
`tools/verify_events.py` and `tests/native/replays/graveyard-walk.txt`.

Full original actor/enemy AI, camera interpolation, puzzles,
projectile/special-weapon combat, saving and progression through all rooms are not
complete. All disc data being present does not make those systems playable.

Use `tests/native/replays/lighter.txt` for the authentic equipment flow and
`lighter-walk.txt` for running after it. The old teleport-based diagnostic is
preserved only under `archived/tests/` and is not a gameplay regression.

## Room transitions and doors

Active original type-0 ATR interactions request the RDT destination and original
spawn. Facing/floor filters and the original 4.5-unit door probe are applied.
DoorCall opcode 0x33 uses the same deferred transition path. It never destroys
the VM while a script is executing.

Original MRY MPAC door packets drive generated FadeProc1, ViewProc1..7 and
DoorProc1..7 procedures. The model hinge/knob and camera animate at native ticks;
PT_WORK selects the forward/reverse parameter case. A new Confirm edge skips
the sequence on keyboard or SDL GameController. Held buttons do not generate a
second edge. Inventory, movement and dialogs are suspended during the sequence.

Inventory, equipment, health and side pack are owned outside RoomWorld/EventVm;
the same Inventory instance is retained. Global event/key/item/map/game flags
are transferred; room/task/dialog state is rebuilt. The cell-to-hall round trip
is tested using actual locomotion after the original opening, without teleport.
Both natural completion and skipped transitions return to the same original
spawn, retain inventory and do not replay the opening.

105 original door parameter cases are rendered and checked through completion
and skip. This checks door resources; it does not claim complete gameplay in all
205 rooms. Door sound banks, special lighting and rumble remain pending.

## Actor attachments and model visibility

Native bindings now handle bhObjLinkSet (0x32), bhObjLinkSetPly (0x34),
bhObjLinkSetEneItem (0x53) and bhObjLinkSetPlyItem (0xa3). Original signed
hundredth-unit offsets attach room objects/items to animated NPC/player nodes.
They inherit the parent transform and visibility; release retains the last linked
position. Pose sampling for linked camera/light/release points reads current
script state, even without rendering a frame. bhDefModelSet (0x3b) hides/restores
an individual model node's geometry while keeping its skeleton active.

These bindings cover room prop attachments, not missing hand geometry. Full
accessory model libraries, MASK/face/lip processing, special cloth/hair
links and object-to-object/enemy links remain pending. The opening regression
now records 109 supported-length dispatches without native bindings and zero
blocked commands.

## Native shooting and lock-on (2026-10-06)

SYSTEM packets supply original weapon meshes, textures and body clips installed
at motion 100. The shared combat path now supports knife, Handgun, Steve's M93R,
Custom Handgun, Magnum, M-100P, dual Submachine Guns, Assault Rifle
and Sniper Rifle. Shotgun, bowgun, grenade/projectile and other special weapon
behavior remain pending; the sniper scope itself is not yet ported.

`tools/generate_targeting.py` extracts bhSearchNearEnemy/bhSearchNextEnemy,
bhCalcLockEneYR (pwksub.c), bhSearchEnemy/SetLockOnDirection and MixSetToJointRot
(playpch.c). Host adapters bind live eligible enemy work, player joint matrices
and the shared wall query. Single guns choose the nearest visible enemy and use
the original visited-target/distance cycle; dual weapons use the original
split/concentrated toggle, body direction and independent arm angles. Dead,
hidden, disabled and collision-disabled enemies and NPCs are excluded. Common 46
binds the original flg 0x20; script-owned animation alone no longer disables hits.
Unoccupied auto aim retries live acquisition while aim is held; manual turning
keeps manual aim until release or target switching. The adapter currently
uses the bound en01 target volume and native wall geometry; it is not a full
port of original joint capsules or every bhCheckC2WallN attribute rule.

Steve's M93R is weapon 4 and fires single shots. Custom Handgun is weapon 5:
WpnTab bits 0x40/0x200 and fend_ct control its three-shot cadence/release latch.
Dual fire follows the original gun_delay of 2; the left hand shares the right
hand's ammo decrement. Other rapid weapons use fend_ct. The common path preserves
inventory quantities, infinite-ammo flags, reload combinations, empty sounds,
modal cancellation and held-input inhibition. Readiness and body motion sampling
retain the existing native handover; the complete original interpolation/state
machine, rapid-weapon recovery and sniper scope remain incomplete.

The primary type-0 muzzle effect uses original WpnEffTab, bhEff003 and the type-0
bhDrawWeaponEffect branch, including its UV table, three quads, two-frame lifetime,
SYSTEM common texture group 3 and original 8/10 blending. Both hands use the
original wrist offsets and MASK script indices. Other muzzle variants (including the SMG-specific effect 229), smoke, ejected shells, bullet
impact/blood effects, weapon lights and physical controller vibration remain
pending. ARMS shot/reload/empty sounds play through the existing Sony transport.

Bound en01 zombies receive distance-dependent original damage. Walls stop shots;
HP reaches a latched death state and room scripts observe bhDieCk. The native rigid
collapse remains provisional; original joint-specific damage/reactions/death,
enemy attacks/AI and other enemy families are not completed by this change.

Validation: nearest/cyclic/occluded/dead targets, dual arm geometry and distinct
zombie hits with one ammo decrement, real weapon assets for owners 0–3, burst
release/cadence, infinite/empty/reload and modal controls. The walked original
Steve event grants 12 rounds; aiming/shooting afterward leaves 11 and returns to
idle without teleports, flag edits or debug inventory. See tools/verify_combat.py.

Weapon packet overrides: mods/weapons/0022.bin (Claire knife), 0023.bin (handgun).
Texture replacements: mods/textures/weapons/0022/right/0000.tga or the analogous
0023/right/left path. Existing asset dimension/format validation applies. Packets
come from loose exported SYSTEM files; runtime never opens the ISO or AFS.

Original floor-item pickup remains the RDT GetItem/Yes-No/ItmCk/ItmSetCk flow,
including full inventory rollback and persistence across room changes. Real
keyboard Enter and SDL virtual-controller A now exercise knife/ammo pickup and
weapon input routing in the same input regression.

## Shared actor and timing integration (2026-10-05)

The PS2 main loop waits for two VSyncs. Native simulation, movies, menu pulse and
ADX mixing now use the same 30-Hz clock (rounded NTSC), independently of drawing.
Item pickup/examination initializes dsptbl angles; New Game plays ADV/0003.
The default window is 1280×960 with integer nearest scaling of the 640×480
renderer. Original font atlases remain; high-resolution UI rendering is pending.

ActorNodeLayout reconstructs procedural MASK child insertion and original
flattening. Script link indices and motion channels are mapped to immutable
skin nodes; Rodrigo's wrists no longer resolve to an ankle. Face/lip morphs
are still pending. en01's lower 8 and upper 10 channels (MTN+200) share original
clocks and are composed; InitType supplies initial poses 2/3/13. This fixes
partial/T-pose animation, not the missing enemy brain/attacks.

Door sound triggers and FootSeCall are bound through shared original-data paths.
See AUDIO.md for limits. All 205 rooms load after Claude's loader corrections;
81 rooms still report blocked commands in the 600-tick scan. Explosion/particle
rendering, including the cemetery helicopter effect, remains incomplete.

Live-target regression (2026-10-06): real room motions in RM_0020/0070/0080/0090,
Common 46 disable/release, hide/reveal, held-aim acquisition and manual aiming.
Original RM_0080 event 40 restores collision even while the native scripted
motion marker remains set. This does not establish the reported shutter/barrel
sequence as fully working. Destructible barrels/explosion damage and complete
enemy joint capsules remain unbound.

## Enemy damage, death, blood and barrels (Claude, 2026-10-06)

Hits no longer subtract HP and tip the mesh over. The original en01 damage path runs
through `tools/generate_en01_damage.py` → `native/en01_damage.c` (generated `en01_damage.inc`):
`bhEne01_DmgCheck`, `DamageAdd`, `PartsDamageCheck`, `DmgCheckType00/02`, `DmgModeJumpCheck`,
`bhEne_CalcDamage`/`InitDamage`, the reactions `DG00..04/16`, the death states `DD00..03`, MV04
(downed/get-up and the hp<0 hand-over to death), `bhEne01_Init`/`InitType*`, the weapon tables
(`En01_WpnDamageTbl`, `CombWepTbl`, `chg_mtn_tbl`) and the damage flinch of `bhEne01_SetMtn`
(object 8, `rot_tbl`). `weapon.c` assignment (joint = nearest joint matrix, near/mid/far `comb_flg`,
shot direction) is `CombatController::hit` → `RoomWorld::damage` → `recvx_en01_hit`.
What a pistol does is therefore the original: one hit = flinch (`nm_act` 0), three near hits inside the
timeout reach `CombWepTbl` crit 25 → knockback (DG03/DG04), lethal → MV04 → DD00 with the original
death MTN and blood pool. Explosions use `wpnr_no` 20 (`En01_WpnDamageTbl[20]`).

Blood: `bhEne_SetBlood/2/3/4` and `bhDGCdirCheck2` are transcribed; effect 5 (hit sprite) uses the original
`bhEff005`. Particle sprays are the original emitter (`generate_effect_particles.py`:
`bhSetEffParticle`, `bhEff302`, `PtclTbl`, `Eff302Prm`) drawn like `bhEff_PtclSpriteDrawB`
(effect texture category 4, blend 8/6). Floor pools are `bhSetBloodPoolLnk`/`bhEff300` (category 17).
Barrels are enemy id 0: `bhEne00` is transcribed (movable wall → collision wall, hit flag 0x4 →
`bhSetExplosion`: sound 534, flash light slot 0, effects 16/23/24). Effect 16 runs the original
`bhCheckBombAtari` blast (radius 20, 150/100) against en01 works, other barrels (chain) and the player.

Still unbound (reported by `RoomWorld::unbound_enemy_routines()` and printed by
`recvx-enemy-damage-tests`): zombie AI (MV00/MV01 walk/brain, `bhEne01_MVType` prelude), `bhEne01_SePlay`
(enemy hit voices), stairs/ledge helpers (`DansaCheck`, `KaidanAtrCheck`, `ZulzulCheck`), head/limb
removal, fire, `FlyingCap`, effect 24 debris and 25 smoke, SetBlood3 skin chunks (effect 370), the spark
line draw (`bhEff_PtclLineDraw`), per-joint capsules for weapons with `WpnTab.flg & 8`, other enemy
families, and a game-over on player death (blast damage only lowers HP, minimum 1).
After a reaction the zombie holds its last pose until the walk AI is ported.
Verification: `recvx-enemy-damage-tests <data> [out-dir]` (RM_0020 zombie: flinch, combo, lethal, blood,
RM_3160 barrel blast; frames written to `out-dir`), `recvx-combat-tests`, `verify_combat/events/progression`.
Effect sprites bound in this change (5, 23) use their original `njColorBlendingMode` pair; the older atlas
sprites keep their additive approximation until their pairs are compared with the original look.

### Zombie AI, hit voices and the debug switch (Claude, 2026-10-07)

The en01 chase AI is the original code, transcribed by `generate_en01_damage.py`: `bhEne01_EneSearch`
(`bhSearchPlayer2` view cone, memory timer EXP0 0x28/0x29), `Brain00/02` (`bhCheckRoute`/`bhCheckRouteID`
over `rom->rutp/ruttp`), `ActionModeCheck`, `MVType00/02`, `MV00..03`, `MV05` (reach/attack), `ChgWalkMtn`,
`FastWalkCheck`, `ikou/ikou3/NitenDir_ck`, `bhEne_CheckDirWall/SideWall`, `bhEne01_Move` (player distance
EXP0 0x54) and `bhEne01_SetMtn`'s tail `CheckMtnTbl`. Walking is the original foot lock: `bhCalcFixOffset`
compares where the new local pose puts the planted foot chain (`en01_tree`) with the previous pose's joint
matrix and shifts px/pz by the difference. The host supplies those poses (`RoomWorld::tick_en01`), the
wall probe/move (`step_clear`, `move_walls`), line of sight (`shot_distance`) and the player state.
The upper-body work (`exp1`) is kept as real state so the routines address both works; its motion
(`upper_mtn`) pairs with the lower clip when the frame counts match (otherwise motion + 200).
`bhEne01_SePlay` is original code too; `AudioMixer::request_enemy_se/exec_enemy_se` follow
`RequestEnemySeBasic`/`ExecEnemySeManager` (six voices 18..23, priority, same-SE limit, bank (SeNo>>8)&0xF).
Programs the Sony decoder rejects are listed in `audio_unavailable` instead of stopping the game.

Debug menu entry "Gegner-KI" (`enemy_ai`) and `--enemy-ai on|off` switch it. Default: on for interactive play,
off for `--headless`/`--replay` and in `RoomWorld` itself (unit tests), so test zombies stand. With the AI off
idle zombies stay put; hit reactions, knockdown recovery (MV04) and death still run.
Not ported: the grab/bite (`EatCheck`, `bhEne01_Nage`, player `PlyDG*` states), stairs (`KaidanCheck`), poison,
neck turning (`SearchNeck`), `CollCheckWall` limb sub-checks and crawler branches, the push of zombies by other
zombies (`bhCheckEnemies`; only the player capsule is kept apart), jump-down/ledge states. Zombies therefore
walk to the player and swing but cannot hurt them yet.
Still open: effect 24 debris/25 smoke after explosions, spark line draw, per-joint capsules for WpnTab flg&8
(only weapon 13 of the supported guns), player death/game over.

### Zombie grab/bite, player damage and death (Claude, 2026-10-07)

Switched off by default; debug menu "Gegner-Angriff" (`enemy_grab`) or `--enemy-grab on` enables it (the AI must be on).
With it off `EatCheck` finds nothing, exactly as before. The code is original (generator): `EatCheck`, `bhEne01_Nage`
(`NG00` bite, `NG01`, `NG02` crawler), `PlyDG00/01` (the grabbed player's states), `bhEne01_PlayerControl`,
`bhEne01_PlayerLink` (the player follows the zombie, wall-checked), `EnemyPushChk`, `bhEne_LeverCheck`,
the `ply_pos` tables and `En01_PlyMtn_OffsetTbl` (grab clips 400+ in the zombie MTN list).
The player is a persistent work (`player_work`) that these routines own while mode0 is 4/6; the host mirrors it:
`RoomWorld::sync_player_grab` sets `player_scripted`, plays the clip from the enemy library through
`PlayerAsset::sample_clip`, moves the actor and frees it again after the release (idle motion 42). hp lost in a grab
reaches `take_blast_damage()`; a negative hp (death condition) is remembered until the host revives the player.
Mashing: the runtime derives sys->pad_ps from newly pressed direction/button edges (`set_player_lever`).
Verified (`recvx-enemy-damage-tests`): off = no grab; on = clips 406/410, release after mashing, hp 200 -> 170;
fatal bite = clip 412, `player_dead()`; frames in `build/validation/enemy-damage-20261006/grab5`.
Host placeholders: game over (gameover.c is not ported) fades to black and returns to the title menu with full hp.
Player voices now load the original CORE sound pack and play through SE bank 4, slot 7 (Codex, 2026-10-07). Not ported: crawler head cutting effects, stairs/ledge grabs,
`bhEne01_EatCheck` mode 2 (needs `npCollisionCheckSC`), the second player (`ply_id` follows the character index only).

### Original player voice banks (Codex, 2026-10-07)

`RequestPlayerVoiceSoundBank` selects `ply_id | 0xfff0` in the current disc's MULTSPQ index.
The runtime loads its CORE SE bank 4 after the room bank, including debug character changes
and snapshot room reconstruction; SYSTEM entry maps to `(entry - 10) % 4`, including costume 14.
`CallPlayerVoice` uses the existing bank/list route and independent slot 7. Replacing CORE
invalidates decoded bank-4 samples and stops the previous voice. Audio diagnostics now retain
the originating pack for each SE bank across room changes.

Targeted checks: four original character banks on each disc, audible bite/death PCM,
character cache replacement and CORE retention across room-bank loads. Original RM_0020
grab/mash and fatal-bite cases also generate Claire voice PCM. This verifies decoding/routing,
not perceived in-game timing or the original room volume, pitch and SPU processing.
Enemy attacks remain off by default; original gameover.c and the other grab gaps remain open.
Logs: `build/validation/player-voice-20261007`.
