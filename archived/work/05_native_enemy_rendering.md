# 05 — Vollständige native EN07-Darstellung

## Auftrag

Verbinde die aus Aufgabe 03 aktualisierten nativen Gelenk-/Modellzustände mit
dem nativen Renderer. Das Ziel ist das vollständige EN07-Modell aus den echten
Raumressourcen inklusive Material-/Texturbindung, nicht ein Decoder-Proxy,
eine Teilgeometrie oder ein PS2-Renderer.

## Referenzen

- `MdlPut.c` Draw-/Tree-Aufrufpfad und die jeweiligen Ninja-C-Algorithmen.
- vorhandene native Modell-, Morph-, Skinning- und Materialports.
- RDT-Modell-/Texturdaten des in Aufgabe 01 gewählten Raums.

## Abnahme

Ein produktiver Runtime-Screenshot oder deterministischer Frame-Dump desselben
EN07-Actors vor/nach einer Poseänderung zeigt vollständige, texturgebundene
Geometrie und veränderte Vertexpositionen. Der Nachweis muss über die normale
`RoomWorld::append`-/Renderer-Kette gehen.
