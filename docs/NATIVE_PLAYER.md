# Player models, skinning, and animation

## Player data

Playable characters come from `data/archives/SYSTEM/0010.bin` through
`0014.bin`. The format follows `dread.c::bhReadPlayerData`: length-prefixed
sections for models (SKIN/MDL), motions (MTN), translation curves, and PVP
textures. The port reads every field with bounds checks and as little-endian;
disc offsets are never interpreted as host pointers.

`PlayerAsset` loads all models in `plp->mdl[0..n]`. Additional models such as
Claire's ponytail (`mdl[4]`, attached to head node 5) are linked through
`lkmtab`, as in `bhInitPlayer`, and moved by `bhCalcHair`.

## Nodes and skinning

Ninja Chunk nodes are 52 bytes long (child/sibling references, bind
translation, and bind angles). The packed skin word selects a bone and a weight
step in tenths. Skinning follows `npSkinConvert`/`npInitCalcSkin`/`npCalcSkin`:
it blends the fully animated matrix with one in which the affecting bone's
local rotation has been canceled. This is not general two-bone blending.
Normals use the same blend. Ninja `evalflag 0x8` hides nodes.

`ActorNodeLayout` reconstructs the MASK layout from `fmCnkInitContext`:
child nodes for teeth, tongue, and eyes are inserted, and the tree is
flattened as in `_fmSetObjP`. Script links and motion channels thus map to the
correct skin nodes; `npCalcSkinFM` shifts indices above 10 by four.

## Motion

16.16 frame counters wrap at the clip length. Fast clips select integer keys;
slow clips interpolate positions and angles along the shortest path. Clip
transitions use the Hokan frames from `Motion.c` (`bhSetMotion` and the SetMtn
routines). Bone matrices are available to linked cameras, lights, objects, and
footstep contacts even when no frame is rendered.

Room actors (NPCs and enemies) load models and textures from the room's pointer
tables. Texture banks are shared according to `bhSetEneMdl` identity. Empty
`MN_WORK` tables retain the last pose when the scene is released.

## Open work

- Face and lip morphs from MASK packets.
- Exact PS2 material shading and EasyLight.
- Arm IK (`bhArmIkMdk`).
