# Native gameplay 0.5

The opening now follows the original RM_0000 event scripts rather than a manually
placed Claire and a proximity hint. The normal initial spawn remains in the cell.
The intro hides the playable actor and restricts controls. Original dialogue,
ADX music/voices and the first film execute through the VM. The original skip
branch remains available through Space.

## Opening and lighter

After the intro (or its original skip), event 16 selects event camera 37 and
message 4: "If I were equipped with a lighter, I could see outside...". Rodrigo
is rendered in the dark beyond the bars; the hint is not shown with Claire
standing outside. Acknowledge it with Enter. The original cinematic flags then
permit opening the inventory. Tab, Enter and Enter select/equip the lighter.

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
fog and translucent materials still need complete bindings. The pre-film intro
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
save/load UI remain pending. Health presentation exists; combat-driven damage
and its full animations do not yet exist.

## Movement and collision

I/K or W/S walks forward/back, J/L or A/D turns and Shift runs. A mapped controller
uses its left stick/D-pad, with B/right shoulder/right trigger to run. Bindings,
deadzone and menu repetition are configurable in controls.ini; see [CONTROLS.md](CONTROLS.md).
Body clips 0/4/9 and original
translation speed curves 0/3/6 drive locomotion at 60 Hz. Motion is inhibited
while a menu/dialog/cinematic or script owns Claire. Room rectangular ATR walls
use their original min corner/extents and script enable flags; swept movement
prevents tunneling through these rectangles. Diagonal/round walls, full floors,
stairs, ledges and the original physical/player state machine remain pending.
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
Menu sounds, footsteps, weapon/environment sound-bank routing and the full
EasySE/MULTSPQ system remain pending. See AUDIO.md for formats and limits.

Known commands without a real native world binding are recorded in the pending
trace and advanced; they are not claimed as implemented effects. The opening
still records over one hundred such dispatches, including effects, linking,
item speed/motion and some actor subcommands. Unknown commands instead block
the task at its original PC, report an error and fail headless validation.
Full original actor/enemy AI, camera transitions, puzzles/item pickups/doors,
room transitions, combat, saving and progression beyond this room are not
complete. All disc data being present does not make those systems playable.

Use `tests/native/replays/lighter.txt` for the authentic equipment flow and
`lighter-walk.txt` for running after it. The old teleport-based diagnostic is
preserved only under `archived/tests/` and is not a gameplay regression.
