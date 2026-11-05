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
keyboard actions explicitly. Player 2 consumes movement, turning, run, aim and
attack. Its initial equipment is a knife. The shared trade window accepts either
controller; the original menus and room interactions remain with Player 1.

## Peer-to-peer session

`--netplay-host <port>` / `--netplay-join <address:port>` connect two runtimes
in lockstep: the lobby host selects Player 1 or 2; the guest takes the other seat
with its own keyboard or controller. Details, limits and validation in [NETPLAY.md](NETPLAY.md).

## Item exchange

With an active partner nearby, **Shift+Tab** or **L1/LB + Inventory** (Triangle/Y
by default) opens **ITEM EXCHANGE**. Both bags appear side by side. Left/Right
moves through columns and across the panel edge. Confirm selects a source,
then a destination in the other bag; Confirm again places, stacks or swaps.
Selecting an empty source retrieves the other item. Cancel returns to source
selection or closes the window. Inventory or the trade chord closes it too.
Both controllers can operate the shared cursor; simultaneous commands favour
Player 1. The world, animation, events and game clock pause during the window.

The partner must be alive, on the same floor, within 16 horizontal
units and without an intervening wall. Doors, scripts, dialogue and other
screens block entry. Downed NPCs can receive items and medicine; dead humans cannot trade. Each transaction checks these conditions again.

The existing native `ItemBoxChange` entry operation supplies item placement,
stacking, width/capacity and equipped-weapon removal; `ItemSort` settles both
bags. A transaction journal restores both bags and equipment on refusal.
Original room restrictions and actor-specific reserved equipment remain in
force. Story flags, files and crate contents are not exchanged or copied.
Trading an equipped weapon holsters it; an unarmed NPC automatically equips a received weapon when simulation resumes.
Human partners retain manual equipment selection. Select a weapon or medicine in the **partner pane**, press **Aim**
(default R / R1/RB), and confirm **EQUIP**, **UNEQUIP** or **USE**. Cancel/Back
closes this action menu without a mutation. An equipped firearm also offers
**RELOAD**: the original `bhSearchBullet` / `Combi_99` compatibility and magazine
limits consume ammunition only from the partner bag. Full magazines or missing
matching ammunition leave the bag unchanged. Both panes show their health
condition next to HP. Plain Confirm retains crate-style
transfer selection. Both controllers can operate these partner actions.

Equipment uses the existing `Inventory::equip` / `WeaponSet` owner; its reload
request reaches the partner's native `CombatController` after closing the
window. Medicine uses `ItemUse` / `Use_00` and `ItemSort` on the partner's bag,
with the shared game mode, updating actual partner HP/poison flags.
Reloading uses the same existing inventory owner, including equipped-slot
correction if a consumed ammo entry is removed by sorting. Original
healing amounts, caps, consumption and refusal messages remain in effect.
No primary inventory, floor trigger or story-event flags are passed to these
actions. The partner must remain nearby and outside an enemy takeover.
Using medicine that raises NPC HP above 1 restores its active state; invalid
medicine and cancellation leave it incapacitated. A living human at 1 HP can heal, a dead human cannot.

NPC recipients accept weapons, reload ammunition with a combination table,
medicine and ink ribbons. Key/file/utility items are refused in either swap
direction; human Player 2 may receive key items. These are explicit host rules,
not original multiplayer semantics. Trading does not make unsupported Player-2
firearm bindings complete. Partner inventory persists in the session across
room changes, with the save limitations below.

`--player2-model 10..14` selects a loose player asset (default 11): SYSTEM entry
`ply_id + 10 + costume * 4` (system.c:1419), so 10 Claire, 11 Chris, 12 Steve,
13 Wesker, 14 Claire in costume 1. The debug row **Player 2 model** switches it
at runtime: the room reloads and the companion's session equipment/HP restart.
The asset is loaded with the same
`mods/players/NNNN.bin` override as Player 1. On first entry and when following
through a door, Player 2 uses Player 1's entry position. They initially overlap;
there is no guessed spawn
offset. `--player2-spawn <index>` selects a room POS entry for diagnostics and
requires that index to exist in every room visited during that run.

