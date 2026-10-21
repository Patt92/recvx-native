# Plan: RECVX Native sparsamer ausführen

Stand: 2026-10-20. Analyse und Umsetzungsvorschlag. Phase A (Messbasis) und erste
Schnitte aus B sind umgesetzt: [performance_phaseA_scalpel_20261020.result.md](../archived/work/performance_phaseA_scalpel_20261020.result.md).
Phase D1 (GPU-Machbarkeit) ist bestanden: [performance_gpu_feasibility_20261020.result.md](../archived/work/performance_gpu_feasibility_20261020.result.md)
— Pixelarbeit 62–73 ms CPU → ≈ 0,2–0,7 ms GPU, Punktabtastung 99,7–99,9 % bitexakt. B1 (Workerzahl)
ist durch `powermetrics` entschieden: weniger Worker kosten mehr Watt und laggen, Automatik bleibt.
Phase D2 (macOS) umgesetzt: [performance_gpu_raster_d2_20261020.result.md](../archived/work/performance_gpu_raster_d2_20261020.result.md).

## Ziel und Empfehlung

Das Spiel soll bei unverändertem Spielverhalten und gleicher gewählter Bildqualität deutlich weniger Energie benötigen. Ein schnellerer Benchmark allein ist kein Erfolg: Entscheidend sind CPU-Zeit und Energie pro realer Spielsekunde bei gehaltenen 30 Spieltakten pro Sekunde.

**Empfohlene Reihenfolge:** reproduzierbare Ausgangsmessung → unnötige CPU-/Präsentationsarbeit entfernen → Datenaufbereitung entlasten → Grafikarbeit auf einen nativen GPU-Renderer verlagern. Die GPU-Machbarkeitsprüfung soll bereits nach der Ausgangsmessung beginnen; sie muss nicht auf jede CPU-Mikrooptimierung warten.

Nicht den Spieltakt senken, Gegner seltener aktualisieren, Effekte entfernen oder die Auflösung stillschweigend reduzieren. Originalgetreue Semantik und effiziente Host-Implementierung sind vereinbar. Die PS2-Hardware und deren Auslastung müssen nicht nachgebildet werden.

## 1. Was der Code tatsächlich zeigt

