# room_render_round2 — Treppen-Tür RM_0010/0020, Schild/Laser RM_0090, Bett RM_2050

Nur Arbeitsbaum + `./build.sh` (Dev-App), nichts installiert/paketiert. `check_no_original_core.py` vor
jedem Build PASS. Belege: `build/validation/room_render_round2-20261014/`.

## 1. Treppe RM_0010/RM_0020 — Ursache bestimmt, Clipregel der Chunk-Streifen portiert

**Wo es passiert:** Die Treppe mit den „nachspringenden Stufen“ ist die Treppen-Türdemo
(MRY 0072, Fall 1), nicht die Raumkamera. `bhSetDoor` setzt für genau diesen Fall
`dmo_clip {dmo_typ 1, prti_no 5, vew_no 4}` → `njClipZ(-10,-34)` (door.c:191-205,
Partition 5 = MRY.AFS sdfunc.c:98/441). Das Treppenmodell steht still (DoorProc 0), nur die Kamera
bewegt sich: ViewProc4 → ViewProc6 (Fahrt z 16→10 und Hochschwenken auf -5461, das ist die
„Winkelkorrektur“) → ViewProc3 (Steigen). Parameter: `stairs_door_parameters.txt`.
Mit far = 34 sind am Anfang nur die Stufen bis Tiefe 34 da; weitere kommen beim Schwenken/Steigen
hinzu. **Das ist Originalverhalten** (dmo_clip), keine Raumabweichung und nicht abschaltbar ohne
Ersatzlogik.

**Belegte Abweichung im Renderer:** Nativ wurde jedes Dreieck exakt an near/far geschnitten. Das
Original schneidet Chunk-Streifen so nicht:
- `njCnkCsUvh`/`njCnkCsUvn` verwerfen ein Paket (≤60 Eckpunkte, Fortsetzung 58 weiter) nur, wenn
  alle Eckpunkte außerhalb **derselben** Ebene liegen (UND der `vclipw`-Bits gegen `ClipMatrix2`,
  ps2_NinjaCnk.c:2017-2104 VU1-Weg, :2240-2272 EE-Weg mit `_CVV`); `njCnkCs` (Typ 64) hat das nicht
  (ps2_NinjaCnk.c:1720).
- Je Dreieck (vu1DrawTriangleStrip* ps2_Vu1Strip.c:943/1057/1178/1420; VU1 `DoubleMain`/`SingleMain`
  im ELF-Mikrocode gleichlautend, `_Double_DisplayAreaPoint_False`): ganz gezeichnet, solange kein
  Eckpunkt den Anzeigebereich verlässt (`_Check_DisplayAreaPoint` ps2_Vu1Strip.c:766: Bild ±2047 um
  Versatz 2048 ps2_NaView.c:39, Sicht-z in [1,65535)) **oder** keiner außerhalb des Clipvolumens
  liegt; sonst Schnitt an sechs Ebenen (`_Check_ScissorPlane` ELF 0x002D3EB0: nah, fern, ±x, ±y).
  Dreiecke jenseits von far im Bild werden also ganz gezeichnet.

**Änderung:** `SceneTriangle::strip_clip/strip_segment` (scene.h), gesetzt im Streifendekoder
(scene.cpp, Paket `(i-2)/58`, prozessweit eindeutige Kennung), Regel in `SoftwareRenderer::draw_view`
nur für den Ninja-Bildschirm aus `draw()` (Spiel-/Türkamera; Sonderansichten unverändert):
`ClipMatrix2` mit ClipVolume 320×240 (ps2_NaView.c:24/233), near ≥ 1 / far ≤ 65535 (njClipZ
ps2_NaView.c:142, Grundwert 65535 ps2_NaView.c:21). Nicht-Streifen (Basic-Modelle, Effekte) behalten
die exakte Klippung.

