# Peer-to-peer two-player session (netplay)

A user-requested host feature on top of the [Player 2 framework](PLAYER2.md).
Two runtimes connect directly over one TCP connection, without a server. The
host chooses Player 1 or Player 2 in the lobby; the guest gets the other seat with the keyboard or controller of
its own machine. It is not original Code: Veronica behaviour and not complete
co-op: menus, items, doors, saves and the camera stay with Player 1 exactly as in
the local two-player mode.

## Start

Use **MULTIPLAYER** in the title menu, then **SOLO** (start a new game directly with the NPC companion) or **LOBBY**. The host selects CREATE LOBBY, its player
seat, and CONNECT. The guest selects JOIN LOBBY, enters the host address or
seven-word address phrase, then CONNECT. The host decides the seats, including
host Player 2 / guest Player 1. Player 1 controls the title and shared menus.

The title retains the original ADV background/plates and system font; the new
rows and lobby are an explicitly requested host extension, not original game
content. The original LOAD/NEW/OPTION plates move up to fit both host rows.

Default TCP port **57341** is in the dynamic/private range. Availability is
checked by binding; no port is guaranteed unused. Edit the port if occupied.
The address field accepts `IPv4:port` or the phrase; a phrase reversibly encodes
IPv4, port and a typo-detection checksum. It is not a password or matchmaking
server. The displayed LAN address is only a suggestion: edit it to the host's
reachable address when needed. Internet connections require router forwarding
and firewall permission; there is no automatic public-IP discovery or NAT relay.

Keyboard: OK edits the address. Text inserts at the visible cursor; Left/Right,
Home/End and Backspace move/edit it, Delete clears the field. Enter accepts the
edit and Escape restores its previous value. Ctrl/Cmd+A selects the whole field.
Typed letters are isolated from gameplay and menu bindings.

Ctrl/Cmd+C copies the normalized IP and port, Ctrl/Cmd+Shift+C copies the phrase,
and Ctrl/Cmd+V pastes a validated IP or phrase (surrounding whitespace is trimmed).
An invalid paste leaves the address unchanged. The clipboard row offers the
same COPY ADDRESS / COPY PHRASE / PASTE ADDRESS / CLEAR ADDRESS actions using
left/right and OK on a controller. Copies remain available while connecting;
editing or pasting requires cancelling the connection first.

Controller: left/right selects a visible character position, up/down changes
it; move right past the end to append. Cancel exits editing without changes or
cancels the pending connection. The last entered address stays when reopening
the lobby. Network connect, handshake and timeout run off the SDL thread.
A pending connection stages its Player-2 assets privately; it changes the
selected mode only after a successful handshake. Cancel, Back or an error
preserves the previous companion mode and state. SOLO releases the network
session and starts a new game with the NPC companion through the existing title fade.
Main-menu NEW GAME always starts without Player 2. A successful lobby handshake
starts both players directly, without returning to the main menu. The Solo/Lobby
submenu uses the title background and centered original system font.

The listener does not use address sharing: an existing listener, including one
bound only to a particular interface, must cause a port-in-use error. A recently
closed connection can also temporarily keep its port unavailable; choose another
port or retry once the OS releases it.

LOAD GAME is disabled while connected because independent save lists are not
synchronized. Start a new game with the player assigned Player 1.

Command-line alternative (default host seat is Player 1):

Host (Player 1):

```sh
build/macos-arm64/recvx-runtime --data data --netplay-host 57341
```

Guest (Player 2):

```sh
build/macos-arm64/recvx-runtime --data data --netplay-join 192.168.1.10:57341
```

Both commands imply `--player2 local`. The host waits for the guest
(`--netplay-timeout`, default 120 s), then both runtimes start the game directly through the title fade. `--player2-model 10..14` selects the
guest's character (Claire, Chris, Steve, Wesker, Claire costume 1) and must be
the same on both sides. Forward the TCP port on the host's router for sessions
across the internet.

## How it works

Deterministic lockstep. Every game tick each runtime samples its own
`InputFrame` (the configured gameplay actions, no raw keys) and sends it to the
peer; the frame applies `--netplay-delay` ticks later (default 2, about 67 ms)
on both sides. A tick runs only when both frames for it are present, so both
runtimes execute the identical simulation: the host's frame is Player 1, the
guest's frame is Player 2. No game state is sent. The connection needs the same
input delay on both sides; raise it on slow links.

