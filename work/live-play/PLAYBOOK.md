# PLAYBOOK — an agent plays RECVX live

Written for the model, not for a script. Goal: fluent play, small context, no images.
User rules: Metal (`--renderer gpu`), never the software rasterizer; read dialogue, do not skip
blindly; snapshots only in slot 2; no canned per-room sequences; screenshots are the last resort.

## 1. The loop — one decision = one tool call

```
READ   radar.txt (or nav.py A)                      ~15 lines: input owner, enemies, zones, inventory
PLAN   pick a target from zones/text/enemies; path with `path`, never guess angles or distances
ACT    append one line (route / fire / confirm) and block with `wait` until something changes
```

```sh
A=build/play/auto.json; L=build/play/live.txt; N=tools/agent_play/nav.py
python3 $N $A path 61 25 run face 75 25 then confirm >> $L && python3 $N $A wait $A $L 60 stop room danger | tail -12
```

No `sleep` (except at start), no named dumps in the loop, no screenshots. If a call fails or the
answer is unexpected, read the state first — do not resend the same line.

## 2. Start

```sh
rm -f build/play/live.txt build/play/live_state.json build/play/auto.json build/play/stream.log.lock
printf '10 new_game\n70 select\n110 confirm\n150 select\n190 confirm\n230 select\n270 confirm\n330 cancel\n350 cancel\n440 autodump 5 auto\n441 run auto 18\n' > build/play/live.txt
```

The runtime reads the control file from offset 0 at startup, so recreate it before every start.
Load snapshot slot 2 only when the user asks for it (append once the game is running):
`sdl_key down F2`, `sdl_key up F2`, `debug_select snapshot_load 1`, `confirm`.

Runtime in a host terminal tab (the sandbox has no Metal device):

```sh
cd /Users/patrickhoffmann/Projects/recvx-decomp && "build/start/macos-arm64/RECVX Native.app/Contents/MacOS/recvx-runtime" --data data --mods mods --controls controls.ini --debug-config debug.ini --video-config video.ini --renderer gpu --enemy-ai on --enemy-grab on --snapshots build/play/snap --control build/play/live.txt --state build/play/live_state.json
```

Check `Renderer: Metal …` in the terminal. Start the stream (section 5) in the background. After
~20 s `radar.txt` exists; line 1 must say `FREE`, otherwise see section 9.

## 3. Runtime interface (`--control <file>`; lines `<tick> <action>`, tick relative to reading, 30 Hz)

| Action | Effect |
|---|---|
| `route x z [x z …] [run] [stop d] [face x z] [then <line>]` | walks waypoints itself (next point at < 3), then faces, then runs one line (e.g. `confirm`). 45 ticks without progress → `control.result = stalled`, `then` dropped |
| `goto x z [run] [stop d]` / `face x z` | single point / turn only |
| `keys forward\|back\|left\|right\|run\|aim\|attack\|none … [ticks]` | raw pad hold; cancels a route |
| `confirm` `cancel` `up` `down` `left` `right` `inventory` | menus and dialogue |
| `select` / `skip` | cinematic skip (SELECT, the only one) / video skip |
| `run on\|off\|auto [d]` | real time; `auto` holds again only when nothing is queued and an active enemy is closer than `d` |
| `autodump <ticks> <name>` | live state file, written atomically (`autodump 5 auto`) |
| `dump <name> [shot]` | one-off state; `shot` adds a BMP — last resort only |
| `sdl_key down\|up F2` | real debug-menu toggle (more reliable than `debug_key`) |
| `debug_select <id> <value>` + `confirm` | `snapshot_save 1` / `snapshot_load 1` = slot 2 |

Without `--control` none of this is active. Bad lines show up only as `Control: …` in the terminal.

## 4. State fields worth knowing (`auto.json`)

`actor` `yaw` `hp` `room` · input owner: `cinematic` `dialog_open` `inventory_open` `box_open`
`door_active` `debug_open` `gameover_active` · dialogue: `dialog_text` `dialog_choices`
`dialog_choosing` `dialog_selection` · `interactions` (type 0 door, 1/2 stairs, 3 text, 4 item;
`attribute` bits 0x400/0x800/0x1000/0x2000 block facing +z/−x/−z/+x) · `walls`/`floors` ·
`enemies[].body` {`posture` standing/low/lying, `contact` ground/air/under} · `room_items` ·
`items` (id<<16|count, [0] = equipped) · `security_items` · `control` {`steer`, `result`,
`waypoints_left`, `held`} · `player_work.mode` ([0]==4 grab) · `vm_flags` (sparse event flags).

## 5. Debug stream instead of images

```sh
python3 tools/agent_play/stream.py build/play/auto.json            # background task, one per log
python3 tools/agent_play/stream.py build/play/auto.json --until near,grab,gameover,choice,room   # wake-up mode
```

