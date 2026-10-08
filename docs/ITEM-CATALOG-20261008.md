# Itemliste, Handbuchabschluss und Originaltypen (2026-10-08)

## Originalnachweise und Änderungen

`item.c:itemdata[156]` bleibt die originale Typ-/Mengen-/Kombinationstabelle.
Dateien haben keinen separaten FILE-Typ: Schlüssel und Memos teilen 0x40.
`fileview.c:FileNumberSwitch` liefert die raumabhängigen Dateinummern.
Der Generator extrahiert die Modell-/Dateibit-Zuordnungen aus dessen switch;
keine handgeschriebene Memo-ID-Liste. Der nachfolgende TG-01-Override (85 → 2)
ist ausdrücklich kein Datei-only-Modell: `ItemModelChangeZoomIn` erhält TG-01
im Inventar und öffnet zusätzlich den Bericht. Handbuch 145 bleibt ebenfalls
Inventaritem bis zum originalen Leseabschluss.

Host-Debugliste `Item / File geben`: spielbare Originalitems 1–145 mit echten
Typwerten, Datei-only-Raummodelle ersetzt durch alle 24 eindeutigen Dateititel
(Systemnachrichten 159+Dateinummer). Eine Datei setzt ausschließlich itm[383].
Keine Zuordnung anhand des aktuellen Raums beim Debuggeben: eine Modell-ID
kann verschiedene Dateien darstellen. Leere Waffen bleiben über Typbit 1 /
originales Maximum 0 gültig. Reguläres GetItem bleibt unverändert.

`FileExit01` unterscheidet automatische Handbuchöffnung (statusflg 0x40000000,
ItemSearch(145), EraseItem, ic_flg[15]) von der allgemeinen FILE-Lektüre
(globale erste 0x910001 in itm[0..191]). Runtime reicht diese originale
Unterscheidung weiter. ItemSearch und EraseItem werden aus sub1.c extrahiert.
Der Host hatte das Item nur auf null gesetzt: fehlendes pip[0]-Dekrement,
fehlende Sortierung. EraseItem korrigiert den Ausrüstungsindex vor ItemSort.
Das folgende SpriteOnOff/ItemSort sortiert pip[2..11], nicht den besonderen
pip[1]-Slot. Keine erfundene Verschiebung in diesen besonderen Slot.

ItemCommand-Typaufbau und TrigerSet aus sub1.c werden für die Command-Sprites
extrahiert und ausgeführt: Sichtbarkeitsbits, Höhen, Wiederholungen, Use/Equip,
Check/Combine, zusätzliche Waffenaktionen und originale Auswahlfarben. Der
Renderer benutzt die zurückgegebenen Originalsprites statt eigener Geometrie-
Kürzung und eines ASCII-File-Labels. Für das Handbuch heißen die Aktionen im
Original Use/Check/Combine; die ersten beiden öffnen dessen Ordner.

## Kompatibilität alter Host-Snapshots

Vom Nutzer angeforderte Reparatur bleibt am Host-Debug-/Snapshot-Einstieg,
nicht im originalen bhPopGameData/GetItem. Doppelte Datei-only-Modelle werden
in Spielerbanken entfernt, **nur** wenn mindestens eine zugehörige Original-
Datei bereits in itm[383] besessen ist. Originalsortierung und Ausrüstungs-
identität bleiben erhalten. Keine neuen Besitzflags, keine geratenen
Zuordnungen unbesessener mehrdeutiger Modelle, kein TG-01-/Handbuch-Entfernen.
Snapshot-Dateien auf Disk werden beim Laden nicht umgeschrieben. Separate
Boxen werden nicht migriert; deren Herkunft ist mit dieser Prüfung nicht belegt.

## Prüfung und Grenzen

39/39 CTest bestanden. Inventar-Datentest: alle 156 Inspektionen/Typen,
11 Transformationen, 117 Rezepte / 1872 Differenzialkombinationen,
149 Originalmodelle und 167 Dateiseiten. Neue Checks: alle originalen
Dateimodelle, 24 slotfreie Dateigrants, Legacy-Memo mit erhaltener Ausrüstung,
TG-01-Erhalt und keine geratene Diary-Zuordnung. Handbuch vor ausgerüstetem
Messer für vier Spielerbanken: EraseItem → ItemSort → echte GetItem-Munition,
keine Munitionsausrüstung. Gezielter Datentest auch ASan/UBSan bestanden;
macOS ohne LeakSanitizer.

`tools/verify_item_catalog.py` reproduziert normales Handbuchlesen, Rückkehr,
15 originale RM_0070-Regalpatronen bei weiterhin ausgerüstetem Messer,
Debug-Dateigabe und Laden eines alten doppelten Memoitems. Snapshot-Aufstellung
ist diagnostisch, ohne injizierte Fortschrittsflags. Kontextmenü mit Original-
texturen visuell geprüft. Bestehender FILE-/Ordner-Replay und Director-Memo-
Abschluss bestehen ebenfalls. Mac-Paket mit eigenen Daten geprüft.

Windows x64 Runtime/Test crossgebaut; kein Windows-Lauf. Beide Starter
aktualisiert, Mac-App signiert, Runtime/SDL ausschließlich arm64. Neun überwachte Benutzerdateien per SHA-256 unverändert. Der vorhandene
Mac-Paket-Snapshot 1 änderte sich während der Arbeit außerhalb der isolierten
Replay-Snapshot-Verzeichnisse; sein aktueller Stand wurde erhalten, nicht
auf den vorigen Hash zurückgesetzt. Logs: build/validation/item-catalog-20261008.
Backup: archived/releases/item-catalog-before-20261008. Kein Commit/Push.

Dies ist kein vollständiger Port des gesamten Inventarmenüs: Command-Eingabe-
und Übergangsphasen, weitere ItemUse-/Use00-Hostbindungen, Gesamt-StatusMain,
Map-/Save-Screens und gesamte PS2-Darstellung haben weiterhin offene Dienste.
Die neuen Originalsprite-Routinen ersetzen nicht die ganze ItemCommand-
Zustandsmaschine. Kein Durchspiel-/Controller-/Windows-Hardware-Nachweis.
