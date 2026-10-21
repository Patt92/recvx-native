# Q01 — Anschlussprüfer für native Quellen

Stand: 2026-10-10, Europe/Berlin. Bearbeitet von der Aufsichtssession.
Dies ist ein Prüfwerkzeug. Es ist keine Portierung, keine Gameplay-Abnahme und
kein Ersatz für ein Semantik-Audit.

## Anlass

Die Übergabe verlangt (Z. 136): „Implementierung ohne Anschluss ist offen."
Für diese Trennung gab es kein Werkzeug. Vorhanden sind:

| Werkzeug | prüft | prüft **nicht** |
|---|---|---|
| `check_no_original_core.py` | Originalkerngrenze | ob eine Quelle gebaut wird |
| `port_check.py` | belegte Mechanismenabdeckung | ob sie im Build landet |
| `windows_link_audit.py` | PE-Bauidentität der Linkeingänge | ob eine Quelle Eingang ist |

Dadurch konnte ein Zustand entstehen, in dem sorgfältig portierter Code im Baum
liegt, nie kompiliert wird und trotzdem neben älteren Erfolgsmeldungen steht.
Besonders heikel ist das hier, weil die frühere EN07-Abnahme
(`07_en07_dispatch_integration.result.md`, `08_runtime_acceptance.result.md`)
über `core_enemy_world_control` → originales `bhControlEnemy()`/`bhJumpEnemy[]`
lief — genau den Dispatcher, den AGENTS.md:219-222 seit 2026-10-09 verbietet.
Jene „48/48 CTest" gelten dem quarantänisierten Pfad, nicht dem heutigen
kernfreien Neuaufbau. Ohne Anschlussmaß ist dieser Unterschied unsichtbar.

## Kollisionsabgrenzung

Ausschließlich **neue** Dateien:

- `tools/check_build_coverage.py`
- `tests/check_build_coverage_test.py`
- `archived/work/Q01_build_anschluss_pruefer.md` (dieses Dokument)

Nicht angefasst, weil in fremdem Besitz oder aktiv bearbeitet:
`CMakeLists.txt`, `native/world.cpp`, `native/events.cpp`, `native/ninja_math.*`,
`native/ninja_matrix.*`, `tests/native/continuity_test.cpp`,
`tests/native/matrix_test.cpp` (Integrator/Orchestrator), alle `native/en*.*`,
`enemy_dispatch.*`, `enemy_work.*`, `subplayer.c` (Gegner-Agent),
`player_hair.*` (B04), `player_stairs.*` (B01), `player_head.*` (Besitzer offen).

Kürzel `Q01` ist frei; vergeben sind A01, B00, B01, B04–B06, D03–D06, D10, E01,
E03, G01, P03, P04, R02 sowie die Nummern 00–12.

**Eine Anschlussaufgabe bleibt beim Integrator:** die Registrierung als CTest.
Das Werkzeug ist eigenständig lauffähig; die Zeile gehört in den Python-Testblock
von `CMakeLists.txt` (neben Z. 17–23) und wird hier nicht selbst eingefügt:

```cmake
add_test(NAME build_coverage
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_build_coverage_test.py")
```

## Was das Werkzeug meldet

- `nicht_gebaut` — Quelle liegt im Baum, steht in keinem `add_library` /
  `add_executable` / `target_sources`. Nie kompiliert, nie typgeprüft.
- `gebaut_ohne_aufrufer` — kompiliert, aber kein Einstiegspunkt aus ihrem Header
  wird von einer anderen gebauten Quelle aufgerufen.
- `nur_test` — Einstiegspunkte werden nur aus Testquellen aufgerufen.
  Komponentengeprüft, nicht produktiv gebunden.
- `nur_referenziert` — Name wird genannt, aber nicht aufgerufen; trennt
  Funktionszeigertabellen vom echten Befund.

Aufrufe: `python3 tools/check_build_coverage.py [--json] [--strict]
[--directory native] [--root .]`. `--strict` gibt 1 zurück, sobald ein Befund
vorliegt, und taugt damit als CI-Gate; ohne `--strict` ist es ein Messbericht.

## Ergebnis des ersten Laufs (2026-10-10)

61 Ziele, 137 native/Test-Quellen, davon **114 gebaut**.

`nicht_gebaut`: **23 Quellen, 20.107 Zeilen.** Die größten:

