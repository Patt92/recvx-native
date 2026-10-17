# Fassexplosion: nativer Gegnerschaden (2026-10-17)

## Bestätigte Ursache / Original

- `native/world.cpp` bisheriger `bomb_damage`-Gegnerteil liest `e.en01` statt
  `e.enemy`; dessen Fassade hat flg 0, `recvx_en01_bomb` ist leer.
- Referenz `src/ps2/veronica/prog/weapon.c:1419–1490`
  (`bhCheckBombAtari`, nur Gegnerhälfte): Tabelle `ene[0..ewk_n)`,
  flg 1/0x20/!2, stflg !0x1000000, gleicher Parent-Filter bei flg 0x80;
  reale watr, npDistanceP2C, Sichtkapsel r=0.1 / bhCheckC2WallN 0x400;
  Trefferfelder, Waffe 20, nächstes owP-Gelenk ab 1, dam += dmax/dmin nach
  strikter 3D-Rootdistanz < ar/2 und comb 0x10/0x20.
- Referenz `pwksub.c:2172–2462`: C2WallN verwendet Kapselprimitive,
  ignoriert ungerade Typen mit Abfragebit 0x400; kein attr-0x10-Ausschluss.
  Bestehender c2_wall_n-Adapter über combat_wall_distance war nur ein Strahl.

## Änderungen / Bindung

- `native/world_enemies.cpp`: neue `RoomWorld::bomb_enemy_damage`, über die
  bestehende `enemy_table_`, ohne Identitätsfilter, ohne HP-Direktschreibzugriff.
  `epw->sv->obj_num/joint_matrix` lesen vorhandenes mlwP/owP einschließlich
  geteiltem Modellfenster; keine Neuberechnung/Sampling der Pose.
- `native/world.h`: ausschließlich eine private Deklaration ergänzt.
- `native/wall_capsule.{h,cpp}`: `check_c2_wall_n` verwendet bestehende echte
  Box-/Kapsel-/Dreieckprimitive; gemeinsamer innerer Lauf mit ursprünglichem
  C2Wall. Dessen öffentliche Semantik bleibt unverändert. Neue Originaltore
  werden nur im N-Pfad verwendet, einschließlich attr 0x2000000/query 0x40000000.
- Fehlendes lkwkp/Gelenkdienst, ungültiger owP-/dam-Index bleiben benannte Lücken,
  kein geratenes Gelenk/keine Ersatzkapsel. Parent-stflg läuft über den vorhandenen
  typisierten RECVX_ENEMY_WORK-Link (keine zweite O_WRK-/BH_PWORK-Welt).
- `world.cpp` NICHT bearbeitet: Parent muss die bisherige Gegnerschleife durch
  `bomb_enemy_damage(bomb);` ersetzen. Spielerhälfte unverändert lassen.
- `tests/native/barrel_blast_test.cpp`: Original RM_0080-Fass/zwei Zombies über
  pending Waffentreffer -> bhEne00 -> bhSetExplosion -> eff016 -> Weltschaden;
  dam/DP/DV/day/owP-Auswahl, dmax/dmin, nachfolgende echte HP-Abnahme,
  ausgeschlossene Works, verlinkter Parent, sparse Tabelle, generische Identität,
  einmaliger Zünder. Separat C2WallN-Tore und Unterschied Kapsel/Strahl.
  Diagnoseaufstellung ist ausdrücklich markiert: Lage per bestehenden
  Eventbefehlen, Fass y=-5 für dmax-Fall, keine erfundene Gegnerkapsel.

## Prüfung / noch offen

- Policy-Prüfer vor Arbeit und abschließend PASS; `git diff --check` PASS.
  `python3 tools/port_check.py --top 0` ausgeführt, kein Ledger-/STATUS-Edit.
- Auftragsgemäß KEIN Build, keine App-/Paketersetzung, kein Commit.
- Parent registriert Ziel `recvx-barrel-blast-tests` (Link `recvx_game`) und
  CTest `native_barrel_blast` mit Datenargument `${CMAKE_CURRENT_SOURCE_DIR}/data`.
- Nach seriellem Parent-Build exakter Lauf:
  `build/macos-arm64/recvx-barrel-blast-tests "$PWD/data"`
  bzw. `ctest --test-dir build/macos-arm64 -R '^native_barrel_blast$' --output-on-failure`.
  Wegen gemeinsam verwendeter Geometrie außerdem gezielt `wall_collision`.
- Vorher-/Nachher-Nachweis und erster kompilierter Testlauf stehen beim Parent
  aus; der Test ist bislang nur quelltextgeprüft. Erfolg/HP-Zahlen werden nicht
  vorweggenommen. Auslieferungsstand bleibt unverändert.
