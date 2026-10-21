# Native Enemy-Integrationaudit — 2026-10-08

## Nachtrag 2026-10-09: Kette geschlossen

Die unten geforderte Reihenfolge ist durchlaufen. Alle Teilaufgaben haben ein
Ergebnisdokument; `08_runtime_acceptance.result.md` belegt den produktiven Lauf
in RM_0130 (Spawn, Modell, Pose, KI, SFX, Kollision) mit 48/48 CTest.
`native/en07_bat.c` und der ILP32-Originalkern sind archiviert
(`archived/en07-bat-draft-20261008`, `archived/core32-20261008`).
Die Tabelle darunter beschreibt den Stand **vor** dieser Arbeit.

## Ergebnis

Kein vorhandener Work-Strang stellt bereits die produktive EN07-Kette her.
Die akzeptierte Reihenfolge bleibt `01 -> 02 -> 03/04/05/06 -> 07 -> 08`.
Ein Link- oder Unit-Test-Erfolg darf keinen dieser Schritte überspringen.

| Work | Befund | Produktiv nutzbar? | Nächster konkreter Schritt |
| --- | --- | --- | --- |
| 01 | RM_0130 und fünf ID-7-RDT-Instanzen sind aus Originaldaten belegt. | Daten-/Spawnbeleg ja. | Als Runtime-Testraum verwenden. |
| 02 | `core_enemy_arena` enthält originale Arena- und Resetsemantik. | Nein: nicht an RoomWorld-Lebenszyklus gebunden. | Gemeinsamen nativen World-Owner beim Raumladen/-wechsel binden. |
| 03 | Die `.done`-Datei ist leer. | Nein. | Echte EN07-Motiondaten in den gemeinsamen Work und die Pose-/Matrixkette einspeisen. |
| 04 | Nur Auftrag, kein Ergebnis. | Nein. | Originale EN07-Abfragen gegen RoomWorld-ATR-/Bodengeometrie binden. |
| 05 | Nur Auftrag, kein Ergebnis. | Nein. | Fortgeschriebene Gelenke über den normalen Model-/Rendererpfad ausgeben. |
| 06 | Core-Interaktion ist gegen Originalroutinen getestet. | Nein: noch kein Runtime-Host. | Audio, Effekte und Player-Work aus derselben World-Instanz binden. |
| 07 | `native/en07_bat.c` besitzt eine eigene Mock-Welt, feste EXP-Speicher, Dummy-Motions und Null-/No-op-Bindungen. | Nein, ausdrücklich unzulässig. | Erst nach 02--06 den Originalalgorithmus gegen gemeinsame native Works integrieren. |
| 08 | Kein Ergebnis. | Nein. | Erst nach 07 RM_0130 im normalen Raumlauf prüfen. |

## Sofortmaßnahme

`native/en07_bat.c` und sein synthetischer Test bleiben als unfertiges
Referenzmaterial im Arbeitsbaum, sind aber nicht Bestandteil von `recvx_game`
oder der CTest-Abnahme. Die produktive Runtime kann dadurch keine EN07-Mock-
Work mehr verwenden.

## Harte Integrationsregel

Der nächste Code darf keine zweite `BH_PWORK`-/Player-/System-Instanz, keine
festen `exp0`/`exp1`-Blöcke und keine Rückgabe-Nullstubs einführen. Jede
EN07-Abhängigkeit wird entweder aus dem gemeinsamen nativen RoomWorld-Zustand
mit Originalsemantik gebunden oder bleibt sichtbar offen.