| Zeilen | Quelle | Einstiegspunkte |
|---:|---|---:|
| 6330 | `native/en01_core.c` | 163 |
| 2638 | `native/en07.c` | 1 |
| 2402 | `native/en03_lib.c` | 77 |
| 927 | `native/en24.c` | 1 |
| 897 | `native/en01_tables.c` | 1 |
| 681 | `native/en03_collision.c` | 13 |
| 639 | `native/en16.c` | 4 |
| 624 | `native/en25.c` | 1 |
| 602 | `native/en10.c` | 1 |
| 483 | `native/en08.c` | 2 |
| 437 | `native/en20.c` | 1 |
| 285 | `native/enemy_work.c` | 10 |
| 280/267/257 | `native/en54.c` / `en55.c` / `en56.c` | je 1 |
| 216 | `native/combat_geometry.c` | 7 |
| 176 | `native/en00.c` | 1 |
| 141 | `native/enemy_dispatch.c` | 4 |
| 137 | `native/en71.c` | 1 |

Dazu **vier Testdateien, die nie gebaut werden** und deshalb nie laufen:
`tests/native/en01_core_test.c` (676), `subplayer_test.cpp` (501),
`effect_particles_test.cpp` (341), `enemy_spawn_test.cpp` (170). Das ist der
wichtigste Nebenbefund: die Komponentenprüfungen zur neuen Gegnerarbeit
existieren, belegen aber nichts, solange sie außerhalb des Buildsystems liegen.
`subplayer_test.cpp` betrifft genau den Apparat, dessen Erwartungen am
2026-10-10 in `continuity_test.cpp` angepasst wurden.

`native/combat_geometry.c` entstand **während** dieser Arbeit und war sofort im
Befund — der Prüfer fängt neue Quellen also beim nächsten Lauf, nicht erst bei
einer späteren Durchsicht.

`gebaut_ohne_aufrufer`, `nur_test`, `nur_referenziert`: je 0. Der gebaute Teil
des Baums ist durchgehend angeschlossen; der offene Punkt ist allein, was gar
nicht gebaut wird.

Folgerung, unverändert zum Audit von heute: Das erste verbindliche produktive
Ziel (EN07 in RM_0130) kann mit diesem Stand nicht erreicht sein, weil
`native/en07.c` nicht kompiliert wird. Korrekter Status für alle 23 Quellen:
*implementiert, nicht gebaut, nicht angeschlossen, nicht geprüft.*

## Prüfung dieses Werkzeugs

`python3 tests/check_build_coverage_test.py` → **17/17 bestanden**, auch unter
`-W error::ResourceWarning`. Fixtures liegen in einem temporären Baum, das
Ergebnis hängt nicht vom Arbeitsstand des Projekts ab.

Abgedeckt: mehrzeilige Zielbefehle, `target_sources` auf dasselbe Ziel,
Schlüsselwörter statt Quellen, unaufgelöste `${VARIABLE}`-Listen, auskommentierte
Ziele, mehrzeilige Deklarationen, erste Deklaration in einem `extern "C"`-Block,
`#define` mit Klammer, Funktionszeigerfelder in Structs, nicht gebaute Quelle,
gebaut ohne Aufrufer, Aufrufer in anderer Quelle, Aufrufer nur im Test,
genannt ohne Aufruf, Quelle ohne Header, `--strict`-Rückgabe, Berichtsform.

Zwei Parserfehler wurden beim Bau gefunden und sind als Regressionstest
festgehalten:

1. Eine über zwei Zeilen laufende Deklaration (`native/collision.h:17`
   `move_walls`) wurde nicht erkannt. Folge: `native/collision.cpp` erschien
   falsch als `nur_test`, obwohl `native/world.cpp:658` es aufruft.
2. Die erste Deklaration in einem `extern "C"`-Block fiel aus, weil das
   Stringliteral `"C"` im Segment vor dem Rückgabetyp stand. 37 Header dieses
   Projekts nutzen diesen Block.

Beide Fälle hätten den Bericht in Richtung „nicht angeschlossen" verzerrt, also
Falschalarm erzeugt. Nach der Korrektur sind die drei Nicht-Build-Kategorien
leer, was zu `grep`-Gegenproben passt.

## Grenzen, ausdrücklich offen

- Textparser über die CMake-Dateien, keine CMake-Auswertung. Quellen hinter
  `if()` gelten als gebaut; `${VARIABLE}`-Listen werden gemeldet, nicht geraten.
- Aufruferkennung über `name(`. Aufrufe, die ausschließlich über einen
  Funktionszeiger mit nacktem Namen laufen, erscheinen unter
  `nur_referenziert` und müssen von Hand beurteilt werden.
- Klassenmethoden eines C++-Typs sind keine freien Einstiegspunkte und werden
  nicht erfasst. Für Besitzer wie `RoomWorld` trifft der Prüfer deshalb **keine**
  Anschlussaussage; dort bleibt die Beurteilung am Integrator.
- Ein Befund von 0 heißt „gebaut und aufgerufen", nicht „richtig portiert".
  Semantik prüft weiterhin nur der Abgleich gegen Originalcode und -daten.
- Kein Build, kein Testlauf der C/C++-Ziele und keine Runtime-Prüfung in diesem
  Work-Item. Kein Commit, kein Paketstand.
