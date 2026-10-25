# FSR-4-Präsentationsstufe (Windows, Direct3D 12)

## Stand

2026-10-25, Arbeitsbaum `main`, nicht committet. Auftrag: FSR 3/4 einbauen; nach Sichtung des SDK auf
Windows/D3D12 mit FSR 4 beschränkt (Nutzerentscheidung), Vulkan für Linux/macOS bleibt TODO.
Implementiert und im Windows-x64-Crossbuild (Zig/mingw) übersetzt, macOS-Build und Settings-Tests grün;
**auf einer Windows-GPU noch nicht ausgeführt**.

## Variantenwahl

- Vorliegend: FidelityFX SDK 2.1.0 (`/Volumes/SD-Daten/Downloads/FidelityFX-SDK-2.1.0`, Upscaler-API
  4.0.3, signierte DX12-DLLs) sowie ein Community-Paket mit SDK-2.0-DLLs und einer INT8-Variante der
  Upscaler-DLL (`FSR4 INT8`). Das SDK enthält nur DX12-DLLs; der Vulkan-Pfad (FSR 3.1) ist nicht dabei.
- Entscheidung: ffx-api (die 5 C-Funktionen der DLLs) zur Laufzeit per `LoadLibrary`/`GetProcAddress`,
  wie von AMD empfohlen und mit dem Zig/mingw-Crossbuild verträglich (keine `.lib`). Ins Repo kommen nur
  die MIT-Header (`native/third_party/FidelityFX-SDK/include`, 37 KB, `HEADERS.sha256`) und `LICENSE.md`;
  DLLs sind per `.gitignore` ausgeschlossen (Nutzerwunsch: keine hunderte MB im Repo).
- FP8 und INT8 als getrennte Upscaler-Werte (`fsr4`, `fsr4-int8`); INT8 ist dieselbe DLL mit
  `fsr4_model_v07_i8_*`/dp4a statt `fsr4_model_v07_fp8_*`, gedacht für GPUs ohne FP8-Matrixpfad.

## Architektur

- `native/fsr_present_d3d12.cpp`, Fabrik `create_fsr_presenter(hwnd, sdk_dir, initial, reason)` in
  `metalfx_present.h` (Schnittstelle `MetalFxPresenter` unverändert; Stub auf Nicht-Windows).
- Eigenes D3D12-Gerät (FL 12.0, erster Hardware-Adapter), Direct-Queue, zwei Befehlsallokatoren
  (Bilder in Arbeit), DXGI-Flip-Swapchain (3 Puffer, `DXGI_SWAP_EFFECT_FLIP_DISCARD`, Tearing bei
  `frame_rate = unlimited` wenn `DXGI_FEATURE_PRESENT_ALLOW_TEARING`), `MakeWindowAssociation(NO_ALT_ENTER)`.
- Präsentations-Thread wie die Metal-Stufe (`run()`: Bild übernehmen, sonst Wiederholung im Zeitfenster;
  `slots` aus `presents_per_tick`), Hauptthread kopiert nur das Bild.
- Kette: Upload (Farbe/Szenenbild/1/z über einen Upload-Puffer je Bild) → `motion_cs` (Port von
  `motion_k`: umgekehrte endliche Tiefe `a*(1/z)-b` aus njClipZ near/far, Kamera-Reprojektion mit
  Vorbildprojektion, 2D-Pixel = Endbild≠Szenenbild → Tiefe 1, Bewegung 0, Reaktiv 1) → `ffxDispatch`
  Upscale → `present_cs` (Port von `present_k`: Schärfen, CRT light/full, 4:3-Einpassung; Bloom-Kette
  `bloom_down_cs`/`bloom_blur_cs` für CRT full) in den Backbuffer → `Present`.
- ffx-api: `GET_VERSIONS` am Loader (Upscaler-Provider FSR 4 / FSR 3.1.x / FSR 2.x), Kontext mit
  `ffxCreateBackendDX12Desc` + `ffxCreateContextDescUpscaleVersion` (SDK-2.1-Pflicht) +
  `ffxOverrideVersion`; erster Eintrag zuerst, bei Fehlschlag der nächste (FSR-3.1-Fallback), Name per
  `GET_PROVIDER_VERSION`. Flags `DEPTH_INVERTED | NON_LINEAR_COLORSPACE`. Dispatch: `jitterOffset` 0,
  `motionVectorScale` 1 (Pixel, aktuell→vorig), `renderSize` 640×480, `upscaleSize` = Einpassung,
  `frameTimeDelta` aus dem Takt, `reset` bei Schnitt (`scene_key`), Kamera near/far/FOV aus der Projektion.
  Nach `ffxDispatch` werden Wurzelsignatur und Heaps der Stufe neu gebunden.
- DLL-Suche: `RECVX_FSR_SDK`, Ordner der Runtime, `<Runtime>/FidelityFX-SDK`,
  `native/third_party/FidelityFX-SDK` (Dev-App). Upscaler-DLL der gewählten Variante wird vor dem Loader
  per Pfad geladen (der Loader lädt sie per Namen → trifft das Modul); Wechsel FP8↔INT8 nach Neustart,
  Statuszeile meldet es. Loader-Name SDK 2.x `amd_fidelityfx_loader_dx12.dll`, SDK 1.x/Community
  `amd_fidelityfx_dx12.dll`.
- Build: CMake `RECVX_ENABLE_FSR` (Windows, Standard an), `build.sh`/`build_windows.py --fsr on|off`;
  vorhandene DLLs werden nach dem Build neben die EXE kopiert, `package_native.py` nimmt sie (plus
  `FIDELITYFX-LICENSE.md`) ins Windows-Paket.
- Einstellungen: `Upscaler::Fsr4`, `Fsr4Int8`; `video.ini upscaler = fsr4|fsr4-int8`; OPTION-Zeile
  „Upscaling“; Debugmenü-Einträge 3/4; Test `third_person_aim_camera` prüft das Durchschalten (5 Werte).
- Runtime: auf Windows `create_fsr_presenter` statt MetalFX (HWND per `SDL_GetWindowWMInfo`);
  Startmeldung „Presentation: FSR stage available“, Abschlussbericht „Presentation stage: …“.

## Prüfung

- `tools/check_no_original_core.py`: PASS.
- Windows-x64-Crossbuild: `fsr_present_d3d12.cpp.obj` ohne eigene Warnung (eine `-Wswitch` aus AMDs
  `ffx_api_dx12.h:244`), `recvx-runtime.exe` gelinkt, DLLs + `FSR4 INT8/` neben der EXE.
- macOS `./build.sh --test-regex`: 7/7 grün (Settings-Test mit den neuen Upscaler-Werten).
- **Offen:** Lauf auf Windows-GPU (Bild, Reset bei Schnitten, FP8/INT8, FSR-3.1-Fallback auf
  Nicht-RDNA4, Tearing); Objekt-Bewegungsvektoren; Subpixel-Jitter; Frame Generation
  (`amd_fidelityfx_framegeneration_dx12.dll` wird kopiert, aber nicht angesprochen); Mip-Bias.

## Nicht gemacht (TODO)

- Vulkan (Linux, macOS/MoltenVK): FSR 3.1 braucht `ffx_api_vk.h` und die FSR-3.1-Quellen/SPIR-V des SDK,
  die im vorliegenden Stand fehlen. OpenGL: kein FSR-Pfad.
