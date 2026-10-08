# Events: Portierungsstand 2026-10-06

Ziel bleibt die vollständige Wirkung aller originalen Events. Ein vorhandener
Opcodeparser allein bedeutet weder vollständige Bindung noch spielbare Progression.

## Neu übernommene Routinen

`tools/generate_event_inventory.py` extrahiert sieben Routinen aus `event.c`.
Der Host ersetzt nur den System-/Inventarspeicher und unaligned Little-Endian-
Lesezugriffe. Kein Nachbau anhand einzelner Räume:

| Opcode | Originalroutine | Wirkung |
| --- | --- | --- |
| 0x31 | bhPlItemLost | Erstes Item in Slots 2–11 entfernen, originale Kompaktierung/Ausrüstkorrektur |
| 0xb2 | bhItemGetGet | Direkter Skripteintrag in Slot 2 der angegebenen Spielerbank; 500 Schuss für Item 34; Itemflags/Ausrüstung |
| 0xbf | bhPlItemLostEx | Erstes Item in Security-Bank 14 übertragen und aus Spielerbank entfernen |
| 0xc1 | bhArmsItemSet | Erstes passendes Item ausrüsten; originale Taschenbegrenzung |
| 0xc2 | bhItemGetGetEx | Ersten freien Slot der Zielbank belegen; Menge und originale Shotgun-/Equip-Korrektur |
| 0xc7 | bhPlItemChangeM | Erstes Item ersetzen, dabei Menge/Flags wie im Original löschen |
| 0xc9 | bhPlItemTamaSet | Menge des ersten passenden Items setzen |

Ungültige Spielerbanken/Security-Indizes blockieren vor einer Mutation.
Inventar und Equip-Slot bleiben raumunabhängig. Waffenidentität wird weiterhin
vom gepackten Inventar/Equip-Slot abgeleitet; diese Bindung implementiert keine
neue Waffen-Kampfsteuerung oder vollständige originale Monitor-Zustandsmaschine.

Weitere Bindungen: `bhPlayerPoisonCk` (0x8a), `bhEnemyHpUp` (0x99, Division vor
Multiplikation mit vorzeichenrichtigen HP), `bhMovieTimeCk` (0xac, sechs invertierte
Vergleiche, Zeit in Hundertstelsekunden, inaktiver Film ergibt 1) und
`bhWalAtariSet2` (0x7b, gemeinsame Wandattribute, Parameter und Typ). Die Wand-
änderung wirkt in bestehenden Geh-/Boden-/Schussprüfungen; diese Kollisions-
algorithmen sind weiterhin native Nachbauten.

## Gemeinsame VM-Korrekturen

- `bhCk`/`bhSet` maskieren Flagindizes auf zehn Bits wie das Original.
- `FOR` übernimmt den vorzeichenbehafteten 16-Bit-Startzähler.
- `NEXT` verwendet auch den gemeinsamen `WHILE`-Stack. RM_9070 Event 22
  kombiniert beide tatsächlich; es ist keine fehlerhafte FOR-Struktur.
- Null wird beim Countdown zu minus eins; Event_init behält die Zähler beim
  Wiederverwenden eines Taskslots. Keine Sonderregel für RM_9070.

## Prüfung und offene Arbeit

25 CTest-Gruppen auf Mac arm64 bestanden. Mechanismustests prüfen alle vier
Spielerbanken, beide Taschengrößen, Ersttreffer, volle Taschen, Mengen/Flags,
Operandengrenzen, fehlende Bindungen, Filmvergleiche, Zähler und Task-Wiederverwendung.
Echte Spielressourcen: Game-Datensuite und bestehende Event-, Kontinuitäts- und
Metalldetektor-Replays bestanden. Begrenzte Replays, teilweise mit diagnostischer
Positionierung; kein vollständiger Durchspielnachweis.

Der neue Scanner `tools/scan_event_rooms.py` erfasst **alle 256 Tabelleneinträge**
und berichtet fehlende Parser sowie tatsächlich erreichte Pending-Bindings
getrennt. 600 Ticks, Spawn 0, keine Eingaben/Mods: **205/205 Räume erreichen den
Bericht, 28 Räume melden blockierte Befehle**. Stand:
`build/validation/events-20261006/confirmed-scan/summary.txt`. Der historische
35-Raum-Stand ist kein kontrollierter Vorher/Nachher-Vergleich dieser Änderung.

Prioritäten aus diesem Scan:

1. Retry-/Game-over-Zustand (0xbd/0xc6), einschließlich tatsächlicher Systemabläufe.
2. Morphstart und erweiterte Morphdaten (0x55/0x84).
3. Spieler-/Gegner-Motionposition, Treppensteuerung und Sichtflags (0x7d/0xa4/0xcc).
4. Skripteingaben und Objektsteuerung (0x5d/0x9d).
5. Drei Pickup-Interaktionsfehler, fehlende Motion/Work und nicht vorhandene
   Audioressourcen MRY/0326, BGM1/0121, /0122, /0124 untersuchen.

Auch ohne Blockade fehlen Effekte, Gesichtsmorphs und weitere World-Bindings.
Ihre vollständige Liste bleibt im Scannerbericht und in den Runtime-Zuständen.
Keine Ersatzressourcen oder Freigabe-Hacks. Keine Aussage, dass alle Events
vollständig portiert sind.

Zusätzliche Zielprüfung: ASan/UBSan ebenfalls 25 Testgruppen und Game-Datensuite
bestanden; Windows x64 erfolgreich crossgebaut, kein Windows-Laufzeittest.
Die zwischenzeitlichen parallelen Combat-Buildfehler waren im danach geprüften
Gesamtstand behoben. Beide Starter-Binaries aktualisiert; Mac-App signiert.

Mac-Paket mit eigenen Daten: Event-/Steve-Geschenk-/RM_0070-Replays bestanden;
Paketstarter RM_9070 mit SDL-Dummy-GUI: 60 Presents, 61 Ticks, keine Blockade.
Mac-Runtime und SDL nur arm64, Signatur geprüft. Benutzer-INIs/Snapshots
per SHA-256 unverändert. Backup: archived/releases/events-before-20261006-214742.
Keine physische Controller-/Windows-Hardware-Prüfung, kein Commit/Push.