The debug menu's **Player 2** row switches Off / Local player / NPC follow + fight
when the run was started with Player 2 enabled. The World API also provides
`set_player2_mode` and `set_player2_driver`. An NPC driver receives the current
`RoomWorld` as a read-only view and returns `Player2Command` (move, turn, run,
aim, attack). With no custom driver, the built-in companion follows and fights.

## Companion decisions and incapacity

The following are explicitly new host rules requested by the user, not claims
about original game AI. The companion follows Player 1 with a 14-unit stopping
distance and runs beyond 24 units. It engages the nearest visible, living,
damage-enabled enemy on its floor within a 48-unit leader leash. Friendly
subplayers and invulnerable entry/death states are excluded. The original room
route table supplies intermediate destinations when direct body passage is
blocked. Missing or obstructed routes produce `blocked-route`; the NPC waits
instead of walking through walls. This is not universal room navigation.

The same `RoomWorld::npc_ai` decision also serves AI play for Player 1 (debug
row **Player AI**, `--player-ai`): no leader, so it only engages; firearms use a
20-unit host engagement distance and fire only on an original lock. Details in
[AI play](AI_PLAY.md).

The AI supplies aim/attack input to the existing `CombatController` and native
knife state machine, pose, hit capsule, damage tables and real room enemy works.
It starts a new swing in native ready mode 65 and releases attack during the
swing. There is no proximity damage or independent hit timer. Human Player 2
uses that same combat path. Linked firearm effects and secondary camera modes
remain open. The initial weapon is a knife; supplied firearms use the partner's
own lock table, target cycling and ammunition. Empty magazines use the native
reload operation on the partner bag.

**The NPC never picks up items.** No companion command can invoke interaction,
pickup, item granting or story tasks. This includes all key items; supplies may
be given through the explicit trade window. Its incapacity animation never runs the item-pickup
state machine or sets the GetItem request flag.

