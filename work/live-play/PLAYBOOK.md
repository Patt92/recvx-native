# PLAYBOOK — Agent Playmode

General play techniques, control mechanics and processing only. No story, room solutions, item
positions, progress notes or snapshot contents. Session data stays under `build/play/`, check
results under `build/validation/`.

## Mandatory flow: two stages

1. **Think:** From the last compact observation, choose exactly one next action.
2. **Execute:** Call one function. It performs the chosen input and already returns the
   compact observation for the next decision.

No extra desktop queries, wait tools, full reports or planning rounds between these stages.
On `running`/`busy`, choose `wait` directly; do not resend commands.
No fixed room sequences and no game strategy hidden in the tool.

## Fastest way: host driver with your own LLM

For fluent play, `tools/agent_play/driver.py` asks a model behind an OpenAI-compatible endpoint
(`/v1/chat/completions`) directly, one action per decision. An agent with a shell tool needs
10–30 s per round; the driver needs model time plus 0.1–0.6 s of game time for buttons and a
few seconds for routes.

```sh
RECVX_LLM_BASE_URL=http://localhost:8080/v1 RECVX_LLM_MODEL=<name> python3 tools/agent_play/driver.py
```

- The system prompt is `tools/agent_play/system_prompt.md` (rules, action schema, examples); it
  stays unchanged and the endpoint computes it only once through its prefix cache.
- Per decision only the state (`play.py` observation), the last result and `note` (the model's
  memory) go in as new tokens; the history grows to `--window` decisions and is then halved in
  one block so the cached prefix stays valid.
- `zone` and `hint` are function calls; without function calling use `--no-tools` (the model then
  replies `{"op":"zone","i":4}`).
- Scenes, doors, videos, running routes and locked menus are waited out by the driver without a
  model call.
- Every decision is in `build/play/driver.log` (llm_ms, act_ms, tokens, action, result).
- `--dry-run` shows the first request; `--steps n` limits the run.
- Endpoint, model and API key are in the untracked `.env` in the repository root
  (`RECVX_LLM_BASE_URL`, `RECVX_LLM_MODEL`, `RECVX_LLM_API_KEY`; `.gitignore`). Environment
  variables that are already set take precedence; the key is only sent as
  `Authorization: Bearer` and appears in no log. Other file: `--env PATH`.

The driver does not start the game; the runtime runs as below with `--control` and
`autodump 5 auto`. Agent playmode with `play.py` remains for diagnosis and tool development.

## User rules

- The game runs visibly with Metal (`--renderer gpu`). **No vision, screenshots,
  accessibility/window queries or repeatedly bringing the window to the front in the play loop.**
  Data comes only from runtime metrics and the stream. Make the window visible at start; while
  playing, do not touch the window with desktop tools.
- In agent playmode **no Git commands and no `tools/check_no_original_core.py`**, not at entry
  and not per heartbeat either. This explicit user decision takes precedence for playmode over
  the general pre-checks in AGENTS.md. It does not allow engine changes, builds, installation or
  changes to the policy checker.
- Read AGENTS.md before the work phase; keep its rules, do not reload history per action.
- Read dialogue text and confirm it deliberately. No automatic ACK and no blind SELECT.
  SELECT is the only cinematic skip; video and door have their own mechanisms.
- Snapshot slot 2 is the playing agent's own: it may save and load it whenever it helps,
  without asking. Save after real progress (new rooms, a solved puzzle) and before a risky fight;
  load after game over or when stuck, instead of replaying from the retry point. Never use any
  other slot; those belong to the user. A new run still starts from the title screen; do not
  assume an existing slot-2 snapshot as its starting point unless the user says so.
  Control lines (value 1 = slot 2), sent only while no action is pending (`busy` false):
  `sdl_key down F2`, `sdl_key up F2`, `debug_select snapshot_save 1` or
  `debug_select snapshot_load 1`, `confirm`, then `sdl_key down F2`, `sdl_key up F2` to close the
  debug menu. Save only with owner `FREE`; loading also works on the game-over screen and ends it.
  The debug menu reports the outcome in `debug_status` ("Snapshot 2 gespeichert/geladen: <room>").
- On abort, end all automatic continuation. No restart after the game was closed. No periodic
  heartbeats that interrupt running control decisions.
