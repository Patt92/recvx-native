# Hubschrauber-Explosion RM_0020: Flackern im oberen Bildteil

Subagent `event-effects`, Thema `heli_flicker`, 2026-10-14 (zweite Runde nach
`archived/work/native_port_event_effects_heli_steve.result.md`). Kein Commit/Push/Paket.
Belege: `build/validation/heli_flicker-20261014/`.

## Kurzfassung

- **Ursache des Flackerns gefunden und nach Original behoben (Renderer):** Der
  native Renderer schnitt die eff[]-Effektsaetze an der njClipZ-Nahebene der
  Kamera ab (Eventkamera 12 der Explosion: `clip_near 9`). Das Original klippt
  Effektaufrufe nie an njClipZ: `njDrawTextureSub3D` (ps2_NaDraw.c:302) verwirft
  nur bei `scr.z < 0`, `Ps2AddPrim3D` (ps2_dummy.c:486) laesst einzelne
  Streifendreiecke ueber das GS-Fenster (vclipw gegen 2047) fallen. Folge nativ:
  die Rauchwelle `bhEff119` Art 0 (alle vier Passes aus `bhEff103` Art 1, Raum-
  effekt 0x27) verschwand ein Bild zu frueh (z 8,5 < 9) -> heller Blitz im
  oberen Bildteil im Vier-Bild-Takt; ausserdem zeichnete der Host grosse
  aufsteigende Rauchsaeulen (`bhEff119` Art 1), die das Original am GS-Fenster
  verwirft.
- **Fehlender Explosionseffekt gefunden und portiert:** `bhCamYureSet`
  (Ereignisbefehl 0x5A, event.c:4701) war ungebunden; Event 7 der Explosion
  ruettelt damit die Kamera sechsmal (Amplitude 1,0/0,4/2,0/0,8/4,0/1,0) und
  setzt sie zurueck. Jetzt gebunden; `pending_commands` der Szene enthaelt nur
  noch `bhInitSetKage` (0x2F, fremd).
- **Funken (Hinweis des Hauptagenten):** In dieser Szene fehlen keine Funken.
  Beide Funkenerzeuger des Raums (`bhEff154`->`bhEff155` hibana, Raumeffekt
  0x12; `bhEff179`->`bhEff180` hiba2, Raumeffekte 0x10/0x2C/0x2D) laufen vor
  der Explosion und werden **vom Originalskript** bei Explosionsbeginn
  abgeschaltet (Event 7, Offset 3342-3354: `bhEffDispSet` v1=0 ->
  `stflg |= 0x1000000`, event.c bhEffDispSet; effect.c:782 ueberspringt dann
  die Prozedur). Es gibt keine ungebundene Effektkennung (keine `bhEffNNN`-
  Luecke in den Laeufen bis Bild 420 vorher bzw. 380 nachher).
- **bhEff352 ist originalbelegt** und zeichnet nicht doppelt: die 14 Flammen
  sitzen am brennenden Zombie (Gegner 3, EN01 Art 1), nicht am Wrack. Erzeuger
  im Original: `bhEne01_MainLoop` (en01.c:2507-2516, Art 1/3/7 ->
  `bhEne01_LinkFireEffect(epw, rand()%4)`, en01.c:11225/11272 `sys->ef.id =
  352`), nativ `recvx_en01_link_fire_effect` (en01_core.c:7706/7775). Typ
  0..3 ergibt sieben Eintraege, je zwei Saetze = 14, wie im Zustand. Die
  Wrackfeuer sind die Raumeffekte 218/181/182. (Die erste Runde nannte 352
  „Flamme am Wrack“ und zonzon.c:939 — das war die falsche Zuordnung.)
- **Verbleibender Vier-Bild-Puls ist Originalverhalten** (nach den portierten
  Regeln): jede `bhEff119`-Rauchwelle (Groesse 22, `spd = 0.1*sz = 10.1`,
  Lebensdauer 15 Passes, effsub1.c:7890) fliegt auf die Kamera zu und faellt
  heraus, sobald ihre Ecken das GS-Fenster verlassen; zwischen zwei Wellen ist
  das Bild heller. Gegen echte Hardware nicht pruefbar.

## Lokalisierung (Nachweis)

Replay: natuerlicher Weg `graveyard-noskip.txt` (aus helicam-20261013, ohne
Teleport), Explosion ab Bild 339. Jedes Bild 338-420 einzeln aufgenommen.

1. Helligkeit oben (Zeilen 0-159) zeigt einen Saegezahn im Vier-Bild-Takt
   (351, 355, 359, 363, 367, 371 …), Tabelle `brightness_top_region.txt`.
