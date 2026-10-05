# Native player and room actors 0.5

Playable Claire loads the loose original `data/archives/SYSTEM/0010.bin`.
SYSTEM entries 10–14 can be inspected with the native player decoder. No PS2
executable runs and no replacement character geometry is invented. The first
body model is supported; additional accessory/model libraries remain pending.

## Binary and skin layout

The original `dread.c::bhReadPlayerData` describes length-prefixed model (SKIN/MDL),
motion (MTN), translation-speed and PVP texture sections. Native input uses bounded
little-endian fields and finite floats instead of casting disc offsets to host
pointers. Ninja Chunk nodes are 52 bytes with child/sibling links, bind translations
and angles. Fast MTN keys use three float positions and three unsigned 16-bit
angles. Missing channels keep original bind translations or zero angular channels.

The original packed skin word selects a bone and a tenth-weight level. Native
skinning follows npSkinConvert/npInitCalcSkin/npCalcSkin: it blends the full animated
matrix and a matrix with the influencing bone's local rotation cancelled. This
is not a generic two-bone blend. Translation/yaw are included once and texture
indices are remapped into the combined scene. Original normals use the same influencing-bone matrix/weight blend as positions,
following the normal-vector part of npCalcSkin. Ninja evalflag 0x8 prevents
drawing hidden model nodes. Complete PS2 material shading still needs verification.

## Motion and room actors

Signed 16.16 frame clocks wrap by clip length. Fast clips select integer keys;
slow clips interpolate position and shortest-path angles at fractional frames.
Motion transition blending is not yet implemented. Bone matrices are exposed
for original actor-linked camera targets and lights.

RM_0000 cinematic Claire and Rodrigo load from the room's original model/PVP
pointer tables. Their 27-node models receive 22-body-node clips by skipping facial
nodes 6–10, following the original mask/node layout. The first skin/body model
is rendered; additional facial MASK/MDL packets, expressions and lip motion are
not yet fully processed. Room scripts own visibility, position, angles, motions,
frame waits and pause/release. After releasing script control, actors retain the
last supported room motion; this does not implement the full original NPC AI.

## Movement

Gameplay and headless replay share fixed 60 Hz movement. I/K, J/L and Shift use
healthy unarmed clips 0/4/9 (idle 42) and original speed curves 0/3/6; forward
walking starts at key 5. Movement uses original sine/cosine directions. Original
cut areas determine active cameras after movement and scripted repositioning.
Rectangular original ATR collision and script wall flags are implemented.

The full player state machine, weapon/damage variants, motion transitions,
diagonal/circular collision, floors/stairs, physics, camera interpolation and
cut hysteresis remain pending. Host atan2 uses the original angle scale; exact
SDK rounding has not been established. `--preview` retains free inspection.

Synthetic motion/skin tests run through CTest. Real-data opening/equipment and
post-event movement use the replays documented in NATIVE_PORT.md.

## Opening resource audit

Both cinematic actor bodies in RM_0000 contain SKIN and 27-node Ninja MDL records.
Claire's records are 6,480-byte SKIN and 83,200-byte MDL, followed by an additional
8,128-byte 31-node MDL and an 88,748-byte MASK. Rodrigo has 6,124-byte SKIN,
79,488-byte MDL and 87,260-byte MASK. These bytes remain in the productive RDT;
MASK absence is not the cause of the incomplete facial rendering. Its native
facial/lip decoder and additional Claire model are still pending.

PVP indices account for the texture table's leading count and the stage entry.
Opening actors have separately exported TGA bundles with stable mod paths.
The pre-lighter event uses original event camera 37 and its light-hide mask,
rendering Rodrigo as a dark silhouette. Triangle shading, point attenuation,
material transparency and camera interpolation remain approximations, so the
native output is not asserted to match a PS2 frame pixel-for-pixel.