- No subagents/new chats without an explicit request. A control process only executes the
  action chosen by the LLM; it does not decide goals or puzzles itself.

## Function interface

Entry, once:

```sh
python3 tools/agent_play/play.py state
```

After that **one** call at a time, with a sufficient tool time window for the result:

```sh
python3 tools/agent_play/play.py act '{"op":"move","to":[10,12],"run":true,"face":[10,20]}'
python3 tools/agent_play/play.py act '{"op":"interact","zone":2,"to":[10,12],"face":[10,20]}'
python3 tools/agent_play/play.py act '{"op":"press","key":"confirm"}'
python3 tools/agent_play/play.py act '{"op":"keys","keys":["left"],"ticks":1}'
python3 tools/agent_play/play.py act '{"op":"fire","shots":2,"low":true}'
python3 tools/agent_play/play.py act '{"op":"shake","presses":8,"then":{"op":"move","to":[10,12]}}'
python3 tools/agent_play/play.py act '{"op":"wait"}'
```

The numbers are syntax examples, not a room route. Choose `to` and `face` from the current
state. `interact` needs an explicit free approach and appends a confirm at the arrival point;
a zone alone does not justify an invented front probe.
`press`: `confirm/cancel/up/down/left/right/inventory/select/skip`.
`keys` holds known pad actions for 1–90 ticks and releases them afterwards.
`fire`: only with the handgun equipped (item 9 in slot `items[0]`; `weapon` = `wpnr_no` is not
what counts), 1–8 shots, `low` aims low. The runtime control line `fire n [low]` holds aim for
10 ticks and then sets a fire edge every 20 ticks; before that, `face` turns to the nearest
active/grabbing enemy (or `"target": index`). Several `keys` calls do not replace `fire` (aim is
released between calls). The result additionally has `fire`: `shots`/`hits` of this call,
`target` (lock-on `combat_target`) and `target_hp`.
`shake` only under owner `GRAB`: 1–30 newly pressed directions alternating left/right (every
3 ticks; `bhEne_LeverCheck` reads `sys->pad_ps`). `keys` is also allowed under `GRAB`.
`"then": {"op":"move",...}` appends the escape route without a decision gap; the runtime waits
out the grab scene running at start and only steers afterwards (without a `danger` abort).
`move`/`interact` with `"danger": false` run without a danger abort.
A hit does not end `fire`, `shake`, `move`, `interact`, `keys` or `wait`: they run to their own end
and report the hp lost as `damage` beside the result; `damage` is the result only when nothing else
happened. `danger` is judged against the state at the start of the call, also when it began in a
hit stagger. An `interact` that the runtime reports `stalled` while the player stands within the
zone's `near` of the point is sent once more from there (`retried`).
`use`/`equip` (`item`: inventory slot) and `reload` drive the status menu press by press and wait
for each press to show in the state; `reload` combines the matching ammo (combine table) into the
equipped weapon and returns `rounds` [before, after]; `equip` on the equipped slot sends nothing.
`snapshot` (`do`: `save`/`load`) uses slot 2 only, with the debug-menu lines listed above.

Default budget per call: 4 seconds, at most 8 with `--seconds`. This is a bounded wait for
metrics, not a thinking budget. The call returns earlier at a decision point. Do not wait on the
same call with one-second polls and further LLM rounds.
The result contains owner, position, HP, inventory, at most five near zones, four near enemies
and three new stream events. The current dialogue text stays complete; an item name in the text
(message code FF03, e.g. the pickup question) is filled in.

Fields of the compact observation (only while the respective owner is active):

- `inv`: `[slot, id, count, name]`; name as in the status list (`item_names` in the dump).
- `equipped`: `[name, count]` of the equipped slot (`items[0]`).
- `heal`: `[slot, name]` of carried items whose Use restores hp (Use_00 cure table).
- `file`: `{n, page: "x/y"}` in the document reader (`document` = text of that page), `{n, list: true}`
  in the file list. A document is read page by page with right; cancel closes it.
- `enemies[].reach` / `in_reach`: enemy closer than 7 (grabs observed to start at 5.8–6.3);
  walking past is not free there.
- `gameover`: `retry_room`/`retry_play_time` of the retry point (`bhPopGameData`). Without a later
  retry point this is the game start; retry then replaces the progress (load snapshot 2 instead
  when it is newer).
