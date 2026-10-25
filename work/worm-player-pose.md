# Wurm: Spielerpose bei Niederschlag und Aufstehen

## Befund und Korrektur

RM_7231 enthält EN02 (Gulp Worm). Diagnostischer Replay mit Chris (`--player 11`, Position 30/0/35 ab Bild 5, Gegner-AI/Angriffe an) reproduziert Treffer und Freigabe: bei 350 Bildern mode0 4, Motion 29, Gegnerbibliothek; bei 450 wieder mode0 1, Motion 43, Spielerbibliothek. Evidenz `build/validation/worm/chris-{350,450,650,750}.json`.

`RoomWorld::player_pose_edits` und `player_local_pose` verwarfen den berechneten objP bei `player_scripted && !player_smove_.active` auch dann, wenn der Gegner den Spieler führt. Das betrifft EN02 wie andere Gegnergriffe: `control_enemy_player_motion` führt bhSetMotion bereits aus, die Darstellung tastete dennoch den unüberblendeten Clip ab. Original: `player.c:1502` schreibt den gemeinsamen objP auch bei dieser Spielerführung; `en02.c:2680–2784` steuert Bibliothek/Motion/Hokan und gibt danach zurück. Keine Änderung der EN02-Zustandsmaschine, Daten oder Timer.

Die beiden Poseabfragen akzeptieren jetzt den vorhandenen `enemy_player_driving_`-Besitzer. Raumskript-/smove-Auswahl unverändert. Regression in `tests/native/en02_world_test.cpp` prüft Draw-/Lokalpose und Erhalt bei Freigabe. Vorher rot (`pose-before.log`), danach grün (`tests.log`); allgemeiner player_motion_blend-Test ebenfalls grün.

## Visuelle Nachprüfung

Mit Chris in RM_7231 zwei aufeinanderfolgende Niederschläge im echten Gegner-/Spieler-/Zeichenpfad aufgenommen. Diagnose: Startposition 60/0/40 ab Bild 5, feste Originalkamera 2; keine Gegnerinjektion. `build/validation/worm/visual/sequence-contact.png` zeigt Fall/Aufstehen/Freigabe (480–680). Übergänge zusätzlich **jedes Bild** 535–556 und 641–656 kontrolliert: `recovery-exact.png`, `second-recovery.png`, zugehörige JSONs und BMPs. In diesen sichtbaren Abläufen kein Zerreißen/Verschwinden oder einbildiger Poseausreißer erkannt. Bei 548 Wechsel Motion 32 -> 42, bei 647 -> 43 (verletzte Standhaltung). Kontaktblätter/GIFs sind für dunkle Szene nachträglich aufgehellte Diagnosen (3x), Original-BMPs unverändert. GIFs verlangsamt (100 ms/Bild).

## Grenzen

Diagnoseposition und feste Raumkamera, kein natürlicher Raumfortschritt. Frühere Bild-410-Ansicht war verdeckt und wurde nicht als Abnahme benutzt. Sichtprüfung gilt für die beschriebenen Folgen, nicht alle Trefferrichtungen/Renderer. Kein Windows-/Hardware-/Controllernachweis. Installierte App unverändert. Nach der sichtbaren Prüfung Event-Audit wieder aufgenommen: `work/event-logic-audit.md`.