| Befund | Fundstelle | Bedeutung / Grenze |
|---|---|---|
| Der 3D-Pfad benutzt `SoftwareRenderer`. Rasterisierung, Texturfilterung, Nebel und Blending laufen als C++-Pixelschleifen. | `native/renderer.cpp:162–474`, `native/runtime.cpp:1596` | Ein nativer Programmcode ist hier noch kein GPU-Renderer. CPU-Grafikarbeit ist ein konkreter Hauptverdächtiger; ihr tatsächlicher Lastanteil ist noch nicht gemessen. |
| SDL erhält ein fertiges 640×480-ARGB-Bild über `SDL_UpdateTexture`; danach Clear/Copy/Present. | `native/runtime.cpp:1646–1652,1754–1756` | `SDL_RENDERER_ACCELERATED` beschleunigt hier nicht die 3D-Pixelschleifen. Auch wenn SDL intern Metal nutzt, bleibt deren Arbeit auf der CPU. |
| Ohne video.ini ist High aktiv: Filterung plus 2×2-SSAA. | `native/video_settings.h:14–16`, `native/runtime.cpp:597–599` | Intern 1280×960 = 1.228.800 Rasterpositionen statt 307.200, anschließend Downsampling. Ultra: 1920×1440 = 2.764.800, also neunfache Basisfläche. Das sind Flächenfaktoren, keine gemessenen Laufzeitfaktoren. Im Projektwurzelverzeichnis lag bei der Analyse keine video.ini. |
| Der Standard nutzt `max(4, hardware_concurrency - 2)` Worker, begrenzt durch die Bandzahl. | `native/renderer.cpp:478–502` | Nicht energieorientiert: fast alle Kerne werden angeboten, mit mindestens vier Workern. Neue Threads entstehen und enden bei jedem ausreichend großen Rasterdurchlauf; Schatten-/Spiegelbilder können mehrere solcher Durchläufe haben. Die konkrete Kernzahl wurde hier nicht bestimmt; die sysctl-Abfrage war durch die Sandbox gesperrt. |
| Jedes der 60 horizontalen Bänder prüft die gesamte Setup-Liste. | `native/renderer.cpp:478–495` | Wiederholte Arbeit auch für Dreiecke außerhalb des jeweiligen Bands. Vorab geordnete Bandlisten können dieselbe Zeichenreihenfolge erhalten. |
| Pro verarbeitetem Dreieck entstehen mehrere temporäre Vektoren für Transformation/Clipping. | `native/renderer.cpp:244–281` | Konkreter Allokationskandidat. Begrenzte Scratch-Puffer erlauben denselben Algorithmus ohne Heap-Allokation je Dreieck. |
| Das Grundbild der Szene wird pro Komposition kopiert; danach folgen Modell-/Effektaufbereitung und Lichtarbeit. | `native/runtime.cpp:564–580`, `native/world.cpp:4033ff` | Relevanter Messbereich, aber kein Nachweis, dass er die Hauptlast ist. Texturpixel sind bereits Copy-on-Write geteilt (`native/scene.h:18–61`); nicht fälschlich einen vollständigen Pixelkopiervorgang pro Texturkopie behaupten. Auch der Cache-Fingerprint ist bereits O(1), kein Pixelhash. |
| Die Simulation läuft bereits fest mit 30 Hz; die Ausgabe läuft separat über VSync. | `native/timing.h:5–8`, `native/runtime.cpp:1692–1702,1754–1759` | Kein Beleg für unbegrenzt schnell laufende Spielsimulation. Present/Clear/Copy laufen jedoch auch ohne neues Bild. SDL-Ereignisse markieren pauschal das Bild als dirty. VSync-Fallback und Hochfrequenzdisplays müssen separat gemessen werden. |
| Der Buildcache ist Release, arm64, mit `-O3 -DNDEBUG`. | `build/macos-arm64/CMakeCache.txt` | „Einfach Release einschalten“ ist keine fundierte Lösung. Das belegt die Buildkonfiguration, nicht die Herkunft jedes installierten Bytes. |

**Startstand:** Start.command bevorzugt aktuell die vorhandene Dev-App unter `build/start/macos-arm64/RECVX Native.app`. Deren Runtime und `build/macos-arm64/recvx-runtime` haben unterschiedliche SHA-256-Hashes. Deshalb muss die Baseline mit der tatsächlich gestarteten App erhoben werden, nicht mit einem beliebigen Build-Binary.

**Nicht erhoben:** CPU-/GPU-Leistungsanteile, Watt, Energie pro Minute, Temperaturverlauf und thermische Drosselung. Aus der Codeanalyse lässt sich kein seriöser Prozentwert für die erwartete Einsparung und kein gemessener Vergleich mit RE4 Remake ableiten.

## 2. Phase A — Ausgangslast sauber messen

### A1: Reproduzierbarer Stand

- Starterziel, Binaryhash, Daten-/Modstand, Bildschirmfrequenz, Fenstergröße und effektive Videoeinstellungen festhalten.
- Vorhandene lokale Änderungen erhalten. Keine App ersetzen und keinen kompletten Build allein für die Diagnose anstoßen.
- Für spätere A/B-Läufe unveränderte Baseline-Artefakte sichern; Vorher und Nachher aus demselben Quellstand abgesehen von genau der zu messenden Optimierung bauen.
- Messartefakte unter `build/validation/performance-20261020/`, Ergebnisbericht unter `work/`. Keine Benutzer-INIs, Speicherstände oder Snapshots überschreiben.

### A2: Kleine feste Szenenauswahl, kein kompletter Raumscan

1. Titel/Host-Pause: Leerlauf- und Präsentationskosten.
2. Normales Gameplay mit freier Bewegung und Kameraschnitt.
3. Gegner-/Effektszene aus einem bestehenden Replay oder reproduzierbaren Spielstand.
4. Konkrete Grafikpfade mit Schatten, Spiegeln oder Monitor-Renderziel, soweit vom Umbau betroffen.
5. Inventar/Karte sowie ein Türübergang als separate Betriebsarten.

