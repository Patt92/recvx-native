# Plan: Erweiterung der Rasterer um Vulkan

Stand: 2026-10-24, Arbeitsbaum `main`, Policy-Gate `tools/check_no_original_core.py` = PASS.
Auftragsklasse: Mehrstufiger Ausbau des Host-Rasterers mit neuen Build- und
Auswahl-Schnittstellen. Kein Eingriff in Spiellogik, World oder Originalkern.

## 1. Ziel (beobachtbares Verhalten)

Ein viertes Backend des Host-GPU-Rasterers, das **dieselbe** `raster::Setup`-Liste des
`SoftwareRenderer` rastert wie Metal, Direct3D 11 und OpenGL, mit dem Pixelkörper der
Bandstufe, und deren Bild sowie Tiefe zurückliefert. Wählbar über `video.ini`
`renderer = vulkan`, `--renderer vulkan` und das Debugmenü; Abweichung bleibt der
Softwarepfad.

Abnahmekriterium ist wie bei den bestehenden Backends **Bitgleichheit zur Bandstufe**
bei Punktabtastung, Farbe einschließlich Pixelalpha und Tiefe (1/z), bei SSAA 1 und 2,
synthetische Fälle und echte Raumdbilder — kein „ähnlich wirkendes" Mischen.

## 2. Befund (belegte Ausgangslage)

- Schnittstelle: `native/gpu_raster.h` (40 Zeilen) — `render()`, `name()`,
  `unsupported()`, `draw_calls()`, `shadow_composites()`, `Timings timings()`;
  Fabriken `create_gpu_raster(reason)` (Plattformstandard) und
  `create_opengl_raster(reason, force_copy)`. `cache_owner()` (Zeile 17) ist der
  eindeutige Ressourcenbesitzer für das Texturcaching.
- Datenauftrag: `native/raster_setup.h` — `Setup` (alle Werte der Pixelschleife),
  `Frame` mit Phasen `[0,before_modifiers) | Modifier | [before_modifiers,
  before_mirror_effects) | Spiegel-Modifier | Rest (OT)` (Zeilen 45-55),
  `ShadowGeometry`/`ShadowWork`/`ShadowFace`/`ShadowPass` (Zeilen 31-43).
- Bestandsbackends: `gpu_raster_metal.mm` (420), `gpu_raster_d3d11.cpp` (547),
  `gpu_raster_opengl.cpp` (663), `gpu_raster_none.cpp` (17, Stub).
- **Kernproblem, das Vulkan lösen muss:** programmierbares Mischen. Metal liest per
  Framebuffer-Fetch (`[[color(0..2)]]`, `gpu_raster_metal.mm:171`), D3D11 über Rasterizer
  Ordered Views (`gpu_raster_d3d11.cpp:8-10`; ohne ROV bewusst **kein** Backend), OpenGL
  hat beides nicht und braucht deshalb einen Kopierpfad mit A/R-Flächensätzen und Blit je
  Gruppe (`gpu_raster_opengl.cpp:6-16`). Vulkan-Entsprechung:
  `VK_EXT_fragment_shader_interlock` (Begin/End Interlock, garantierte
  Ausführungsreihenfolge überlappender Fragmente).
- Rechenkopien sind je Backend doppelt, nicht geteilt: `blend_gs` in
  `native/chunk_blend.cpp:57` (Original) und jeweils in der Shadersprache in
  `gpu_raster_metal.mm:63`, `gpu_raster_d3d11.cpp:66`, `gpu_raster_opengl.cpp:137`.
  Ein Vulkan-Backend bekommt eine vierte Kopie in GLSL.
- Layoutverträge: `GpuSetup` 208 Byte (`gpu_raster_metal.mm:194-195`), `GpuShadowTri` 64 Byte
  (`:199`), Flag-Bits (`:46`), `FrameU`/`CompositeU`/`MaskU` (`:196-201`).
