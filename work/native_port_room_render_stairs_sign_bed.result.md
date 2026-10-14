# room_render_stairs_sign_bed — Treppen RM_0010/RM_0020, Schild + Laser RM_0090, Bett RM_2050

Arbeitsbaum, nicht installiert. Keine Raum-Sonderfälle. `check_no_original_core.py` PASS.
Belege: `build/validation/room_render_stairs_sign_bed-20261014/`.

## Behoben: RM_0090 3D-Laser blockierte an Opcode 0xc0 (bhCyodanSetEx)

Fundstelle Original: `event.c:8307 bhCyodanSetEx` (28 Byte). RM_0090 **Event 23** ist der
3D-Laser: `0xad bhEffTypeSet` auf die Effekte 0x35/0x36/0x37, danach zwei Datensätze
0xc0. `EventVm::instruction` hatte keinen Fall für 0xc0 → `default:` warf
„unimplemented opcode 0xc0“, die Task blockierte dauerhaft (Audit-Bericht, Abschnitt A).

Gemessen (Standalone-EventVm/RoomWorld, Raumbytes unverändert), `laser-event-23-proof.txt`:
- vorher: `event23 blocked after 56 ticks: offset 0x20fa (opcode 0xc0): unimplemented opcode 0xc0`
- nachher: `blocked=0 finished=1 ticks=297 pending_0xc0=4`

Länge 28 belegt aus den Daten (zwei benachbarte 0xc0-Sätze 28 Byte auseinander), nicht geraten.

Fix `native/events.cpp`: `case 0xc0:world(t,op,28);return 1;`. Die Cyodan-Arbeit selbst
(`bhSetEffParticleMk2` → Effekt id 302 / `PtclTbl`, `effsub3.c:1008`) hat noch keinen nativen
Besitzer (302 nicht in `hand_ported_effect_sprite`). Sie bleibt daher – genau wie
`0x6d bhCyodanSet` (Audit Kategorie C) – eine **benannte Lücke/Pending-Bindung**, statt das
ganze Event einzufrieren. Kein Ersatz-Code.

Fester Test: `tests/native/event_lifecycle_test.cpp` startet RM_0090 Event 23 und prüft
kein Block + Evt-Ende + gemeldete 0xc0-Pending-Arbeit. `ctest -R event_lifecycle` grün.

## Treppen RM_0010 / RM_0020 — Mechanismus belegt, keine Abweichung im Maschinenkern

- `native/scene.cpp:67` liest `camera.hidden_objects[16]` aus `CAM_WRK.hidobj` (cam+0x58,
  `types.h:1246`). `renderer.cpp:139` blenden je Dreieck aus (`camera_mask && object<512`).
  Original: `bhSetHideObjLgt`/`bhSetCut` setzen `rom->mdl.objP[i]->evalflags |= 0x8`
  (`cut.c:324/585`, `NJS_CNK_OBJECT.evalflags` Bit 8 = HIDE); `scene.cpp:226` überspringt
  Objekte mit Bit 8. Schlüsselraum (Modellobjektindex) ist konsistent (`rom->mdl.objP[i]`
  ≡ Knotenindex der 52-Byte-Tabelle). Maske greift korrekt im Zeichenpass.
- Schnitt-Hide-Listen `objitm.c:659` → `RoomWorld::object_cut_hidden` (`world.cpp:2247`)
  sind als **Whitelist** gebunden (nur in `hide[]` genannte Schnitte wird gezeichnet), siehe
  `tests/native/object_cut_test.cpp` (RM_0090 Objekt 20).
- **RM_0010 hat keine einzige gesetzte Objektmaske** (alle `hidobj` = 0, `cuts-0010-0020.txt`)
  und nur ein gesperrtes Raumobjekt (OBJ 4 id 2 `hide=[1]`, nur in Schnitt 1). Die im Bild
  mehrfach sichtbare Treppe (Frames `wa_90` Schnitt 2 / `wa_150` Schnitt 1, montiert) ist
  also Stage-Geometrie, die in beiden Schnitten sichtbar ist. Sie kann hier weder durch
  `hidden_objects` noch durch `object_cut_hidden` verschwinden.
- Der Clip dieser Schnitte ist unkritisch (RM_0010 Raumtabelle `rm0`: Schnitt 0 near 1.5 far
  20000, Schnitt 1 near 1.01 far 20000; `native/view_clip.cpp`).
