# Geskriptete Dateiaufnahme: Abschluss (2026-10-08)

Originalfundstelle: `src/ps2/veronica/prog/sub1.c`, `ItemTaskCheck`,
Zeilen 3116–3125. Beim Ende des Status-Tasks werden sowohl `cb_flg 0x10`
(GetItem) als auch `0x20000` (GetFile) gelöscht. `StatusMain` priorisiert
GetFile, wenn beide gesetzt sind. `fileview.c` GetFile / FileNumberSwitch
ordnet das Originalitem 48 in Raum 007 der Datei 8 zu, Besitz in `itm[383]`.

Der Port löschte in `EventVm::finish_file` nur `0x20000`. Originales RM_0070
scd1 setzt für Interaktion 7 beide Anforderungen. Nach dem Schließen der
Leseansicht blieb `0x10` stehen und `service_item_request` startete GetItem.
Repro vor Änderung: Datei 8 besessen **und** `pip[3]=0x00300001`, Item 48,
nach Bestätigung der zweiten Aufnahme. Keine Änderung des Raumskripts.

Gemeinsamer Abschluss löscht jetzt beide originalen Aufnahmeflags.
Neue Regression im Gameplay-Binding-Test führt das echte Szenarioskript aus,
prüft Datei 8, ursprüngliche gemeinsame Flags, ACK/it_flg 97, Reader-Cancel,
keine zweite GetItem-Anforderung und unveränderte Inventarslots. Der Test
scheiterte vor Korrektur auf dem übrig gebliebenen Flag. Vorher/Nachher-
Runtime-Replay: `tools/verify_file_pickup.py`; Spieleraufstellung ist eine
Diagnosehilfe, keine natürliche Weg-/Durchspielprüfung. Direkte RM_6040-
Memoaufnahme bleibt zusätzlich im selben Test abgedeckt.

Mac arm64: gezielter Test, Runtime-Replay und 39/39 CTest-Gruppen bestanden.
Windows x64 Runtime und gezielter Test crossgebaut; kein Windows-Lauf.
Paket-Replay mit eigenen Paketdaten bestanden. Beide Starter-Runtimes aktualisiert, Mac-App signiert; Runtime/SDL arm64.
Zehn Benutzer-INI-/Snapshot-Dateien per SHA-256 unverändert.
Logs: `build/validation/file-pickup-20261008`.
Backup: `archived/releases/file-pickup-before-20261008`.

Bestehende fehlerhafte Inventareinträge werden nicht automatisch umgewandelt:
Originale Item-IDs werden für unterschiedliche Dateien wiederverwendet; ohne
Aufnahmekontext ist die Zuordnung nicht eindeutig. Das Debugmenü kann weiterhin
beliebige Item-IDs direkt in Slots einfügen. Keine vollständige Inventar-/
Durchspiel-/Windows-Hardware-Zusage. Kein Commit/Push.
