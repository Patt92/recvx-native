# Anwendungsicon — 2026-10-14

## Änderung

`data/app-resources/app-icon.jpg` (1024×1024 RGB) bleibt unverändert.
`tools/build_app_icons.py` konvertiert optional mit Pillow zu den bereitliegenden
`native/resources/app-icon.icns` und `app-icon.ico`. Normale Builds benötigen
Pillow nicht. Das ICO enthält 16/24/32/48/64/128/256 px, ICNS bis 1024 px mit
Retina-Repräsentationen. Keine Ausschnitte oder erfundene Transparenz.

Beide macOS-Bundle-Erzeuger kopieren ICNS nach `Contents/Resources` und setzen
`CFBundleIconFile` vor der Signierung. Bestehende Staging-/Backupmechanismen
bleiben erhalten. Windows bindet ICO über RC am Ziel `recvx-runtime` ein.
SDL2 2.32.10 lädt die erste RT_GROUP_ICON auch für das Fenster
(`SDL_windowsevents.c:2082–2083`); kein Runtime-Eingriff erforderlich.

Zig-RC erzeugt ausdrücklich x64-COFF, nicht nur eine umbenannte RES-Datei.
Veraltete CMake-RC-Metadaten mit `windres` werden beim Toolchainwechsel gezielt
zur Neuerkennung entfernt. Der abschließende Windows-Binäraudit verwendet
nun denselben projektlokalen Zig-Cache wie der Crossbuild.

## Nachweis

- Konvertierung zweimal ausgeführt: beide Ergebnisse byteidentisch (Pillow 12.3.0).
- `python3 tests/build_native_test.py`: 6 Tests PASS, einschließlich Icon/Plist
  beim Signieren und fehlendem Icon ohne Veröffentlichung über die alte App.
- `python3 tests/app_icons_test.py`: 2 Container-/Bildgrößentests PASS.
- Beide Python-Testgruppen in CTest registriert und gezielt erfolgreich ausgeführt.
- `./build.sh`: macOS-Build PASS, Dev-App unter
  `build/start/macos-arm64/RECVX Native.app` aktualisiert. ICNS bytegleich mit
  Ressource, Plist korrekt, Runtime arm64, `codesign --verify --deep --strict` PASS.
- `./Start.command --help`: bevorzugten Dev-App-Starter ausgeführt, Version 0.5.63.
- Separater macOS-Paketpfad mit temporären minimalen Testdaten geprüft: Icon,
  Plist, Datenunterverzeichnis und strikte Bundle-Signatur PASS. Kein Gameplaynachweis.
- `./build.sh --platform windows`: x64-Crossbuild einschließlich abschließendem
  Policy-/Binäraudit PASS. Die ersten Versuche scheiterten am alten windres-Metadatum
  bzw. dem außerhalb des Sandbox schreibgeschützten globalen Zig-Cache;
  korrigiert ohne Prüferänderung oder Rechteumgehung.
- `python3 tests/app_icons_test.py --exe build/windows-x64/recvx-runtime.exe`:
  eine RT_GROUP_ICON mit ID 1 und alle sieben RT_ICON-Bilder bytegleich zum ICO.
- `git diff --check`: PASS. Fremde Änderungen erhalten, keine Spiellogik geändert.

## Auslieferungsstand und Grenzen

Die bevorzugte macOS-Dev-App enthält das Icon bereits. Windows-EXE liegt in
`build/windows-x64/recvx-runtime.exe`; kein Windows-Paket ersetzt. Bestehende
Release-Pakete unverändert. Die neuen Paket-/Buildpfade übernehmen das Icon
bei zukünftiger Erzeugung. Keine tatsächliche Finder-/Dock-Ansicht oder
Windows-Explorer-/Taskleistenansicht visuell geprüft, keine Windows-Ausführung.
Kein Commit/Push/PR.
