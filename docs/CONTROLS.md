# Keyboard and controller controls

The runtime opens SDL GameControllers already connected at startup and handles
connection/removal while running. Windows XInput detection is enabled through
SDL2; macOS uses SDL's native controller drivers with the same logical button
names. Keyboard and mapped gamepads can be used together without changing modes.

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
| Menu/item examination | Arrow keys or WASD | Left stick or D-pad |
| Video / cinematic skip | Space; Enter also skips active videos and permitted scenes | Start/Options; A also skips active videos and permitted scenes |
| Quit | Escape outside menus/dialogue, or F10 | Unbound by default |
| Wireframe diagnostic | F2 | Unbound by default |

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
rejected. Sharing a physical key across contexts is supported: Escape can be
cancel and quit; while a menu/dialogue is open it cancels instead of quitting.

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

Equip Combat Knife or Handgun in the inventory. Hold Aim; left/right turns the
weapon, up/down selects the original raised/lowered aim clips. Attack repeats
once per animation while held. Knife attacks use no ammunition. The handgun
uses its loaded magazine, reloads from pistol ammunition via the original menu
combination rule when empty, and produces a dry shot if none is available.
Confirm interacts/picks up when Aim is released. It stays a menu/dialogue/door
confirmation while those contexts own input. Aim/Attack are separately rebindable
in Settings; legacy controls.ini files gain these defaults without changing old
bindings. Release Aim and Attack after leaving a blocked context before attacking.
