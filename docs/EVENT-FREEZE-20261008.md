# RM_0050 und frühe Enter-Eingaben – Prüfung vom 2026-10-08

Der eingefrorene frühere Build `build/validation/player-push-20261007/frozen/recvx-runtime`
reproduziert den Eventfehler mit unveränderten Raumskripten:

- RM_0050-Szenario: Offset 0x632 / Opcode 0x2b blockiert.
- Hundeevent 15: Offset 0xd8e / Opcode 0x2b blockiert.
- Rückkehr/Treppen-/Torereignis 5: Offset 0xb08 / Opcode 0x2b blockiert.

Alle drei melden `event interpolation requires original FOR clock`. Der
Befehl ist jedoch `bhMotionPauseSet`, keine Interpolation. `event.c:1791`
liest nur Opcode und Modus (zwei Bytes), schreibt bei 0/1 mode3=4/1 und
überträgt dies für Gegner-ID 1 auch an exp1. Im alten Port wurden vier Bytes
konsumiert. Damit wurden zusätzlich Folgekommandos übersprungen, etwa die
Flagfreigabe nach dem Hundeevent und der Beginn des Sleep-Befehls im Torereignis.
Die separat laufende Audit-Arbeit korrigiert bereits die Befehlslänge und
Pause/Fortsetzen in `native/events.cpp`, `native/world.cpp` und den Gegner-Works.
Diese Änderungen wurden übernommen und durch unabhängige Prüfungen validiert.
Keine Raumsonderregel oder Änderung an Originalskripten.

`tests/native/event_lifecycle_test.cpp` prüft beide originalen Hundeevents
15/16 bis zum Ende, danach Padlock-Verwendung, originale Ja-/Verwerfen-Frage
und sämtliche drei Ausgänge **im selben VM-/Weltzustand**. Die frühere
BGM-Prüfung hatte für die Tür jeweils eine neue Welt angelegt und prüfte nur
den Musikstart. Neue Prüfung außerdem: 14 einzelne Confirm-Zeitpunkte nach
Spielinitialisierung, danach FMV-Skip, Hinweis 4, automatische Statusöffnung,
Feuerzeug-Ausrüstung, Rodrigo und Steuerungsfreigabe.

`tools/verify_event_freeze.py` prüft die echte Runtime: Treppe aufwärts und
abwärts, Bereichsaktivierung eines originalen Hundeevents, vollständiges
Eventende und anschließenden Raumwechsel nach RM_0060. Snapshot ev_flg 117,
Padlock-Schlüssel und Aufstellung sind diagnostische Voraussetzungen; keine
Behauptung eines natürlich durchgespielten Rückwegs. Die C++-Prüfung startet
beide Hundezweige ausdrücklich; Originalskripte bleiben unverändert.

Mac arm64: 39/39 CTest bestanden. Feste Runtime-/Test-Binary-Kopien unter
`build/validation/event-freeze-20261008/frozen` bestehen die gezielten Daten-
und Runtime-Prüfungen. Windows x64 Runtime und Lifecycle-Test crossgebaut;
kein Windows-Lauf. Während der Prüfung weitere fremde Kisten-/Rendererarbeit
im gemeinsamen Baum; spätere Gesamtbuildversuche sind deshalb getrennt von
dem bestandenen 39-Test-Stand zu beurteilen. Ein späterer Push-Test war
vorübergehend syntaktisch unvollständig; fremde Arbeit wurde nicht verändert.

Die breitere bestehende `verify_continuity.py` besteht Steuerungs-/Treppen-
und Rückkehrprüfungen, meldet aber eine abweichende Ambient-Audio-Erwartung
(`ambient-inside6`). Kein vollständiges Bestehen dieser breiten Suite behaupten.
Fehlende Vibration-/Effekt-/Morphdienste und sonstige Portierungslücken bleiben
sichtbar. Kein vollständiger Gameplay-, Hardware- oder Durchspielnachweis.

Logs, alte Gegenproben und Zustandsberichte:
`build/validation/event-freeze-20261008`. Starterbackup:
`archived/releases/event-freeze-before-20261008`.

Die neue Lifecycle-Datengruppe besteht auch ASan/UBSan (macOS ohne
LeakSanitizer). Beide Starter-Binaries aktualisiert; Mac-Runtime/SDL nur arm64,
Mac-App neu signiert und streng verifiziert. Paketdaten-Lifecycle und
Paket-Runtime-Replay bestanden; zehn erfasste Benutzer-INI-/Snapshot-Dateien
per SHA-256 unverändert. Kein Commit/Push bei parallel laufender Portarbeit.
