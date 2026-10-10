# B05 — nativer Itembox-Deckel

## Mechanismus

`native/object_itmbox.cpp` bestimmt den vollständigen vierphasigen
`bhObjItmBox`-Mechanismus eigenständig. Zulässige Semantikreferenz:
`objitm.c:799`, ohne Generator-/Quarantäneinhalt oder Originalkörper.
Der bestehende RoomWorld-/WorldEntity-Objektbesitzer bleibt erhalten.

- Init setzt den Zähler auf null, fordert einmal Klang 0x80000240 an und
  führt im selben Pass den ersten Öffnungsschritt aus.
- Der Zähler steigt um 384 je Objektpass. Winkel aus 80*njSin(counter),
  Umrechnung 182.04445 und ursprünglicher 16-Bit-Maske, keine Annäherung.
- Bei 15360 bleibt die Öffnungsphase; erst 15744 wechselt zu Phase 2.
  Insgesamt 41 Öffnungsschritte. Menüanforderung 0x40000 folgt in Pass 42.
- Nach dem Menü setzt Phase 3 den Objektwinkel auf null, Typ auf null
  und löscht ausschließlich Spieler-stflg 0x10000. Keine erfundene
  Schließanimation oder zweiter Deckelklang.

`world_itmbox.cpp` führt die ursprünglichen sp_flg-0x4-/Objekt-flg-0x1-
Gates aus. Der vorhandene Menübesitzer pausiert die Objektsteuerung während
des Menüs; anschließend wird der Objektzustand freigegeben. Die vorhandene
Audio-Bindung spielt SDE-Bank 2, Liste 64 über den System-MIDI-Dienst.

Beim Abgleich des ursprünglichen Interaktionsproduzenten in `hitchk.c`
wurde eine fehlende Mutation entdeckt: plp->flg 0x10000. Der gemeinsame
EventVm-Itemboxzweig setzt jetzt dieses Bit in seiner bestehenden Player-
Flagquelle und aktualisiert deren vorhandene Sicht. Kein Raumsonderfall.
Die bereits vorhandenen stflg-0x18000-/st_flg-0x4-Mutationen bleiben.

## Nachweise

Komponententest `item_box_object`: jeder der 41 Schritte, strikter
Schwellwert, fünf festgeschriebene Winkelsamples (-536/-1071/-9780/
-14493/-14536), einmaliger Klang, Beibehaltung fremder Bits, separater
Menüpass, Rückkehr und unbekannter Dispatcher-Modus ohne erfundene Aktion.

World-/Originaldatenprüfung in RM_1080, RM_3110, RM_4040, RM_6040, RM_9040:
echte ATR-Interaktion startet das Originalobjekt; keine Typ-100-Injektion.
Objekt-Pause, Playerbits, tatsächliche Objektwinkel, Menüanforderung und
Freigabe geprüft. Der gezeichnete Vertexstrom bewegt sich, ohne Geometrie
zu verlieren. Positionierung am ATR-Mittelpunkt ist diagnostisch.

`tools/verify_item_box_object.py`: unveränderte RM_1080-Raum-/Eventdaten,
echte Runtime-Eingabe mit diagnostischer ATR-abgeleiteter Positionierung:
geschlossen → öffnend (noch kein Menü) → echte Standard-Itembox →
Abbruch/Steuerungsfreigabe → erneutes Öffnen. Genau ein Klang pro Öffnung,
HP und Inventory unverändert, keine blockierten/pending Befehle oder
Audioausfälle. Geschlossen-/Öffnend-Bilder visuell geprüft: sichtbarer
Deckel steigt am selben vorhandenen Objekt auf.

Inventar-/Event-Lifecycle-/Progressionsregressionen werden gezielt geprüft;
der bestehende Security-Box-/STANDARD-Tausch-Runtimepfad bleibt erhalten.
Kein Storydurchlauf und kein vollständiger allgemeiner Objekt-/Player-
Zustandsmaschinenabschluss. Reservierte/verknüpfte Objektbesitzer und
deren allgemeine Sleep-/Transformregeln bleiben F01/P00, nicht durch
geratene Deckelmodelle ersetzt.

B04/Haar und C01/Fallen nicht bearbeitet. Kein Paket/Commit/Push.

## Abschlussnachweise

macOS arm64: Runtime gebaut; 5/5 gezielte CTest-Gruppen grün
(Deckel, Inventar, Progressionsressourcen, Event-Lifecycle, Policy).
Runtime-Deckelprüfung und bestehende Security-/STANDARD-Tauschprüfung am
finalen Spielerflag-Stand grün. Quellen-/Build-/Runtime-Binary-Prüfer PASS,
`git diff --check` ohne Befund.

Finale Logs/Zustände/Bilder: `build/validation/item-box-object-final-20261010`
und `build/validation/item-box-progression-final-20261010`. Die zuvor visuell
geprüften Deckelbilder liegen in `build/validation/item-box-object-20261010`;
die anschließende Flagkorrektur verändert nicht den Deckelalgorithmus.

Windows x64: Spielbibliothek und `recvx-item-box-object-tests.exe` am
finalen Stand kompiliert/gelinkt. Nicht ausgeführt; kein Windows-Runtime-
oder Binaryaudit dieses Pakets behauptet. Kein aktualisiertes Startpaket.
