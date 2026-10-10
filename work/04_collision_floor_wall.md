# 04 — EN07-Kollision: Spieler, Gegner, Boden und Wände

## Auftrag

Portiere und binde die von EN07 tatsächlich erreichten Abfragen mit gemeinsamer
World-Geometrie: `bhCheckPlayer`, `bhCheckEnemies`, `bhEne07_FloorCollision`,
`bhEne07_CollisionWalls`, `bhCollisionCheckLine`, `bhCollisionCheckLine2`,
`bhCheckWallType`, `bhGetGroundPosition` und `bhGetHitCollisionNormal`.

## Referenzen

- `src/ps2/veronica/prog/en07.c:109-132`, `1831-2156`, `2258-...`.
- `hitchk.c`, `hitchkl.c`, `pwksub.c` und RDT-ATR-Daten.

## Regeln

Keine „fliegend heißt keine Kollision“-Vereinfachung. EN07 benutzt eigene
Hänge-/Lehnwand- und Bodenmodi; die Originalrückgaben müssen seine
Zustandsmaschine speisen. Bestehende Dummy- oder Nullimplementierungen sind
keine zulässige Bindung.

## Abnahme

Dokumentiere pro erreichten Abfragezweig Originalquelle, native Datenquelle und
Testraum. Der Runtime-Test muss eine echte EN07-Bewegung an Boden/Wand zeigen,
ohne Koordinaten oder Modus speziell für einen Raum zu setzen.
