# GPU-Machbarkeitsnachweis (Plan Phase D1) — 2026-10-20

Plan: [performance_optimization_plan_20261020.md](performance_optimization_plan_20261020.md), Messbasis:
[performance_phaseA_scalpel_20261020.result.md](performance_phaseA_scalpel_20261020.result.md).
Werkzeug: `tools/gpu_feasibility_metal.mm` (CMake-Ziel `recvx-gpu-feasibility`, nur macOS, EXCLUDE_FROM_ALL).
Rohdaten/Bilder: `build/validation/performance-20261020/gpu-*`. Läufe vom Nutzer im Terminal (Sandbox hat kein Metal-Gerät).

## Ansatz

Der SoftwareRenderer behält **Transformation, Clipping, Streifen-/Ninja-Regeln, Sortierung und Phasen**
(< 0,5 % der Zeit); neu ist nur eine Nahtstelle: `set_raster_observer()` reicht die fertige, sortierte
Setup-Liste (`native/raster_setup.h`, reine Daten, aus `renderer.cpp` herausgezogen — Benchmark-Hashes
unverändert) vor der Bandstufe weiter. Das Werkzeug zeichnet diese Liste mit Metal:

- Bildkoordinaten werden durchgereicht (w = 1, Interpolation `center_no_perspective` = bildlineare
  Baryzentrik wie die Software), Tiefe = `invz · 2⁻¹⁰` (Zweierpotenz, ordnungserhaltend) mit GEQUAL.
- Der Fragment-Shader ist die Pixelschleife der Bandstufe: Gouraud `lit`, `modulate` mit Divisor
  128/255, Punktabtastung mit Wrap/Clamp und `tex.read`, GS-Nebel als Ganzzahlformel
  `(C·F + Cf·(255−F)) >> 8`, Alphatest/Abdeckungsregeln, Punch-through.
- GS-Blendregister `(A − B)·C + D` werden auf Fixed-Function-Blend abgebildet (Faktoren As, 1−As,
  1, 0, FIX/128 als Blendkonstante; Add/Subtract/ReverseSubtract). Nicht abbildbar wären Ad-Faktor,
  FIX > 128 und 1+C — in den gemessenen Räumen **nicht aufgetreten**.
- „Farbe schreiben, Tiefe nur bei α ≥ ½“ (Ps2ZbuffOff / AFAIL FB_ONLY) als zweiter Tiefenpass je Stapel
  (Farbmaske aus, `discard` unter der Schwelle). Das ist auf Metal und D3D11 gleich umsetzbar.
- Überabtastung: Rendern in S·640 × S·480, Compute-Resolve mit derselben Rundung `(Σ + n/2)/n`.
- Gefilterte Qualitätsstufen: Hardware-Trilinear/4× Anisotrop auf der hochgeladenen alpha-gewichteten
  Mip-Kette der Software (praemultiplizierte Float-Kette für Ausschnitte, gerade Kette für uniformes
  Alpha/Punch).

## Ergebnis (M4 Pro, Kamera 1, 200 Bilder; Software mit Worker-Automatik)

| Raum | SSAA | Modus | Setups (opak/transl.) | Draws | exakt | Δ≤1 | Δ2..4 | Δ>4 (max) | Software ms CPU/Bild | GPU ms/Bild (GPU-Zeit / CPU-Encode) |
|---|---|---|---|---|---|---|---|---|---|---|
| RM_0020 | 1 | nearest | 486/6 | 22 | 99,93 % | 1 | 136 | 82 (26) | 5,2 | 0,07 / 0,09 |
| RM_0020 | 2 | nearest | 486/6 | 22 | 99,85 % | 395 | 77 | 1 (8) | 18,5 | 0,24 / 0,11 |
| RM_0020 | 2 | filtered | 486/6 | 22 | 88,1 % | 36167 | 358 | 6 (8) | **65,8** | **0,20 / 0,13** |
| RM_0050 | 2 | nearest | 1014/104 | 62 | 99,71 % | 460 | 354 | 76 (14) | 20,5 | 0,27 / 0,14 |
| RM_0050 | 2 | filtered | 1014/104 | 62 | 77,4 % | 64202 | 4763 | 499 (16) | 72,9 | 0,23 / 0,15 |
| RM_1090 | 2 | nearest | 2010/399 | 119 | 99,86 % | 328 | 96 | 15 (24) | 19,0 | 0,24 / 0,19 |
| RM_1090 | 2 | filtered | 2010/399 | 119 | 89,4 % | 30867 | 1483 | 167 (19) | 62,1 | 0,71 / 0,21 |

Δ = größte Kanaldifferenz je Pixel (0..255), Pixelzahl 307.200.

**Die GPU erledigt die Pixelarbeit der Standardstufe „High“ für ≈ 0,2–0,7 ms GPU-Zeit und ≈ 0,1–0,2 ms
CPU-Encode statt 62–73 ms CPU.** Draw-Calls 22–119 je Bild; die Wandzeit des Werkzeugs (0,3–1,0 ms)
enthält ein synchrones `waitUntilCompleted`, das der Produktpfad nicht braucht.