- `title`: `stage` (`fade_in`/`press_start`/`game_mode`/`fade_out`/`settings`), `cursor`,
  `entry` = entry under the cursor, `entries` = rows of the panel, `load_available`.
- `menu`: `cursor`/`navigation` (bar EXIT/FILE/MAP/ITEM), `slot` + `item` = slot and name under
  the cursor; with the command list open, `commands` and `command` (selected entry);
  `examining`; `combine_from` = `[slot, name]` of the source side when combining;
  `equipped: true` when the slot under the cursor is equipped. Equip toggles: Equip on the
  equipped slot unequips. `locked: true` while the screen/command windows slide or the screen
  closes (the pad is ignored); `act` reports `menu` only after that, and the decision hold does
  not apply during that time.
- `dialog.voice` + `dialog.index`: page without printable text (voice page of a scene); confirm
  turns it as usual.
- `enemies[].mode`: work mode `[mode0..3]` (e.g. to tell scripted enemies apart).
- `typewriter`: `mode` (`load`/`save`), `phase_name` (`files`/`confirm`/`no_save`/`io`/...),
  `text` = shown message, `cursor`, `selection` (Yes/No field, 0 = first), `used_slots`,
  `slot` = file under the cursor.
- `zones[].item` and `far_items`: item zones carry the state of the linked room item
  (`present`/`hidden`/`inactive`/`none`/`object`). `far_items` lists all item zones beyond the
  nearest five as `[zone, at, d, state]`.

## Using an item box correctly

An item box is a two-sided **swap**, not a deposit menu. The status screen shows the inventory on the
left; the box list is on the right. A swap exchanges the selected inventory slot with the selected box
row, including when that row is empty.

The compact `box` view shows:
- `mode: inventory`: the left inventory cursor is active; `slot` is the inventory slot under it.
- `mode: list`: the right box-list cursor is active; `at` is its row (0-based), `at_item` is that row's
  item or `empty`, and `contents` lists occupied rows as `[row, name, count]`.
- `refused`: the game's message. The item did not move. Read the reason, press `cancel` once to return
  to the inventory cursor, then choose a different item or stop.
- `empty_rows`: free rows in this box. `box_base` identifies which storage it is; normal boxes share
  storage, special boxes can be separate.

Use one `box` action per swap. The driver performs the inventory-cursor movement, opens the box list,
scrolls to the chosen row, confirms, and returns the compact result; do not send separate `down`/`confirm`
presses while it is executing that swap:

```sh
python3 tools/agent_play/play.py act '{"op":"box","store":4}' # store inventory slot 4 in the first empty box row
python3 tools/agent_play/play.py act '{"op":"box","take":2}'  # take box row 2 into the first free inventory slot
```

**Before each swap:** inspect `inv`, `box.contents`, `box.empty_rows` and any `room_rules`.

1. Read the rule before touching the box. Only store an item if the rule says its presence blocks progress,
   or a game message explicitly asks you to deposit it. A detector rule may name many metal items; the
   inventory slots tell you exactly which of those items you actually carry.
2. Make a checklist from that intersection. Store each needed carried item exactly once, in a currently
   empty row; after every swap check both `inv` and `box.contents` and remove that item from the checklist.
   Do not repeatedly store, take back or swap items already checked off.
3. A successful `box` action returns `swapped`; `refused` means nothing moved. Read `refused`, cancel back
   once and do not retry that item unless something changed. `box` automatically scrolls to the first empty
   row for `store` and the selected occupied row for `take`.
4. When the checklist is empty, close the box with `cancel`, then try the route/door specified by the rule.
   Check whether its wall/zone flags actually changed. If the route is still blocked, inspect the current
   `room_rules`, carried-item list and `hint zone`/`hint event`; do not start swapping checked-off items again.
5. Special boxes may have separate storage. Follow their in-game text; do not assume an item stored in a
   special security box can be retrieved from a normal box (or vice versa). If the game says the items are
   transferred between two special boxes, still verify the destination's `box.contents` before proceeding.

Do not try to store an item when the box is full, or take one when the inventory has no free slot.

Only when really needed, read the details of exactly one zone:

```sh
python3 tools/agent_play/play.py zone 2
```