**Prüfung (echte Vorher/Nachher-Bilder, Diagnose-Aufstellung `stairs_replay.txt`):**
`stairs_door_toprow_before_after.txt`: Oberkante der sichtbaren Treppe vorher/nachher je Bild
identisch oder 1 Zeile (Tritt im Streifschnitt ganz statt angeschnitten); Sprünge bei Bild 160/196/220
bleiben, weil dort ganze Setzstufen-Streifen die Tiefe 34 unterschreiten — so auch im Original.
Bilder angesehen: `stairs_door_v6_before.png`, `stairs_door_pop_after.png`,
`stairs_door_before_after_140_200.png`, `stairs_door_climb_after.png`. Gegenprobe Spielansichten
RM_0010 (3 Replays) und RM_0020: pixelgleich. Fester Test `original_chunk_blend`
(tests/native/chunk_blend_test.cpp): Streifen jenseits far gezeichnet, Nicht-Streifen geschnitten,
Paket ganz jenseits far verworfen, Paket mit einem Innenpunkt gezeichnet.

**Ehrlich:** Der vom Nutzer gesehene Effekt (Stufen erscheinen erst beim Schwenken, ruckweise)
bleibt weitgehend, weil er aus dem originalen far 34 kommt. Ohne Originalvergleichsbild ist nicht
ausgeschlossen, dass noch ein weiterer Unterschied besteht (siehe Lücken: Tiefentransport).

## 2. RM_0090 — Laser behoben, Schild nicht identifiziert

**Laser (behoben):** `0xc0 bhCyodanSetEx` war seit Runde 1 nur „pending“. Inzwischen hat
`bhSetEffParticleMk2` einen nativen Besitzer (`EffectParticles::spawn_mk2`, effsub3.c:1008). Jetzt
in `RoomWorld::bind` portiert (event.c:8307-8461): Farben src/dst aus v0..v3/w0..w3, wps/ps je
u16/100 mit Vorzeichenbits, `WpnTab[v5].hiteff`, w4 1/2 addiert `owP[w6].mtx[12..14]` (fehlender
Knoten → benannte Lücke, kein Ersatzort). RM_0090 Ereignis 23 hat drei Sätze (an obwp[8] Knoten 2,
Waffe 4). Belege: Event-Test erweitert (kein 0xc0 offen, keine Cyodan-Lücke, Partikel > 0) grün;
Standalone-Sonde mit direkt gestartetem Ereignis 23 zeigt die Funken am Lasergerät
(`laser_event23_particles_after_probe.png`; vorher: 0xc0 offen, keine Partikel).

**Schild über den Türen (offen):** Alle Kandidaten über/an den Türen rendern: Objekt 786 (OBJ 19/20,
Lichtschein-Textur), `bhEff122`-Lampen (EFF 38-41, Atlas 41 Palettenbank = type), `bhEff230`-
Anzeigelampen (Atlas 414), Gitter über den Durchgängen (Stufenknoten 3/74). Stufenmasken je Schnitt
geprüft (keine Tür-Oberseite fälschlich ausgeblendet). Ein Schild-Bild mit Schrift („◀ HARRIER ▼“)
liegt im Atlas 41/414, wird aber von keiner Prozedur dieses Raums adressiert (bhEff122 nutzt nur
UV 0..0.125). Ohne Bildposition/Screenshot des fehlenden Schilds keine belegte Ursache → kein Eingriff.
Bitte Kameraposition oder Vergleichsbild nennen.

## 3. RM_2050 Bett — Ablauf vollständig gebunden, kein Defekt reproduziert

Bett-Ereignis = **Ereignis 2** (AOT 4/5): `bhCommonCtr` öffnet zuerst OBJ 4 (id 208), dann hebt
`0x69 1a/1e` den Baldachin OBJ 5 (id 206) in 100 Bildern von y −19.7 auf 0 und gibt das Bett frei;
alle Befehle gebunden außer `0x86 bhEasySESet` (Ton, nicht Grafik). Sonde mit Raumnamen rendert den
Ablauf sauber (`bed_event2_canopy_probe.png`, `bed_event2_cut1_probe.png`). Ohne Raumnamen zeigte
die Sonde falsche Figurentexturen — Ursache war die Sonde (Raumkennung leer), nicht das Spiel.
Im Runtime-Start ist der Baldachin wie im Original abgesenkt (Leiter darüber). Ohne Angabe, was
fehlt/falsch ist, keine Änderung.

