# D10 Teil B — Kartenbild, Konstantmaterial und Fade-Rückweg

Stand: 2026-10-14. Fortsetzung nach Teil A (Kartentask-Eingabesperre).
Referenz: `archived/work/native_port_D10_map.result.md`; verbindlich AGENTS.md/QUALITY.md.

## Ergebnis und Auslieferungsgrenze

Die eigene Build-Runtime `build/mapinput-claude-20261013/recvx-runtime`
zeichnet das Kartenbild aus MRY über die vorhandene native Szene/Rendererquelle.
Statusmenü → MAP → Statusmenü → Spiel und erneutes Öffnen bestehen die
Runtime-Prüfung. **Keine Starter-App oder Pakete ersetzt, kein Commit/Push.**
Start.command bevorzugt aktuell `build/start/macos-arm64/RECVX Native.app`;
diese Arbeit ist deshalb ausdrücklich noch nicht über den Nutzerstarter ausgeliefert.
Kein vollständiger 1:1-Karten-, Controller-, Windows- oder Durchspielnachweis.

## Originalmechanismen und Änderungen

### Native Ausgabe der Zeichenliste

`native/map_render.*` verbindet Fill/Line/Sprite/Marker/MapObject/Compass mit
SoftwareRenderer. Indexdatei bleibt separat von der Etagenlesedatei erhalten;
Marker-MDL, map_texP und stg_texP werden vom vorhandenen MDL/PVP-Decoder gelesen.
MapPurgeTree liefert die Objekttransformationen und Dateiknoten-Identität.
Kartenansicht verwendet den Vertrag aus map.c:356–369 (500, Aspect 1.174,
640×480, Zentrum 320/240, Clip 2/20000). `pri` der 2D-Primitiven entspricht
Kameratiefe −pri; Modell- und 2D-Geometrie teilen den Tiefenpuffer.

Originalfundstellen: map.c:1039/1101/1142/2319/2337/2355/2375/2576/2712;
ps2_NaDraw2D.c:93–164, ps2_NaSprite.c:30–322, ps2_dummy.c:309–482.
Die bestehende primitive Prioritäts-/Clipabbildung und Alpha-Repräsentation
sind keine vollständige Rekonstruktion der ursprünglichen Drawreihenfolge.

### Tatsächlicher Constant-Material-Farbpfad statt vermutetem Lichtshader

map.c:734 setzt njControl3D(0x810), :736 ConstantAttr mit UA.
ps2_NinjaCnk.c:1748–1753 wählt damit ColorCalc 1;
ps2_Vu1Strip.c:1590–1604 (`vu1GetVertexColorCM`) liest ausschließlich
Diffuse und Materialalpha, **nicht** die berechneten Normalenintensitäten.
`vu1SetDiffuseMaterial` (:130–134) gibt 128×Material-RGB aus.
Untexturierte Kanäle werden vor der Ausgabe ganzzahlig abgeschnitten;
Texturmodulation verwendet diese quantisierten Kanäle als Faktor über 128.

Der vorige MapRenderer setzte gouraud=true und multiplizierte die Palette
mit Normalenlicht. Da Typ-41-Normalen gleichzeitig verloren gingen, waren
Kartenflächen schwarz. Nur ein Normalenfix hätte weiterhin den falschen
Farbpfad mit 0.6-Lichtfaktor gewählt. Jetzt sind Karten-CNKs materialbestimmt,
UA und das tatsächliche Chunk-Blendpaar bleiben erhalten; texturierte Streifen
werden nicht mehr pauschal zu untexturierten Dreiecken gemacht.

SceneTriangle::texture_color_divisor trägt die native Farbfaktordomäne:
Standard 255 bleibt für vorhandene native Verbraucher unverändert; Karten-CM
und Kartensprites wählen 128. Point- und Filterpfad verwenden denselben
Integerquotienten. Regression: Texel 200/100/50 mit Materialausgabe 89/128/128
liefert exakt 139/100/50; keine gerundete Konvertierung nach 255 dazwischen.

Kartensprites: 0x22 ist **COLOR|ALPHA**, nicht SCALE|ALPHA
(build-tools/ninja/ninjadef.h:663/668). Ihre konstante Farbe wird berücksichtigt.
UVs übernehmen den halben Texelversatz und den Breiten-basierten Shift beider
Achsen (ps2_NaSprite.c:40–101); ang=0 wählt Quadrant 3 ohne weitere Korrektur.

### Typ-41-Normalen im gemeinsamen Assetdecoder

Typ 41 dispatcht zu njCnkCvVn. Payload bei Chunk+8, Stride 24: Position
+0/+4/+8, Normalen +12/+16/+20. native/scene.cpp las bisher nur für Typ 51
Normalen. Typ 41 wird jetzt vollständig gelesen und mit der Matrixbasis
transformiert, ohne zusätzliche Normalisierung oder erfundene Nullnormalen.

