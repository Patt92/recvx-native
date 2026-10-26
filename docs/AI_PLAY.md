# AI play and the agent control channel

Short version and setup: README, section "Let your AI play the game".
This document has the details: settings, how the driver works, the control
channel underneath and its limits.

## Settings (.env)

`.env` in the project folder holds the endpoint settings for
`tools/agent_play/driver.py`. It is untracked (`.gitignore`) and is never
committed. Environment variables that are already set win over the file;
`--env PATH` reads a different file.

| Variable | Meaning | Default |
|---|---|---|
| `RECVX_LLM_BASE_URL` | OpenAI-compatible base URL, without `/chat/completions` | `http://localhost:8080/v1` |
| `RECVX_LLM_MODEL` | model name the endpoint expects | `local` |
| `RECVX_LLM_API_KEY` | API key, sent only as `Authorization: Bearer`; empty for a local server without auth | empty |
| `OPENAI_API_KEY` | fallback when `RECVX_LLM_API_KEY` is not set | empty |

Driver options (pass them to `Start-AI-Play.command`/`.cmd` or `driver.py`):

| Option | Meaning |
|---|---|
| `--steps N` | stop after N decisions (0 = until stopped) |
| `--window N` | decisions kept in the prompt before the history is halved (default 8) |
| `--no-tools` | endpoint without function calling; the model replies `{"op":"zone","i":4}` instead |
| `--temperature`, `--max-tokens` | sampling (default 0.2, 200) |
| `--save-rooms N` | allow saving only after N new rooms since the last save or load (default 3; 0 = always allow). Too early save screens are closed without a model call. |
| `--seconds S` | game-time budget per action, at most 8 |
| `--timeout S` | seconds per model request (default 60) |
| `--retries N`, `--retry-delay S` | on timeout, refused connection, HTTP 408/429/5xx: retry after S seconds, doubled up to 20 s; `--retries 0` (default) retries until the endpoint answers. Bad key or model (401/404) stops at once. |
| `--dry-run` | print the first request and exit, no endpoint call |

## AI play: how the driver works

A coding agent pays a full tool round per action (10–30 s). **AI play** instead
lets a host driver ask any OpenAI-compatible endpoint (llama.cpp, vLLM, LM
Studio, Ollama, OpenAI, …) for exactly one JSON action per decision while the
game runs in its normal window.

1. Fill `.env` (see "Settings" above).
2. Start **`Start-AI-Play.command`** (macOS) or **`Start-AI-Play.cmd`**
   (Windows). The launcher runs the policy check, starts the game with the
   control channel (`build/play/live.txt`, fresh each session, enemy AI and
   grabs on), waits for the first metrics and then runs
   `tools/agent_play/driver.py`. Extra arguments go to the driver, e.g.
   `--steps 50`, `--no-tools`, `--model name`, `--window 8`.
3. Watch the window; every decision is printed in the terminal and logged to
   `build/play/driver.log` (model time, game time, tokens incl. cached ones,
   action, result). If the driver stops (endpoint error, `--steps` reached) the
   game stays open; closing the game ends the launcher.

How the driver keeps it fast:

| | |
|---|---|
| Fixed prefix | `tools/agent_play/system_prompt.md` (rules, action schema, examples) never changes, so the endpoint's prefix cache prefills it once. |
| Small turns | Each decision sends only the compact state (~600 characters), the last result and the model's own one-line `note`. The history grows to `--window` decisions and is then halved in one block so the cached prefix stays valid. |
| No idle calls | Scenes, doors, videos, running routes and sliding menus are waited out without asking the model; in fights the game holds (`run auto 18`) until the answer arrives. |
| Tools | `zone(i)` returns ready approach points, `hint(cmd)` reads room scripts and texts (`room`, `zone`, `blocked`, `event`, `msg`). Without function calling use `--no-tools`; the model then replies `{"op":"zone","i":4}`. |
| Ready points | The nearest doors, items, stairs and text zones (switches, consoles, boxes) carry `go` (standing point and facing) and the door to the room just left is marked `back`, so the model neither guesses coordinates nor mistakes the nearest door for the way on. A rejected `interact` returns the computed points as `use_one_of`; a blocked or unreachable `move` returns the nearest reachable free points. A hint command written as an action (`{"op":"blocked",...}`) is answered as a lookup. |
| Puzzles | The model sees what the player reads: dialog pages, document pages (`document`), item texts (`menu_text`) and the terminal screen (`computer.screen`) with what it waits for (`printing`, `confirm`, `keyboard`, `script` + `cancel_exits`). `type` enters a password on the soft keyboard (`free` = letters left). The prompt states the goal (play to the end) and a fixed puzzle order: read, `hint zone`, act. |
| Memory for texts | The last texts the game showed travel along as `recent_texts`, so a hint read on one page survives the next screen. `tips` adds facts derived from the state: the game opened the menu on purpose after a text, or the text names an item the player carries that is not equipped. |
| Computed points win | An `interact` on a zone with a `go` point uses that point; the model picks the zone, the geometry picks where to stand. |
| No endless waiting | A terminal that waits for a button ends a `wait`; if several waits change nothing the model is asked anyway (`unchanged_for`). |
| Item box | The open box shows its contents with names, the row under the list cursor and the game's refusal text. `{"op":"box","store":slot}` / `{"op":"box","take":row}` performs one swap; each cursor and list step is checked against the live state before the next one. |
| Save screen | `typewriter.highlighted` names the preselected Yes/No (saving over a used slot starts on No). |
| Robust replies | The first JSON object with `"op"` is taken (code fences and prose are tolerated); a rejected action goes back to the model as `error`. |

