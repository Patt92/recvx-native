# Optionales OpenGL-Backend

## Stand

2026-10-21, Arbeitsbaum auf `main`. Implementiert und kompiliert, **keine GL-GPU-Abnahme**: Diese Sitzung erreicht keinen Displayserver. Kein Linux-Hardwarelauf, kein Windows-Hardwarelauf, keine GL-Leistungsmessung.

Auf Nutzerauftrag zuerst Sicherungscommit `c4e1172` (abgeschlossene Windows-CMake-Erkennung, 18 Wrappertests grün), zunächst auf `feat/opengl-backend`. Nach ausdrücklicher Korrektur auf `main` vorgezogen; der gemeinsam benutzte Branch enthielt inzwischen auch den fremden Audio-Commit `823f0b9`. Die GL-Arbeit wurde danach auf main fortgeführt und hier nicht erneut committet/gepusht. Laufende fremde Audio-/Policy-/Windows-Dokumentationsänderungen erhalten.

## Umsetzung

- `native/gpu_raster_opengl.cpp`: Core-4.1-Backend auf dem bestehenden `GpuRaster`-Vertrag. Linux nutzt es als `gpu`-Standard; macOS Metal und Windows D3D11 bleiben Standard. Optional explizit `--renderer opengl`, INI `renderer = opengl`, Debugwahl.
- Eigener versteckter SDL-GL-Kontext, vorheriges Kontext-/Fensterpaar und geänderte SDL-Erzeugungsattribute wiederherstellen. SDL-Video mit eigener Referenz, globales Runtime-`SDL_Quit` hinter alle GL-Besitzer verlegt. Kein fremder SDL-Präsentationskontext übernommen.
- Dieselben `raster::Setup`/`ShadowWork` vom nativen Renderer, keine zweite Spielwelt. Keine Originalquellen kompiliert oder als Funktionskörper eingezogen.
- Gepackte Integerfarbe inklusive Alpha, native Float-1/z und getrennte transportierte Schattentiefe. Bounding-Rechtecke dienen nur zur Shaderausführung; Abdeckung, Kantenregel, Tiefe, Modulation, Nebel und Blendgleichung kommen aus denselben Operanden wie der Bandstufe.
- GL4.1: vor geordneten Drawgruppen betroffene MRT-Rechtecke in getrennte Leseflächen blitten. Nur aufeinanderfolgende Setups mit gleicher Textur/Sampler und paarweise disjunkten Rechtecken werden zusammengefasst; keine Umordnung. GL4.5/ARB_texture_barrier ersetzt beim Farbrastern die Kopien durch Same-Pixel-Read/Write mit Barriere zwischen Gruppen. `force_copy` erlaubt getrennte Diagnose des Basispfads.
- Schattenmasken 640×480, ±32 und Sättigung/Maskierung je Fläche, getrennte Werke, beide Modifierphasen. Komposite erhalten Zielalpha und Tiefen. Resolve explizit nach vorhandenen SSAA-Regeln. CPU-Linien, Overlays, Monitor-Rendertexturbesitzer und SDL-Präsentation bleiben unverändert.
- Texturkopien aus bestehenden Pixelständen/CPU-Mips, kein `glGenerateMipmap`. Instanzeindeutiger Cachebesitz verhindert API-/Kontextverwechslung auch bei wiederverwendeten Objektadressen. GL-Texturlöschung erfolgt verzögert im eigenen Kontext; beim Kontextabbau gibt der Treiber dessen Ressourcen frei.
- `RECVX_ENABLE_OPENGL` / `./build.sh --opengl on|off`, standardmäßig an. SDL für GL auch außerhalb des Runtimeblocks verfügbar. Windows-GPU-Test/Compare erhalten SDL-DLL-Kopie. `--platform linux` und gezielter CTest-Durchgriff `--test-regex` ergänzt.
- GPU-Paritätstests prüfen jetzt tatsächliche GPU-Ausführung/Submission statt möglichem Software-PASS; Shader-/Programm-/FBO-Initialisierungsfehler sind keine SKIPs. Zusätzliche Fälle für SSAA 3/4, leere Bilder, Zielgrößenwechsel, koplanare Flächen/Kanten, NPOT, Pixelmutation, API-/Kontextwechsel und Modifierphasen.
- Vergleichswerkzeug unterstützt Backendwahl, erzwungenen Kopierpfad, Fallbackfehler und Fehlerstatus bei Punktabweichung. CPU-/Wallzeitmessung erhalten; Werkzeug in dieser Sitzung nur C++-syntaxgeprüft, nicht als GL-Benchmark ausgeführt.

## Semantikbezug

Ausführbare native Referenz: `renderer.cpp` Bandstufe/Phasen/Resolve, `chunk_blend.cpp`, `shadow_raster.h`, `shadow_mask.h`, `raster_setup.h`. Originalreferenzen ausschließlich lesend: `ps2_dummy.c:543` (GREATER 0/KEEP, GEQUAL), `:1980` (OT/FB_ONLY), `ps2_NaDraw.c:421` (Textur-/Nebeloperanden), `game.c:318/417/427` (Modifier-/Spiegelphasen). Keine neue Originalfunktion als done im Ledger: dies ist ein zusätzliches Hostbackend bestehender Mechanismen.

