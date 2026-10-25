# Optionales Vulkan-Backend des Host-Rasterers

## Stand

2026-10-25, Arbeitsbaum `main`, nicht committet. Implementiert, auf macOS arm64 (Apple M4 Pro,
MoltenVK 1.4.2 aus Homebrew) bitgleich zur Bandstufe geprüft. Windows/Linux-Hardware offen.
Plan: [vulkan_raster_plan_20261024.md](vulkan_raster_plan_20261024.md) (gleiches Verzeichnis) — Abweichungen unten.

## Architektur

- `native/gpu_raster_vulkan.cpp`, `native/gpu_raster_vulkan.glsl`; Fabrik `create_vulkan_raster`
  (`gpu_raster.h`), Stub in `gpu_raster_none.cpp`. Nie Plattformstandard.
- Ordnung über `VK_EXT_fragment_shader_interlock` (`pixel_interlock_ordered`) und Storage-Images,
  Gegenstück zu D3D11-ROV. Ohne Interlock kein Backend (Softwarepfad), kein Kopierpfad (Planentscheidung A).
- Shader aus der GL-Fassung (`div_rn`, `precise`) mit D3D11-Ordnungsmodell; im kritischen Abschnitt nur
  Lesen/Schreiben, Abdeckung/Attribute/Texel/Verwerfen davor (ARB_fragment_shader_interlock 8.13.3
  erlaubt discard vor `begin`).
- `SignedZeroInfNanPreserve` (SPV_KHR_float_controls) in jeder Stufe: ohne sie übersetzt MoltenVK mit
  Metal-Fast-Math (Log `MathMode Fast`), mit ihr `MathMode Safe`. Synthetik und drei Räume waren auch
  mit Fast-Math gleich; die Vorgabe (IEEE, `-ffp-contract=off`, Metal `MTLMathModeSafe`) gilt trotzdem.
- SPIR-V zur Buildzeit (`glslc -O0 -mfmt=num`, `build/*/generated/vulkan`), keine Laufzeit-Shaderkompilierung.
- Loader zur Laufzeit (`vulkan-1.dll`, `libvulkan.so.1`, macOS Loader/MoltenVK inkl. Homebrew;
  `RECVX_VULKAN_LIBRARY` exklusiv). Kein Linken gegen Vulkan.
- Ein Gerät je Prozess, von allen Instanzen geteilt (Hauptbild, Quellpass `world.cpp:2974`,
  Auswahlwechsel); Bildriegel `Device::frame` serialisiert Einreichungen; Texturfreigabe über das Gerät.
- Auswahl: `RenderBackend::Vulkan` (Debugmenü-Index 3), `video.ini renderer = vulkan`,
  `--renderer vulkan`, Debugmenü „Vulkan“. CMake `RECVX_ENABLE_VULKAN` (Standard an, ohne Header/glslc
  Warnung und aus), `build_native.py`/`build_windows.py --vulkan on|off`.

## Abweichungen vom Plan

- V0–V3 in einem Schritt: Interlock-, Präzisions- und Paritätsnachweis direkt am echten Backend.
- macOS nicht nur als Option „falls aktiviert“: Vulkan ist dort Zweitbackend wie OpenGL; Metal bleibt Standard.
- Maske R32_UINT statt R8_UINT (Storage-Format überall verfügbar, Werte 0..255 unverändert).
- Keine MoltenVK-Bündelung in App/Paket (keine Paketersetzung beauftragt).

## Prüfungen (build/vulkan-dev, Release, arm64)

| Prüfung | Ergebnis |
| --- | --- |
| `gpu_raster_vulkan_parity` (Synthetik SS 1–4, Drahtgitter, GS-ALPHA, Schattenphasen, Spiegel, NPOT/Mutation, zweite Instanz) | PASS bitgleich |
| `gpu_raster_parity`, `gpu_raster_opengl_parity`, `gpu_raster_opengl_copy_parity`, `texture_filtering`, `development_app_packaging`, `native_port_policy_contracts` | PASS |
| Mutationen im Shader (Maske, GEQUAL, GS-FIX, Resolve-Rundung, Komposit-Nebel) | alle 5 vom Test erkannt |
| `recvx-gpu-compare` RM_1090/RM_2000/RM_1020, SS 1/2, nearest | 100 % exakt, Tiefe exakt |
| dito filtered | identisch zu Metal (bekannte Ableitungsfolge-Abweichung, dokumentiert) |
| Zeit SS 2 filtered | Vulkan 2,4–3,0 ms/Bild Wand, 0,55–0,6 ms CPU; Metal 1,2–1,5; OpenGL 100–200 |
| 3 Threads, 2 Instanzen, 120 Bilder | 0 Abweichungen/Fallbacks |
| `recvx-runtime --headless --in-game --room RM_1090.RDX --renderer vulkan` | Vulkan aktiv, Bild == Metal (byte-identisch), `--nearest` == Software |
| MoltenVK-Log (Level warn) | keine Fehler/Warnungen, 7 Shader `MathMode Safe` |
| `RECVX_VULKAN_LIBRARY=/nonexistent` | SKIP 77 mit Begründung |
| Windows x64 (Zig, mit Homebrew-Headern) | `gpu_raster_vulkan.cpp` kompiliert warnungsfrei |
| `check_no_original_core.py` | PASS |

## Unabhängiges Review (behoben)

Shader-Semantik (RASTER/MASK/COMPOSITE/RESOLVE gegen GL/D3D11) und Interlock-Abschnitte bestätigt. Behoben:
F1 Texturkopie galt nach einem Fehlbild als hochgeladen (jetzt erst nach Einreichung; Test „frame after a
failed frame“, Mutante wird rot), F2 Maskenebenen auf `maxImageArrayLayers`/2048 begrenzt, F3
`shaderSignedZeroInfNanPreserveFloat32` verlangt und `VK_KHR_shader_float_controls` immer aktiviert,
F4 Textur hält das Gerät (kein `vkDestroyDevice` vor Texturen), F5 SKIP nur für benannte
Nichtverfügbarkeitsgründe, F6 INI-Roundtrip `vulkan` in `third_person_camera_mod`, F7 Crossbuilds suchen
keine Host-Header. `RECVX_ENABLE_VULKAN=OFF`: Build grün, Test SKIP 77.

## Offen

- Windows-/Linux-Treiber: Interlock-Verfügbarkeit und Bitgleichheit je Treiber ungeprüft (AMD bietet
  Interlock nur auf Teilen der Hardware; ohne ihn Softwarepfad).
- Windows-Crossbuild hat keine Vulkan-Header → dortige Pakete ohne Vulkan, bis Header bereitgestellt werden.
- Interaktiver Fensterlauf und Controller nicht geprüft; Dev-App/Starter/Pakete nicht ersetzt.
- Mac-Pakete bündeln kein MoltenVK; Vulkan dort nur mit installiertem Loader/MoltenVK.