`zone` additionally returns `approach`: up to three reachable standing points (`to`, `face`,
`path`), free at body radius 3.5, from which the probe lies in the zone while standing (reaches
from `EventVm::interact`: item 2.0, door 4.5, text 6.0 in front of the player; blocked facing
quadrants of the zone excluded). Pure geometry; whether the zone reacts is up to the room script.
Ranking: without overlap first, then probe depth in the zone (`depth`), then closeness to the
centre line across the long side, then path length. Points where the probe of a zone with a
lower index hits are dropped (it answers first); if a zone with a higher index also hits, it is
named in `overlap`. `interact` runs with `near s`; `s` is the distance the player may stop short
of the target without the probe leaving the zone. If the route stops within `s` (45 ticks
without progress, or 8 ticks of walking on the spot, e.g. in front of a door/wall), facing and
confirm are still executed (result `arrived_near`). The final stretch under 3 units is walked,
not run. The runtime accepts `near` only after 8 ticks of walking in place or in the tick it gives up
(45 ticks without progress); a player that turns around the point and comes to rest within `s`
only afterwards is reported `stalled`; play.py then resends the interact once.
Stairs/ladders (type 1) and steps (type 2, only prm0 0) also have `approach`: probe 0.75 resp.
4.5 in front of the player; for type 1 only facings within 45° of the partner zone (other floor),
for type 2 additionally the 6-unit indent point. The step search runs before zones 0/3/4 and
therefore wins on overlap. An empty `approach` carries `approach_note` (e.g. all four facings
blocked, attribute 0x3c00).

## Solving script-driven puzzles

Do not infer the mechanism from a voice line, a visual change or a vague text. When dialogue, a document,
a zone, an event explanation or a stalled route says that a passage is locked, a switch is required, or
an item must be deposited/used:

1. Read the message and note the exact requirement in the run journal. If the event explanation says
   `voice-only`, the game has no printable transcript for that line; do not guess what it said.
2. Inspect the zone with `hint zone <i>` to see its script checks and current flags. For a wider puzzle,
   inspect `hint room` once to list all zone messages and destinations.
3. Follow an item gate or event condition through the room scripts:
   - `hint rules` summarizes room-script item gates whose branches open/close walls.
   - `hint event <n>` shows the opcodes and effects of an event that was started.
   - `hint why <n>` explains why that event starts: current branch conditions, item checks and the
     event's texts, voice-only lines, item uses and wall/zone changes.
   These are general decoders over the current room data, not room-specific solutions.
4. Act on the printed cause. For example, if a rule says the current inventory keeps a wall closed,
   identify the listed items actually carried, check what the game says about storing them, then use
   `box store` for the necessary items. Do not repeatedly open/close a box or re-examine the switch.
5. Verify progress from the next observation: changed wall/zone flags, changed room, or a changed event
   condition. If the expected change did not happen, reread the refusal text/current flags and try a
   different allowed action; do not assume success from a voice or animation alone.

Do not regularly print `nav.py` full lists, complete JSON dumps or `hint.py room`.
Use targeted `hint zone`, `rules`, `event` and `why` calls only when the puzzle state is unclear.
No image/desktop fallback route. Name missing metrics as a tool gap.

## Handling results correctly

- `choice` / `dialog`: read the text, choose the next input deliberately. Wait for printing pages.
- `menu`: status, file, box or debug owns the input; do not treat it as free movement.
- `room`: new room observed; use its current state. This does not prove a playthrough. A
  diagnosis/snapshot start stays marked as such.
- `stalled` / `cancelled`: the route's follow-up command is dropped. No repetition without a new
  situation.
- `danger`: only enemies in work mode active/grabbing (not scripted), only under `FREE`/`GRAB`.
  `wait`/`press`/`keys` report it as soon as one newly comes closer than 15. Routes (`move`/
  `interact`, option `danger 15`) are aborted by the runtime only if one newly comes within 15 AND
  stands closer than 7 to the remaining path; routes under 8 units and `"danger": false` are never
  aborted. `fire`/`shake` do not report `danger`.
- `held`: the runtime is in the decision hold (`run auto`); choose the next action.
- `interrupted`: a cinematic (also a grab), video or room change ended the running route; it
  does not continue afterwards. Read the new situation.
- `grab` / owner `GRAB`: player mode 4; the cinematic bit is set, input stays with the player
  (`shake`/`keys`). Movement is blocked.