2. Diagnose-Runtime (eigene Kopie von runtime.cpp, gegen die Bibliotheken unter
   der Bausperre gelinkt; Patch `diag/diag_runtime.patch`, nicht im Baum):
   `DIAG_HIDE=119` blendet nur bhEff119 aus -> der Saegezahn verschwindet
   (Spalte 2 der Tabelle). `DIAG_POS` liefert Sichtlage/Bildgroesse je Satz
   (`diag/s*.log`).
3. `analyze.py` vergleicht je Satz die native Regel (z >= njClipZ-nah) mit den
   Originalregeln (scr.z < 0, _Clip_Screen, GS-Fenster ±2047, z 1..3854):
   Ergebnis `diag/native_vs_original_cull_before.txt` — `bhEff119` Art 0 bei
   z 8,5 nativ abgeschnitten, im Original gezeichnet (Bilder 367/371 …);
   `bhEff119` Art 1 nativ gezeichnet, im Original am GS-Fenster verworfen.

## Originalfundstellen

- effect.c:1273 (`bhDrawNtxEffect3D` -> `njDrawPolygon3DExSetData`), :1408/1414
  (`bhDrawTrsEffect3D`: id 128/353/123 -> `njDrawTexture3DEx1P`, sonst
  `njDrawTexture3DEx`), :1546/1552 (`bhDrawOpqEffect3D`: id 204 -> 1P).
- ps2_NaDraw.c:302 `njDrawTextureSub3D`: `if (scr.z < 0) return;`,
  `cflag &= _Clip_Screen(&scr.x)`, nur bei `cflag == 0` `Ps2AddPrim3D`.
  ps2_NaDraw.c:222 `njDrawPolygonSub3D`: ohne beide Pruefungen.
- ps2_Vu1Strip.c:1202 `_Clip_Screen`: `(x-2048)*0.75`, `y-2048` gegen 240.
- ps2_dummy.c:486 `Ps2AddPrim3D`: `vf7.xy = Punkt - 2048`,
  `vf7.z = z + 0.062501907*z - 2048.0625`, `vclipw.xyz` gegen 2047; Maske
  0x3FFFF (aktueller und zwei vorige Eckpunkte) setzt ADC (0x8000).
- event.c:4701 `bhCamYureSet`; camera.c:36/49/81 (`cam.px + cam.ofx`).
- effsub1.c bhEff103 Art 1 (alle vier Passes 119 Art 0, `sz` des Erzeugers
  101), bhEff119 (UV-Tabelle 15 Zeilen, `ct2` 192 -8, Blend 11/3).

## Aenderungen

1. `native/scene.h` — `SceneTriangle::ninja_primitive` (Nummer des
   Zeichenaufrufs) und `ninja_screen_reject` (Pruefungen aus
   njDrawTextureSub3D), mit Fundstellen.
2. `native/renderer.cpp` — im Ninja-Bildschirm von `draw()`: Vorpass je Aufruf
   (scr.z < 0 eines Eckpunkts oder gemeinsames _Clip_Screen-Bit -> ganzer
   Aufruf faellt, nur bei `ninja_screen_reject`); je Dreieck faellt es, wenn
   ein Eckpunkt das Ps2AddPrim3D-Fenster verlaesst; sonst **keine**
   njClipZ-Klippung. Projektion ueber die vorhandenen `ninja_sx/ninja_sy`
   (dieselben wie bei `_Check_DisplayAreaPoint` der Chunk-Streifen).
3. `native/world.cpp` — Effektzeichenschleife markiert die Dreiecke jedes
   eff[]-Satzes: ef_ntx (`untextured`) nur Fenster, ef_trs/ef_pnc/ef_opq mit
   Aufrufpruefung; die 1P-Kennungen (Trs 128/353/123, Opq 204) bleiben auf dem
   bisherigen Weg.
4. `native/world.cpp` — `RoomWorld::bind` op 0x5A `bhCamYureSet` auf den
   vorhandenen cam-Offsetbesitzer (`camera_offset_`), drei EE-`rand()` in
   x/y/z-Reihenfolge (`recvx_core_random_next`, core_random.h), v0 != 0 loescht.
   Ohne Besitzer bleibt der Befehl ungebunden.
5. Tests: `tests/native/effect_room_sprites_test.cpp` `effect_call_clip`
   (z 5 bei njClipZ 9 wird gezeichnet, normales Dreieck nicht; Fenster ±2047;
   ADC nur am betroffenen Streifendreieck; z 0,5 faellt; scr.z < 0 verwirft den
   Texturaufruf ganz, den Polygonaufruf nur am Dreieck).
   `tests/native/camera_return_test.cpp` (Datenteil): 0x5A ohne Besitzer
   ungebunden, Offset = Originalausdruck mit gesetztem Seed, Sicht verschoben,
   v0 1 loescht.

