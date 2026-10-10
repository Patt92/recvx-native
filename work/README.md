# Native EN07 / Fledermaus-Port: Arbeitszerlegung

Ziel ist kein isolierter Gegner-Test und kein PS2-Kern.  EN07 muss im
produktiven nativen `RoomWorld`-Ablauf aus echten RDT-Daten entstehen und mit
Originalinitialisierung, Motion/Pose, KI, Kollision, Darstellung und SFX
laufen.

Die Dateien sind absichtlich in Abhängigkeitsreihenfolge nummeriert.  Mehrere
Aufgaben können parallel untersucht werden; Integration in `world.cpp` beginnt
erst, wenn 01 die echte Rauminstanz festlegt und 02--06 die jeweilige Semantik
nachweisbar liefern.

| Aufgabe | Gegenstand | Voraussetzung |
| --- | --- | --- |
| 01 | RDT-Instanz, Spawn- und Eventablauf belegen | keine |
| 02 | gemeinsame native Enemy-Work-Arena | keine |
| 03 | Motion, Pose und Modellmatrizen | 02 |
| 04 | Kollision und Boden/Wandabfragen | 01, 02 |
| 05 | native Modell-/Materialdarstellung | 03 |
| 06 | SFX, Effekte und Spielerinteraktion | 02, 03 |
| 07 | EN07-Originalcode und Dispatcher integrieren | 02--06 |
| 08 | echter Runtime-Nachweis und Regression | 01, 07 |

Grundregel: Jede Abweichung muss auf Originalquelle oder Disassembly
zurückgeführt werden.  Fehlende Bindungen bleiben als Lücke sichtbar; keine
Nullrückgaben, feste Ersatzpuffer, künstlichen Spawns, Raumsonderfälle oder
„ähnlich“ wirkenden Host-Abläufe einführen.
