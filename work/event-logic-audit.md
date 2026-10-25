# Event-Logikaudit – Arbeitsstand

## Umfang und Evidenz

- Statischer Längen-/Grenzlauf: 205 RDT-Dateien, 324080 Befehle, keine unbekannten Längen und keine falsch ausgerichteten Skriptenden. `build/validation/event-logic-audit/script-walk.json`. Kein Kontrollfluss-/Erreichbarkeits- oder Gameplaynachweis.
- Originalvergleich: Event_init/Event_exec, ScenarioCheck/Scheduler, IF/ELSE, FOR/SLEEP/WHILE, CineSet und Spielerpass-Gates anhand `src/ps2/veronica/prog/event.c`, `player.c:1365/1664–1682`. Kein vollständiges Audit aller Opcode-Seiteneffekte oder aller Eventpfade.
- Nach Ergänzung der ursprünglichen Bodenprüfung wurde derselbe fehlende äußere Spielerpass-Gate an `RoomWorld::check_player_mirror_floor` gefunden. Original: `player.c:1365` umschließt auch `:1666–1674`. Hidden/Taskpause müssen mdflg 0x40 erhalten. Korrektur und Regression in `tests/native/mirror_pass_test.cpp`; frisch gebaut vor Korrektur rot (`mirror-real-before.log`), danach grün.
- Acht frisch gebaute gezielte Tests grün: `final-tests.log` (Lifecycle, Scheduler, Längen, Spielerbindungen, Kamera-Hide, Spieler-Events, Körper/Boden, Spiegel).
- Runtime: `tools/verify_event_freeze.py`, Treppe hoch/runter, Hundetrigger und anschließender Ausgang grün, diagnostischer Snapshot/Positionen, kein natürliches Durchspielen (`freeze.log`).

## Offene Befunde / Grenzen

- `tools/verify_events.py`: cemetery-walk erwartet [140,0,90], erhält [139.463,0,89.9818], keine blockierten Befehle. Gegenprobe mit tatsächlich zurückgenommener Spiegeländerung und neu gebauter Runtime identisch: `cemetery-counter.json`. Erwartung nicht abgeschwächt; Ursache noch ungeklärt. Suite bricht dort ab, spätere Prüfungen nicht ausgeführt.
- Buildcache hatte BUILD_TESTING=OFF und alte Testprogramme: erste grüne Läufe waren keine Verifikation neuer Tests. Cache auf ON gestellt, relevante Ziele frisch gebaut; nur `final-tests.log` als finale Testevidenz verwenden.
- Sämtliche erreichbaren Events inklusive Skip, Abbruch, Inventarvarianten und Raumwechsel sind noch nicht dynamisch geprüft. Statischer Scan ersetzt das nicht. Audit unvollständig.
- Installierte App/Paket nicht ersetzt; kein Windows-/Controller-/Vollspielnachweis.