## Pruefungen (echtes Vorher/Nachher)

- `./build.sh --test`: **91/91 gruen** (Build 19:23, enthaelt auch fremde
  parallele Aenderungen). `recvx-camera-return-tests <data>`: bestanden.
  `check_no_original_core.py`: PASS vor Arbeit und vor dem Build.
- Runtime RM_0020, natuerlicher Weg, Bilder 338-380 vorher (Binary 18:26) und
  nachher (`before/`, `after/`, Gegenueberstellung `sheets/ba_349.png`,
  `sheets/ba_365.png`, angesehen):
  - Kamera: vorher bei 341-353 starr, nachher Versatz alle zwei Bilder
    (z. B. 345: +1,87/+0,16/+1,95; 350: +3,66/+3,80/+1,40), ab 354 wieder 0.
  - Helligkeit oben an den frueheren Blitzbildern 367/371: 21,2/20,8 -> 18,0
    (Nachbarn ~15); 349/350 dunkler (30,2/29,1 statt 40,2/35,1), weil Saetze
    mit z < 9 jetzt gezeichnet werden. Der Puls bei 351/355/375/379 bleibt —
    dort verwirft auch das Original die Welle (GS-Fenster).
- RM_0100 Stichprobe (60 Bilder) vorher/nachher bildgleich.

## Offen / Grenzen

- Eigene Zeichenfunktionen (ef_fnc-Vierecke, `EffectSprite::quads`) und der
  1P-Weg (`njDrawTexture3DEx1P`, VU1-Streifen) behalten die bisherige
  Klippung; ihre Originalregeln sind je Funktion verschieden (offen).
- Die Reihenfolge durchsichtiger Primitive: das Original sortiert ueber die
  Ordnungstabelle `Ps2AddOT` (ps2_dummy.c:1164, 4096 Faecher nach mittlerem
  GS-Z je Aufruf, innerhalb eines Fachs Einfuegereihenfolge); der Host sortiert
  je Dreieck stetig nach 1/z. Nicht portiert (fuer Billboards gleichwertige
  Reihenfolge ausser an Fachgrenzen).
- Der verbleibende Vier-Bild-Puls folgt aus den portierten Originalregeln; ohne
  Originalhardware nicht gegenpruefbar.
- Die Szene laeuft im Headless-Lauf ohne Hubschrauber-FMV (movie_active false);
  das ist nicht Teil dieser Arbeit.

## Geaenderte Dateien

- `native/scene.h`, `native/renderer.cpp`, `native/world.cpp`
- `tests/native/effect_room_sprites_test.cpp`, `tests/native/camera_return_test.cpp`

## Ledger-Vorschlaege (TAB-Format)

```
event.c:bhCamYureSet	done	cam.ofx/ofy/ofz = (0.01*v1)*(-rand()/-2.1474836E9f) in x/y/z-Reihenfolge bzw. 0 (event.c:4701) in native/world.cpp RoomWorld::bind op 0x5A auf dem cam-Offsetbesitzer (camera.c:36); RM_0020 Event 7 vorher ungebunden. tests/native/camera_return_test.cpp (Datenteil). archived/work/native_port_heli_flicker.result.md
ps2_NaDraw.c:njDrawTextureSub3D	partial	scr.z<0- und _Clip_Screen-Gesamtverwurf statt njClipZ fuer eff[]-Saetze (ef_trs/ef_pnc/ef_opq ohne 1P-ids) in native/renderer.cpp (SceneTriangle::ninja_primitive) und native/world.cpp; ef_fnc-Eigenzeichnungen offen. tests/native/effect_room_sprites_test.cpp effect_call_clip
ps2_NaDraw.c:njDrawPolygonSub3D	partial	ef_ntx-Saetze nur mit dem Ps2AddPrim3D-Fenster, ohne z<0-Verwurf (native/world.cpp/renderer.cpp). tests/native/effect_room_sprites_test.cpp effect_call_clip
ps2_dummy.c:Ps2AddPrim3D	partial	vclipw-Fenster |x-2048|,|y-2048|<=2047, 1<=z<=3854 mit ADC je Streifendreieck fuer eff[]-Effektaufrufe; GS-Z-Werte und OT-Eintrag (Ps2AddOT) nicht portiert. tests/native/effect_room_sprites_test.cpp effect_call_clip
```