- Präzisionsvorgabe: keine FMA-Kontraktion, kein Fast-Math
  (`gpu_raster_metal.mm:9` `#pragma METAL fp contract(off)`, `gpu_raster_opengl.cpp:20-22`
  `precise`); Division/Rundung bleibt Treibereigenschaft, Bitgleichheit belegt erst der
  Paritätstest auf dem jeweiligen Treiber.
- Displaypfad bleibt SDL: `native/runtime.cpp:1885-1888` (SDL_Renderer, Streaming-Textur
  640x480 ARGB8888) und `:2016` (`SDL_UpdateTexture` aus `image.pixels()`). Der Rasterer
  ist **offscreen**, braucht also keinen Swapchain, keine Präsention, kein Fenster.
  Vorgang: OpenGL erzeugt sich einen eigenen versteckten SDL-Kontext
  (`gpu_raster_opengl.cpp:645`).
- Gerätebesitz: `runtime.cpp:1691-1694` hält bis zu zwei Backend-Instanzen (Auswahl
  Hauptbild/OpenGL) und `world.cpp:2974` gibt das Backend an den Spiegel-/Quellpass weiter.
  Ein Vulkan-Gerät muss daher fabrikseitig getragen werden, nicht pro Instanz.
- Auswahlkette heute: `RenderBackend {Gpu,Software,OpenGL}` (`native/video_settings.h:15`),
  INI-Lesen/Schreiben (`video_settings.cpp:18-19`, `:29`), `--renderer`
  (`runtime.cpp:81`, `:113`, `:171`), Debugmenü-Einträge (`runtime.cpp:1070-1071`),
  `Backends`-Array der Größe 2 (`runtime.cpp:1691`).
- Build: `CMakeLists.txt:179-198` (Option `RECVX_ENABLE_OPENGL` `:184`, Metal-Zweig
  `:190-194`, D3D11-Zweig `:195-198`), Tests `:1285-1290` (`gpu_raster_parity`,
  `SKIP_RETURN_CODE 77`), `:1297-1303` (`gpu_raster_d3d11_warp_parity` mit
  `RECVX_D3D11_DRIVER=warp`), OpenGL-Tests ab `:1305`. Wrapper: `build.sh`/
  `tools/build_native.py:482` `--opengl on|off`, `tools/build_windows.py:61,118`.
- Werkzeuge/Tests: `tests/native/gpu_raster_test.cpp` (190; `--backend gpu|opengl`,
  Rückgabe 77 ohne Backend), `tools/gpu_compare.cpp` (Raumbildvergleich Software gegen
  GPU, Zeit je Bild), `docs/VALIDATION.md:32-43`.
- Lokale Vulkan-Ausstattung (geprüft): `/opt/homebrew/opt/molten-vk` mit
  `libMoltenVK.dylib`/`.a`, `vulkan-headers`, `glslc` in `/opt/homebrew/bin`; **kein**
  `$VULKAN_SDK`, **kein** `vulkaninfo`. Der Extensionsname
  `VK_EXT_fragment_shader_interlock` kommt in `libMoltenVK.dylib` vor; ob MoltenVK sie
  meldet und auf Apple-GPU korrekt reiht, ist erst im Machbarkeitsnachweis geklärt.
  Im Repo gibt es bisher keine Vulkan-Quelle und kein `third_party/`.
- Vorgeschichte: `work/performance_optimization_plan_20261020.md:118-119` verlangt
  plattformnative Backends und verbietet „keine neue große Grafikabhängigkeit **vor**
  diesem Nachweis" — der Metal-Nachweis (Phase D2) ist erbracht, Vulkan ist damit zulässig.

## 3. Eingriff (Phasen, Dateien, Schnittstellen)

Jede Phase endet mit Paritätstest und `check_no_original_core.py`; Vulkan ist durchgehend
optional und ersetzt kein bestehendes Backend.

### V0 — Machbarkeit und Grundsatzentscheidung (kein produktiver Code)

