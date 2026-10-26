# Starting and Controls

RECVX Native is a development build. It requires locally exported
original data and is not yet a complete, playable port.

## Starting

In the project folder:

- **macOS (Apple Silicon):** open `Start.command`.
- **Windows x64:** open `Start.cmd`.
- **AI play (a language model plays, you watch):** `Start-AI-Play.command` (macOS) or
  `Start-AI-Play.cmd` (Windows). Endpoint, model and API key go into `.env` in the
  project folder (untracked); see README, section "Let your AI play the game", details in `docs/AI_PLAY.md`.

The package launchers use the runtime data shipped with each package. No ISO
is inserted or read during play. A source clone without the local `data/`
folder cannot be started.

On macOS, `./build.sh` produces the development app at
`build/start/macos-arm64/RECVX Native.app`. `Start.command` prefers this
app; it uses the current data, mods and settings in the project folder.
**The dev app loads `data/` and `mods/` loosely from the project folder**: changed
data or mods take effect on the next start without a new build. DDS packs belong
in `mods/hd-source/`; `./build.sh` converts them to `mods/textures/hd/`.
The app is built even when no pack is present. A failed build
does not update the launcher.

Standalone packages with copied runtime data are additionally produced by
`./build.sh --platform macos --package --output <new-package-folder>`
(with `--compressed` from `data_compressed/`; the project's mods are baked into
the package data, no `mods` folder is shipped).
[docs/BUILD.md](docs/BUILD.md) describes the complete procedure.
The default package outputs are under `build/packages/`; existing
packages are not overwritten.
iOS/iPadOS builds (device and simulator, with Xcode) are produced by
`./build.sh --platform ios` or `--platform ios-simulator` respectively;
`--platform ios --package` writes an unsigned IPA for
sideloading (AltStore/SideStore or zsign) when no profile is given. Signing, data placement and
installation are covered in the iOS section of the build documentation.
Details on building, data verification and platforms are in
[docs/NATIVE_PORT.md](docs/NATIVE_PORT.md).

## Default controls

| Action | Keyboard | Controller |
|---|---|---|
| Move | W/S or I/K | Left stick or D-pad |
| Turn | A/D or J/L | Left stick or D-pad |
| Run | Shift | B |
| Confirm / Interact | Enter | A |
| Back | Escape | B |
| Pause (on/off) | Escape or P | Back/Select |
| Inventory | Tab | Y |
| Skip cutscene | Space | Start |
| Debug menu | F2 | – |

Keyboard bindings, controller bindings, stick dead zone and menu keys can be
changed in the main menu under **Settings**. The complete description
is in [docs/CONTROLS.md](docs/CONTROLS.md).

## Diagnostics

The debug menu is a development tool, not normal game progression.
It can inspect rooms, positions and states directly; such jumps are
no proof of an authentic playthrough. Configuration and functions
are documented in [docs/DEBUG_MENU.md](docs/DEBUG_MENU.md).

[docs/NATIVE_GAMEPLAY.md](docs/NATIVE_GAMEPLAY.md) describes the current port
state and known gaps. Validation results and the limits of what each one
proves are in [docs/VALIDATION.md](docs/VALIDATION.md).