NPC damage stops at **1 HP**. On reaching it, gameplay input, AI, attacks and
further damage stop immediately. Original pickup motion 45 runs to its lowest
head key (selected from the actual asset's integer keys), then the committed
crouch pose is held. The resulting `Downed` state is immutable: damage, local
input, NPC-driver changes, mode changes and diagnostic placement cannot move,
resume or kill it. Explicit healing through the exchange window is the user-authorized
exception: transfer medicine, select it on the partner side, Aim, USE.
The user-authorized room tether below may transport
its root position across rooms while preserving 1 HP, life state, exact local
pose and committed animation frame. This does not heal or revive it.

**Human Player 2 is mortal:** its health can reach **0 HP**, followed by a
terminal death sequence using original fall/lying clips 25/26. This host
secondary-actor sequence does not reproduce the complete original primary
death/game-over task. Switching a dead human to NPC mode does not resurrect it;
switching a downed NPC to local input does not revive it.

Incoming blast damage uses the shared native explosion sphere/capsule and wall
query. Enemy melee, grabs, throws, bites, poison and the enemy-driven player
modes (plp `mode0` 4/5/6) reach Player 2 through the same enemy services as
Player 1: before each enemy work runs, `RoomWorld::begin_enemy_pass` chooses
the player that work sees. Host rule: the nearer living, visible player; a work
that already drives a player keeps it until release; Player 1 hidden, scripted
or dead leaves only Player 2. During that work's pass `player_get/player_set`,
joints, capsule, contact (`bhCheckPlayer`), lever shaking, `bhSearchPlayer`,
EN01 `PlayerLink` and `bhSetMotion(plp)` operate on the Player-2 fields
(`world.h` `Player2`), never on a second work table. The driven actor samples
the grabbing enemy's motion library like Player 1 (`player2_set_motion_now`),
HP loss and poison bits go to the companion's equipment, the NPC floor stays at
1 HP (the crouch starts after release), a human Player 2 killed in an
enemy-driven death (`mode0` 6 with `flg` 0x2) is dead. The damage reaction
`bhCPM0_damage` (`mode0` 2: hit from the front/back, motion 71+/72+, the
translation curve and flg 0x400000) runs on the secondary actor too, without
the primary's voice call and `gm_flg` bookkeeping. EN04 (dog) collision parts
and the EN09 PlayerLink operate on the targeted actor's tree. Dormant or
scripted works (`type` != 0) keep the primary view, so original wake-up
triggers still answer to Player 1. Player 2 keeps no `nlxb` trace (its root
stands in). The companion pass keeps
running while Player 1 is enemy-driven (grab/throw/death), so a grabbed Player
2 or a fighting NPC is not frozen by Player 1's grab. The central `damage_player2` rule remains
authoritative for host damage sources.

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
- `CombatActorBinding` gives the existing native weapon component this actor's
  flags, committed local pose/joints and motion writer. Player 1's body, status,
  camera and controller rumble are not borrowed. Both combatants use the same
  World enemy table, effects, collision and audio owners. Knife-hit diagnostics
  include the after-model hit pass (`player.c:1689–1724`, `weapon.c:1002`).
- Runtime gates hold Player 2 during menus, dialogue, movies, scripted/event
  cameras, transitions and other periods without free player control.
  NPC callbacks stop with that pass too.

Room reload creates a new room-local actor; mode, equipment, HP, room identity
(including variant), location and committed pose survive in `Player2Progress`.
The room leash below also transports incapacitated NPCs. Dead human Player 2
stays in its original room and never revives, including after a mode switch.
Off hides and stops the actor. A new game resets the extension's session.
Typewriter saves and host snapshots carry an optional two-player block (file
format 3: mode, model, HP, poison, weapon and the companion's 16 inventory
slots). Older files have no block and load as single player; loading a file
with the block re-enables the companion with that equipment, loading one
without it turns the companion off. Position, pose and room residency are not
saved: the companion rejoins at the loaded player's entry position. Files
written by this build cannot be read by older builds. A runtime started
without `--player2` has no second controller slot; a loaded two-player file
then runs the companion as NPC or Local without its own pad until restarted
with `--player2`. The NPC rejoins the loaded player at the saved entry position.

## Room leash and separate visits

These are explicit host companion rules, not original multiplayer behaviour:

- At a regular physical door, an active NPC in the source room follows to the actual
  destination door entry, regardless of its distance from the player.
  Incapacitated NPCs can stay one room behind.
- Returning to that resident room restores the stored location. Moving on to a
  second room, or taking a different branch while the NPC is elsewhere, brings
  it to the new player's actual entry position. It cannot fall two rooms behind.
- Scripted `DoorCall`, direct transitions, stage/chapter changes, room-variant
  changes, player-character changes and loading/teleporting bring the NPC along
  immediately. The source `EventVm` tags scripted versus physical requests;
  there are no room/event ID lists. Pure same-room resource reloads retain its
  location and neighbor residency.
- The rule also transports a collapsing/downed NPC: HP, life state, local pose,
  held frame and the remaining incapacity animation clock are preserved. No
  damage, movement command, trade, pickup or door order can revive it.

**NPC door** in the developer menu, or `RoomWorld::order_player2_door(index)`,
orders a separate visit. The NPC walks through the current room with its native
movement/collision and routing, then takes the selected enabled `etc` type-0
door using the original trigger bounds/floor/facing and 4.5-unit standing probe.
The runtime resolves the real destination POS entry from loose room data/mods.
Item, lock and story triggers, disabled doors and independent chapter changes
are refused. `Follow Player 1` cancels an order. Obstructed or unsupported
routes can still block the approach; this does not bypass stairs or walls.

The neighbor stores the NPC's session state while Player 1 remains in the
current room. Entering the NPC's room rejoins it; going elsewhere brings it
back within the one-room bound. An absent NPC is neither drawn nor ticked in
the wrong room, and cannot receive damage, trade or diagnostic placement there.
**Off-room combat/events/movement are not simulated.** There is still one active
native room, event VM, enemy/effect table, camera and room audio owner; no second
core/work world, split-screen or network simulation is created.

`--state` exposes the NPC's actual `room`, `room_distance` (0/1 while the leash
applies), `travel` reason and forced `teleports` count. Independent door visits
and ordinary escorted doors are not counted as forced teleports.

## Open scope