1. Interlock-Nachweis: meldet MoltenVK `VK_EXT_fragment_shader_interlock`, und ist die
   Reihenfolge überlappender Fragmente garantiert? Kleines Testprogramm mit zwei
   überlappenden Rechtecken und Read-Modify-Write.
2. Präzisionsnachweis: `glslc -O0` (verfügbar) gegen die Referenzrechnung der Bandstufe;
   eine Handvoll GS-ALPHA-Register aus `gpu_raster_test.cpp:45` bitgenau prüfen.
3. Entscheidung **A oder B**, ausdrücklich nach dem ROV-Präzedenzfall
   (`gpu_raster_d3d11.cpp:8-10`):
   - **A (empfohlen):** Interlock ist Pflicht; ohne Extension liefert
     `create_vulkan_raster` `nullptr` mit Begründung und der Aufrufer zeichnet Software.
   - **B:** zusätzlicher Kopierpfad nach Art von `gpu_raster_opengl.cpp` (A/R-Sätze, Blit
     je disjunkte Gruppe) für Treiber ohne Interlock.
4. Entscheidung Plattformumfang: Vulkan primär **Windows x64** (nativer ICD), optional
   Linux (Mesa), auf macOS **zweiter** Rasterer neben Metal — nicht neuer
   Plattformstandard, sonst wäre MoltenVK eine Übersetzungsschicht unter dem Spiel.
5. Entscheidung Shaderweg: SPIR-V **zur Buildzeit** mit `glslc` erzeugt und eingebettet
   (Analogie: die anderen Backends betten Quelltexte, `gpu_raster_metal.mm:27`), statt
   glslang zur Laufzeit; damit keine neue Laufzeitabhängigkeit außer der Loader-Bibliothek.
6. Entscheidung Loader: eigener Minimal-Loader über `vkGetInstanceProcAddr`
   (`dlopen`/`LoadLibrary`, optional `SDL_vulkan`), **kein** neuer großer C++-Wrapper
   (keine Vulkan-Hpp/volk-Pflicht), um Abhängigkeit und Codefläche klein zu halten.

Ergebnis als `archived/work/vulkan_raster_spike_20261024.result.md`. Bei negativem V0-Befund
zu 1 oder 2 endet der Plan hier mit dokumentierter Portierungslücke — kein Ersatzverhalten.

### V1 — Gerüst: Gerät, Bilder, Readback, erste Parität

Neue Datei `native/gpu_raster_vulkan.cpp` mit `class VulkanRaster final : public GpuRaster`
und Fabrik `std::shared_ptr<GpuRaster> create_vulkan_raster(std::string& reason)`
(Deklaration in `native/gpu_raster.h`, Stub in `gpu_raster_none.cpp` wie bei OpenGL,
Zeilen 6 und 15).

- Instance, Device-Auswahl, ein Queue-Familien-Satz; Gerätebesitz fabrikseitig, damit
  Hauptbild und Spiegel-/Quellpass (`world.cpp:2974`) dasselbe Gerät teilen.
- Offscreen-Flächen als `R32_UINT`/`R32_SFLOAT`-Images in `VK_IMAGE_LAYOUT_GENERAL` mit
  einem Subpass und Self-Dependency statt vieler Layoutwechsel — das ist das
  funktionalen Äquivalent zu Metal/D3D11, wo der Shader Tiefe und transportierte Tiefe
  selbst vergleicht (`gpu_raster_metal.mm:8`), ohne Fixed-Function-Depth-Test.
- Ein Upload-Buffer je Bild: Setups (208-Byte-Layout, std430-kompatibel), Schatten-
  Dreiecke (64 Byte), `FrameU`/`CompositeU`/`MaskU`; Textur-Cache nach
  `cache_owner()`-Muster der anderen Backends.
- Readback über host-sichtbares Puffermemory plus Fence; Wartezeit in `Timings::wait`
  wie in `gpu_raster_metal.mm:287` (schlafend warten, damit es nicht als CPU-Zeit zählt).
