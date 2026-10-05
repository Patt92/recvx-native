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
| Run | Either Shift | Hold B, right shoulder or right trigger |
| Inventory | Tab | Y |
| Confirm/interact | Enter | A |
| Cancel menu/dialogue | Escape | B |
| Menu/item examination | Arrow keys or WASD | Left stick or D-pad |
| Cinematic skip | Space | Start |
| Quit | Escape outside menus/dialogue, or F10 | Unbound by default |
| Wireframe diagnostic | F2 | Unbound by default |

The stick controls the current original-style tank movement; this does not add
camera-relative 360-degree movement. Gameplay movement speed still uses the
original animation curves. Menu/cinematic/dialogue restrictions apply equally
to keyboard and controller. Confirm, inventory and skip trigger once per press;
holding a direction repeats menu navigation after a configurable delay.

## Change bindings

Edit `controls.ini` and restart. Packaged builds keep it next to Start.cmd or
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
run = button:rightshoulder, axis:righttrigger+

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
This implementation does not yet offer an in-game rebinding screen or vibration.
The preview inspector retains its separate diagnostic/free-camera keyboard controls.

SDL references: [opening controllers](https://wiki.libsdl.org/SDL2/SDL_GameControllerOpen),
[XInput detection](https://wiki.libsdl.org/SDL2/SDL_HINT_XINPUT_ENABLED),
[device-event IDs](https://wiki.libsdl.org/SDL2/SDL_ControllerDeviceEvent),
[virtual joystick testing](https://wiki.libsdl.org/SDL2/SDL_JoystickAttachVirtual).
