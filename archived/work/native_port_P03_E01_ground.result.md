# P03/E01 — native Boden- und Wandkontaktabfragen

Stand 2026-10-09. Dies schließt das Teilpaket Boden/Wandkontakt, nicht das
gesamte Geometrie-/Kollisionspaket und nicht das Gameplay.

## Eigentum und Implementierung

- `native/geometry_atr.h`: Rechteck-/Dreiecks-/Scanline-Band-Prädikate und
  vier Rampenflächen. Eigenständig organisierter C++-Code; Originalquellen
  wurden nur zur Bestimmung der Semantik gelesen. Quarantäne nicht gelesen.
- `native/effect_collision.cpp`: Bodenreduktion und Wandkontakt auf den
  bereits vorhandenen `CollisionWall`-/ATR-Daten. Kein SYS/ROM/Player/Enemy/
  Effect-Zweitwerk und kein Originalmodul. Die bestehenden CMake-Spielquellen
  binden das Modul bereits produktiv ein.
- `GroundCollision` besitzt nur geometrische ATR-Snapshots, weil der mobile
  Vektor im bestehenden World-Aufruf temporär ist. Treffer enthalten
  Raum/Mobil + Index und einen Zeiger auf diesen Datensatz. Ein späterer
  Query überschreibt nicht den Inhalt eines früher zurückgegebenen Zeigers.
  Nach Geometrie-Update muss der Aufrufer `hit()` neu lesen; Indexidentität
  überlebt Speicherverlagerung, entfernte Einträge werden ungültig.
- Keine Änderungen an Claudes Computer-/Gameover-/Türmechanismen. CMake
  ergänzt allein das eigenständige Originaldaten-Testziel.

## Semantikbelege

Zulässige Referenz `src/ps2/veronica/prog/hitchk.c`:

- `bhGetGroundPosition`, Zeilen 3557–3698: -1000-Sentinel, anfänglicher
  Abstand 10000, enabled-Bit 1, zuerst Raum dann mobile Tabelle, striktes
  kleiner-beim-Ersetzen (frühester Gleichstand bleibt). Typ 0/2/4 liefert
  `py + (h ? h : room_height)`, Typ 7 **py**, einschließlich Toleranz
  -0.0001. Odd-Solid-Typen 1/3/5 sind keine Bodenbeiträge. Rampen nutzen
  tatsächliches h und können über dem Probe liegen, wenn Probe mindestens
  auf Basishöhe ist. Ungültige Rampen-ID außerhalb 0..3 wird diagnostisch
  abgelehnt; kein Wert aus einem uninitialisierten Decompiler-Lokal erfunden.
- `bhCheckWallType`, Zeilen 1939–2045: erster enabled-Kontakt, Typen 0..7,
  0x400 überspringt nur odd solid 1/3/5; Typ 7 hat die eigenen 0x100-
  Kreuzungsbedingungen mit strikten Enden und tatsächlicher Dicke. Kein
  zusätzlicher Etagen-/Attributfilter. Mobilgeometrie wird, wenn vom
  Verbraucher benötigt, in derselben Raum-vor-Mobil-Eintragsfolge geliefert;
  die Kontakt-API selbst prüft den übergebenen Vektor in dieser Reihenfolge.
- `bhCheckInnerTriangle`, Zeilen 3699–3900, `bhCheckInnerTriangle3`,
  Zeilen 4093–4137, `bhCheckInnerP4`, Zeilen 4425–4585: vorzeichenbehaftete
  Ausdehnungen, Radiusband, inklusive Unter-/exklusive Obergrenzen,
  ausgeschlossenes Diagonalgleichgewicht, originale Divisionsreihenfolge.
  Kein Winding-Test oder Distanzheuristik als Ersatz.

## HP-Regression an der Ursache

Der stillgelegte `recvx_en01_player_state` nullte sein Ausgabeobjekt. Die
World verbrauchte HP 0 als tatsächlichen Schaden; die Runtime reduzierte
den vorhandenen Inventarzustand auf 1. Die API meldet jetzt Verfügbarkeit
(int), liefert bei fehlendem Besitzer 0 und lässt die Ausgabe unverändert.
`tick_en01` und `sync_player_grab` konsumieren nur einen gültigen Zustand.
Kein separater Player-Speicher, kein geratener Ausgangs-HP und kein
Sonderfall für Räume/Gegner. Der fehlende Griff-/Gegnerschadensport bleibt
offen. Tests prüfen sowohl unangetastete Ausgabe als auch keinen
Schadens-/Todesimpuls bei vorhandenen HP 160/93/1/0 in der echten World.

