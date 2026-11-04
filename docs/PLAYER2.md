# Optional Player 2 framework

This is a user-requested host extension, disabled by default. It prepares a
second local player and a programmable NPC on the same footing, with the
existing camera. It is not original Code: Veronica behaviour or complete co-op.

Start the development runtime from the repository root:

```sh
build/macos-arm64/recvx-runtime --data data --player2 local
build/macos-arm64/recvx-runtime --data data --player2 npc
```

`local` assigns keyboard/controller slot 1 to Player 1 and controller slot 2 to
Player 2. SDL's controller mapping, including XInput, uses the existing action
bindings. Removing controller 1 never promotes controller 2; a new controller
fills the vacant slot. Without the extension, all controllers retain the
existing single-player merge. Player-1 rumble goes only to its assigned pad.

Player 2 uses controller bindings from the initial controls configuration.
`--player2-controls <controls.ini>` supplies an independent configuration in the
existing format and enables its keyboard bindings too. Choose nonoverlapping
keyboard actions explicitly. Player 2 currently consumes movement, turning and
run; menus, interaction, aim and attack remain with Player 1.

`--player2-model 10..14` selects a loose player asset (default 11), with the same
`mods/players/NNNN.bin` override as Player 1. At each room entry, Player 2 starts
at Player 1's entry position. They initially overlap; there is no guessed spawn
offset. `--player2-spawn <index>` selects a room POS entry for diagnostics and
requires that index to exist in every room visited during that run.

The debug menu's **Player 2** row switches Off / Local player / NPC controller
when the run was started with Player 2 enabled. The World API also provides
`set_player2_mode` and `set_player2_driver`. An NPC driver receives the current
`RoomWorld` as a read-only view and returns `Player2Command` (move, turn, run).
It shares the local player's action path. No automatic follow/pathfinding AI
is installed: the default NPC stands and reports `NPC driver not bound`.

## Native ownership and reused mechanisms

`native/player2.cpp` keeps a `WorldEntity` inside the existing `RoomWorld`.
There is no second engine, enemy table or script world. The second actor owns
its action fields and `WorldEntity::own_models` object tree. Its state never
replaces Player 1 or a room NPC/enemy slot.

- Movement calls the existing `player_action.c` port of `player.c:1820–2716`,
  including the original motion tables and translation curves.
- `motion_set.cpp` (`Motion.c:21`) writes the local pose before advancing the
  animation clock. Skinning/drawing reads the committed object tree. Drawing
  while paused never samples the next motion or advances that clock.
- The front/body probes and floor query reuse `geometry_motion.cpp`
  (`player.c:1578–1664`, `hitchk.c:641/5245`) on this actor's fields and the
  current shared room geometry.
- Runtime gates hold Player 2 during menus, dialogue, movies, scripted/event
  cameras, transitions and other periods without free player control.
  NPC callbacks stop with that pass too.

Room reload creates a new room-local actor; the selected mode survives. Off
hides and stops the actor; release destroys its work and asset reference.
Player-2 state is not serialized into snapshots or typewriter saves. Loading
one recreates Player 2 at the room entry rather than restoring its position.

## Open scope

Combat/damage/enemy targeting, actor-to-actor collision, independent inventory,
interaction, foot anchoring, stairs/ladders, pushing, environmental reactions, linked extra
models (including Claire's hair), face/head control, shadows/mirrors and
character-specific SFX/water effects still require Player-2 bindings. Reached
missing action services are listed separately in `player2.gaps` in `--state`.
The floor/wall primitives alone do not establish complete player physics.
NPC decision-making, navigation and cooperative door/save/game-over rules are
open. There is no network transport or split-screen mode.

## Targeted validation

`player2_framework` tests two SDL virtual controllers, keyboard isolation,
hotplug/reconnect, focus and held-button suppression; a real RM_0080 fixture
checks native movement, Player-1 isolation, committed poses, mode switches,
an injected **test** NPC driver, release and reinitialization.

`tools/verify_player2.py --runtime <binary> --data data --output <directory>`
checks the production runtime: enabled/disabled behaviour, independent motion,
NPC source selection, inventory hold, mode switches (including the debug menu), and a two-character
screenshot. Replay-only `player2_keys forward|back|left|right|run|none` and
`player2_mode off|local|npc` provide explicit diagnostics. These are test setups,
not a natural playthrough or complete co-op acceptance. Hardware controllers
and Windows still need independent verification.