- `stream.log`: change events only — ROOM, OWNER, DIALOG, CHOICE, HP, GRAB, GAMEOVER, ENEMY
  (appears / state / posture), NEAR, SIGHT, ODD, INVENTORY, ZONE, WALL, ITEM, ROUTE.
- `radar.txt`: always-current 15-line report — situation, 5 nearest enemies with bearing
  (left 40° / ahead / behind), posture/contact, state, line of sight; 5 nearest zones; inventory;
  last 4 events. Read this instead of looking.
- `--until …` exits with the first matching event, so a background run wakes the agent exactly
  when it must react (a subagent can hold the same command). Exit 3 = no data for `--idle` s.

Enemy posture comes from the engine's node matrices measured against the floor under the enemy:
`lying` → `fire n low`. `air`/`under` without a script reason is an anomaly worth reporting
(not every one is a bug: scripted objects can sit off the floor).

## 6. Tools (`tools/agent_play/`)

| Call | Output |
|---|---|
| `nav.py A` | state in 10–20 lines (owner, dialogue, zones, enemies with state/posture/SIGHT, items, inventory) |
| `nav.py A path x z [via x z …] [run] [face x z] [then …]` | one `route` line on stdout (~50 ms); `NO PATH` on stderr, exit 2 → `hint.py blocked` |
| `nav.py A wait A L s [stop] [room] [danger]` | blocks until cinematic/dialogue/door/route are over; acknowledges finished pages and prints their text; stops on Yes/No; `room` = after a room change; `danger` = enemy < 14; `stop` = append `run off` |
| `nav.py A fire n [low] [face x z]` | lines for n handgun shots; `low` lowers the aim |
| `nav.py A map [cell [x0 z0 x1 z1]]` | ASCII top view, only when `path` and `hint` disagree with expectations |
| `waitfor.py field value [s]` | e.g. `box_open true`, `control.result arrived` |
| `stream.py` | section 5 |
| `hint.py` | section 7 |

## 7. When the next step is unclear — hints, then code

Escalate cheap → expensive; stop as soon as the answer is clear:

1. Read the zone (`route … then confirm`); the game's own text usually names the solution.
2. `hint.py room` — per zone: message text, item name (taken/hidden), door target, script.
3. `hint.py zone i` — facing locks, direct message, the script block reacting to the zone with
   its flag conditions (current values), where the zone is switched on/off.
4. `hint.py blocked x z` — which wall blocks the path; fixed → detour; script-switched → which
   event opens it and what starts that event.
5. `hint.py event n`, `hint.py op <hex|bhName>`, `hint.py grep <regex>` — decoded events with the
   original opcode names, the opcode's implementation in `native/` and its reference in
   `src/ps2/veronica/prog/`. Read-only; never `archived/`.
6. Screenshot (`dump x shot`) only if the stream and the hints contradict each other, or to check
   a rendering problem.

## 8. Decision rules

- **Obstacle or puzzle:** text first, then hints. `NO PATH` means a scripted gate, not a
  navigation error — do not walk around looking. A locked shortcut usually has a detour, and
  `path` finds it.
- **Enemies:** SIGHT and distance 15–35 → `fire`; < 10 → `path` away first; `lying` → `fire n low`.
  Grab: alternate `keys left`/`keys right` each tick ~20 times. Watch ammo (`inv` 9 = handgun, 12 = bullets).
- **Heal** below 60 HP: `inventory`, cursor to the herb, `confirm`, `confirm`, 3× `cancel`.
- **Snapshot slot 2** after each room change without enemy contact and before fights. Game over →
  load it (the original retry restarts the room and its enemies).
- **No running back without a target.** Decide the target from zones/text before moving.
- **Menus after a route:** `waitfor.py control.result arrived` (or `box_open true`) first, then send
  menu lines — fixed tick offsets after a route land in the void.
- **Budget:** one call per action; a room with two items and an exit in ≤ 6 calls; zero screenshots.

## 9. Pitfalls already hit

- After `snapshot_load` the debug menu may stay open → owner shows `DEBUGMENU`; toggle with `sdl_key F2`.
- `debug_select` before the menu is open is lost (opening resets values): open first, then select.
- `wait` ignores the state file until it is newer than the call; it needs a running `autodump`.
- Shooting a downed zombie with normal aim hits the air: `fire n low`.
- A stale control file replays from the start: recreate it before every runtime start.

## 10. Measured constants

Turning 1310 yaw/tick (90° = 13 ticks); walking 0.49, running 1.0 units/tick; yaw 0 = facing −z,
16384 = −x. Item probe (type 4) 2.0 ahead of the player, must lie inside the zone; stairs (type 1)
probe 0.75 ahead, press OK at the zone edge facing the stairs. Doors 150–600 ticks, cinematics
1200–1700. Idle runtime 4–7 % of one core; `auto.json` 30–60 KB; `path` ~50 ms.
