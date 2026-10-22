# Regeln für jeden Gegner-Subagenten (verbindlich)

1. Vor Arbeit `AGENTS.md` (Abschnitt „Verbindliche Architekturvorgabe“), `QUALITY.md`, `archived/work/subagent_briefings_enemies.md` (Vertrag `native/enemies/enemy_work.h`) lesen; `git status --short`; `python3 tools/check_no_original_core.py`.
2. `archived/FORBIDDEN_DONTUSE` nie lesen oder nutzen. `src/ps2/veronica/prog/*` ist nur Semantikreferenz: keine Funktionskörper kopieren, generieren, kompilieren oder linken.
3. Ein Gegner je Agent. Dateibesitz exklusiv: `native/enemies/enNN*`, `tests/native/enNN_test.cpp`. Gemeinsame Dateien (`enemy_dispatch.*`, `enemy_work.*`, `world*.cpp`, `runtime.cpp`, `CMakeLists.txt`) nur als Patchvorschlag im Bericht.
4. Dienste über `RECVX_ENEMY_SERVICES`; fehlender Dienst = `gap(name)`, nie ein Ersatzwert. Keine Heuristik, keine Raum-/Gegnerregel, keine abgeschwächten Tests.
5. Originalquirks unverändert übernehmen und mit Datei:Zeile kommentieren; Unklares per Disassembly klären oder als Lücke berichten.
6. Eigener Buildordner `build/enNN-<agent>`; keine parallelen Builds im selben Verzeichnis. Kein Commit, Push, Paket.
7. Bericht `work/enemies/ENNN.result.md` und Ledgerzeilen.
