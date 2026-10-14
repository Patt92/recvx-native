# Bericht: Kombinationsablauf im Statusbildschirm und ItemSort je Statusframe

Thema `inventory_combine` (Subagent `inventory-combine`, 2026-10-14). Geteilter
Arbeitsbaum, nur eigene Mechanismen geändert. Kein Commit/Push/PR, keine
Paketersetzung, kein Originalkern (`check_no_original_core.py` vor jedem Build PASS).

## Originalmechanismus

- `sub1.c:5225 ItemCombination` (ItemCommand testmode 2, `sub1.c:4936`), statische
  `ok/mct/mct_00/mpt` (5241-5244):
  - Wahl „Kombinieren“ (`4843-4851`): `subcsr 0`, testmode 2. Das Befehlsfenster
    bleibt stehen (kein `statusflg 0x800000`), TrigerSet läuft nur in testmode 0 (`4625`).
  - subcsr 0 (`5259-5300`): `parts_09b[1]` übernimmt die Cursorposition (anim 0,
    Quelle), `parts_09b[0]` +4/+4, anim 2; Farbpuls neu; fällt in subcsr 1 durch.
  - subcsr 1 (`5301-5571`): Cursorbewegung; 0x800: gleicher Platz oder keine Regel
    -> SE 1, sonst `cmbitm` und `subcsr 2` **ohne Klang**; 0x1000 -> testmode 1
    (zurück ins Befehlsfenster), `listcsr_0 = listcsr_1`, SE 0.
  - subcsr 2 (`5572-5938`, ein Frame später): Combi_xx schreibt pip **sofort**;
    ok 4 -> SE 0, subcsr 1 (Meldung 156 aus Combi_00); ok 5 -> SE 1, subcsr 1;
    sonst `subcsr 3`, `mct_00 = 4`, movedir 1: `mct 132`, `mpt = (p0-4) - p1`,
    movedir 0: `mct 4`, `mpt = p1 - (p0-4)`, SE 3. Klasse 6: Frage 155 mit SE 3,
    Antwort `mes_sel` 100 -> Combi_03, 101 -> ok 5.
  - subcsr 3 (`5939-5986`): 4 Wartebilder (`mct_00`), 4 Schritte `mpt/4` an
    `parts_09b[1]` (mct & 0x80) bzw. `[0]`; danach `pip[0]--` bei ok 1 und pip[0]
    hinter einem Cursor, `statusflg 0x800000` (ItemCommandErase im selben Frame,
    `3496`), subcsr 4, anim 0, `parts_09b[1]` aus, movedir 0: `listcsr_0 = listcsr_1`.
- `sub1.c:8293-8296 SpriteOnOff`: ItemSort bei `subcsr != 3` (Disassembly
  0x002A6660: `lbu 0x7D($s0)` gegen 3, dann `jal ItemSort`); `8318` CursorSet
  ebenso nicht in subcsr 3. SpriteOnOff läuft nach der Statuslogik (`3580`).
- `sub1.c:2009 CursorSet`: Position aus listcsr_2 (+4 mit sichtbarem `[1]`),
  `parts_15[0].sx` 144/76 bzw. `[1..3].sx` 136/68, Farbpuls alle 3 Frames 0.05
  (0.07 beim Kombinieren), bei `statusflg 0x10` fest 0.6.
- ItemSort-Aufrufer per Disassembly geprüft: nur SpriteOnOff, bhSearchBullet,
  EraseItem. **Kein** ItemSort in GetItem, ItemUse, Use_00..05, ItemCombination,
  Combi_00..05, ItemCommand, ItemBox, ItemBoxChange (0x002A53E0), StatusMain.

## Änderungen

- `native/inventory.{h,cpp}`: `combine()` zerlegt in `combine_items()` (subcsr 2,
  schreibt pip sofort, kein Zurückrollen bei ok 4 – Combi_00 schreibt moto/aite
  immer zurück, `sub1.c:6744`) und `combine_finish()` (`5963-5966`). `combine()`
  bleibt als Gesamtablauf ohne Bildschirm (+ ItemSort des Folgeframes); sein
  `result_slot` ist der Original-listcsr_0 und folgt dem Sortieren **nicht**
  (Host-Nachführung `compact(&destination)` entfernt). Streu-`compact()` entfernt
  aus `pickup()` (GetItem), `use_item()` (ItemUse) und `box_change()`
  (ItemBoxChange, zwei Stellen). Belassen: `erase()`/`reload()` (Originalaufrufer),
  `acquire()` (Host-Debugkatalog), `repair_owned_file_items()` (Host-Kompatibilität),
  `deposit()` (Host, ohne produktiven Aufrufer). `compact()` ohne Tracking-Parameter.
- `native/ui.{h,cpp}` `StatusMenu`: ItemCombination als Zustandsautomat
  (`combine_subcsr_` 1/2/3, `step_combine()` im Tick = StatusMain-Dispatch),
  Befehlsfenster bleibt offen und zeigt die Quelle, Cancel -> Befehlsfenster,
  Frage 155 per Antwort im Folgeframe (Cancel beantwortet sie nicht,
  `message.c:233` liest nur 0x800), SE über neues `tick_sound`.
  Neues `sprite_on_off()` = ItemSort (außer subcsr 3) + CursorSet-Farbpuls.
  `cursor_parts()` liefert `parts_09b[0]/[1]` (CursorSet bzw. eingefrorene und
  verschobene Werte in subcsr 3). `GameUi::draw` zeichnet beide als parts_15-Sprites
  an cen_pos[5] (z 2 vor den Icons z 4) statt der Host-Rechtecke; Host-Text
  „Combine“ entfernt. Lückenkommentar zu sub1.c:8296 entfernt.
