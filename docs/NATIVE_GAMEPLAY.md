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
models. The full original menu transition system, map/files, box storage and
save/load UI remain pending. PulseInit/PulseAnim/PulseFadeAnim are generated directly from original sub1.c
and render the animated condition pulse. Original command-frame sprites, their
settled position and item-dependent height replace the custom rectangle. Holding
left/right rotates examined models every simulation tick instead of waiting for
menu-key repetition. Combat-driven damage and the full heal/poison transition
animations remain pending.

## Movement and collision

I/K or W/S walks forward/back, J/L or A/D turns and Shift runs. A mapped controller
uses its left stick/D-pad, with B/right shoulder/right trigger to run. Bindings,
deadzone and menu repetition are configurable in controls.ini; see [CONTROLS.md](CONTROLS.md).
Body clips 0/4/16 and original
translation speed curves 0/3/6 drive locomotion at 60 Hz. Motion is inhibited
while a menu/dialog/cinematic or script owns Claire. Backward movement uses the
original backstep clip from key 25 and the signed backward curve, without turning
the actor around. ATR wall types 0..5 use original box/cylinder/triangle geometry,
script enable flags, vertical extents and floor-specific attributes. Height zero
inherits the original room height. Claire's 3.5 radius/16.5 height, rounded box
corners and wall sliding are applied with bounded substeps to prevent tunneling.
Full floor/slope traversal (types 6/7), stairs, ledges and the original
physical/player state machine remain pending.
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
Footsteps, weapon/environment sound-bank routing and the full
EasySE/MULTSPQ system remain pending. See AUDIO.md for formats and limits.

Known commands without a real native world binding are recorded in the pending
trace and advanced; they are not claimed as implemented effects. The opening
still records over one hundred such dispatches, including effects, linking,
item speed/motion and some actor subcommands. Unknown commands instead block
the task at its original PC, report an error and fail headless validation.
Type-4 item interactions now use the original Take/Yes/No, capacity and acquired
messages from SYSMES.ALD. FF03's dynamic-item operand and FF04 choices are parsed
without truncating the text. Yes acquires original RDT item IDs and quantities;
No or a full inventory preserves the item. Original ItmCk/ItmSetCk commit global
pickup flags, hide the model and prevent reappearance after room reload. The
first-room knife and handgun ammunition are checked with original resources.
The automatic bend/pickup animation and the full original pickup status layout
remain pending.

Shared NPC texture banks follow bhSetEneMdl's cache identity (enemy/model/texture
version), instead of consuming a new bank for every actor. This fixes the texture
range crash in RM_0020 after the first stairs. That room renders and runs its
initial bytecode without blocked commands; fog/effects and enemy AI remain pending.

Full original actor/enemy AI, camera interpolation, puzzles,
combat, saving and progression through all rooms are not
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
