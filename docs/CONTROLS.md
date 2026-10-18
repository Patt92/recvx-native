# Keyboard and controller controls

The runtime opens SDL GameControllers already connected at startup and handles
connection/removal while running. Windows XInput detection is enabled through
SDL2; macOS uses SDL's native controller drivers with the same logical button
names. Keyboard and mapped gamepads can be used together without changing modes.

On macOS, Cmd+W (close window) does not end a running game: the runtime drops
the close request while Cmd and W are held. Quit stays Cmd+Q, the window's
close button or the configured quit key.

## Defaults

| Action | Keyboard | Xbox-layout controller |
|---|---|---|
| Walk forward/back | W/S, I/K or Up/Down | Left stick up/down or D-pad |
| Turn left/right | A/D, J/L or Left/Right | Left stick left/right or D-pad |
| Run | Either Shift | Hold B or right trigger |
| Aim | Hold R | Hold RB |
| Attack while aiming | Enter or Left Ctrl | A |
| Inventory | Tab | Y |
| Confirm/interact | Enter | A |
| Cancel menu/dialogue | Escape | B |
| Pause | P, Escape | Back/Select; press again or Start/Options to resume |
| Menu/item examination | Arrow keys or WASD | Left stick or D-pad |
| Cinematic skip (PS2 SELECT, `select`) | P | Back/Select |
| Video skip (`skip`) | Space; Enter and Escape also skip active videos | Start/Options; A and B also skip active videos |
| Quit | F10 | Unbound by default |
| Wireframe diagnostic | F2 | Unbound by default |

Only SELECT skips a running cinematic, as in the original (`system.c:545`
`bhSysCallGame` reads `pad_ps & 0x10000`; `pad.c pad_tab_a[16]` maps it to the
SELECT button). Confirm, Start and Space never request the skip; Confirm only
acknowledges dialogue text. The host maps SELECT to P / Back by default (the
same keys as the host pause, which is blocked while a cinematic runs); a
`controls.ini` without a `select` line gets this default. There is no on-screen
skip hint; the original shows none.

Escape never terminates the runtime. Inside a menu or dialogue it cancels;
outside one, with free control, it opens the pause overlay and closes it again.
`system.c:668` `bhSysCallEvent` enters the overlay (`ss_flg 0x80000000`,
`ssv_md0 10`) and `system.c:691` leaves it on the same newly pressed button, so
the pause key toggles; the gate is the original `st_flg 0x1C040208` mask, which
keeps the pause out of cinematics, the map task, terminals, the status screen,
doors, movies and scripted scenes. The same line also requires
`(ts_flg & 0x3DE00) == 0x3DE00`, whose bit 15 belongs to the typewriter task
(`main.c:195`, task 15 `bhSysCallTypewriter`): while the save or load screen is
up, Escape is its Cancel and leaves the slot list instead of pausing. A key
listed under `cancel` is therefore
ignored under `quit`: an older `controls.ini` that still says
`quit = Escape, F10` keeps F10 and loses Escape.

The stick controls the current original-style tank movement; this does not add
camera-relative 360-degree movement. Gameplay movement speed still uses the
original animation curves. Menu/cinematic/dialogue restrictions apply equally
to keyboard and controller. Confirm, inventory and skip trigger once per press;
holding a direction repeats menu navigation after a configurable delay.

In item examination, Left/Right rotates horizontally and Up/Down vertically;
Confirm inspects the facing surface. Confirm advances original text pages and
answers Yes/No; directions choose the answer. Cancel dismisses the question
without changing the item. For locked cases, carry a lockpick and choose Examine
rather than Combine. Combine selects a second item for the original recipe.

## Change bindings

Start in the main menu and select **Einstellungen**. Left/Right switches between
keyboard and controller pages, except on deadzone/menu-threshold rows where it
adjusts the value. Select an action, press configured Confirm, then press a new
key/button or move a centered stick. Escape cancels capture. Each successful edit
is saved atomically to controls.ini and applied immediately. Confirm/Cancel
conflicts swap the previous binding so the menu remains operable. Restoring
defaults restores both device pages. The connected-controller count is shown.

Alternatively, edit `controls.ini` and restart. Packaged builds keep it next to Start.cmd or
RECVX Native.app, outside the signed application. Do not edit the internal app
Resources directory. Development launches look next to the selected data folder.
An explicit `--controls <path>` selects a different file. If the default file is
absent, built-in defaults are used; an explicitly selected missing/invalid file
reports an error. The complete default template is included in source/packages.

```ini
[keyboard]
confirm = E
run = Left Shift, Right Shift

[gamepad]
confirm = button:x
forward = axis:lefty-, button:dpup
run = button:b, axis:righttrigger+

[settings]
deadzone = 0.25
menu_threshold = 0.55
repeat_delay_ticks = 18
repeat_interval_ticks = 5
```

These are edits within the full template, not a complete config. Keep each
existing action in both device sections. Multiple alternatives use commas;
`none` disables an action for that device. Axis suffix `+`/`-` selects a direction
and permits inversion. Supported axis names: leftx, lefty, rightx, righty,
lefttrigger, righttrigger. Common buttons: a, b, x, y, start, back, leftshoulder,
rightshoulder, leftstick, rightstick, dpup, dpdown, dpleft, dpright. Names are SDL
logical Xbox-layout names, including for PlayStation hardware.