- `no_interaction_observed`: arrival without an observed dialogue/menu/room change; not a success.
- `running` / `no_update`: do not trigger anything again. `wait` keeps reading and writes no input.
- `error` with stale/non-advancing metrics: no command was sent. Clarify the runtime state, no
  camera query, no restart and no input on suspicion.
- A running route/pending commands lock further actions; several clients are locked out.
  Do not use old `nav.py`/manual file writers in parallel with the function interface.

Before an input the interface additionally checks for an advancing metrics sample, so a
fresh-looking final file of a game that has ended is not enough to proceed.
It changes no engine state and makes no game decisions.

## Navigation and input

Pathfinding plans over the current collision data; blocked targets are rejected, not silently
moved to some free cell. The checked exact target point is kept as the last waypoint with
`stop 0.35`. The native controller still uses 3 units for intermediate points. Arrival and a
successful interaction are separate results.

The approach planning uses the existing walking radius 3.5 (native/runtime.cpp, step entry;
native/world.cpp, collision interface). This is agent navigation, not new engine collision.
Intermediate points are planned with an extra 0.5 clearance when the room allows it, otherwise
with 3.5; the last stretch to the exact target is checked against the wall geometry directly
(not the raster). If the straight line to the target is free (with 0.5 clearance, under 3 units
with the body radius), it steers straight without a raster detour. The front point/front offset
while walking is not in the dump: `approach` only computes the standing case (`fpx == px`,
`arn` 0). The item probe reaches 2 units in front of `actor`, the step probe 0.75; check a valid
facing and the zone from current metrics.

A pickup can report status ownership before the pickup question exists. Read the question first,
then confirm; the menu exit can take longer than the dialogue afterwards. Files can stay open.
The tool reports such owners; it does not confirm them itself.

Legacy tools remain for targeted diagnosis. `nav.py wait` no longer confirms text automatically
and no longer reports an open status screen as free. Do not cut off active producers with
`head`; prefer using the compact function result directly.

## Runtime and cache

Only one runtime and one stream reader. Do not truncate existing control files. For a new,
requested run, back up old data recoverably; the runtime reads control lines from offset 0 at
start. No fixed-timed new-game/skip sequence.

For the function interface use running metrics (`autodump 5 auto`). `run off` suspends updates;
the interface does not lift such a hold on suspicion.

The agent chooses the run mode with `act '{"op":"run","mode":"auto","dist":18}'` or `"mode":"on"`:

- `on`: real time; the game keeps running during the LLM decision too.
- `auto` (decision hold, recommended with enemies): real time, but with no pending lines and no
  running route the runtime holds as soon as an active enemy is closer than `dist` and a decision
  point exists (no video, no door, no scene except a grab, no message still printing). Each action
  then runs for exactly its own ticks (route until arrival, `keys`/`shake`/`fire` until release)
  and holds again afterwards. While held, the runtime writes a heartbeat dump every 200 ms
  (`control.held`, `control.heartbeat`, `control.tick` stands still); the metrics stay fresh.
  Compact: `held: true`, result `held`. `play.py` no longer puts `run on` before the action under
  `auto`.
- Let time pass deliberately while held: `act '{"op":"wait","ticks":30}'` (1–300 ticks, control
  line `tick` without input). `wait` without `ticks` writes nothing.
- The hold only counts bound enemies in mode active/grabbing. For the danger abort of routes see
  `danger` under "Handling results correctly".

Optionally, a **purely request-driven** process can keep JSON and geometry between calls. It
does not play by itself and starts neither the game nor an LLM. Local Unix sockets need a host
process if the sandbox blocks their bind/connect. The direct `play.py` call works without a
server or socket; it is the default.

```sh
python3 tools/agent_play/play.py serve
python3 tools/agent_play/play.py --socket build/play/play.sock state
python3 tools/agent_play/play.py --socket build/play/play.sock act '{"op":"wait"}'
```

The cache only applies with identical room/wall geometry, floor, height and raster parameters.
Wall positions and extents invalidate it too, not only activation flags. Enemies and owners are
read from current metrics. No second work world.
Precomputing neighbouring rooms remains an open optimization: prepare only by reading loose data,
validate against real runtime metrics on entry and measure cost/benefit before claiming a speed
gain. Do not store room knowledge in this playbook.
