# Prüfung von docs/CLAUDE_NEXT_TASKS.md

Stand: 2026-10-21, Basis f18acea. Der historische Auftrag vom 2026-10-09 wurde
gegen aktuelle Besitzer, ausgewählte Originalstellen und gezielte Tests geprüft.
Keine vollständige Durchspiel- oder Windows-Abnahme.

## Ergebnis je Auftrag

| Abschnitt | Aktueller Befund | Belege |
| --- | --- | --- |
| R02: ADX-Mischgenerator | Erledigt. CMake bindet native/adx_tables.c; der Mischgenerator wird nicht ausgeführt. Der Policy-Prüfer erkennt gemischte Erzeugung und Erzeugen-mit-nachfolgendem-Löschen; reine Daten sind erlaubt. | CMakeLists.txt:45, tests/port_policy_test.py:55-87, native_port_policy_contracts |
| D02a/b: Aufnahme, Kapazität, Stapel, Sortieren/Suchen/Löschen | Implementiert auf Inventory und EventVm, einschließlich Ja/Nein, volle Tasche, breite Items, Geschenk und Sofortgebrauch. Sortieren folgt dem Statusframe, nicht jeder Mutation. Sonder-flgchk 0x10/0x20 in ItemSort hat keinen Originalsetzer; die alte offene Ledgerangabe war überholt. | native/inventory.cpp, native/events.cpp, sub1.c:2266-2343/:3735 ff.; inventory_events_assets, inventory_item_sort, pickup_status_reset, pickup_gap_pad, crouch_pickup |
| D02c/d: Munition und ItemUse | Native Bestands-/Flagpfade vorhanden, gemeinsamer Inventarbesitzer; Heilung, Gegengift, Sprays, Bodentrigger und Meldungen werden geprüft. Kein neuer Waffen-/Spielerbesitzer. | native/inventory.cpp, ammunition_test.cpp, inventory_engine_test.cpp, status_pulse_test.cpp |
| D07: Kombination | Alle sechs Combi-Routinen und zwölf Selektoren vorhanden. Auch Menüzustände und Verschmelzen sind implementiert: Frage, vier Wartebilder, vier mpt/4-Schritte und Sortierpause. Alte pauschale Meldung „Animation fehlt“ entfernt. | native/ui.cpp:155-219; sub1.c:5259-5986/:8294; inventory_combine_inspect, original_inventory_table_coverage, inventory_item_sort |
| D01: Kiste | Transfer, Stapel, Einschränkungen, Kapazität und Ausrüstung vorhanden. Der konkret fehlende Slotcursor bei Breitenwechsel ist seit f18acea nach Original-ELF 0x2A5C5C-0x2A5D40 gebunden; vier Kombinationen (breit/schmal, Tasche an/aus) geprüft. | native/box_menu.cpp, Inventory::box_change; engine_progression_resources, item_box_pad_frames, item_box_equipment_slot |
| D06: sieben Eventmutationen | Bereits auf Inventory gebunden; feste Bereiche, Bankwahl, Mengen/Flags und Ausrüstung geprüft. Nicht erneut portieren. | docs/port_ledger.tsv event.c:bhPlItemLost/Ex, bhItemGetGet/Ex, bhArmsItemSet, bhPlItemChangeM, bhPlItemTamaSet; inventory_events_assets |
| D09: Ziffern/Farben/Befehlsfenster | Mengen, Unendlichzeichen, Munitionsfarbbänke und TrigerSet-Fensterhöhen/Markierung umgesetzt. | native/ui.cpp; sub1.c:6036-6089; ui_quantity (echte Atlanten und Pixelunterschiede) |
| D08: EKG | PulseInit/Main/Anim/Fade, Heil- und Gegengiftband mit UV/Farben vorhanden; Menü benutzt denselben Heil-/Giftzustand. | native/status_pulse.cpp; sub1.c:9075-9800; status_condition_pulse |
| Gemeinsame Prüfung | 16 gezielte CTests frisch gebaut und bestanden. Alte Angabe „34/41“ ist kein aktueller Befund. | build/validation/next-tasks-audit/build-tests.log |

## Tatsächliche Restgrenzen — bleiben erhalten

Das Entfernen des historischen Auftrags erklärt nicht das ganze Paket für vollständig:

- **StatusMain / ItemTaskCheck:** Einzelne originale Flag-/Taskübergaben, etwa
  flgchk 0x2, die vollständige sp_flg/ts_flg-Weitergabe und Waffenlese-/Monitor-
  Verzahnung, sind weiterhin nicht vollständig nachgewiesen. Der aktuelle
  Rahmen (Monitor-Eintritt, vier Abbaupässe, bhStandPlayerMotion) wird separat
  durch status_screen_hold geprüft. Ledger: sub1.c:ItemTaskCheck,
  system.c:bhSysCallItemselect und sub1.c:MainCommand bleiben partial.
- **Kistendarstellung:** Trotz geschlossener Breitenwechsel-Cursorlücke ist die
  vollständige Farb-/Fenster-/Message-/StatusMain-Integration nicht belegt;
  sub1.c:ItemBox bleibt partial.
- **End-to-End und Windows:** Die 16 Tests sind gezielte native Mechanismus-
  und einzelne Runtimeprüfungen, kein vollständiges Durchspielen. Windows
  wurde in dieser Prüfung nicht gebaut oder ausgeführt.
- Die allgemeinen Ziele „alle Gegner vollständig“ und die separaten D03/D04/D05-
  Pakete sind kein neuer Auftrag dieser historischen Inventardatei. Aktuelle
  Fachseiten und einzelne Ledgerzeilen bleiben maßgeblich.

Diese Grenzen stehen weiterhin in docs/NATIVE_GAMEPLAY.md und
 docs/port_ledger.tsv; sie werden nicht zusammen mit der alten Aufgabendatei gelöscht.

## Prüfstand

`./build.sh --platform macos --test-regex ...` hat 16 passende Tests frisch
gebaut und bestanden, einschließlich Policy, Status-/Inventartabellen,
Aufnahme, Kombination, Sortieren, Kiste, Mengenanzeige und EKG. Der Wrapper hat
die Start.command-Dev-App aktualisiert. Eine eigene visuelle Menüabnahme
oder ein neuer Windows-Lauf gehört nicht zu diesem Dokumentenabgleich.

Zusätzlich: Policy-Prüfer und tools/port_check.py ausgeführt, Ledgertexte zu
ItemSort, ItemCombination und ItemBox aktualisiert. Kein neuer Gameplaycode
in dieser Prüfungsrunde. docs/CLAUDE_NEXT_TASKS.md wird entfernt; die Roadmap
verweist auf Fachstand, Ledger und diesen Bericht.