- Y-Richtung explizit prüfen: Bildzeile `y` muss Speicherzeile `y` bleiben
  (`gpu_raster_opengl.cpp:17`), Vulkan-Viewportflip gegen `gl_FragCoord`-Zählung.
- Leeres Bild und einfache undurchsichtige Dreiecke mit Punktabtastung: Parität bei SSAA 1.

### V2 — Volle Setup-Semantik

Abzuarbeiten anhand der Felder in `raster_setup.h:14-28` und des Metal-Fragments als
Referenz (`gpu_raster_metal.mm:40-181`):

- `blend_gs` in GLSL, alle GS-ALPHA-Register (Testliste `gpu_raster_test.cpp:45`),
  Art-Codierung undurchsichtig/additiv/GS wie `gpu_raster_metal.mm:353`.
- Eckalpha, Gouraud-Licht, `vertex_alpha`, Nebel `fogged`/FGE-Interpolation,
  `fog_color_set`, `fog_fixed`, `texture_color_divisor`, `punch_through`,
  `depth_write=false`, `general_water`, `mirror_room`, `mirror_effect`, `post`.
- `clamp_u`/`clamp_v` und Punktabtastung als Texel-Laden; `wireframe` wie die Bandstufe.
- Phasentreue: drei Phasen mit Modifier- und Spiegel-Einschub, Schattenmasken im
  640x480-Bild mit `GEQUAL` gegen transportierte Empfaengertiefe und Komposit
  (`gpu_raster_metal.mm:142-181`), `shadow_composites()`-Zähler.
- Batching wie `gpu_raster_metal.mm:357-359`: aufeinanderfolgende Setups gleicher
  Textur/Sampler, **kein Batch über eine Phasengrenze**; `draw_calls()`-Zähler erhalten.
- `unsupported()` gefüllt für alles, was dieses Backend nicht exakt abbildet; der Aufrufer
  fällt pro Bild in Software (`renderer.h:56` `set_gpu`, `last_gpu_frame_`).

Abnahme: Parität SSAA 1 und 2, synthetisch und Räume, Punktabtastung.

### V3 — Host-Qualitätsstufen und Supersampling

- Gefilterte Stufen mit `VkSampler` (trilinear, 4x anisotrop) und analytischen
  Ableitungen der Bandstufe; die vierpixel-abhängige Ableitungsfolge der Software wird
  bewusst **nicht** nachgebildet (`gpu_raster_metal.mm:11-13`).
- Texturvarianten/Ketten nach `enum Variant` (`gpu_raster_metal.mm:203`): Punkt, gerade,
  praemultipliziert, Punch; `max_alpha`/`cutout`/`uniform_alpha`-Flags.
- SSAA-Auflösung `W*ss x H*ss` und Resolve als Compute- oder Fragmentpass nach dem
  Muster `resolve` (`gpu_raster_metal.mm:183-188`): Mittelung mit Rundung `(x+n/2)/n`,
  Tiefe als Maximum je Block, `S==1`-Sonderpfad.

### V4 — Auswahl, Build, Prüfung, Doku

- `RenderBackend` um `Vulkan` erweitern (`video_settings.h:15`), INI-Wert `vulkan`
  (`video_settings.cpp:19`) und Kommentarzeile (`:29`), `--renderer vulkan`
  (`runtime.cpp:113` plus Hilfetext `:171`), Debugmenü-Auswahlliste
  (`runtime.cpp:1070-1071`), `Backends`-Array 2 → 3 (`runtime.cpp:1691`).
  Reihenfolge der Auswahl unverändert: Plattformstandard bleibt Metal/D3D11/GL.
- `CMakeLists.txt`: `option(RECVX_ENABLE_VULKAN ...)` neben `:184`,
  `find_package(Vulkan)`/MoltenVK-Pfad, Quelle und `RECVX_HAVE_VULKAN=1` neben `:185-189`,
  `glslc`-Regel für SPIR-V; `build.sh`/`tools/build_native.py`/`tools/build_windows.py`
  um `--vulkan on|off` wie `--opengl` (`build_native.py:482`, `build_windows.py:61`).
  Mac-Paketierung: MoltenVK in `Contents/Frameworks` und
  `install_name_tool`/Codesign analog `build_native.py:71-78`,
  `package_native.py:81-106` — nur falls macOS-Vulkan aktiviert wird.