Originaldatenbeleg MRY/0000: erstes Marker-Vertexchunk 0x3C, Kopf
29 00 19 00 00 00 04 00, erste Normale bei 0x50 = (0,1,0).
MRY/0005: Chunk 0x253C, erste Normale bei 0x2550 =
(−0.5547002,0.8320503,0). Semantikreferenz ps2_NinjaCnk.c:1162–1276.
Originale Nullnormalen bleiben null. Typ-51-Pfad unberührt.

### Fade-Zustand muss innerhalb desselben Passes sichtbar sein

ScreenFade bildet screen.c:21/50 ab: vorheriges Ziel in fade_pbk,
Ausgangschannels festhalten, hkn=1 oder 1+ct, erster Kontrollpass zählt nur,
Interpolation und Snap bei ct>=hkn−1. Fade 8 endet im achten Kontrollpass,
Fade 3 im dritten (nicht jeweils einen Pass später).

system.c:770–899 führt bhControlMap vor bhControlScreenFade aus.
map.c:540 startet im bhControlMap-Schließzweig den Schwarzfade;
FtskMapExit (:1772) liest danach **live** sys->cb_flg & 2.
Der Port las nur den MapHost-Eingangssnapshot vor diesem neuen Fade und
beendete die Task im selben Pass. MapServices::screen_fade_pending bindet jetzt
den bestehenden EventVm-Flagbesitzer, keine zweite Systemwelt.

Vorher am Screenshot-/State-Endpunkt nach dem Schließen: alpha 31.875,
cb_flg 2 weiterhin gesetzt, Karte schon verlassen. Nachher: alpha 255,
Fade beendet; anschließend Statusmenü schließen: alpha 0 und Spiel sichtbar.

### Task 7 steht still, solange die Karte offen ist (Nutzer: „Claire kann laufen“)

Gemeldet: „wenn die karte offen ist, kann ich rumlaufen“. Ursache im Host, nicht
im Skript. Original:

- Spieleinstieg über Dreieck/`bhMapSystemOn`: `bhSysCallMap` Case 0 setzt
  `sys->ts_flg |= 0x180` (system.c:777) und Case 2 stellt es zurück (:830).
  `njUserMain` (main.c:196) führt einen Task nur bei `tk_flg & (1<<i)` und
  `!(ts_flg & (1<<i))` — Task 7 = `bhSysCallGame` und Task 8 = `bhSysCallEvent`
  ruhen damit.
- Statusmenü-Einstieg: Task 7 wurde beim Öffnen des Statusbildschirms gestoppt
  (`sub1.c:3038`, `ts_flg |= 0x80`) und `bhCheckSubTask` hat `sp_flg = 0x30`
  hinterlassen (system.c:608/609); Case 6 der Karte setzt nur `ts_flg`-Bit 9 um.
- `bhControlPlayer` läuft ausschließlich mit `sys->sp_flg & 0x1`
  (player.c:1365). Ist Bit 0 klar — wie in beiden Karteneinstiegen — gibt es
  **keinen Spielerpass**: keine Bewegung, keine Drehung, kein Kamera-Follow.

Host vorher: `world->tick()` und `events->tick()` waren schon mit `!map_active`
gewacht, die Spielerpass-Aufrufe (`follow()`/`tick_camera`, `locomote`/
`head_hair_step`, `third_person_tick`) aber nur mit `!menu.active`, dem
Ersatz für den vom Statusbildschirm gestoppten Task 7. `map_active` ist der
vorhandene Besitzer des Kartentaskzustands; die drei Wachen führen es jetzt
mit. Kein neuer Zustand, kein zweiter Taskautomat.

Messung (`tools/verify_map.py`, neuer Fall `status-map-move`: Karte offen,
`150 keys forward` bis Bild 240). Kontrollierter A/B-Bau aus demselben Baum,
nur diese vier Wachen zurückgenommen (`runtime-guard-before` /
`runtime-guard-after` in `build/validation/mapbug-20261013`):

| Binary | Lauf | Actor bei Bild 240 | Motion | Frame |
|---|---|---|---|---|
| `runtime-guard-before` | Karte ohne Richtung (Referenz) | 99.4249, 0, 149.949 | 42 | 1507328 |
| `runtime-guard-before` | Karte + gehaltene Richtung | 64.4897, 0, 149.939 | 0 | 1507328 |
| `runtime-guard-after` | Karte ohne Richtung (Referenz) | 99.4249, 0, 149.949 | 42 | 2162688 |
| `runtime-guard-after` | Karte + gehaltene Richtung | 99.4249, 0, 149.949 | 42 | 2162688 |

