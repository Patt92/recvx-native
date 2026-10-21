# 03 — Ergebnis: Motion, Pose und Modellmatrizen

## Portierungslücke, die diese Aufgabe aufgedeckt hat

Der Entwurf `native/en07_bat.c` deklarierte Animationsschlüssel mit falschen
Größen. Das Original verwendet **6 Byte je Winkelschlüssel** und **12 Byte je
Positionsschlüssel**. Die Dummy-Motions des Entwurfs konnten die originale
Routine daher nie korrekt speisen. Der Entwurf ist archiviert
(`archived/en07-bat-draft-20261008`).

## Native Zeigerrealisierung statt PS2-Adressraum

`native/core_actor_assets.c` überträgt `binfunc.c:4/198` nativ: die
zeigerführenden Datensätze (`ML_WORK`, `NJS_CNK_OBJECT`-Baum, `MN_WORK`) werden
in Hoststrukturen aufgefaltet, während die gepackten Key-/Chunk-Ströme ihre
**originalen Elementgrößen** behalten. Die RDT-Bytes bleiben unverändert; der
Test prüft das (`room == immutable`).

| Original | Nativ |
| --- | --- |
| `binfunc.c:4` Modellzeiger | `core_actor_model_realize` → `ML_WORK` |
| `binfunc.c:198` Motionzeiger | `core_actor_motion_realize` → `MN_WORK` |
| `Motion.c` `bhSetMotion` | unverändert aus `recvx_original_engine` |
| `MdlPut.c:240-310` `bhCalcModel`/`bhCalcTree` | `native/core_model.c` |

## Ursache der zunächst statischen Pose

Im ersten Prüflauf schritten zwar die Frames fort, die Gelenke bewegten sich
aber nicht: Es fehlte die originale Initialisierung der Sinustabelle
(`ps2_dummy.c:193` `_Make_SinTable`). Ohne sie liefern `njSin`/`njCos` 0 und
jede Rotation kollabiert die Matrix. `core_enemy_world_create` führt die
Startfolge jetzt in Originalreihenfolge aus.

Derselbe Fehler lag in `tests/native/core_model_test.cpp` (CTest
`core_model_matrices`) und ließ ihn **schon vor dieser Arbeit** scheitern.
Test, Quelle und CMake-Ziel waren in dieser Sitzung unverändert; der Test ruft
jetzt wie das Original zuerst `_Make_SinTable()`.

## Belege

`recvx-core-actor-assets-tests` (CTest `core_actor_assets`) gegen die echte
`data/rooms/RM_0130.RDT`:

- 14 Knoten, erhaltene Kind-/Geschwisterverkettung, erhaltene Mesh-Zeiger;
- verkürzte MTN-Blöcke werden abgelehnt (`core_actor_motion_realize(..., 15) == NULL`);
- `bhSetMotion` über einen vollen Clipdurchlauf: Framewrap und Rückgabewert
  entsprechen dem Original (`(clock>>16)+1 == frm_num → 1`);
- die Modellmatrizen ändern sich dabei nachweisbar und bleiben endlich;
- Übergangsinterpolation: `hokan_count` zählt 5→0 wie im Original;
- der gepackte Schlüssel an Knoten 3 ergibt Winkel 17407.

Laufzeitseite in `tests/native/en07_runtime_test.cpp`: Nach 30 produktiven
`RoomWorld::tick()` haben sich die Gelenkmatrizen des Works gegenüber dem
ersten Pass verändert, und die Motionuhr steht nicht mehr auf 0.

## Grenzen

Gesichts-/Lippenmorphs (`MASK`-Sätze) und `npCalcSkinFM` sind an dieser Stelle
nicht Gegenstand; EN07 besitzt keinen MASK-Block.
