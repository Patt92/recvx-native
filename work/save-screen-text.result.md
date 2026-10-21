# Speicher-/Ladedialog: generische Dateitexte

## Änderung

Die Originalmeldungen des Speicherbildschirms nennen das PS2-Medium
(`MEMORY CARD ` / `slot 1 `, `Now saving to memory card (PS2)`,
`Loading from memory card (PS2)`). Der Host speichert bereits in eine Datei im
Benutzerdatenverzeichnis (AGENTS.md "Spielstände"). Angepasst werden nur diese
Texte; Schrift, Positionen, Meldungsnummern und Ablauf bleiben original.

`native/typewriter.cpp`, `save_screen_host_message`:

- 463/464 (Kopf): ersetzt durch `Save games`, mittig in der Originalzeile
  (`native/ui.cpp`, gleiche Schrift wie `DisplayGameModePlate`-Hostzeile).
- 460 (Laden): `Now loading from the save file.` plus
  `Do not close the game or turn off the computer.`
- 484 (Speichern): `Now saving to the save file.` plus derselbe Hinweis.
- 461/462/482/483/485/486 (`File corrupted.`, `Overwrite data?`, ...) bleiben
  unverändert; sie nennen kein Medium.

Die Ersetzung wird nur im Speicher-/Ladebildschirm angewandt
(`GameUi::draw_typewriter`). Andere Systemmeldungen mit gleicher Nummer
existieren nicht; `draw_pause` nutzt weiter die Originalmeldungen.

## Prüfungen

- `CTest typewriter_slot_list_draw`: prüft die Ersatztexte, dass 464 leer ist
  und generische Originalmeldungen nicht ersetzt werden.
- `CTest typewriter_save_slots`, `main_menu_settings_video_skip`: grün.
- Speicherdialog über den echten RM_0010-Pfad (`--in-game`, Farbband,
  Schreibmaschine): Bild `build/validation/save-screen-text/save.png` zeigt
  `Save games` oben und den Dateitext unten.
- Ladedialog über `Start.command` und den Titel: Bild
  `build/validation/title-load-navigation/runtime/sdl-load-open.png` zeigt
  denselben Kopf; `tools/verify_title_load.py` grün.
- `python3 tools/check_no_original_core.py` und `port_check.py --domain SaveScreen` PASS.
- Dev-App über `./build.sh` neu gebaut und über `Start.command` geprüft.

Kein Commit, kein Push, keine Paketersetzung. Benutzerdateien und Spielstände
wurden nicht angefasst; die Prüfläufe verwendeten eigene Verzeichnisse.
