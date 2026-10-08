# Kistenkante in RM_0100 – 2026-10-08

Der gemeldete Raum ist RM_0100.RDX. Vorherige Binary eingefroren unter
`build/validation/crate-edge-20261008/frozen/before-runtime`.
Diagnostische Aufstellung auf Originalkiste 5 (-19.4933,13,-74.5499), danach
normales Vorwärtsgehen: seitlich endet Claire auf Höhe 4 statt 13.
`before-16384.json` und `before-49152.json` belegen den Fall; die entsprechenden
`after-*` bleiben auf Höhe 13. Keine Raumskript- oder Fortschrittsflagänderung.

## Originalmechanismus

- hitchk.c:5050 `bhSetDansaLimitAtari` erzeugt bis zu vier temporäre Wände aus
  der aktuell betretenen oberen type-2-ETC-Fläche. Der originale `dan_ap`-Zeiger
  erhält die letzte Fläche, wenn der Mittelpunkt bereits außerhalb liegt.
- hitchk.c:5216 `bhCheckDansaAtari` durchsucht Raum-ETC vor mobilen ETC,
  einschließlich Besitz-/Typ-/prm0-/Etagenprüfung und inklusiver Grenzen.
- hitchk.c:5245 `bhCheckFloorP` setzt `stflg 0x20000` anhand des Mittelpunkts
  oder vier Radiusproben. player.c:909 initialisiert diesen Zustand;
  player.c:1664 erneuert ihn nach der Kollisionsphase.
- hitchk.c:641 `bhCheckWallEx` bindet die Kantenwände vor der Kollision und
  entfernt sie danach über `mwal_n -= dla_n` (1262).
- hitchk.c:1939 `bhCheckWallType` liefert den ersten Kontakt; der Aufrufer
  bhCheckExmAtari (4730ff/4950ff) erlaubt bei Auf-/Abstieg nur keinen Kontakt
  oder dessen attr 4. Die negative Höhe einer type-7-Fläche ist eine
  Ebenenprüfung, kein normaler Quader.
- objitm.c:850ff `bhObj001` erzeugt für aspd 0/6 zwei ETC-Flächen, oberen
  type-7-Boden und Bodenklangfläche. RM_0100 enthält drei dieser Kisten.

## Korrektur

Der Generator übernimmt die vier vollständigen Originalfunktionen. Der
leere Kantenbegrenzungsdienst ist entfernt. Die Spielerbindung hält `dan_ap`,
führt FloorP vor der ersten Kollision und nach jeder Kollisionsphase aus und
bindet bei externem Actor-Wechsel (separater Auf-/Abstiegswork, Debugaufstellung)
dessen aktuellen Bodenzustand. Raum-ETC/FLR bleiben von mobilen Tabellen getrennt.
Vier zusätzliche ATR_WORK-Slots tragen die temporären Kantenwände, auch wenn
keine bewegliche Kiste vorhanden ist. Es gibt keine Kisten-ID-/Raumregel.
Die angenäherte Host-Freiraumprüfung verwendet jetzt bhCheckWallType mit
Originalreihenfolge, Geometrie und type-7-Ebenenprüfung.

## Nachweise

- Eigener arm64-Release-Build: 39/39 CTest, `final-ctest.log`.
- Originaldaten-Test: Gehen/Rennen in vier Richtungen; benachbarte obere ETCs
  ohne Nahtbarriere; Entfernung temporärer Wände; type-7-/0x400-Proben;
  alle drei RM_0100-Kisten in vier Richtungen; bisherige MTN49/50-/Wand-/
  Loslass-/RM_2070-Prüfungen erhalten.
- Dieselbe gezielte Gruppe ASan/UBSan bestanden, LeakSanitizer auf macOS aus.
- `tools/verify_crate_edge.py`: echtes RM_0100, diagnostische Aufstellung auf
  Kiste 7; vier Geh- und Rennrichtungen bleiben auf 13. Aktion auf freier
  Nordseite startet MTN22 (DND) und endet auf 4 mit freier Steuerung.
- Mac-Paketlauf mit eigenen Paketdaten ebenfalls bestanden.
- Windows x64 Runtime/Test crossgebaut; kein Windows-Lauf.
- Beide Starter-Binaries aktualisiert, Mac-App signiert und streng verifiziert,
  Runtime/SDL arm64. Backup: `archived/releases/crate-edge-before-20261008`.
  Sieben Benutzer-INI-/Snapshot-Dateien per SHA-256 unverändert; Nachweis
  in `user-files-before/after.json`.

## Grenzen

Diagnostische Aufstellung ist kein natürlicher Durchspielnachweis. In RM_0100
bleibt vor und nach dieser Änderung derselbe ausstehende bhAddObjSe-Befehl
(0x8b, Event 0, Offset 34); der Replay prüft ihn ausdrücklich als bestehende
Portierungslücke, nicht als erfolgreich ausgeführt. Effekt29 und physischer
Vibrationstreiber bleiben separate offene Dienste. Der Statusabgleich sämtlicher
FloorP-Seiteneffekte mit dem Event-VM, Änderungen statischer ETC/FLR durch
Raumskripte sowie die vollständige gemeinsame Spieler-/Aufstiegssteuerung sind
weiter zu auditieren; dieser gezielte Fix ist keine vollständige 1:1-Zusage.
Parallel vorhandene Änderungen erhalten. Kein Commit/Push.
