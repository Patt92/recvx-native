# 04 — Ergebnis: EN07-Kollision gegen gemeinsame World-Geometrie

Es gibt keine „fliegend heißt keine Kollision"-Vereinfachung. Alle von EN07
erreichten Abfragen laufen als **unveränderte Originalroutinen** gegen die
ATR-Tabellen des geladenen Raums.

## Datenquelle

`core_enemy_world_create` übernimmt vor dem ersten `bhSetEnemy` die
Originaltabellen aus derselben RDT (`room.c:47` `bhSetRoom`):

| `ROM_WORK` | RDT-Offset | RM_0130 |
| --- | --- | --- |
| `walp` / `wal_n` | `header+0x18` / `+0x98` | Wandflächen |
| `etcp` / `etc_n` | `header+0x1c` / `+0x9c` | ETC-Bereiche |
| `flrp` / `flr_n` | `header+0x20` / `+0xa0` | Bodenbereiche |
| `grand[32]` | `header+0x120` | Etagenhöhen |
| `h` | `header+0x118` | Raumhöhe |

## Erreichte Abfragen und ihre Herkunft

Jede ist ein echtes Archivsymbol, kein Stub:

| Routine | Originaldatei | Objekt im Link |
| --- | --- | --- |
| `bhCollisionCheckLine` | `hitchkl.c` | `hitchkl.c.o` |
| `bhCollisionCheckLine2` | `hitchkl.c:2b8` | `hitchkl.c.o` |
| `bhGetHitCollisionNormal` | `hitchkl.c:3090` | `hitchkl.c.o` |
| `bhCheckPlayer` | `hitchk.c` | `hitchk.c.o` |
| `bhCheckEnemies` | `hitchk.c` | `hitchk.c.o` |
| `bhEne03_Collision` | `en03.c` | `en03.c.o` |
| `bhEne07_CalcAtariOffset` | `en07.c:2318` | `en07.c.o` |
| `bhEne_SetWeponAtr` | `zonzon1.c:591` | `zonzon1.c.o` |

`bhEne07` ruft pro Pass in Originalreihenfolge `bhCheckPlayer`,
`bhCheckEnemies`, `bhEne07_FloorCollision`, `bhEne07_CollisionWalls`,
nochmals `bhEne07_FloorCollision`, dann `bhCalcModel` und
`bhEne_SetWeponAtr(epw, 6, 11, 2.0f)`.

## Messbarer Nachweis am Raum

**Deckensuche beim Spawn.** `bhEne07_Init` (en07.c:246) prüft mit
`bhCollisionCheckLine` eine Linie von der Work-Position 999 Einheiten nach oben
und übernimmt die Trefferhöhe. Die fünf RM_0130-Einträge stehen in der RDT auf
`y = 26.0`; nach dem ersten Kontrollpass stehen sie auf **`y = 27.0`**. Das ist
der echte Treffer an der Gewölbedecke, keine gesetzte Konstante. Der
Laufzeittest prüft genau diesen Wert.

**Wandantwort.** Im produktiven Lauf (`build/validation/en07-runtime-20261008/rm0130.json`)
weichen zwei Works von ihrer RDT-Position ab:

| Index | RDT z | nach `bhEne07_CollisionWalls` |
| --- | --- | --- |
| 0 | 93.17194 | 92.5 |
| 1 | 86.96921 | 87.49 |
| 4 | 93.61502 | 92.5 |

Das ist die Rückschiebung aus `bhEne03_Collision` gegen die echten Wandflächen
des Raums; die übrigen beiden hängen frei und bleiben unverändert.

**Etagenzuordnung.** `bhSetEnemy` ruft `bhSetFloorNum`. Die RDT trägt `flr_no 3`
ein, das Original berechnet aus `grand[8] == 26` bei `y = 26` jedoch **Floor 6**.
Der Komponententest fordert diesen Originalwert, nicht den Rohwert der Datei.

## Offen

`bhEne07_CheckHangingWall`, `bhEne07_AvoidWall` und `bhEne07_ObjEdge` werden von
den im Test erreichten Zuständen (MV00 Ruhe, MV01 Abflug, MV02 Flug) nicht
angelaufen. Sie sind gelinkt und unverändert, aber nicht durch einen Lauf
belegt.