Keyboard names are SDL scancode names (physical key positions), such as Return,
Space, Escape, Tab, E, Left Shift and Right Shift. Duplicate keys/settings,
unknown names, empty lists, nonfinite thresholds and invalid repeat counts are
rejected. Sharing a physical key across contexts is supported: Escape cancels
inside a menu or dialogue and is the host pause key outside one. A key listed
under `cancel` never terminates the runtime, so `quit` ignores it.

Axis thresholds are fractions from 0.05 to 0.95. The menu threshold must be at
least the movement deadzone. Repetition values are ticks at the 60 Hz simulation
rate. Defaults correspond to about 300 ms initial delay and 83 ms between repeats.

```sh
# Create a new complete template; existing files are preserved
recvx-runtime --write-controls my-controls.ini
# Inspect actual attached devices without loading game assets
recvx-runtime --list-controllers
# Use an external SDL mapping database for otherwise unrecognized devices
recvx-runtime --controller-mappings gamecontrollerdb.txt --controls my-controls.ini
```

A custom SDL mapping database describes a device's raw axes/buttons. It is separate
from controls.ini, which maps normalized controller inputs to game actions. No
external mapping download is required for SDL's built-in supported devices.
Focus loss clears keyboard/action latches and suspends gamepad input; reconnecting
or regaining focus does not synthesize a confirm press. Disconnect clears that
pad's held movement while retaining keyboard/other-controller inputs.

## Validation and limits

`recvx-input-tests` uses SDL's actual virtual GameController driver to test axes,
buttons, deadzone, trigger, short presses, repeats, keyboard/gamepad remapping,
axis inversion, hot-plug/removal and focus handling. With the local data path it
also runs the original cell skip, dialogue acknowledgement, inventory equipment
and Rodrigo/control-return flow through the same input router as gameplay:

```sh
recvx-input-tests data
```

Virtual-controller and Wine tests validate the native input path. They do not
establish that a particular physical USB/Bluetooth/XInput controller has been
tested on Windows hardware. Use --list-controllers to inspect available devices.
The main-menu settings screen supports rebinding; vibration remains pending.
The preview inspector retains its separate diagnostic/free-camera keyboard controls.

SDL references: [opening controllers](https://wiki.libsdl.org/SDL2/SDL_GameControllerOpen),
[XInput detection](https://wiki.libsdl.org/SDL2/SDL_HINT_XINPUT_ENABLED),
[device-event IDs](https://wiki.libsdl.org/SDL2/SDL_ControllerDeviceEvent),
[virtual joystick testing](https://wiki.libsdl.org/SDL2/SDL_JoystickAttachVirtual).

During a door transition, a fresh press of configured Confirm (default Enter /
A) skips directly to the destination spawn. Held Confirm does not immediately
skip a newly entered door; navigation and inventory inputs are ignored there.

Active videos accept Confirm, Skip or Cancel without requiring the event cinematic
skip flag. Movie playback and its PCM audio are stopped; the original script sees
movie completion. This avoids carrying a cinematic-skip request into the later
Rodrigo scene. Video hints display configured keyboard and controller bindings.
A single inventory press in the first second after dismissing a dialogue is
buffered across the brief cinematic handover. Inventory still opens only once
control is released; videos and door transitions discard this buffer.

Completing the initial lighter hint automatically opens the inventory before locomotion. No Inventory button press is required. Holding left/right while examining an item rotates it continuously at 60 Hz. Dialogue choices use navigation and Confirm/Cancel; confirming a completed voice line stops that line.

## Combat and pickups

Equip the weapon in the inventory. Hold **R / R1 (RB)** to aim;
**Enter or Left Ctrl / controller A** fires. **Q / L1 (LB)** changes the target.
All three actions are configurable in Settings. Existing INIs receive the missing
`next_target` default in memory; custom bindings and explicit `none` are preserved.

Aim acquires the nearest eligible visible enemy. Single weapons cycle through
visible targets by distance; the original dual-weapon routine toggles between
split and concentrated fire. Dual Submachine Guns can point at two
different enemies, with separate animated arm angles and a delayed left shot.
Left/right turns manually; up/down selects raised/lowered aim where the weapon
allows it. Knife attacks use no ammunition. Handgun, Steve's M93R, Custom Handgun,
Magnum, M-100P, dual weapons, Assault Rifle and Sniper Rifle use their original
weapon resources. The M93R fires single shots; Custom Handgun fires up to three
shots per trigger and requires release before the next burst. Loaded ammo,
infinite ammo, available reserve reload and dry shots share the inventory state.

Confirm interacts/picks up when Aim is released. It stays a menu/dialogue/door
confirmation while those contexts own input. Release Aim and Attack after leaving
a blocked context before attacking. Combat enemies are currently the bound en01
zombies; other enemy combat and projectile/special weapons remain incomplete.

## 3rd-person camera mod

Optional host mod, off by default. Toggle it in the main menu under the
settings screen (`3rd-Person-Kamera (Mod)`, left/right or OK). The choice is
saved to `third_person.ini` beside `controls.ini`; `--third-person` enables it
for one run without saving. The over-the-shoulder camera only replaces the
fixed room camera while the player has free control; event cameras, FMVs,
cinematics and door transitions keep the original view. Movement keeps the
original tank controls.
