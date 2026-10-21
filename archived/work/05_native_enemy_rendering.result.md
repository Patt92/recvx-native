# 05 — Ergebnis: Vollständige native EN07-Darstellung

Die Gelenkzustände aus Aufgabe 03 gehen ohne zweites Sampling in den nativen
Renderer. Kein Decoder-Proxy, keine Teilgeometrie, kein PS2-Renderer.

## Weg der Daten

1. `bhEne07` ruft am Ende jedes Passes `bhCalcModel` → `bhCalcTree`
   (`MdlPut.c:240-310`, nativ in `native/core_model.c`). Ergebnis sind die
   `O_WORK.mtx`-Weltmatrizen der 14 Knoten.
2. `RoomWorld::capture_core_poses` liest pro Knoten Position, Winkel, Scale,
   `evalflags` und die fertige Matrix über `core_enemy_world_node`.
3. `PlayerAsset::sample_work` setzt diese Matrizen als
   `ModelGeometryOptions::world_matrices` ein. `scene.cpp` übernimmt sie an
   Stelle der eigenen Knotentransformation; es wird **keine** Motion erneut
   abgetastet und keine zweite Pose gebaut.
4. `decode_model_asset` erzeugt daraus die Chunk-Geometrie mit den
   Originaltexturen des Raums; `append_actor` hängt sie an die Szene.

## Originales Zeichengate

`eneset.c:807-814` ist portiert, nicht ersetzt:

- `core_enemy_world_visible` prüft `flg & 1`, `!(flg & 0x80000000)`,
  `!(stflg & 0x41000000)`, `(st_flg & 0x20) || !(mdflg & 0x200)`, die
  verknüpfte Work (`flg & 0x80`), ruft bei `mdflg & 4` `bhCalcModel` und
  verlangt `!(mdflg & 1)` sowie `objP != NULL`.
- `core_enemy_world_clipped` ist das originale `bhCheckClipModel`
  (`pwksub.c:2465`) mit `mdflg & 0x20` als Umgehung.

Dafür braucht das Original `cam.mtx` und die Ninja-Bildschirmdaten.
`RoomWorld::append` baut die Sichtmatrix in derselben Reihenfolge wie
`bhControlCamera`/der Renderer und übergibt sie mit der Perspektive an
`core_enemy_world_set_camera`. `native/core_view.c` enthält jetzt nativ
`njSetScreen`, `njSetAspect`, `njSetPerspective`, `njSetScreenProjection`,
`njViewScreenMatrix`, `njProjectScreen` (`ps2_NaView.c:27/55/69/81/217`,
`ps2_NaMatrix.c:1572`) und `core_view_init_screen` mit den Werten aus
`system.c:419-442` (640×480, Mitte 320/240, Distanz 500, Aspekt 1,174/1,0).

## Nachweis

`tests/native/en07_runtime_test.cpp` rendert über die normale
`RoomWorld::append`-Kette durch jede Raumkamera und vergleicht gegen den
originalen `bhEneSetCk`-Zustand (`stflg |= 0x1000000`), der genau diese Works
entfernt:

```
RM_0130 runtime: 5 EN07 works, pose 14 joints,
cut 1 renders 2898 triangles of which 2490 are the bats
```

Produktiver Lauf mit Bild:

```
recvx-runtime --data data --room RM_0130.RDX --in-game --spawn 1 \
  --replay tests/native/replays/rm0130-bats.txt --frames 130
```

`build/validation/en07-runtime-20261008/bats.png` zeigt vier der fünf
Fledermäuse texturiert an der Gewölbedecke, während die Spielerin darunter
hindurchgeht.

## Grenzen

Der Schattenwurf, den `bhEne07_Init` über `bhSetShadow(SdwTab, …)` als
Effektwork mit `mdlver 1` anlegt, wird erzeugt, aber noch nicht gezeichnet.
Spiegelglanz und die originale OT-Sortierung des Chunkpfads sind unverändert
offen.
