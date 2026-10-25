# Optionales Direct3D-12-Backend des Host-Rasterers

## Stand

2026-10-25, Arbeitsbaum `main`, nicht committet. Implementiert und im Windows-x64-Crossbuild (Zig/mingw)
übersetzt; **auf Windows-Hardware und WARP noch nicht ausgeführt** — Parität ist offen. Auftrag des
Nutzers: zuerst DX12 bauen, FSR als TODO (nach der Machbarkeitsanalyse, dass FSR 4 DirectX 12 braucht).

## Architektur

- `native/gpu_raster_d3d12.cpp`; Fabrik `create_d3d12_raster` (`gpu_raster.h`), Stub in
  `gpu_raster_none.cpp` (andere Plattformen / `RECVX_ENABLE_D3D12=OFF`). Nie Plattformstandard (bleibt D3D11).
- HLSL aus `gpu_raster_d3d11.cpp` bytegleich nach `native/gpu_raster_hlsl.h` verschoben; beide Backends
  kompilieren denselben Quelltext (D3D11: SM 5.0, D3D12: SM 5.1), `D3DCOMPILE_IEEE_STRICTNESS` wie bisher.
- Ordnung über Rasterizer Ordered Views (`D3D12_FEATURE_D3D12_OPTIONS.ROVsSupported`), sonst kein Backend.
  D3D12 synchronisiert UAV-Zugriffe zwischen Zeichenaufrufen nicht wie D3D11: nach jedem Draw eine
  UAV-Barriere (Gegenstück zu den Fragment-Barrieren des Vulkan-Backends). Die ROV-Ordnung gilt je Aufruf.
- Root-Signatur: vier 32-Bit-Konstantenblöcke (b0 Frame, b1 Composite, b2 Resolve, b3 Draw), SRV-Tabelle
  t0..t3, UAV-Tabelle u0..u5, Abtastertabelle s0 (8 Abtaster im Sampler-Heap, Index wie D3D11
  `linear*4+clamp_u*2+clamp_v`). Ein Bild = eine Direct-Befehlsliste; Zaun vor dem Rücklesen.
- Deskriptoren: nicht sichtbarer Heap für die sechs UAVs (Clear braucht CPU- und GPU-Handle), sichtbarer
  Heap je Bild als Bump-Allokator (UAV-Blöcke, 4 SRVs je Batch). Wächst die Maske mitten im Bild, entsteht
  ein neuer UAV-Block; bereits aufgezeichnete Befehle behalten den alten.
- Maske `R32_UINT` (typisierte UAV-Loads überall garantiert, Werte 0..255 unverändert; wie Vulkan).
  Ausgabe über UAV-Puffer → Kopie in READBACK-Puffer → Map.
- Texturen: DEFAULT-Heap, Upload über Staging-Puffer in derselben Befehlsliste. Cache-Besitz erst nach
  erfolgreicher Ausführung (`pending_owner_`); ein nach dem Aufzeichnen gescheitertes Bild setzt die Slots
  zurück (Testfall „frame after a failed frame differs“).
- Dreieckspuffer der Masken je Phase neu (der vorige Pass ist aufgezeichnet, nicht ausgeführt).
- Laufzeit-Laden von `d3d12.dll`, `dxgi.dll`, `d3dcompiler_47.dll`; kein Linken. `RECVX_D3D12_DRIVER=warp`
  (EnumWarpAdapter), `RECVX_D3D12_DEBUG=1` (Debug-Layer über minimale `ID3D12Debug`-Deklaration, da
  mingw kein `d3d12sdklayers`-Interface liefert).
- Auswahl: `RenderBackend::D3D12` (Debugmenü-Index 4), `video.ini renderer = d3d12`, `--renderer d3d12`,
  Debugmenü „Direct3D 12“. CMake `RECVX_ENABLE_D3D12` (Windows, Standard an), `build.sh`/`build_windows.py
  --d3d12 on|off`. Tests `gpu_raster_d3d12_parity`, `gpu_raster_d3d12_warp_parity`; `recvx-gpu-compare
  --backend d3d12`.

## Prüfung

- Policy-Gate `tools/check_no_original_core.py`: PASS.
- Windows-x64-Crossbuild: siehe Build-Log im Abschlussbericht der Sitzung (Zig 0.16, mingw-Header
  `d3d12.h`/`dxgi1_4.h` der Toolchain).
- macOS-Build unverändert (Stub); Testlauf dort ohne Windows-Backend.
- **Offen:** Ausführung auf Windows (Hardware + WARP), Bitgleichheit Synthetik/Räume, Bild im Fenster,
  Zeiten je Bild (UAV-Barrieren je Draw sind ein messbarer Posten).

## FSR — TODO

Festgehalten in `docs/NATIVE_PORT.md`, Abschnitt „FSR 3.1 / FSR 4 (not integrated)“: D3D12-Präsentations-
stufe (DXGI-Swapchain im SDL-Fenster, Bild bleibt auf der GPU, 2D-Ebene als UI-Textur), FidelityFX-SDK als
optionaler Build-Eingang, Objekt-Bewegungsvektoren, Jitter, Reaktivmaske, Historienreset je Schnitt, Frame
Generation mit Präsentations-Thread, Validierung auf Hardware/WARP. Erst danach eine Einstellungszeile.
