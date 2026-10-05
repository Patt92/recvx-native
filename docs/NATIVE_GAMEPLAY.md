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

## Native knife and handgun combat

SYSTEM weapon packets supply right/left meshes, PVP textures and body clips
installed at motion 100. Knife and handgun use original PlMtnWpn ready/aim/attack
clips, raised/lowered variants, original 9-tick ready handover, turning rate and
original attack clip duration. Reload uses clip 116 and Combi_00 ammunition rules.
WpnTab, KnfAtrTab, EneDamNear/Mid/Far and en01_hp_tbl are generated directly from
the supplied decompiled constants. One handgun round is consumed per shot; an
infinite-ammunition item flag bypasses consumption. Knife contacts transform the
original right wrist/forearm nodes only in the original active frame window.

First-room en01 zombies are targetable using original 3.8/18.2 body dimensions.
Closest targets receive distance-dependent damage; wall volumes stop shots and
knife segments. The muzzle cannot shoot through a wall between it and Claire.
NPCs and cinematic actors are excluded. Native HP reaches a latched death state;
bhDieCk writes the original global death flag for room scripts. A provisional
30-tick rigid collapse displays the original mesh as a corpse. This is explicitly
not the original segmented damage/death animation. Enemy AI, attacks against
Claire, joint-specific damage, blood/muzzle particles, aim assist, personal zombie
HP modifiers/random initialization, original reactions and other weapon/projectile
families remain unfinished. Health currently selects an original normal/easy
table entry deterministically by room enemy index.

Weapon effects read the original ARMS sound-bank keys in MULTSPQ1; handgun shot,
empty/reload sounds and knife swing play on an independent mixer channel. Modal
inventory/dialogue/video/door/event control cancels combat and requires release
of held weapon controls. SYSTEM/0020.bin supplies unarmed hands. The selected weapon remains omitted while
a script owns Claire; the original bare-hand packet can still attach to her body.
This does not complete the segmented Rodrigo/cinematic-NPC hand models.

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
