# Inventarmenü: Cursor beim Öffnen/Schließen und Textzeitpunkt (2026-10-21)

Nicht committet. Ausgelöst durch die Beobachtung: „beim Schließen wird immer das
Feuerzeug ausgewählt“ und „die Textbox zeigt zu früh Text“.

## Befund am Original

- **Cursor:** `CursorInit` (sub1.c:8972-9004) setzt beim Betreten des
  Statusbildschirms `listcsr_0 = listcsr_2 = 2`, also den ersten Taschenplatz.
  `StatusCancel` (:4408) ändert weder `listcsr_0` noch `pip[0]`/`wpnr_no`.
  Nativ setzte das Öffnen `selected = 1` (Sonderslot = Feuerzeug). Das Feuerzeug
  war also nicht ausgerüstet, aber jedes Mal der Cursor darauf; im Schließbild
  und beim nächsten Öffnen sah das wie „Feuerzeug gewählt“ aus. Die Ausrüstung
  selbst blieb unverändert (Test belegt).
- **Text:** `MainCommand` (sub1.c:4295-4306) startet `bhSetMessage(1, 180-maincsr)`
  erst ohne laufende Meldung, außerhalb testmode 2 und **ohne statusflg 0x2000**,
  also nach dem Ende von SpriteH. Der Text läuft über `bhControlMessage`.
  `MainCommand`/`ItemCommand` löschen statusflg 0x100000 (:4184/:4627); damit
  verschwindet der Itemname (SpriteOnOff :8383-8411).
  Nativ zeichnete `GameUi::draw` die Beschreibung bzw. den Itemnamen sofort und
  vollständig, auch während der Einfahrt.

## Änderung

- `native/ui.cpp`: Öffnen und erster Statuspass setzen `selected = 2` und den
  Cursorfarbpuls zurück (CursorInit); Schließen ändert den Cursor nicht.
- Die MainCommand-Beschreibung läuft jetzt über den MessageDialog des Menüs
  (`navigation_message_`), startet erst nach SpriteH und wird bei
  Cursorwechsel neu gestartet. Der Zeichenpass malt keine sofortige
  Beschreibung und keinen Itemnamen mehr während MainCommand, Befehlsfenster,
  Untersuchen-Einfahrt und auf dem Aufnahmebildschirm.
- Testaufbauten und Replays, die stillschweigend „Cursor startet auf dem
  Feuerzeug“ annahmen, gehen jetzt den originalen Schritt nach oben
  (frontend_test, game_test, event_lifecycle_test, lighter/door/knife-Replays,
  verify_lighter/verify_fog/verify_frontend).

## Prüfungen

- 14 gezielte CTests grün (status_screen_hold mit neuem `menu_lifecycle`,
  status_condition_pulse, ui_quantity, inventory_item_sort,
  inventory_combine_inspect, item_box_pad_frames, pickup_status_reset,
  pickup_gap_pad, native-gameplay-bindings, crouch_pickup,
  inventory_events_assets, original_file_reader, main_menu_settings_video_skip,
  event_lifecycle_control_release).
- `tools/verify_lighter.py` am installierten App-Binary grün.
- Bilder: `build/validation/inventory-menu-lifecycle/images2/` (Eintritt ohne
  Text, Beschreibung erst nach der Einfahrt).
- Dev-App über `./build.sh`, arm64, signiert.

## Grenzen

- Die genaue Reihenfolge von Task 8 (`bhControlMessage`) gegenüber Task 9
  (`StatusMain`) ist nicht gesondert belegt; der Text erscheint im Nachrichtenpass
  nach dem Ende der Einfahrt.
- `tools/verify_frontend.py` scheitert am Titelcursor (`title_cursor` 0 statt 1);
  derselbe Fehler tritt mit der Vorgänger-App `.previous-m3on3nw7` auf, ist also
  Vorherstand und nicht Folge dieser Änderung.
- Weitere offene Statusbildschirm-Lücken unverändert: vollständige
  Task-/Flagweitergabe (ItemTaskCheck/MainCommand/ItemBox partial) und die
  Kisten-Farb-/Fenster-/Message-Integration.
- Keine Bildabnahme im laufenden Fenster durch dich; bitte im Spiel ansehen.