Ohne die Wache bewegt die gehaltene Richtung die Figur um 35 Einheiten bis
(64.4897, 0, 149.939) und wechselt auf Motion 0 (Gehclip); mit der Wache ist
der Lauf mit gehaltener Richtung in Position, Motion und Frame identisch zum
Lauf ohne Richtung. Die Prüfung schlägt
mit dem Vorher-Binary an genau dieser Zusicherung fehl und ist grün mit dem
Nachher-Binary. Zusätzlich sind Progression und Aufnahme-Prüfung gegen das
Nachher-Binary grün.

### Ressourcen-Wiedereintritt

bhInitMap löscht den ursprünglichen Kartenwork. Die nativen Ressourcenrevisionen
sind aber Cacheidentitäten, kein Gameplayzustand: sie bleiben je MapScreen-Owner
monoton, damit ein überlebender MapRenderer nicht die vorherige Spieler-/Index-
oder Etagenressource unter wiederholter Revision 1 verwendet. Neue Init leert
alle alten Modell-/Texturcaches vor dem frischen Indexlesen. Gleicher Renderer
nach Wiedereintritt ist pixelgleich mit einem unabhängig neu dekodierenden Renderer.

## Gezielte Prüfungen und Gegenproben

Logs: `build/validation/mapbug-20261013`.

- `ctest-reentry.log`: 6/6 — original_map_screen, original_map_render,
  scene_renderer, original_chunk_blend, computer_terminal,
  lighting_fog_semantics.
- `verify-reentry.log`: MAP aus Statusmenü, Eingabesperre, reguläres Abblenden,
  Rückkehr zum Statusmenü und Spiel sowie erneutes Öffnen bestanden.
  RM_0020: mode 7, 1 Tag, 74 Drawcalls, 4 Etagenobjekte, 4 Sprites,
  2044 farbige Stichproben innerhalb des Viewports, keine unaufgelösten
  Modell-/Sprite-Ressourcen. Itemgabe über Debugkatalog ist Diagnoseaufbau,
  kein natürliches Durchspielen.
- `pickups-cm.log`: Karten-/breite Waffenaufnahme-Prüfung bestanden.
- Vorher-Runtime gesichert als `runtime-before-resume`: dieselbe neue Bildprüfung
  scheitert mit **0 farbigen Kartenstichproben** (`countercheck.log`).
- Isolierte Fade-Gegenprobe gegen dasselbe Vorher-Binary, Bildprüfung nur in
  der Wegwerfsonde übersprungen: scheitert mit alpha 31.875/running true
  (`countercheck-fade.log`). Produktivtest nicht abgeschwächt.
- Frame-RGB und Rasterproben: Hintergrund innerhalb des ScrIn-Viewports,
  Gitter row 96/column 80, Rahmenplatte RGB 0/4/0. Die erste Testfassung
  erwartete Hintergrund außerhalb des Viewports und volle 0xFF-Framebufferalpha;
  die Originalrahmenplatte hat dort 0x80-Quellalpha und verdeckt das entfernte
  Gitter. Erwartungen anhand Originalmechanismus korrigiert, nicht Enginefarbe
  passend zum Test geändert.
- Policy-Prüfer vor Builds sowie Quellen/Build/Binary abschließend PASS;
  Runtime arm64-only per lipo. Kein vollständiger Sanitizer-/Raumscan,
  kein Windows-Crossbuild/-Lauf dieser Fortsetzung.

Ansicht: `cm2/status-map.png` (aus Runtime-BMP, dem Nutzer gezeigt).

## Verbleibende Grenzen

- pad_ax/ay/al/ar und keytype haben weiterhin keine vollständige Bindung;
  analoges Verschieben/Zoomen nicht als fertig melden.
- Globaler Fade-/Taskbesitzer aller anderen Spieltasks, ursprüngliche
  Material-/Primitive-Drawreihenfolge und gemeinsame Alpha-/Texturfunktion
  bleiben Teilports. Füllungen/Modelle und Linien verwenden die bestehende
  native Tiefen-/Rasterquelle, nicht einen emulierten GS-/OT-Kern.
- screen_point hält nicht-3D-geclippte 2D-Tiefe im Nearbereich auf der nativen
  Clipebene; im geprüften Kartenpfad gibt es keine Draws unter Near. Kein
  Vollständigkeitsnachweis für beliebige andere njDraw-Aufrufer.
- ScreenFade-Zeichenweg und MapRenderer benutzen bestehende native
  Alpha-/Filterdomänen; die Systemintegration ist ausdrücklich nicht das
  Ende des gemeinsamen Transparenzports.
- Start.command-App und Standalone-Pakete unverändert. Benutzer-INIs,
  Mods und Snapshots nicht verändert. Kein Commit/Push.
