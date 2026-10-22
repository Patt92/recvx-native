# native/enemies — Gegnerroutinen

Hier liegen die nativen Gegnermodule. Angelegt am 2026-10-10, als `native/`
flach wurde und 43 Gegnerdateien darin standen. Reiner Ordnerumzug: keine
Datei wurde inhaltlich verändert, keine Logik angefasst.

## Inhalt

| Gruppe | Dateien | Bedeutung |
|---|---|---|
| `enemy_work.*` | 2 | Gemeinsamer Feld-/Dienstvertrag (`RECVX_ENEMY_WORK`, `RECVX_ENEMY_SERVICES`). Verbindlich für alle Gegnermodule, Teilaufgabe T1/C02. |
| `enemy_dispatch.*` | 2 | Verteilung auf `bhJumpEnemy[id]` (eneset.c). |
| `enemy_spawn.*` | 2 | Raum-/Eventspawn und Fallenaktivierung (C01). Im Build. |
| `en01_damage.*` | 2 | Trefferreaktion/Tod EN01. Im Build. |
| `en01_core.*`, `en01_tables.*`, `en01_work.h` | 5 | EN01-Familie (C03). |
| `en00`, `en03_*`, `en04.h`, `en07`, `en08`, `en10`, `en16`, `en17.h`, `en20`, `en24`, `en25`, `en54`, `en55`, `en56`, `en71` | 28 | Einzelgegner, Teilaufgaben T3/T4. |

Stand des Builds (C02, 2026-10-10): `enemy_work.c`, `enemy_dispatch.c`,
`en00/07/08/10/16/20/24/25/56/71.c` sowie `en03_lib.c`/`en03_collision.c`
(Dienst `collision_works`) sind in `recvx_game` und ueber
`native/world_enemies.cpp` an RoomWorld angeschlossen. Nicht gebaut:
`en54.c`/`en55.c` (brauchen das fehlende `en17.c`), `en01_core.c`/
`en01_tables.c` (C03, unvollstaendig). Angeschlossen heisst nicht fertig:
offene Dienste stehen in `archived/work/native_port_C02_enemy_work.result.md`.

## Include-Pfad

`native/enemies` wird in `CMakeLists.txt` zusammen mit `native` öffentlich
weitergegeben (`target_include_directories(recvx_data PUBLIC native
native/enemies)`). Deshalb bleiben die kurzen Namen gültig und mussten in
keiner Quelldatei geändert werden:

```c
#include "enemy_work.h"   /* aus native/subplayer.c, player_combat_core.h */
#include "enemy_spawn.h"  /* aus native/world.h */
#include "en01_damage.h"  /* aus native/world.h */
```

Neue Gegnerdateien gehören in diesen Ordner und dürfen sich untereinander
genauso kurz einbinden.

## Regeln

Es gelten [AGENTS.md](../../AGENTS.md) und [QUALITY.md](../../QUALITY.md).
Der [damalige Gegnerauftrag](../../archived/work/history/2026-10-21-enemy-assignment/THIRD_AGENT_C01_TASK.md)
ist nur Historie, keine aktuelle Arbeits- oder Dateibesitzvorgabe:
Originalquellen unter `src/ps2/veronica/prog/` sind ausschließlich
Semantikreferenz, kein Originalkern und kein Generator; einziger Besitzer der
Gegnerzustände bleibt die vorhandene WorldEntity/RoomWorld-Quelle, kein
zweites `sys`/`plp`/`rom`/`ene`/`eff` und keine `BH_PWORK`-Spiegeltabelle;
ungebundene Dienste werden über `gap` mit dem Originalnamen gemeldet statt
durch geratene Werte ersetzt. `archived/FORBIDDEN_DONTUSE` bleibt gesperrt.

Dateibesitz je Teilaufgabe steht in `archived/work/subagent_briefings_enemies.md`.
