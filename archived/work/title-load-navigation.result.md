# Titelmenü: LOAD GAME wieder erreichbar

## Ursache und Korrektur

`native/frontend.cpp:Frontend::check_button` benutzte beim Hochgehen stets
Cursor 1 als Untergrenze. Die Startauswahl berücksichtigte `has_saves` dagegen
bereits korrekt. Daher war LOAD GAME mit Spielständen anfangs ausgewählt, nach
Verlassen aber unerreichbar.

Originalbeleg: `src/ps2/veronica/prog/adv.c:1864–1898` überspringt Cursor 0 nur
bei Flag 0. Der bestehende Hostdienst in `native/runtime.cpp` bildet vorhandene
bzw. beschädigte Schreibmaschinen-Slots auf dieses Flag ab. Die Untergrenze
ist jetzt abhängig davon 0 oder 1; keine neue Speicherwelt/Dateisemantik.

`DisplayGameModePlate` (`adv.c:1654`) zeichnet alle nicht ausgewählten Einträge
mit `0x7F7F7F`. Ohne Saves bleibt LOAD GAME deshalb grau und nicht anwählbar.
Es wurde keine zusätzliche, vom Original abweichende Farbstufe eingeführt.

## Prüfungen

- Neuer Regressionstest gegen unveränderte Engine gebaut: rot mit
  `cannot return to LOAD GAME with saves` (`build/validation/title-load-navigation/before.log`).
- Nach Korrektur: vier gezielte CTests grün: `main_menu_settings_video_skip`,
  `snapshot_savefile_roundtrip`, `typewriter_save_slots`, `typewriter_slot_list_draw`.
- Frontend-Test prüft Startauswahl, runter/hoch, obere Grenze, verlorene/neu
  verfügbare Saves, Cancel/Start und Returncode 3. Tastatur- und Controller-
  InputFrames werden geprüft; kein Nachweis physischer Controllerhardware.
- `tools/verify_title_load.py` prüft die tatsächlich von `Start.command`
  gewählte Dev-App ohne In-Game-Start. Eigene Benutzerpfade, lesbare
  Snapshot/SAVEFILE-Fixture als Schreibmaschinen-Slot, keine echten Saves verändert.
  Echte SDL-Tasten durch InputManager: runter/hoch und Bestätigen öffnen den
  Ladebildschirm erfolgreich (SDL-Dummytreiber; `sdl-load-open.json/.bmp`).
  Prüfungen umfassen Cursorzustände und Bildpixel: LOAD ohne Saves entspricht
  exakt der unselektierten LOAD-Platte mit Saves und ist dunkler als selektiertes LOAD.
- Reale Titelbilder unter `build/validation/title-load-navigation/runtime/`;
  Vergleich `comparison.png`: links ohne Saves (NEW GAME selektiert), rechts
  nach runter/hoch mit Saves (LOAD GAME selektiert).
- Build über `./build.sh --test-regex ...`; Dev-App mit Sicherung der Vorversion,
  arm64-only und `codesign --verify --deep --strict` geprüft. Release-Paket
  unverändert. Policy-Prüfer und `tools/port_check.py` ausgeführt.

## Prüfgrenze des Headless-Ladeübergangs

Ein zusätzlicher Headless-Aufruf von LOAD GAME bricht beim Übergangsbild mit
`actor texture outside scene table: index 1, base 0, count 0` ab. Derselbe Lauf
gegen die unmittelbar vor dem Build gesicherte Dev-App
(`build/start/macos-arm64/.previous-fy2qmc0l/RECVX Native.app`) bricht identisch ab;
Belege in `build/validation/title-load-navigation/load-countercheck/`.
Der Befund ist keine durch diesen Navigationsfix verursachte Regression.
Der gezielte Titeltest behandelt ihn nicht als bestandenen Ladebeleg.

Fremde zwischenzeitliche Änderung an `native/runtime.cpp` (Tür-Zielauflösung)
nicht verändert. Keine Commits, Pushes oder PRs.