## Einordnung der Abweichungen

- **Punktabtastung (originaltreuer Modus „Original“):** 99,7–99,9 % bitexakt. Die Restpixel sind
  verstreute Einzelpixel (`gpu-nearest-ss1/diff.png`): Texelwahl `uint(u·w)` genau an Texelgrenzen
  (andere Gleitkomma-Interpolation der Baryzentrik) und Kantenbesitz (Top-Left-Regel der GPU gegen die
  halboffene Regel der Software). Keine Fläche, kein Muster, keine falsche Tiefen- oder Blendentscheidung
  erkennbar; bei 2×2-SSAA bleibt davon 1 Pixel > 4.
- **Gefilterte Stufen (Host-Qualität, keine Originalsemantik):** 77–89 % exakt, fast alles ±1 LSB
  (Hardware-Trilinear/Anisotrop gegen die 4-Tap-Software). Δ > 4 ≤ 0,16 % und an Silhouetten/Türkante
  (`gpu-filtered-ss2/diff.png`): andere anisotrope Fußabdruckwahl. Da der Modus ohnehin eine Hostwahl
  ist, ist die Hardwarefilterung die passende Umsetzung; sie ist nicht die Referenz der Originaltreue.
- **Blending:** Alle in den Tabellen vorkommenden Register (0x44, 0x48, 0x46, 0x0, 0x80·2³²|0x68,
  0x10·2³²|0x26) sind Fixed-Function-abbildbar; das Werkzeug meldet jedes nicht abbildbare Register —
  keines gemeldet. Tiefensortierte Transparenz (bis 399 Setups) zeigt keine Flächenabweichung; der
  Zweipass je Stapel kann theoretisch bei sich durchdringenden Transparenzflächen abweichen (hier nicht
  beobachtet, bleibt in D2 ein Prüfpunkt).

## Abbruchkriterium geprüft

Keine schwierige Regel musste sichtbar verfälscht oder über Rücklesen je Pixel gelöst werden. Der
Ansatz „CPU entscheidet, GPU rastert dieselbe Liste“ hält die Originalsemantik an einem Ort und ist
auf beiden Plattformen umsetzbar: **Metal auf macOS, Direct3D 11 auf Windows x64** hinter derselben
Setup-Liste; Shader als MSL-/HLSL-Zwillinge (ein Fragmentprogramm, ≈ 60 Zeilen), geprüft mit demselben
Differenzwerkzeug. Keine neue große Abhängigkeit (SDL3-GPU/Vulkan/WebGPU nicht erforderlich).

## Noch nicht abgedeckt (Umfang von D2)

1. **Schattenmodifier** (`zc`, bhDrawMdfEffect): Maskenpässe zwischen den Phasen. Die Maske entsteht
   heute auf der CPU aus der transportierten Tiefe; auf der GPU als zweites R32-Ziel + Maskenkomposit
   als Vollbildpass, oder Maske weiter auf der CPU und nur das Komposit auf der GPU. In jedem
   Gameplay-Bild vorhanden → muss in D2 vor der Integration gelöst sein.
2. **Spiegelpässe** (sind nur weitere Setups in Reihenfolge; Phasengrenzen `setups_before_*` sind in
   `raster::Frame` noch nicht exportiert), **Linien** (njDrawLine3D mit Tiefentest, heute CPU nach der
   Bandstufe), **2D-Overlays/UI/Blenden** (zeichnen heute in `pixels_`): erster Integrationsschritt ist
   Rücklesen des GPU-Bilds in `pixels_`/`depth_` (Unified Memory, geteilter Speicher) und unveränderte
   CPU-Overlays; damit bleiben `pixels()`/`depth()`/Screenshots/Tests gültig.
3. Rendertexturen (Monitor 512², Terminalfenster, Itemansicht) bleiben zunächst CPU (klein, selten).
4. Präsentation: anfangs weiter `SDL_UpdateTexture` aus dem Rücklesen; später direkte Übergabe.
5. Windows-Backend (D3D11 + HLSL-Zwilling) und dessen Differenzlauf; Gleichheit der Hardwarefilterung
   zwischen GPUs ist nicht gegeben (Hostmodus), Punktabtastung ist es bis auf Grenzpixel.
6. Energie: erwartet CPU-Raster 62–73 ms → ≈ 0,5 ms je Bild; die Wattzahl am Starter (heute 12–17 W CPU,
   0,1 W GPU) ist erst nach der Integration messbar, nicht aus diesem Nachweis ableitbar.

Policy: `tools/check_no_original_core.py` PASS; keine Originalquelle im Werkzeug, keine Hardware-
Nachbildung — die GS-Register werden wie in `chunk_blend.cpp` als Blendgleichung gelesen.