- Tests: `--backend vulkan` in `tests/native/gpu_raster_test.cpp` (Argumentprüfung
  `:130`) und `tools/gpu_compare.cpp`; neue CMake-Tests `gpu_raster_vulkan_parity`
  (und bei Kopierpfad `..._copy_parity`) mit `SKIP_RETURN_CODE 77`; Windows-Lauf gegen
  nativen ICD, Crossbuild/Wine getrennt melden.
- Doku: `docs/NATIVE_PORT.md:209-228` (Backend-Auswahl, Option), `docs/NATIVE_ASSETS.md:58-60`,
  `docs/VALIDATION.md:32-43`, `STATUS.md`; Ergebnis nach `archived/work/`,
  Eintrag in `docs/port_ledger.tsv` nur für geprüfte Reimplementierungen;
  `tools/port_check.py` nach Engine-Arbeit.

## 4. Nachweis (welche Prüfung belegt was)

| Prüfung | Belegtes Ergebnis |
| --- | --- |
| V0-Zwischenprogramm | Interlock vorhanden/reihend, SPIR-V ohne Kontraktion bitgenau |
| `gpu_raster_vulkan_parity` (SSAA 1 und 2, Drahtgitter, Punktabtastung) | Bitgleichheit Farbe und Tiefe gegen die Bandstufe; 77 = übersprungen ohne Gerät |
| `gpu_raster_parity`, `gpu_raster_opengl_parity`, `gpu_raster_opengl_copy_parity`, `gpu_raster_d3d11_warp_parity` | keine Regression der Bestandsbackends |
| `recvx-gpu-compare --backend vulkan` auf echten Räumen | Pixelgleichheit plus Zeit je Bild gegen Software und Metal |
| `recvx-runtime --renderer vulkan` im Fenster | Backendname im Debugmenü, keine `unsupported()`-Gründe im Dauerbetrieb, Schatten und Spiegel sichtbar |
| `python3 tools/check_no_original_core.py` | Policy unverändert PASS |
| `tools/port_check.py --readme README.md --markdown docs/PORT_STATUS.md` | Abdeckungstabelle, ohne Handeingriff |

## 5. Risiken und offene Punkte

1. **Interlock-Verfügbarkeit/-Semantik** ist die tragende Annahme. Fehlt sie auf einem
   Ziel, gilt Entscheidung A (Softwarepfad) oder B (Kopierpfad) — kein angenähertes Mischen.
2. **MoltenVK ist eine Übersetzungsschicht.** Auf macOS bleibt Metal Plattformstandard;
   Vulkan dort wie OpenGL ein ausdrücklich wählbares Zweitbackend, sonst wäre die
   Plattformanpassung selbst eine Nachbildung.
3. **Treibereigenschaft Rundung/Division:** Bitgleichheit ist pro Treiber zu belegen,
   nicht pro API (Lehre aus `gpu_raster_opengl.cpp:20-22`).
4. **Vulkan-Boilerplate vs. Codefläche:** 400-700 Zeilen erwartet (Metal 420, D3D11 547,
   GL 663). Kein generischer Framegraph, keine zweite Workwelt, kein zweiter Renderer.
5. **Zweite Geräteinstanz** durch den Spiegel-/Quellpass (`world.cpp:2974`) und die
   Backend-Auswahl im Debugmenü: Geräte- und Cache-Besitz vor V1 festlegen.
6. **Build-Parallelität:** keine gleichzeitigen Builds in `build/macos-arm64`;
   Tests mit gemeinsamen temporären Fixtures nacheinander laufen lassen.
7. **Kein Commit ohne Auftrag.** Der Plan allein ändert nichts am Arbeitsbaum außer
   dieser Datei.