EN14/EN09 player links, actor-to-actor collision, equipment/use UI,
interaction, foot anchoring, stairs/ladders, pushing, environmental reactions, linked extra
models (including Claire's hair), face/head control, shadows/mirrors and
character-specific SFX/water effects still require Player-2 bindings. Reached
missing action services are listed separately in `player2.gaps` in `--state`.
The floor/wall primitives alone do not establish complete player physics.
Universal navigation and cooperative door/save/game-over rules remain open.
There is no network transport or split-screen mode.

## Targeted validation

`player2_framework` tests two SDL virtual controllers, keyboard isolation,
hotplug/reconnect, focus and held-button suppression; a real RM_0080 fixture
checks native movement, Player-1 isolation, committed poses, mode switches,
an injected **test** NPC driver, release and reinitialization. Further cases
check actual follow movement, a native knife hit and enemy HP loss, Player-1
isolation, explosion damage, NPC 1-HP clamping, held crouch, immunity to commands,
room departure/return, and mortal human Player 2.

`tools/verify_player2.py --runtime <binary> --data data --output <directory>`
checks the production runtime: enabled/disabled behaviour, independent motion,
NPC following, inventory hold, mode switches, real enemy HP loss through NPC
combat, immutable incapacity and human death, with two-character/combat/crouch
screenshots. Replay-only `player2_keys forward|back|left|right|run|aim|attack|none`,
`player2_mode off|local|npc`, `player2_damage <positive amount>` and
`player2_position x y z yaw` provide explicit diagnostics. Placement is rejected
after incapacity. The combat check positions actors explicitly in RM_0080 and
uses the room's original enemy entry/control path. These are test setups,
not a natural playthrough or complete co-op acceptance. Hardware controllers
and Windows still need independent verification.

`player2_trade` checks real inventory ownership, rollback, quantities/flags,
wide items, equipped weapon indices, NPC restrictions, cancellation/proximity
and immutable partners. Partner-action checks cover UI confirmation/cancel,
weapon toggle, actual HP/poison/cap/consumption, unchanged primary ownership,
1-HP NPC protection and normal healing for a living 1-HP human. Two SDL virtual pads verify same-device chords;
configurations round-trip and older INIs receive defaults.
`tools/verify_trade.py --runtime <binary> --data data --output <directory>`
checks the production modal path, both inventories, source/destination views,
transfer/refusal, simulation hold/resume, unavailable partners and physical SDL
Shift+Tab/Tab/Escape and Aim/Confirm events under the dummy video driver.
It also transfers/equips a handgun and checks native partner firing and ammo
consumption, plus a green herb healing the actual actor from 60 to 110 HP.
Evidence for these additions: `build/validation/partner-actions-final/`. Screenshots are real
runtime renders; item grants and actor placement are diagnostic setups.

`tools/verify_player2_rooms.py --runtime <binary> --data data --output <directory>`
checks separate visits through the real RM_0080 door, nearby escort, one-room
hold, return/rejoin and second-door catch-up through RM_0050/0060. Explicit
diagnostic positions are used. Its event fixture retains the source room's
original scripts with an added scheduler-enabling scan prefix and a delayed
`DoorCall`; it verifies immediate event transport, including a downed NPC's
unchanged 1 HP, held frame and inventory. Native `player2_framework` additionally
compares exact local poses after transport and tests chapter/variant changes,
reloads, corpse exclusion and no primary/story writes on independent visits.
Replay `player2_door <etc index>` issues the same order as the developer menu.

Partner reload validation: `player2_trade` checks cancellation, no-ammo/full-magazine refusal, an 18-round handgun cap and conservation (17 + 25 becomes 18 + 24). `tools/verify_trade.py` transfers a handgun and ammunition through the real exchange UI, reloads 1 + 25 to 18 + 8, and fires after closing it. Rendered evidence: `build/validation/partner-reload-final/`.

Companion assistance checks: `build/validation/npc-help/` covers physical
Shift+Tab while downed, medicine recovery, automatic firearm equipment/firing,
and active door following from beyond the old proximity gate. Inactive actors
remain protected until an explicit medicine action; human death is unchanged.