- **Bleibt offen (belegte Abweichung):** die Schnitt-Umschaltung. Original `bhCheckCut`
  (`cut.c:108-127`) schaltet `cam.ncut` erst, wenn der Spieler **vollständig** im neuen
  Bereich steht (vier ±1-Sonden `p0..p3` gegen den alten Schnitt). Nativ fehlt diese
  Hysterese: `runtime.cpp:312 follow()` und `scene.cpp:398 camera_for_position` wechseln
  schon, sobald der Mittelpunkt den neuen Bereich betritt. Das verschiebt Zeitpunkt von
  `bhSetCut` (Maske/Clip/Fog) und damit den abrupten Geometrie-Wechsel beim Treppensteigen um
  bis zu ~1 Einheit – die vom Nutzer beschriebene Abgehacktheit. Zusätzlich fehlt die
  Per-Modell-Sphären-Clip `njControl3D` 0x100 (`ps2_NinjaCnk.c:686`, von `bhAllDrawModel`
  mit `njControl3D(0x2500)` aktiviert), die ganze Modellobjekte gegen `njClipZ` verwirft; der
  Renderer clippt nur je Dreieck (`renderer.cpp:146`). Keine der beiden Änderungen ohne
  Original-Vergleich eingebaut (Policy: keine geratene Semantik).

## Schild RM_0090 — Objekt bestimmt, rendert aber; Ursache noch offen

- Das Schild = `id 786` (zwei gespiegelte Objekte, je ein 2-Dreieck-Texturquad, y=25.6):
  - **OBJ 19** (77.2,25.6,2.0), `flg 0x00000001`, `hide=[255,255,255,255]` → **ohne**
    0x40000000, also `object_cut_hidden` = false ⇒ immer gezeichnet.
  - **OBJ 20** (77.1,25.6,138.1), `flg 0xC0000001`, `hide=[7,255,255,255]` → nur in Schnitt 7
    gezeichnet (Whitelist). `object_cut_test.cpp` zählt genau diese 2 Dreiecke.
- Beide Modelle laden im nativen Dekoder (Standalone-Dump: `object_count=1, tris=2,
  textured=2`). Die Maske ist also nicht die Ursache. Restkandidaten laut Auftrag bleiben
  Textur-Alpha/Blend des Schildquads bzw. ein `njControl3D`-Sonderpfad; ohne
  Original-Vergleichsbild ist nicht entscheidbar, welcher Schnitt/welche Textur fehlt.
  **Offen.** (Das im Kamera-4- und Kamera-1-Bild oben an der Tür sichtbare Plättchen mit
  rotem Streifen ist gerendert.)

## Bett RM_2050 — Raum/Objekte erfasst, Event noch nicht lokalisiert

- RM_2050 = Residenz-Schlafzimmer (Kamera 3/5 zeigen das Bett). Objekte u. a. `OBJ 5 id 206
  type 11` (50,-20,60) und `OBJ 7 id 204` (25,0,60); ETC Typ 2 „dansa“-Stufen bei
  (34,0,39)/(39,0,34) und (34,9,34). `type 11 = bhObj011` (`objitm.c:2066`) setzt nur
  `mdflg 0x8` ⇒ Zeichnung über `njControl3D(0x2500)` (Per-Modell-Clip, s. o.).
- Die „Bettanimation“ ist ein Event; welcher Befehl/welche Modelle (mdl[n]/Morph/Textur)
  beteiligt sind und was fehlt, ist noch **nicht bestimmt** (Event-Skriptauszug +
  Triggerplatzierung nötig). Offen.

## Prüfung / Nachbarstände

- `./build.sh --test` (Sperre, `room-render`): 89/90 grün. Meine Änderung:
  `event_lifecycle_control_release` grün inkl. neuem RM_0090-Event-23-Block.
- Roter Nachbar **`inventory_events_assets`**: Meldung „bhRandTest/bhRandamSet did not share
  the original EE RNG and task result“. Das ist unabhängig von meiner 0xc0-Änderung –
  betroffen ist `bhRandamSet`/`bhRandTest` (`events.cpp` `tasks_[0].wpnl_no`) aus der
  laufenden Fremdarbeit (Audit-Datei `work/native_port_event_parser_audit.result.md`), nicht
  der Opcode-Dispatch. Eigener echter Vorherstand (Standalone ohne Fix) grün, mit Fix
  identisch rot/grün → nicht meine Ursache. Build schlug dadurch mit Exit 1 ab;
  Runtime/Libs wurden kompiliert.
- Standalone-Harness `build/validation/room_render_stairs_sign_bed-20261014/laser23-harness.cpp`.

## Geänderte Dateien

- `native/events.cpp` (case 0xc0, 7 Zeilen + Kommentar)
- `tests/native/event_lifecycle_test.cpp` (RM_0090-Event-23-Regression)

## Ledger-Vorschläge (TAB)

```
event.c:bhCyodanSetEx	partial	event.c:8307; Opcode 0xc0 (28 Byte, aus RM_0090 ev23 belegt) als world-Bindung gefuehrt statt Block; die Cyodan-Arbeit bhSetEffParticleMk2/id 302 bleibt benannte Pending-Luecke (wie 0x6d). Test RM_0090 ev23 in tests/native/event_lifecycle_test.cpp
```
