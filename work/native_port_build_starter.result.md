# macOS build.sh → Entwicklungs-App → Start.command

## Ursache

`sync_macos_start_package` setzte ein bestehendes Paket mit Runtime voraus.
`build/packages/macos-arm64-v0.5/RECVX Native.app/Contents/MacOS` war leer.
Der normale Build übersprang die Erstellung ausdrücklich; der Starter fiel
auf das lose Binary zurück. Keine Wiederherstellung aus Quarantäne notwendig
oder zulässig.

## Umsetzung

Der normale macOS-Build erstellt jetzt aus seinem frisch auditierten Binary
und der gebauten arm64-SDL2 eine neue Entwicklungs-App unter
`build/start/macos-arm64/RECVX Native.app`. Launcher und Info.plist werden neu
bestimmt, keine Alt-App eingelesen. Der Launcher nutzt die aktuellen losen
Projektdaten, Mods sowie controls.ini/debug.ini. Diese App ist kein autonomes
Weitergabepaket; `--package` bleibt der getrennte Weg mit kopierten Daten.

Die App wird im Staging aufgebaut, nach Linkpfadkorrektur erneut auditiert,
signiert und geprüft; erst danach veröffentlicht. Bei Erneuerung bleibt die
vorherige Entwicklungs-App unter `.previous-*` im gleichen Buildordner
wiederherstellbar. Alte Pakete und Benutzereinstellungen bleiben unverändert.
`Start.command` bevorzugt die neue App und prüft deren Binary vor Ausführung.

## Nachweise

- `python3 tests/build_native_test.py`: 5/5. Fehlendes Paket, deaktivierte
  Alt-App, Aktualisierung/Erhaltung, Auditfehler ohne Veröffentlichung,
  fehlende SDL2. macOS-Werkzeuge im Komponententest gemockt; der echte
  Nachweis folgt unten.
- `./build.sh --platform macos --jobs 4`: exit 0, App tatsächlich erzeugt,
  Schutzprüfer für Quellen, Build und Binary PASS.
- `codesign --verify --deep --strict` am veröffentlichten Bundle: exit 0.
- `otool -L`: SDL2 aus `@executable_path/../Frameworks/`.
- `./start.command --headless --in-game --room RM_0050.RDX --frames 2
  --state /private/tmp/recvx-starter-check.nHE63x/state.json`: exit 0,
  2 gerenderte Frames, 249910 sichtbare Pixel, 0 blockierte Befehle.
- Shell-Syntax und Diff-Whitespace geprüft.

Headless-Start ist kein Finder-Doppelklick-/Controller-/Gameplay-Nachweis.
Keine Spielelogik, Windows-Dateien oder Claudes Tür-/Gameover-Dateien geändert.
Kein Commit/Push.