Die Szenen werden nach derselben Ausgangsaufnahme und identischen Eingaben abgespielt. Headless-/Replay-Läufe ausdrücklich mit `--enemy-ai on` und dokumentiertem Grab-Modus durchführen, sonst wird eine künstlich billige Welt gemessen.

### A3: Messgrößen und Werkzeuge

- Reale SDL-Laufzeit: Spielticks, vollständige Renderdurchläufe und Presents getrennt zählen; p50/p95/p99 von Frame- und Abschnittszeiten, verpasste 33,33-ms-Budgets.
- CPU-Zeit des gesamten Prozesses einschließlich Worker: User + System. Zusätzlich CPU-ms pro Tick/Bild und CPU-Sekunden pro realer Spielminute. Mehr Threads können die Framezeit verbessern und trotzdem mehr Energie verbrauchen.
- Passiv aggregierte Zeitbereiche: Event-/Spielsimulation, Pose-/Szenenaufbau, Licht, Setup/Clipping, Rasterisierung, Schatten, Downsampling, Audio, Texturupload, Present und Warten. Instrumentierung nicht pro Pixel und nicht als dauernder Logstrom.
- Arbeitszähler: Dreiecke/Setups pro Pass, Bandzuordnungen, Workerzahl/Threadstarts, Cache-Neuaufbauten, temporäre Allokationen, Renderzielgrößen und übertragene Bytes.
- Time Profiler und anschließend passende Energie-/GPU-Messung über Instruments bzw. auf dem Gerät verfügbare Systemwerkzeuge. Messmöglichkeiten und notwendige Nutzerfreigaben erst prüfen; CPU-Prozent nicht in Watt umdeuten.
- Für thermische Bewertung nach Aufwärmphase mehrere längere Wiederholungen, vergleichbare Helligkeit, Stromversorgung und Hintergrundlast; Baseline und Variante abwechselnd messen. Nicht während Builds messen.

Das vorhandene `tools/benchmark_renderer.cpp` wird erweitert statt ein paralleles Benchmark-System zu erfinden: CPU-Zeit, feste Workerzahlen, Filter-/SSAA-Auswahl und Streuung ergänzen. Es misst wiederholte statische Raumgeometrie, **nicht** das gesamte Gameplay. Wichtig: `--headless --frames N` der Runtime zeichnet derzeit regulär nur das Endbild (`runtime.cpp:1634`); seine Gesamtdauer ist kein N-Bilder-Rendererbenchmark. Eine Diagnoseoption für jedes Bild muss die existierenden Zeichen-/Zustandsübergänge beachten.

**Ausgang dieses Schritts:** Lastbudget nach Mechanismus, nicht nur ein FPS-Wert. Danach die tatsächliche Priorität der CPU-Arbeiten festlegen.

## 3. Phase B — Geringes Risiko, unverändertes Bild

### B1: CPU-Parallelität nach Energie statt Kernauslastung

- Feste, begrenzte Workerzahlen 1/2/4 und den bisherigen Auto-Modus bei identischem Bild vergleichen.
- Einen persistenten Workerpool statt Thread-Erzeugung und Join pro Pass einsetzen. Zwischen den bestehenden Zeichenphasen echte Synchronisationsbarrieren behalten; untätige Worker schlafen, kein Spin-Wait.
- Den energieärmsten Modus wählen, der die gemessenen Bildbudgets mit Reserve hält. Nicht blind „ein Kern“ oder „alle Kerne“ festschreiben; keine unnötige automatische Regelung während der Simulation.
- Workerwahl betrifft nur die Darstellung, nie Tickzahl oder die Reihenfolge von Spielzustandsänderungen.

**Abnahme:** identische Farbe und Tiefe für alle geprüften Workerzahlen, stabile Framezeiten sowie weniger CPU-Zeit/Threadstarts. Niedrigere Parallelität allein zählt nicht als Einsparung, wenn CPU-Zeit/Energie nicht sinkt.