## Verbleibende Lücken (benannt)

- Rückseitenwurf `fNz` der Single-Funktionen (ps2_Vu1Strip.c:939/1174, njCnkSimple*, MdlPut.c/effsub)
  nicht portiert; `SceneTriangle` kennt den Zeichenmodus nicht.
- `vu1DrawTriangleStripTransDoubleI` (Modus |4, ps2_NinjaCnk.c:2741) schneidet mit fünf Ebenen ohne
  fern (`ScissorTriangle`) — nicht unterschieden.
- GS-Tiefentransport für Streifen (`Ps2_zbuff_a/b`-Klemmung, PSMZ16S, GEQUAL) fehlt; Renderer nutzt
  float 1/z, strikt größer. Jenseits-far-Geometrie wird daher echt tiefensortiert statt geklemmt.
- `bhChangeClipVolume`/`_Make_ClipVolume` (event.c:13643, ps2_NaView.c:298) ohne Besitzer; fest 320×240.
- `cam.camver` (cut.c:300-313, cam[1] bei Flächenattribut 0x8000) nicht portiert; Szene liest nur
  cam[0] (in RM_0010/0020/0090/2050 kein 0x8000).

## Nachbarstände

- Build 1 und 2 (Renderer, dann 0xc0): 91/91 grün. Build 3 (nur zusätzlicher chunk_blend-Test): 90/91;
  rot `typewriter_save_slots` „relativer Dev-Datenpfad verliert die Git-Wurzel“ —
  tests/native/save_slots_test.cpp wurde nach Build 2 von fremder Arbeit geändert (Benutzerdatenpfad),
  meine Dateien seit Build 2 nur chunk_blend_test.cpp. Nicht meine Ursache.
- Paralleländerung in scene.h (`ninja_primitive`/`ninja_screen_reject`, Effekt-Clip eines anderen
  Agenten) koexistiert mit `strip_clip`; renderer.cpp hatte beim Abschluss noch keine fremde Änderung.

## Geänderte Dateien

- native/scene.h, native/scene.cpp (Streifenkennung), native/renderer.h, native/renderer.cpp (Clipregel)
- native/world.cpp (`0xc0` in `RoomWorld::bind`), native/events.cpp (Kommentar 0xc0)
- tests/native/event_lifecycle_test.cpp, tests/native/chunk_blend_test.cpp

## Ledger-Vorschläge (TAB)

```
ps2_Vu1Strip.c:vu1DrawTriangleStripOpaqueDouble	partial	Clipregel (Anzeigebereich ps2_Vu1Strip.c:766, Schnitt nur bei Verlassen + Clipvolumen, sechs Ebenen ELF 0x002D3EB0) in renderer.cpp fuer SceneTriangle::strip_clip; GS-Tiefenklemmung offen; Test original_chunk_blend
ps2_Vu1Strip.c:vu1DrawTriangleStripOpaqueSingle	partial	wie Double; fNz-Rueckseitenwurf (:939) offen
ps2_NinjaCnk.c:njCnkCsUvh	partial	Paketverwurf (UND der vclipw-Bits je <=60 Eckpunkte, :2017-2104/:2240-2272) als strip_segment; Rest des Streifenpfads wie bisher
event.c:bhCyodanSetEx	done	event.c:8307-8461 in RoomWorld::bind: 28 Byte, Farben, wps/ps, WpnTab hiteff, owP-Link w4 1/2, bhSetEffParticleMk2 ueber EffectParticles::spawn_mk2; Test RM_0090 ev23 (Partikel > 0, kein 0xc0 offen)
```
