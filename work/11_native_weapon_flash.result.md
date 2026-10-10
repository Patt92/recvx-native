# Native Waffen-Effekte und Mathematik (Codex, 2026-10-09)

## Grenze und Ausgangsbefund

Natives eigenständig laufendes Game auf macOS arm64/Windows x64; die
Originalquellen sind ausschließlich Semantikreferenz. Kein Originalkern als
Engine, kein zweiter Actor-/Workbesitzer und keine Hardware-Nachbildung.

Vor Beginn geprüft: Die aktuelle CMake-Konfiguration hatte die großen
`recvx_original_engine`-/`recvx_core_enemy_world`-Ziele bereits entfernt. Der
alte `build/native/CMakeFiles/recvx-runtime.dir/link.txt` enthielt beide Archive
aber noch. Zudem waren direkte Originalquellen weiterhin in produktiven
Abhängigkeiten (`item.c`, `ps2_NaMath.c`, `ps2_dvd_image.c`) eingetragen. Diese
Verstöße wurden vor der Portierungsarbeit gemeldet. Claudes paralleler Rückbau
in Actor/World/Face wurde nicht übernommen oder zurückgesetzt.

Kein produktiver Runtime-Link/Start wurde in dieser Arbeit ausgeführt.
Die früheren Gegner-Sweeps über den nun verbotenen Kern gelten hier nicht
als Abnahme des erlaubten nativen Ports.

## Portierung in vorhandene native Besitzer

- `native/weapon_flash.cpp`: Zustand und Zeichnen der sechs `bhEff003`-
  Varianten aus `effsub1.c:1220–2115` auf der bestehenden `WeaponFlash`.
  Kein generierter Funktionskörper, kein `sys`, keine Effekttabelle oder
  `BH_PWORK`-Welt und kein Aufruf von `bhEff003`/`bhDrawWeaponEffect`.
- Originale UV-Rechtecke, Palettenfolge, Farbe, Blendpaare, Lebensdauer,
  Typ-0-Verzögerung, Skalierung, Spokewinkel/-zahl und asymmetrische
  Typ-4-Koordinaten bleiben erhalten. Szene und bestehender nativer Renderer
  sind die einzige Zeichenausgabe; keine Konsolen-Transportpakete.
- Typ 3 verwendet den bestehenden gemeinsamen nativen Original-RNG statt
  `effect_random() == 0`. Der Draw erhält eine ausdrücklich zu bindende
  `bhCheckL2Wall`-Semantikschnittstelle. Ohne diese meldet er die offene
  Bindung; `wall_ray_distance` ist bewusst **kein** Ersatz.
- Explizite Matrixoperationen auf der Work-/Drawmatrix statt globaler
  Schein-Queue und Matrixstack. Wiederholtes Zeichnen akkumuliert keine
  Skalierung. Bei aktueller verknüpfter Pose wird die Workmatrix aktualisiert.
- `CombatController` übernimmt nun auch die anderen ID-3-Typen der originalen
  Waffeneffekttabelle. Verknüpfte Effekte lesen die aktuelle vorhandene
  Handpose. Texturzahl folgt den tatsächlich ausgewählten Paletten, nicht
  einem pauschalen Vergleich des Lebensdauercounters mit der Atlasgröße.
- `native/ninja_math.c`: vollständiger nativer Port der schon vorhandenen
  Math-API (Winkeltabelle, Quadranten/Wrapping, Wurzel, Splines, Fakultät).
  CMake kompiliert dafür nicht mehr `src/.../ps2_NaMath.c`. Kein Generator,
  Hardwarezugriff, Emulator oder neuer Originalkern.
- Der Waffen-Effektgenerator bleibt als unbenutzte historische Referenz
  erhalten; `weapon_flash.inc` ist aus CMake und dem nativen Effekt entfernt.

## Verifikation

Frische separate Konfiguration:
`/private/tmp/recvx-native-flash.00JXOJ/cmake`.

Nur `recvx-weapon-flash-tests`, `recvx-math-tests`, `recvx-matrix-tests` gebaut.
CTest **3/3 bestanden**. Alle 65.536 Winkel, Matrixoperationen und beide
Modellvarianten der Nicht-Wand-Effekte; Typ-3-RNG und Draw-/Query-Vertrag,
Hide-/Game-Gates, Tint/UV/Paletten/Blend, Lifetime und wiederholte Draws.
Typ-3-Wandantwort ist eine Testfixture, keine Original-Kollisionsabnahme.
`native/combat.cpp` separat ohne Link auf Syntax geprüft. `git diff --check`
ohne Befund.

Der tatsächliche Waffen-Testlink enthält ausschließlich vier native C++-
Objekte und `librecvx_math.a` aus `native/ninja_math.c`/`native/ninja_matrix.c`
sowie die System-Mathematikbibliothek. Das generierte Blend-Include enthält
nur Konstanten, keine Originalfunktionen. Symbolprüfung: keine Original-
Dispatcher, Gegnerwelt, `bhEff003`/`bhDrawWeaponEffect`, `sys/plp/rom/ene`.

## Offen, nicht als erledigt ausgegeben

- Produktiver Waffen-/Raumtest erst nach sauberem Runtime-Link und Abschluss
  des parallelen Rückbaus. Kein Windows-x64-Build in dieser Arbeit.
- Native vollständige `bhCheckL2Wall`-Bindung für Typ 3 fehlt; die vorhandene
  vereinfachte Strahlprüfung wurde nicht eingebunden.
- Das seltene Point-Filter-Flag `0x20000000` meldet eine offene Bindung, bis
  die vorhandene native Szene die Original-Samplerwahl pro Draw tragen kann.
- Andere Waffen-Angriffsabläufe, sekundäre Effekte/Schusslicht, sämtliche
  Gegner, Morph/Face/Shadow und Runtime-/Windows-Lücken sind nicht durch
  diesen Komponentenport abgeschlossen.

Kein Commit/Push; vorhandene und parallele Änderungen erhalten.