## Gezielte Nachweise

Eigenes Buildverzeichnis `build/native-ground-20261009` verhindert Konflikte
mit Claudes laufenden Builds.

- macOS arm64: Runtime und fünf Testziele gebaut. Fünf CTest-Gruppen:
  `wall_collision`, `native_ground_world`, `effect_collision_debris`,
  `enemy_damage_original_en01`, `inventory_events_assets`.
- Positive ATR-Fälle: alle Formen/Flagzweige, vier Rampen, Dreiecke in allen
  Vorzeichenrichtungen und mit Radiusband, drei Scanline-Band-Regionen in
  beiden Orientierungen, Stapel/Abstand/Toleranz, room/mobile Tie und
  Identität, Geometrieupdate, Entfernen und Leertreffer. Die alten Host-
  `move_walls`-Tests bleiben bestehen, sind aber keine Originalmechanismus-
  Abnahme. Reflexion/Linie/Effekte bleiben explizite Lückenverträge.
- `native_ground_world`: Original-RM_0050, Spawn aus Raumdaten,
  `RoomWorld::combat_ground` liefert Höhe 0 und Raum-ATR-Eintrag 0.
  Native Eventbindung 0x0a deaktiviert reale Einträge: Sentinel/kein Treffer;
  wieder aktivierter Originaleintrag liefert denselben Boden. Dies ist
  eine Integrationsdiagnose mit echten Daten und Bindungen, kein Beleg eines
  vom Spieler durchlaufenen Kollisionsablaufs.
- ATR-Test separat mit AddressSanitizer/UndefinedBehaviorSanitizer gebaut
  und ausgeführt, ohne Befund.
- Produktive Runtime RM_0050, headless/in-game, enemy-ai off/grab off:
  nach 2 Ticks vorher HP 1, nach Fix HP 160; nach 600 Ticks weiterhin 160,
  0 blockierte Befehle. Kein Gegnerkampfbeleg.
- Quellen-/Build-/macOS-Runtime-Symbolaudit PASS.
- Windows x64, eigenes `build/windows-ground-20261009`: `recvx_game`,
  `recvx-collision-tests.exe`, `recvx-ground-world-tests.exe` kompiliert und
  gelinkt. Kein Windows-Lauf und kein Windows-Runtime-Binaryaudit hier.
  Nach der zunächst fehlgeschlagenen Compiler-Erkennung (globaler Cache
  nicht schreibbar) musste der Projektcache auch für **Configure** gesetzt
  werden. Der dabei angelegte Cache hatte leere C-Release-Flags; erneutes
  Configure mit `CMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG` stellte den normalen
  Release-Build her. Keine Checker-/Toolchain-Ausnahme, keine UBSan-Stubs.

## Offene, nicht abgeschwächte Nachweise

`recvx-game-tests data` scheitert bereits vor den neuen World-Checks an
`full inventory removes original pickup`. Gegenprobe mit dem bisherigen
`build/macos-arm64/recvx-game-tests data` ergibt dieselbe Fehlermeldung.
Dieser Inventarfehler wurde nicht bearbeitet oder durch eine neue Erwartung
verdeckt. Der eigene verpflichtende Originaldaten-Test läuft unabhängig
davon; CTest `inventory_events_assets` ohne Daten ist nur Komponentenbeleg.

Linienprüfung und letzte Treffer-Normale, Reflexion, echte `bhCheckL2Wall`,
Capsule-/weitere P03-Primitiven, B00-Spielerauflösung, Push/Treppen,
Effektprozeduren/Atlas und Gegner-KI/Kampf fehlen weiterhin. Boden und
Kontakt allein reparieren weder Feuerzeugflamme noch Pferdeschwanz oder
Gegnerbewegung. E01/P03 insgesamt bleiben offen. Keine Prozent-/Ledger-
Aufwertung, kein Paket, Commit oder Push vorgenommen.

## Reproduzieren

```sh
python3 tools/check_no_original_core.py
cmake --build build/native-ground-20261009 --parallel 8 --target recvx-runtime recvx-collision-tests recvx-ground-world-tests recvx-effect-debris-tests recvx-enemy-damage-tests recvx-game-tests
ctest --test-dir build/native-ground-20261009 --output-on-failure -R '^(wall_collision|native_ground_world|effect_collision_debris|enemy_damage_original_en01|inventory_events_assets)$'
build/native-ground-20261009/recvx-ground-world-tests data
python3 tools/check_no_original_core.py --build-dir build/native-ground-20261009 --binary build/native-ground-20261009/recvx-runtime
```
