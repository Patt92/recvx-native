# GPU-Rasterer im Spiel (Plan Phase D2, macOS) — 2026-10-20

Plan: [performance_optimization_plan_20261020.md](performance_optimization_plan_20261020.md),
Machbarkeit: [performance_gpu_feasibility_20261020.result.md](performance_gpu_feasibility_20261020.result.md).
Rohdaten: `build/validation/performance-20261020/gpu-compare-d2b.txt`, `runtime-compare/`.
Stand: Arbeitsbaum + Dev-App (`./build.sh`), kein Paket, kein Commit. GPU-Läufe im Nutzer-Terminal
(Sandbox ohne Metal-Gerät).

## Aufbau

- `native/raster_setup.h`: Setup-Liste der Bandstufe (aus `renderer.cpp` herausgezogen), Phasengrenzen
  der Schattenmodifier, `ShadowComposite`.
- `native/gpu_raster.h`: Schnittstelle `GpuRaster`, `create_gpu_raster()`; je Betriebssystem ein Backend:
  `gpu_raster_metal.mm` (macOS), `gpu_raster_none.cpp` (sonst: Software). Windows/D3D11 offen.
- `SoftwareRenderer::set_gpu()`: Transformation, Clipping, Streifen-/Ninja-Regeln, Sortierung,
  Phasen und Schattenmasken bleiben in `renderer.cpp`. Mit Backend rastert die GPU dieselbe Liste
  und liefert Bild und Tiefe in `pixels_`/`depth_`; Linien (njDrawLine3D), 2D-Effekte, HUD, UI,
  Blenden, Screenshots und alle `canvas()`-Leser laufen unverändert danach. Drahtgitter, Monitor-
  Rendertextur, Vollbildquelle, Terminal-/Itemfenster bleiben Software. Gerätefehler → Meldung,
  danach Software (kein Abbruch).
- Metal: programmierbares Mischen (Framebuffer-Fetch). Jedes Setup als Rechteck seiner Pixelgrenzen;
  der Fragment-Shader ist der Körper der Pixelschleife: Kantenfunktionen mit halboffener Regel, 1/z,
  GEQUAL gegen die gelesene Tiefe, `lit`/`modulate`, Punktabtastung, GS-Nebel ganzzahlig,
  `blend_gs` für **jedes** GS-ALPHA-Register (auch Ad, FIX > 128, 1 + C), Abdeckungsmischung,
  Tiefenschwellen, transportierte Tiefe. Ziele: Farbe R32Uint (0xAARRGGBB wie `pixels_`), 1/z R32F,
  transportierte Tiefe R32F; SSAA-Auflösung als Compute mit der Rundung der Software.
- Schatten: die GPU zeichnet auch die Masken. `renderer.cpp` liefert über `raster::ShadowGeometry`
  die transportierten Dreiecke (Projektion, Transport, Unterdrückung, Fläche, Eckreihenfolge und
  Pixelgrenzen exakt wie `raster_shadow_mask`), der Shader rastert sie in eine 640×480-Maskentextur
  je Werk (sättigende ±32-Akkumulation über Framebuffer-Fetch, GEQUAL gegen die transportierte Tiefe),
  das Komposit (`shadow_mask_composite`) liest diese Textur. Alles in **einem** Befehlspuffer: kein
  Warten zwischen den Phasen und kein Rücklesen der Tiefe mehr (vorher 1,96 Übermittlungen und
  4,9 MB je Bild).
- Arithmetik: Metal `MTLMathModeSafe`, `#pragma METAL fp contract(off)`; `renderer.cpp` jetzt mit
  `-ffp-contract=off` (vorher vom Compiler kontrahierte FMA). Damit ändert sich der Softwarebild-Hash
  (RM_0020 SSAA 2: 315a…→a6fb…), nicht die Rechenvorschrift; MSVC kontrahiert ohnehin nicht.
  IEEE-Gegenprobe GPU gegen CPU (4,2 Mio. Zufallswerte, Division/Kehrwert/Kantenfunktion/Mul+Add):
  0 Abweichungen (`ieee/`).
- Einstellungen: `video.ini` `renderer = gpu|software` (Standard gpu), `--renderer gpu|software`;
  headless ohne Schalter Software (Prüfläufe unverändert).

## Ergebnis

Punktabtastung (Modus „Original“) ist **bitgleich** — Farbe inkl. Pixelalpha und Tiefe:

| Prüfung | Ergebnis |
|---|---|
| `gpu_raster_parity` (synthetisch: 11 GS-Register inkl. Ad/FIX>128/1+C, additiv, Eckalpha, Gouraud, Nebel, festes F, direktes Wasser, Punch, Tiefe ohne Schreiben, Nahebene, SSAA 1/2, Drahtgitter; dazu RM_0020/RM_1090) | bitgleich |
| `recvx-gpu-compare` RM_0020/0050/1090 Kamera 1, SSAA 1 und 2 | 100 % RGB, Alpha und Tiefe gleich |
| Headless-Endbilder (`--nearest --ss 2`) graveyard-walk (300), Steve-Feuergefecht RM_0030 (3505), Fledermäuse RM_0130 (130), Uhrenraum RM_1090 (90) — mit 1–6 Schattenkompositen je Bild | 100 % gleich |

Gefilterte Host-Stufen (Low/High/Ultra): Hardwarefilter mit den analytischen Ableitungen der Bandstufe;
81–91 % exakt, sonst überwiegend ±1, max 4–19 an einzelnen Kanten. Die Software leitet die
Ableitungen nur jedes vierte Pixel ab (reihenfolgeabhängig) und filtert per Software-4-Tap; das ist
eine Host-Qualitätsstufe, keine Originalsemantik.

Zeit je Bild (M4 Pro, statischer Raum, `recvx-gpu-compare`):

| Raum | SSAA 2, gefiltert: Software CPU | GPU-Pfad CPU (Encode, Warten, Rücklesen) |
|---|---|---|
| RM_0020 | 73,6 ms | 0,30 ms |
| RM_0050 | 82,2 ms | 0,43 ms |
| RM_1090 | 69,4 ms | 0,51 ms |

## Offen

1. **Energie am Starter** (`powermetrics`, gleiche Szene wie die 12–17-W-Läufe) und Fenster/Titelstart
   — nur am Gerät des Nutzers prüfbar.
2. Windows: Direct3D-11-Backend; D3D11 hat kein programmierbares Mischen → Rasterreihenfolge über
   ROV (Feature Level 11.3) oder Compute-Rasterer. Bis dahin Softwarepfad (Crossbuild geprüft).
3. Das GPU-Bild wartet synchron (`waitUntilCompleted`) und wird zurückgelesen, weil Linien/Overlays auf
   der CPU folgen; bei 30 Bildern/s unkritisch (< 1 ms), spätere direkte Präsentation möglich.
4. Kleine Renderziele (Monitor, Terminal, Item) und Drahtgitter bleiben Software.