### B2: Rastervorbereitung und Speicherarbeit

- Vorab pro Band die überlappenden Setups zuordnen, in exakt bisheriger Reihenfolge und mit denselben Phasengrenzen. Nicht transparente Geometrie zur Beschleunigung neu sortieren.
- Heap-Vektoren je Dreieck durch ausreichend große begrenzte Scratch-Puffer mit nachgewiesenen Clippinggrenzen ersetzen.
- Setup-, Band- und Renderzielpuffer wiederverwenden; Mip-/Texturrevisionen und Raumfreigabe erhalten.
- Den Pixelschleifen nur nach Profilnachweis spezialisierte Pfade für häufige Materialkombinationen geben. SIMD nur mit unveränderten Rundungs-, Kanten-, Alpha- und Tiefenentscheidungen. Kein globales Fast-Math und keine Änderung der Simulations-Floatregeln.

**Abnahme:** Pixel-/Tiefendifferenz null zur Baseline bei gleicher Qualität; Randfälle für Near/Far, Punch-through, Nebel, Blendpaare und Schatten. Jede Maßnahme einzeln messen.

### B3: Präsentationsschleife und echte Ruhephasen

- Presentrate auf 60/120-Hz-Displays und beim SDL-Softwarefallback erfassen. Nur neue Bilder bzw. notwendige Fensterwiederherstellungen präsentieren; keine nutzlosen wiederholten Clears/Copies/Presents und Fenstertiteländerungen.
- Ereignisse nach Relevanz invalidieren statt jeden Mouse-/Window-Event als neue 3D-Szene zu behandeln.
- Bis zum nächsten Input-/Audio-/Spieltakttermin ereignisgesteuert schlafen statt nur `SDL_Delay(1)` als allgemeinen Fallback zu verwenden. Eingabelatenz und Audio-Puffergrenzen abnehmen.
- Titel, Pause und statische Menüs getrennt behandeln. Blinkzustände/Fades weiter korrekt aktualisieren. Ein stehender Spieler ist keine stehende Welt.
- Hintergrund-/Minimierungsverhalten als explizite Host-Entscheidung behandeln: keine neue automatische Spielpause ohne Freigabe. Zunächst nur nicht benötigte Ausgabe vermeiden, keine Zustandsfortschreibung auslassen.

**Wichtig:** Zeichenarbeit ist derzeit nicht durchweg zustandsfrei: `player_sleep_draw()` zählt herunter (`world.h:336`), bestimmte Effekt-Zeichenpässe schreiben Zustand (`world.cpp:3837–3852,4403`). Vor Redraw-Caching oder Renderauslassung müssen diese Fortschreibungen am bestehenden Besitzer exakt einmal am vorgesehenen Pass verbleiben. Ein Present kann ohne diese Weltarbeit wiederholt oder ausgelassen werden; ein kompletter Draw ist nicht beliebig austauschbar.

**Abnahme:** keine verdoppelten/verlorenen Draw-Seiteneffekte, gleiche Timer/Flags/Animationen und Audiofolge, korrekter Titelstart ohne `--in-game`, Fensterwiederherstellung und unveränderte Hotkeys.

## 4. Phase C — Szenen-/Assetaufbereitung nur bei Bedarf

Nur starten, soweit das Profil hier relevante Kosten zeigt:

