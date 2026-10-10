# D03: Datei- und Untersuchungsmechanismen nativ (2026-10-09)

Plattform macOS arm64. **Windows x64 ungeprüft** (Stand unten). Kein Commit,
kein Paketstand. `python3 tools/check_no_original_core.py` meldet PASS,
`tests/port_policy_test.py` 24/24.

Semantikreferenz: `itemview.c` (ItemModelCheck, ItemModelMessageWait,
ItemModelActionSet, ItemModelChangeZoomIn/Out, MdlDirChk, MdlAction00/01/02,
MdlEvalflagsSet, MdlHideCheck, FlagErase, LighterOpen, DrawSubItem),
`fileview.c` (FileNumberSwitch, PlayPageCheck, FileScrollSet/PageScroll,
ReadFstx, SearchTag, SelectFile, SelectTag) und `sub1.c` (Namensauswahl).
Kein Funktionskörper übernommen, kein Originalkern, keine PS2-Nachbildung.
Die reinen Datentabellen sind übernommen und als solche gekennzeichnet.

## Ergebnis

- CTest macOS arm64: **42/42 bestanden**.
- `verify_item_catalog`: **bestanden** — Handbuch lesen und entfernen, Messer
  bleibt ausgerüstet, Dateikatalog, Reparatur eines Altbestands.
- `verify_file_pickup`: **bestanden** — Director Memo als Datei 8, Leser
  schließen, unverändertes Inventar, keine zweite Aufnahme.
- `verify_file_folder`: **bestanden** — Ordner, Register, Seiten,
  Handbuchrückkehr und Statusschluss in der echten Runtime.
- `verify_events`, `verify_progression`, `verify_pickup_slide`: bestanden.
- 205-Raum-Scan: 205/205, 0 blockierte Befehle.

## Was jetzt nativ vorliegt

**Untersuchungsautomat** (`native/inspection.cpp`): Richtungsfenster über
`itemflg[hide][4]` und `c_a` mit der originalen 16-Bit-Verkürzung, die
Ausnahmeliste (19/33/34/136/142), die Dietrichsperre (Meldung 181 nur mit Item
50, sonst 188 und kein Vorgang), die Namensfreischaltung des unbekannten
Objekts (Bit 0x80000000 bei Meldung 72), der stille Start von Item 106, die
Bestätigungsfrage, die drei Aktionsroutinen mit ihren Knotentiefen (MdlAction01
unterscheidet 128/63/129), beide 32-Durchlauf-Zoomphasen mit der Drehung
4095/2047 je Durchlauf, die Umwandlung über `itemflg[...][6]`/`idsettbl`, die
Ergebnismeldung aus `getmestbl`, die Startpose des Ergebnisses (feste Winkel für
105/30/35, sonst `dsptbl`), der Übergang in den Dateibildschirm bei Ergebnis 85
und der Abbruchzweig, der die Magnummunition auf Item 13 normalisiert.

**Modellgeometrie** (`inspection_geometry`): `FlagErase` löscht an jedem Knoten
evalflags 0x1/0x2, `MdlHideCheck` setzt die Sichtbarkeitsbits mit der
originalen Maskenverdopplung über Kind und Geschwister (unsigned-char-Überlauf
inklusive), `LighterOpen` öffnet den Feuerzeugdeckel, und die Aktionspose dreht
den Deckelknoten um 1456 je Durchlauf gegen die Gegendrehung 728 am ersten
Kindknoten.

**Dateizuordnung und -tabellen**: `FileNumberSwitch` als Raum/Item-Tabelle
(26 Einträge plus Raum 551 und die Sonderregel für Item 85), die
Modellmaske der reinen Raumdokumente, `fstbl` (Seiten), `fsheader`
(Seitentextbasis 280) und `wallpaper` (ITEM-Archivnummer). Insgesamt 24
Dateien mit 167 Seiten, wie vor dem Rückbau.

**Namensauswahl**: unbekanntes Objekt nach der Untersuchung (158), leerer
Feuerlöscher (149), gelesenes Handbuch (159), sonst die Item-ID.

**Dateiordner** (`native/file_folder.cpp`): `SearchTag`, `SelectFile` mit
Stapeldrehung und Herausschwenken, `SelectTag` mit Registercursor,
Leseranforderung und Rückdrehung, die Startwinkel aus `FileSyu` und
`PlayPageCheck` als Seitenreduktion des Handbuchs (Reihenfolge 0x199, 0x2B,
0x129, 0xE0, 0x96, sonst 7).

## Dabei gefundene und behobene Ursache

`Inventory::repair_owned_file_items` (Host-Kompatibilität für doppelte
Dateiitems aus Altbeständen) entfernte Einträge ohne die originale
Nachführung des Ausrüstungsindexes. Es benutzt jetzt dieselbe `erase()`-Regel
wie jede originale Entnahme; nur die Auswahl der Einträge bleibt
Host-Kompatibilität.

## Offene Punkte

- `si->mw.pos.y` sinkt im Original während der Aktion um 0,05 je Durchlauf bis
  −0,76 (der Gegenstand rutscht leicht nach unten). Der Host leitet seine
  Ansicht aus den Modellgrenzen ab und übernimmt diesen Versatz nicht; das ist
  eine Auslassung in der Darstellung, keine Ersatzlogik.
- `inspection_geometry` zählt die Deckelschritte (1456 je Durchlauf, fertig bei
  32768). Das Original vergleicht den aufsummierten Knotenwinkel, also den
  Dateiwert plus die Schritte; beide stimmen überein, solange der Deckel im
  Modell geschlossen bei 0 steht. Die Gleichheit ist über die echten Pakete im
  Test geprüft, aber nicht für jeden denkbaren Startwinkel.
- Die Kamerafahrt der Zoomphasen (3,2 Einheiten je Durchlauf) bildet der Host
  als Faktor 1→0→1 ab; die absolute Kameradistanz des Originals ist nicht
  nachgebildet.
- `verify_detector`, `verify_continuity`, `verify_combat`, `verify_pickups`,
  `verify_ammunition` bleiben an Effekten, Treppenmotion, Spielerkampf und
  Kollision blockiert. Das sind getrennte Pakete.
- Die Besitzeroperanden der Skripte sind weiterhin **nicht vollständig
  statisch belegt** (siehe `work/native_port_inventory_tests.result.md`): die
  Operandenlängen mehrerer Eventbefehle sind in der dekompilierten Quelle nicht
  fest, ein Bytescan liefert nur eine verrauschte Obermenge, und 205 Raumläufe
  beweisen nicht alle Skriptpfade. Keine geratenen Längen eingesetzt.