- `native/runtime.cpp` (beide Tickschleifen): `menu.sprite_on_off(inventory)`
  direkt nach `box_menu.tick` (nach Status-, Ereignis- und Kistenlogik, vor dem
  Zeichnen). `native/input.cpp`: `tick_sound` wird wie `slide_sound` abgespielt.
- Tests: `inventory_sort_test.cpp` (neu: Kistenlücke bis SpriteOnOff, ItemUse
  ohne eigenes Sortieren, Kombination movedir 0 mit 8 Frames Lücke und
  Cursorbahn 92→24, movedir 1 mit Quellbahn 52→4 und pip[0]-- erst am Ende),
  `inventory_menu_test.cpp`, `inventory_engine_test.cpp`, `input_test.cpp`,
  `progression_test.cpp`, `game_test.cpp` an den Statusframe/Originalcursor
  angepasst (jeweils mit Fundstelle; Erwartungen nicht abgeschwächt, sondern um
  Zwischenzustände ohne Sortieren erweitert).

## Prüfung

- Vorher (Binaries 18:26, `build/validation/inventory_combine-20261014/before/`):
  alle 15 betroffenen Suiten grün. Nachher (`after/`, `build-ctest-2.log`):
  `./build.sh --test` 91/91 grün. Zwischenzeitlich rote `door probe`/`exm gate`-
  Prüfungen in game_test gehörten zu fremder laufender Arbeit (events.cpp);
  game_test ohne diese Blöcke war mit meinen Änderungen grün
  (`after/game-without-foreign-door-check.log`), im letzten Volllauf grün.
- Runtime (headless, RM_0090, isolierte INIs; Testaufstellung per Debug-give,
  keine natürliche Spielsituation), Bilder angesehen, Werte in
  `runtime-summary.txt`:
  - Kräuter (Handbuch auf Slot 3 hinter Zwei-Slot-Waffe 34): Frage 155 (c112),
    Antwort Frame 115, Frame 116 Produkt 25 auf Slot 4 und Lücke auf Slot 5,
    Lücke bleibt bis 124, Cursorrahmen wandert (c122), Frame 125 rücken
    Pistole/Munition nach, Befehlsfenster fährt aus (c125, c140).
  - Munition auf Pistole (Klasse 1, movedir 1): roter Quellrahmen wandert zur
    Pistole (a192), Pistole 16 Schuss (a210).
  - Handbuch über „Use“ gelesen und abgelegt: Frame 501 Slot 3 frei und sofort
    nachgerückt (m501/m520).
  - Sicherheitskiste: Kräuter korrekt mit Meldung 200 abgelehnt; Pistole
    abgelegt, Munition rückt im selben Frame nach (b364/b380).

## Offene Punkte / Grenzen

- `ItemCommand` case 2 verlangt `mn_md0 == 0` (Modellnachladen); nativ ohne
  Besitzer, nicht abgebildet. `statusflg 0x100000`-Wechsel in ItemCombination
  (Namensanzeige in SpriteOnOff:8408) nicht abgebildet.
- `statusflg 0x10` des Farbpulses ist nativ `examining` (Modell sichtbar).
- Beobachtet, nicht geändert: nativ startet der Statuscursor auf pip[1]
  (`selected=1`), CursorInit setzt `listcsr_0 = 2` (`sub1.c:8998`). Die Kiste
  zeichnet weiter Host-Rechteck + statisches parts_09b ohne CursorSet-Puls.
  ItemCommandErase gibt nativ einen Frame früher frei (bestehendes erase_frames-Modell).
- Nach einer Kombination mit movedir 0, bei der der Cursorplatz durch ItemSort
  frei wird, steht der Cursor (wie im Original) auf dem leeren Platz.
- Nicht über Start.command-App/Paket ausgeliefert geprüft; geprüft ist der
  Build `build/macos-arm64` (Dev-App von build.sh aktualisiert).

## Ledger-Vorschläge

```
sub1.c:ItemCombination	partial	subcsr 1/2/3/4 in StatusMenu::step_combine: Entscheidung ohne SE, Combi_xx im Folgeframe, Frage 155 (mes_sel 100/101), ok4 SE0/ok5 SE1, mct_00 4 + 4x mpt/4, movedir 0/1, pip[0]-- am Ende, listcsr_0=listcsr_1, ItemCommandErase; inventory_sort_test, build/validation/inventory_combine-20261014; offen mn_md0-Tor, statusflg 0x100000
sub1.c:ItemSort	done	Nur Originalaufrufer: SpriteOnOff je Statusframe ausser subcsr 3 (StatusMenu::sprite_on_off nach der Statuslogik), EraseItem, bhSearchBullet; GetItem/ItemUse/Combi/ItemBoxChange sortieren nicht (Disassembly); flgchk 0x10/0x20 von keiner Quelle gesetzt; inventory_sort_test
sub1.c:SpriteOnOff	partial	ItemSort (8293-8296) und CursorSet-Teil fuer parts_09b des Statusbildschirms nativ; uebrige Zeichenteile im GameUi
sub1.c:CursorSet	partial	Statusbildschirm: Position/+4, parts_15-Breiten, anim, Farbpuls 0.05/0.07 je 3 Frames, statusflg 0x10 als examining; Kiste noch eigener Zeichenweg
sub1.c:GetItem	partial	Original GetItem inkl. Zweig 153 ohne eigenes ItemSort; Sortierung durch SpriteOnOff desselben Statusframes; StatusCancel/Sprite-Ein-/Ausfahren und ganzer StatusMain nicht vollständig gebunden
```