- Statische Raumgeometrie nicht jedes Bild vollständig als mutable Dreiecksmenge vervielfältigen. Unveränderliche Geometrie und veränderliche Material-/Licht-/Sichtbarkeitsdaten trennen.
- Bereits veröffentlichte Modellbäume für mehrere echte Leser desselben Passes wiederverwenden. Keine zweite Poseautorität und kein Resampling aus dem nächsten Motionzustand bei angehaltenem Pass.
- Cacheinvalidierung an konkrete Änderungen koppeln: Motion, Morph/Face, Knotenflags, Skriptpose, Licht-/Materialzustand, Spiegel-/Kamerapass und Mod-/Texturrevision. Kein Cache allein nach Objekt-ID oder Animationsframe.
- Texturen pro Ressource referenzieren statt wiederholte Tabellenanhänge je Effekt, sofern die Lebensdauer und Palette derselben Quelle erhalten bleiben.
- Audio erst bei nachgewiesenem Anteil optimieren: wiederverwendbare Mixpuffer, konstante Pitch-/Panwerte außerhalb der inneren Sampleschleife, ohne Stimmen-/Hüllkurven-/Sequenz-/Hallfortschritt zu ändern. Kein pauschales Abschalten von Hall oder leisen Stimmen.

**Abnahme:** gleiche Zustandsfolge, Geometrie, Pixel und PCM für die betroffenen Mechanismen; Raumwechsel, Wiedereintritt und Ressourcenänderung gezielt prüfen.

## 5. Phase D — Der strukturelle Schritt: echter GPU-Renderer

**Empfehlung:** Die CPU soll Spielzustand und vorhandene Pose-/Zeichenquelle führen; die GPU soll die massiv parallele Pixelarbeit übernehmen. Der SoftwareRenderer bleibt als Vergleichsreferenz und Diagnosefallback, nicht als versteckter zusätzlicher Vollrender hinter dem GPU-Bild.

### D1: Begrenzter Machbarkeitsnachweis mit Abbruchkriterium

- Ein GPU-Backend als Verbraucher derselben nativen Scene-/Material-/Passdaten, ohne zweite World, Originalkern oder PS2-Hardwaremaschine.
- Auf macOS Metal verwenden. Die Schnittstelle von Beginn an auch für einen nativen Windows-x64-Backendpfad vorsehen; dessen konkrete API mit dem Ressourcen-/Blendbedarf festlegen. Keine neue große Grafikabhängigkeit vor diesem Nachweis einführen.
- Zuerst eine normale aufgezeichnete Szene **und** kleine schwierige Referenzfälle: Punch-through, abweichende Blendgleichungen, Nebelquantisierung, Clipping, Schatten, Spiegelung und Rendertexturen. Kein beeindruckender Einfachraum als alleiniger Erfolgsnachweis.
- Prüfen, was reguläre GPU-Raster-/Blendstufen semantisch exakt leisten. Für nicht passend ausdrückbare Blend-/Tiefen-/Schattenoperationen kontrollierte Shader-/Compute-Pässe prüfen. Native Blendhardware nicht ungeprüft als identisch behandeln; keine PS2-Register-/Befehlsemulation bauen.
- Entscheiden anhand von Bild-/Tiefenvergleich, CPU- und GPU-Zeit, Synchronisationskosten **und Energie bei 30 Hz**, nicht anhand maximaler GPU-FPS.

**Gate:** Wenn schwierige Pfade nur durch sichtbare Abweichungen oder teure Readback-/Synchronisationsschleifen funktionieren, den Ansatz vor dem Vollumbau korrigieren. Weder Abweichungen still akzeptieren noch mit Raumsonderfällen kaschieren.

### D2: Vollständige Anbindung nach erfolgreichem Gate

- Texturen und unveränderliche Geometrie auf der GPU halten, revisionsbasiert aktualisieren; keine vollständigen Uploads pro Frame.
- Dynamische Geometrie/Materialdaten über wiederverwendete Streamingpuffer übertragen, begrenzte Frames-in-flight und korrektes Ressourcenrecycling.
- Opaque-, Modifier-/Schatten-, Spieler-, Spiegel-, Wasser- und Transparenzreihenfolge sowie Sonderansichten/Monitor und UI vollständig erhalten.
- 640×480-Basisbild und die vorhandenen freiwilligen Qualitätsstufen beibehalten. Kein GPU→CPU→GPU-Rückweg pro Bild; CPU-Overlays gezielt migrieren oder effizient zusammensetzen. Readbacks für Screenshots und Testvergleiche, nicht als Standardausgabe.
- Beide Plattformen gezielt abnehmen. Ein Mac-Spike ist kein Windowsnachweis.