The handshake compares a session description: data version, character and
Player-2 model, enemy AI/grab/attack switches, Player AI, start room, disc,
presence of a mods folder and the build platform. A different description is
refused with the reason. Every 15 ticks both peers exchange four part hashes
(player 1/room, player 2, enemies, screens/flags); a mismatch shows
`DESYNC at tick N (part, ...)` in the status line and the window title, naming
the drifting part (the session keeps running). Protocol 3 (including host-selected player assignment).

The status line at the top of the picture shows host/guest, delay, the seconds
spent waiting for the peer and the disconnect reason. When the peer leaves, or
no frame arrives for 30 s, the runtime drops the session and continues alone
with its local player.

## Rejoin after a drop

After a drop both sides keep playing locally and try to rejoin for up to an
hour: the host listens again on the same port, the guest reconnects to the same
address (status line `reconnecting`). Once the handshake succeeds, the host
captures its save image (the same format-3 snapshot as a typewriter save, with
the two-player block), sends it over the connection and both sides reload the
room from it: progress, inventories, flags, Player 2 equipment and the player's
position come from the host; enemies and effects of the room restart as after
any load, and the guest's local progress since the drop is discarded. Lockstep
tick numbering restarts with the new connection. A rejoin on the title screen
reconnects without an image. SOLO or NEW GAME ends a pending rejoin.

## Limits (host rules, open)

- Same platform on both sides: the simulation uses the C library `rand()` and
  the platform's floating-point code paths; macOS arm64 against Windows x64 is
  refused by the handshake (platform mismatch) and not supported.
- Local-only state changes are blocked during a session: snapshot hotkeys F5/F9
  and the developer debug menu. `--control`, `--headless` and `--preview` are
  refused with netplay. LOAD GAME is disabled during a session.
- Video/audio settings, the 3rd-person camera mod, rumble (host only) and
  presentation options stay local and do not affect the simulation.
- There is no NAT traversal, encryption, spectator mode or split screen; a
  rejoin reloads the room from the host's save image (see above). The original camera follows Player 1; the guest watches the same
  picture.
- Input over the network reaches the simulation `delay` ticks late for both
  players; the peer's and your own frames are applied at the same tick.

## Validation

`netplay_lockstep` (tests/native/netplay_test.cpp) exercises the transport over
loopback: handshake refusal on differing session or delay, frame exchange with
the shared delay, drift detection and peer loss.
`tools/verify_netplay.py --runtime <binary> --data data --output <directory>`
starts a host and a guest runtime under SDL's dummy drivers in RM_0080, sends
physical key events to each, and requires identical end states on both peers
with the guest moving Player 2 and the host moving Player 1. A session between
two real machines, controllers and Windows still need a check by the user.

`tools/verify_lobby.py` additionally drives the real title/lobby navigation,
SDL address text entry and connection setup, negotiates host Player 2 / guest
Player 1 and checks 900 common ticks without reported drift. Native tests cover
phrase round trips and invalid endpoints, cancellation/rebinding, assignment,
and frontend Solo/Back. UI captures are under `build/validation/lobby/`.
These are diagnostic macOS arm64 development-binary checks, not an installed
app delivery, Windows runtime check, hardware-controller check or internet test.

`tools/verify_lobby_sessions.py` drives pending-connection cancellation and a
real occupied loopback port, then starts the native game and requires the NPC
to remain selected/present in both cases. The successful lobby test starts both
peers in NPC mode and verifies their transition to local-human Player 2 only
after the handshake. Frontend tests isolate SDL's clipboard with the dummy video
driver and cover keyboard/GUI shortcuts, controller clipboard actions, invalid
paste rollback, cursor insertion/backspace and edit cancellation. Evidence:
`build/validation/lobby/sessions-final/`, `connect-expanded/`, `editor/`.

Direct start and main-menu isolation: `tools/verify_multiplayer_start.py`,
`tools/verify_lobby.py`, `tools/verify_lobby_sessions.py`; diagnostic captures
and states in `build/validation/multiplayer-direct/`.