Measured against the headless runtime: buttons 0.1–0.6 s, a route a few
seconds; a decision costs model latency on top. With ~1100 tokens/s prefill and
45–70 tokens/s decode that is about one second per decision (estimate). The
driver never starts or restarts the game itself, never takes screenshots and
never touches the desktop; the key is only sent as `Authorization: Bearer` and
never logged. `driver.py --dry-run` prints the first request without calling
the endpoint.

What the agent cannot do yet: react within a tick (grabs cost ~10 HP each; it
struggles free with alternating inputs after the fact), see what the collision
tables do not encode (stairs and pushable objects are in the tables, cosmetic
geometry is not), or skip a cinematic that the original does not let SELECT skip.

## The control channel underneath

The runtime has a host-side control channel that lets a coding agent (Claude
Code or any model with a shell) play the real game in the real window, on the
native GPU backend, while you watch. No emulation, no screen scraping, no
synthetic input driver: the agent appends lines to a text file and reads the
engine's own state back as JSON. The game logic is untouched; the channel is
inactive unless `--control` is given.

```sh
"build/start/macos-arm64/RECVX Native.app/Contents/MacOS/recvx-runtime" \
  --data data --mods mods --renderer gpu --enemy-ai on --enemy-grab on \
  --snapshots build/play/snap --control build/play/live.txt
```

| Primitive | What it does |
|---|---|
| `--control <file>` | Replay syntax, but the file grows while the game runs. New complete lines are read every frame; their tick numbers count from the moment they were read. With no pending lines the simulation holds (scene stays on screen, audio pauses) so the agent never falls behind real time. |
| `route x z [x z …] [run] [face x z] [then <line>]` | The runtime walks the waypoints itself (turn, walk, next point), then faces a point and runs one follow-up line such as `confirm`. Progress is reported in the state as `control.result` (`moving`/`arrived`/`stalled`). `goto x z` and `face x z` are the single-point forms. |
| `keys forward run 40` | Raw pad hold; a trailing number is a duration in ticks (30 Hz). Cancels a running route. |
| `dump <name> [shot]` | Writes `<name>.json` immediately without ending the process; `shot` adds the last frame as BMP. The JSON is the same state block `--state` writes: player, camera, dialog text and choices, inventory, room items, interaction zones (doors/texts/items with position and extent), collision walls and floors, enemies with hp/mode/position, event tasks, door/fade/movie state, route state. |
| `run on|off|auto [d]` | Real time; `auto` holds again only when nothing is queued and an active enemy is closer than `d`, so cinematics and walks play at full speed while fights wait for the agent. |
| `autodump <ticks> <name>` | Atomic periodic state file — the live feed the agent reads instead of screenshots. |
| Debug menu over the channel | `sdl_key down/up F2`, `debug_select <id> <value>`, `confirm`: snapshots (slot 2 for recovery), room/spawn jumps, HP, items, enemy AI/grab/attacks. Lines are delivered while the menu is open and while the game-over screen is up (a snapshot load ends game over). |

The agent works from [work/live-play/PLAYBOOK.md](work/live-play/PLAYBOOK.md)
(loop, start recipe, interface tables, decision rules, measured constants,
pitfalls) and the tools in [tools/agent_play/](tools/agent_play/):
`nav.py` prints the state in 10–20 lines (who owns input, dialog text, zones,
enemies with line of sight), plans paths over the room's own collision tables
(`path` → one `route` line), blocks until a scene, dialog, door or route is over
(`wait`, acknowledging finished dialogue pages and stopping on Yes/No), and
emits shooting sequences (`fire n [low]`). `stream.py` is a debug stream an
agent can keep in the background (or hand to a subagent): it reads the live
state file and logs only changes — room, input owner, dialogue, HP, grabs,
enemies appearing/closing in/losing sight, items, zones and walls switching —
and keeps a 15-line `radar.txt` current. Enemy posture (`standing`/`low`/`lying`)
and ground contact (`ground`/`air`/`under`) come from the engine's own node
matrices measured against the floor below the enemy, so a downed zombie is
known to be on the floor and gets a lowered aim. When the next step is unclear,
`hint.py` peeks into the room script and code, read-only: per-zone texts and
script blocks with their flag conditions, which wall blocks a path and which
event opens it, decoded events with the original opcode names, and where an
opcode is implemented. Screenshots are the last resort. The playbook is
written for the model, not for a script: there are no canned sequences per room
— one tool call reads, plans, acts and waits.

**Cost and efficiency (measured on an M4 Pro, MetalFX spatial 2–3.8x, frame
interpolation, CRT filter on):** the window idles at ~4–7 % of one core while
holding; a dump is ~15–60 KB JSON depending on wall count and is read through
`nav.py`, so one decision costs the model ~10–30 lines of text, not an image.
Turning is 1310 yaw units per tick (90° = 13 ticks), walking 0.49 units/tick,
running ~1.0 units/tick; a door transition takes 150–600 ticks, longer
cinematics 1200–1700 ticks (`run auto` + `wait`, read once). A
pathfinding call takes ~50 ms. Target: one tool call per action, a room with two
items and an exit in at most six.
Screenshots are opt-in and only for checking what the raster shows, never for
navigation: positions, walls, zones, dialog text and enemy state are already
numbers.