Der GPU-Pfad ist voraussichtlich der größte strukturelle Hebel. Seine tatsächliche Einsparung bleibt bis zur Messung offen: auch eine GPU kann durch unnötig viele Frames, Uploads oder Synchronisationen verschwenderisch betrieben werden.

## 6. Qualitäts-/Akkumodus ist eine separate Option

Zur unmittelbaren Diagnose ist die vorhandene Einstellung **Low (filter)** sinnvoll: Sie behält die Filterung und schaltet das zusätzliche SSAA ab. **Original** schaltet zusätzlich die Host-Filterung ab. Das ändert die Bildqualität, nicht den 30-Hz-Spieltakt, und ist ausdrücklich kein Ersatz für die Optimierung.

Keine Benutzerkonfiguration automatisch ändern. Später kann ein bewusst gewähltes Akku-Profil Qualität und Rendererbudget bündeln. Kein stilles dynamisches Herunterschalten und keine erzwungene niedrige Qualität als angeblicher Effizienzfix.

## 7. Erfolgskriterien und Auslieferung

- **Gleichheitsbedingung:** Spielticks, RNG-Aufrufreihenfolge, Events, Bewegung, KI, Kollision, Timer, Draw-Seiteneffekte und Audio bleiben unverändert. Bestehende Host-Funktionen bleiben erhalten.
- **Messziel, keine Zusage:** zunächst mindestens 50 % weniger CPU-Zeit pro realer Spielminute im repräsentativen Gameplay bei gleicher Qualität anstreben. Für die GPU-Umstellung zusätzlich klar geringere gemessene Energie verlangen; sinkende CPU-Last allein reicht nicht.
- **Flüssigkeit:** 30-Hz-Ausgabe ohne schlechtere p95/p99-Frametimes, Audioaussetzer oder zusätzliche Eingabelatenz. Abschnittsbudgets nach Phase A festlegen, keine geratenen Millisekundenwerte als Ist-Zahlen ausgeben.
- **Leerlauf:** keine dauernde 3D-Vollberechnung für ein nachweislich unverändertes Pausenbild. Audio und beabsichtigte UI-Animationen als Restarbeit getrennt ausweisen.
- Für CPU-Umbauten exakte Farb-/Tiefenvergleiche; für GPU-Pfade Unterschiede einzeln klassifizieren und die semantischen Fälle gezielt prüfen. Keine globale Bildfehlertoleranz, die Blend-/Clipfehler verdeckt.
- Vorher/Nachher je Änderung, mit Bildqualität, CPU-Zeit, Frameverteilung und Energiebeleg. Nicht nur „FPS höher“ oder „Lüfter leiser“ melden.
- Ausschließlich betroffene Suiten und konkrete Abhängigkeiten prüfen. Rote Nachbarsuiten an einem echten Vorherstand belegen. Kein pauschaler Vollscan.
- Vor jedem Build Policyprüfung, regulärer Build über `./build.sh`; kein eigener Buildbaum. App-/Paketänderung erst nach Umsetzungs-/Auslieferungsauftrag, Bestand sichern und Benutzerdaten erhalten. Nach Installation tatsächlich gewählte Start.command-Runtime einschließlich Titelstart verifizieren.

## Nächster sinnvoller Arbeitsauftrag

**Phase A plus B1/B2 als erstes begrenztes Paket:** Messfähigkeit herstellen, die aktuelle CPU-Arbeit aufschlüsseln und Worker-/Rastervorbereitung einzeln optimieren. Parallelität der Arbeit ist hierfür nicht erforderlich; keine Unteragenten ohne ausdrücklichen Auftrag. Danach Messbericht mit klarer Empfehlung zum GPU-Gate und den verbleibenden Hauptkosten.

In dieser Planung wurden keine Enginequellen, Einstellungen oder Apps verändert, keine Builds gestartet und keine Leistungs-/Energiegewinne als gemessen ausgegeben. Die Architektur-Pflichtprüfung wurde ausgeführt und meldete PASS im geprüften Umfang.
