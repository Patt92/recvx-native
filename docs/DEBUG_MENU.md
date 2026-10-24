# Debug menu mod

F2 opens and closes the debug menu. The debug hotkey takes precedence over game
controls and the former F2 wireframe binding. New default controls put
wireframe on F3; existing user bindings are preserved.

## Configuration and controls

`debug.ini` is independent of `controls.ini`:

```ini
[debug]
hotkey = F2
```

SDL key names such as `F6` are supported. Restart after changing the file. The
runtime creates a missing file with F2 and never overwrites an existing one.
Invalid or duplicate settings are reported as errors.

- macOS release package: `~/Library/Application Support/RECVX/debug.ini`.
- Windows release package: `%APPDATA%\RECVX\debug.ini`.
- Development build and project starter: `debug.ini` in the project directory.
- `--debug-config <path>` explicitly selects another configuration file.

Navigate with the configured menu actions: Up/Down selects a function,
Left/Right changes its value, and Confirm applies it. Keyboard and controller
use the same path. PageUp/PageDown or LB/RB move by 20 values in long lists.
Escape, controller B, or the debug hotkey closes the menu. Holding a button
does not repeatedly open the menu or apply an action.

While the menu is open, the event VM, world, animation, combat, video, and audio
are paused. The scene remains visible. On closing, held gameplay actions are
suppressed until released, so Confirm cannot trigger a pickup or attack behind
the menu.

## Functions

| Action | Behavior |
|---|---|
| Character | Load original SYSTEM bodies 0010–0014, including Claire and Chris, and reinitialize the current room. The existing inventory mapping remains: 0010/0013/0014 use Claire, 0011 uses Chris, and 0012 uses player 2. This does not implement new character campaigns. |
| Equip / weapon | Equip an existing item; give a missing weapon with its original quantity and equip it. “None” unequips. The mod does not add combat code for weapons whose combat behavior is still missing. |
| Room | Select from existing RDX room names. Target resources are decoded and checked before spawn 0 is loaded. Inventory and global flags remain; active room-local tasks restart. |
| Spawn | Move the player to an original spawn in the current room. |
| Item quantity / give item | Select a quantity from 1–999 and an original item from 1–145. Original inventory capacity and stacking rules apply; a failed pickup leaves items unchanged. |
| Health | Set HP within the current maximum. |
| Bag | Select 8 or 10 slots, including the original bag flag. Removal is rejected if more than eight slots are occupied. |
| Enemy grab | Toggle zombie grabs and bites (off by default; requires enemy AI); `--enemy-grab on/off`. |
| Enemy AI | Toggle zombie AI (when off, enemies stand still but hit reactions continue). `--enemy-ai on/off` sets the initial value. |
| Enemy attacks | Host convenience switch, not an original mechanic (default on = original behaviour). Off: enemies and enemy effects deal no HP loss, poison (`stflg` 0x80000/0x200000), grab/throw takeover (player `mode0` 4/5/6) or kill to the player; enemy AI, motions and attack animations keep running. Central gate in `RoomWorld::enemy_player_set`, `s.player_damage`, `t.player_poison`, the bomb player half (`bhCheckBombAtari`) and the EN01 facade grab state; modules with an attack-start gate (`debug_attack_disabled`: en01, en04, en07, en09, en10, en21, en26) skip the attack entirely. `--enemy-attacks on/off`, `debug.ini enemy_attacks = on/off`. Not covered: the room gas kill bhEff127 (room event, not an enemy). |
| Wireframe | Toggle wireframe rendering. |
| Textur-Mods (HD) | Host comparison switch: off ignores `mods/textures/<asset>` and `mods/textures/hd/<md5>.tga` (PS2 textures from `data/`), on uses them again. Reloads the current room at the same position like a snapshot (room script restarts); needs free player control, otherwise it applies from the next room. Inventory item models keep their cached textures until restart. The room line in the terminal counts HD, mod-file and `data/` textures. |
| Continue | Close the debug menu. |

Errors appear in the debug menu. A rejected room is never marked as loaded.
Room jumps are diagnostics; they do not supply missing event or world bindings
in the incomplete port. Changes affect the current session.

## Extending the menu

`native/debug_menu.*` implements configuration, hotkey handling, navigation,
and the overlay. `DebugMenu::add(DebugAction)` registers functions without UI
changes:

```cpp
debug.add({
    "example", "New function", "Confirm: run.",
    [] { return std::vector<recvx::DebugChoice>{{0, "Off"}, {1, "On"}}; },
    [&] { return unsigned(enabled); },
    [&](unsigned value) { enabled = bool(value); return std::string("Set"); }
});
```

Each action has a stable ID, dynamic choices, a current value, and a validating
execution callback. Gameplay bindings are registered in `native/runtime.cpp`.
The callback is the first place where game state changes. Check resources first;
update global references and actor assets cleanly when changing rooms or
characters. Inventory changes are checked on a copy before the inventory and
status menu are updated together. Pending item questions are then discarded so
slot shifts cannot trigger old combinations or inspections.

## Verification

```sh
build/macos-arm64/recvx-debug-tests data build/validation/debug-menu.bmp
python3 tools/verify_debug.py --runtime build/macos-arm64/recvx-runtime --data data
```

The CTest test covers the default and remapped hotkeys, repeats, software path,
registry extensions, errors, and input suppression. Runtime verification
pauses real original rooms and checks all five bodies, equipment, items,
HP/bag/wireframe, and room/spawn changes. Replay commands `debug_key` and
`debug_select` use the same hotkey, selection, and application routines as the
UI. The replay command `sdl_key down|up <SDL-scancode-name>` (SDL mode only,
for example with `SDL_VIDEODRIVER=dummy`) injects a real SDL key event into the
event loop and checks the physical input path (InputManager, controls.ini)
without a window.
