# Nächste Gegner – Integrationsbedarf, 2026-10-13

Gemeinsame World-/Körper-/Spielersyncdateien werden parallel bearbeitet;
diese Sitzung verändert sie nicht. Keine hier beschriebene Bindung ist allein
wegen dieser Beschreibung produktiv abgeschlossen.

## Einzelne Tests tatsächlich dauerhaft anschließen

Nach Freigabe des Integrators in CMake im BUILD_TESTING-Block aufnehmen:

```cmake
foreach(id 10 11 20 71)
  add_executable(recvx-en${id}-tests tests/native/en${id}_test.cpp)
  target_include_directories(recvx-en${id}-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en${id}-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en${id}-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en${id}_component COMMAND recvx-en${id}-tests)
endforeach()
```

EN53 erst nach vollständiger Modell-/Materialdienstbindung:
CMake recvx_game um native/enemies/en53.c ergänzen, -ffp-contract=off wie bei
anderen Gegnern, enemy_dispatch.c include en53.h und jump[53]=recvx_en53.
Den Test vorher isoliert mit en53.c kompilieren (bereits bestanden), damit
unfertiger produktiver Anschluss nicht über einen Test erzwungen wird.

## EN10 und EN71: echter mlwP-Modellwechsel

Original en10.c:51/en71.c:41: mlwP=mdl[n]. Native mdl_no wurde bisher nie
konsumiert. Bei Raumload (room.c:437-449) für alle realisierten mdl[] die
bestehenden ModelPose/owP/Evalflags-Datensätze an derselben Entity bereithalten.
set_motion, joint_matrix/point, obj_num, calc_enemy_model und append müssen
auf den ausgewählten Satz zugreifen. Einen Satz bei jedem Wechsel auf
Bindepose zurücksetzen wäre falsch: jeder mdl[] behält seinen Zustand.
Keine ID-Sonderregel im Renderer erfinden; ursprüngliche Modellwahl ist der
Mechanismus. EN10 ist Modell-Flipbook ohne bhSetMotion, EN71 sampelt seine
Motion im ausgewählten Modell, EN20 hat normalen Motionablauf.

## EN10: bhCheckWall und Spieler-/Effektpass

s.check_wall ist NULL; ursprünglicher bhCheckWall (hitchk.c) muss denselben
Raum-/mobilen ATR-Besitzer erreichen und seine Flags/Offsätze zurückschreiben.
Nicht unbesehen durch wall_move/radius anders berechnen. Körperpass-Sitzung
bereitet den gemeinsamen Kontaktpfad vor; nicht parallel überschreiben.
EN10-Biss und Todesblut nutzen bereits das vorhandene ext.effect_tb;
ursprüngliche gemeinsame sys->ef-Vorlagenlebensdauer bleibt eigenes Effekt-
Mechanismusproblem (nicht je Gegner mit synthetischen Defaults lösen).

## EN11: nicht nur weitere Funktionsnamen

init_enemy_works bindet derzeit nur se_pan. Konkret ergänzen:
- flags_clear: ursprüngliche Flagbank (which=1 → EventVm flags[4][0]).
- node_angles_get/set: tatsächliche objP[2/3] des Work-Besitzers, dieselben
  lokal veränderten Knoten wie EN01 node_local/node_angle_set.
- node_work_flags: owP[2/3].flg set/clear, direkt enemy_owp_flg.
- effect_create/get/set: tatsächlicher eff[]-Slot, mode0/ct2/ax/axp bleiben
  beim Effektbesitzer, gleiche Linkwork-Lebensdauer; kein lokales Lampenmock.
- Eff261 selbst samt Detektion/Zeichenpfad fehlt. Original-ELF oder weitere
  zulässige Referenzquellen klären, keine Alarmdistanz/Zeittoleranz erraten.

Erst danach natürliche Aktivierung/Detektion/Hunterruf in allen sechs
Originalräumen und Reset/Skip/Roomwechsel prüfen.

## EN53: Quellen und gemeinsame Modelladapter

Originalkörper stehen in en15.c, nicht en53.c. Neue en53-Datei implementiert
Pulse/Tod, baut aber noch nicht produktiv. Bindung siehe EN053.result.md.
ChangeAmbient betrifft EIN Byte je Chunk, nicht allgemeines Material-RGB;
Morphpaare müssen am bestehenden Modellbesitzer geändert werden, Textur-
Nachfeld ist epw->txp[0] (nicht ohne Beleg mlwP->texP).

## Nachweise / offene Fehler

Eigenes cleanes Runtime-Build build/enemies-next-claude-20261013 erfolgreich,
C07/Policy PASS. Individuelle 10/11/20/71/53-Komponenten PASS.
Vorhergegenproben 10/11/20 scheitern erwartungsgemäß. Runtime RM9170:
59 EN10 bewegen sich, EN20 schalten Clips, 13 EN71 laufen in ihren Original-
Zuständen; Modellwahl/Wanddienst bleiben offen. RM7081 dokumentiert den
EN11-Initblocker statt ihn als bestandene Abnahme auszugeben.

Keine Paketinstallation/Commit/Push. Work-Items verlangen Abschlussnachweise;
Syntax/Komponenten ersetzen keine ursprünglichen produktiven Spielabläufe.