GL-Synchronisationsreferenz: https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_barrier.txt (Same-Pixel-Read/Write, ein Lesen/Schreiben je Texel eines Draws; Barriere vor nachfolgendem Zugriff). Kein gewöhnliches GL-Blending, ungeordnete Image-Stores oder Fragment-Spinlocks als Ersatz.

## Tatsächlich geprüft

| Prüfung | Ergebnis | Beleg |
|---|---|---|
| Originalkern-Gatter vor Builds, nach Build und am installierten Binary | PASS im geprüften Policyumfang, kein Gameplaynachweis | Buildlogs / direkter Abschlusslauf |
| Python-Buildwrapper | 20 Tests grün | `tests/build_native_test.py`, zusätzlich CTest `development_app_packaging` |
| Mac arm64 GL an | Build grün; 11 gezielte Tests bestanden, 3 GPUtests SKIP | `build/validation/opengl-final-build.log` |
| Letzte Integrationsänderungen Mac | Build grün; 3 Tests bestanden, 3 GPUtests SKIP | `build/validation/opengl-final-refresh.log` |
| Mac arm64 GL aus | Build grün; 2 Tests bestanden, Plattform-GPUtest SKIP | `build/validation/opengl-disabled-build.log` |
| Windows x64 GL an | Crossbuild und Policy grün, nicht ausgeführt | `build/validation/opengl-windows-final.log` |
| GL-Kontext direkt | Exit 77: `OpenGL: no video (The video driver did not add any displays)` | direkter `recvx-gpu-raster-tests data --backend opengl` |
| Titel ohne `--in-game` über `Start.command` | Exit 0, Titel nach 90 Takten sichtbar, `title_active=true`, mode 6; **Softwarefallback** | `build/validation/opengl-runtime/title90.{log,json,bmp,png}` |
| RM_0020 über `Start.command` | Exit 0, Raum/Spieler sichtbar, 0 blockierte Befehle; **Softwarefallback** | `build/validation/opengl-runtime/room.{log,json,bmp,png}` |
| Installierte Dev-App | arm64, `codesign --verify --deep --strict` grün; vom aktuellen Starter bevorzugt | `build/start/macos-arm64/RECVX Native.app` |
| Portstatus | `tools/port_check.py` ausgeführt | `build/validation/opengl-port-status.log` |

Die GPU-Tests heißen `gpu_raster_parity`, `gpu_raster_opengl_parity`, `gpu_raster_opengl_copy_parity`. Alle drei konnten mangels Display/Gerät nicht rastern. Die übrigen gezielten Tests: Entwicklungspaketierung, Videoeinstellungen, Frontend, Chunk-Blending, Texturfilter/Clamp, Scene-Renderer und Schattenarithmetik/-raster/-tiefentransport/-komposite. Kein kompletter Raumscan, keine breite Gameplayabnahme.

## Auslieferung und Grenzen

Der bestehende Buildwrapper hat die gesicherte/signierte Start.command-Dev-App aktualisiert, zuletzt mit GL aktiviert. Titel-/Raumprüfung nutzte genau diesen Starter und einen eigenen `--user-dir` unter `build/validation/opengl-runtime`; Benutzer-INIs, Mods, Snapshots und Releasepakete nicht ersetzt. Buildbinary und signiertes Appbinary haben wegen Bundle-Anpassung/Signatur unterschiedliche Hashes; nicht als byteidentisch behauptet.

**Offen:** tatsächliche GLSL-Kompilierung/Link auf dem Gerät, GL4.1-Pixel-/Tiefenparität, moderner TextureBarrier-Pfad, Kontextinteraktion mit sichtbarem SDL-Renderer, reale Wechsel/Freigabe sowie Linux-/Windows-Treiber und Performance. Auch die neu ergänzten GPU-Testfälle sind wegen SKIP noch nicht ausgeführt. Runtime-OFF/GL-an und SDL-loser Runtime-OFF/GL-aus-Build sind strukturell vorgesehen, hier nicht als separate Buildvarianten geprüft.

GLSL `precise` verhindert unerlaubte Kontraktion, garantiert aber nicht allein treiberübergreifende Divisions-/Rundungsbitgleichheit. Keine Toleranzen oder Epsilons eingeführt. Vorhandene gefilterte Hostqualität hat bereits CPU/GPU-Unterschiede und wird nicht als bitgleich ausgegeben. Der Kopierpfad kann besonders bei hoher SSAA teuer sein; ohne Messung keine Beschleunigungsbehauptung.

Sandbox-Grenze nicht über Terminal, anderen Prozess oder deaktivierte Sandbox umgangen. Die noch ausstehende echte GPU-Abnahme bleibt Teil dieses Auftrags, nicht erledigte Arbeit.
