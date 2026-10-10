# B00 — native Spieler-Wandauflösung

Getrennt von Claudes G01/Türenarbeit. Kein Originalkörper, Generator,
Originalkern, zweiter Player-/SYS-/Roomwork oder PS2-Nachbildung. Kein
Commit/Push/Startpaketbau. Gemeinsame fremde Änderungen erhalten.

## Ursache und produktiver Anschluss

Nach dem Rückbau gab PlayerPush die Bewegungsposition unverändert zurück.
Ein Boden-/Kontakt-/Linientest löst diese Position nicht auf. Der freie
Runtime-Spielerpfad benutzt nun `RoomWorld::move_player` mit einer
eigenständigen nativen Implementierung in `geometry_motion.cpp`.

Semantikreferenzen außerhalb der Quarantäne: `hitchk.c` bhCheckWall,
bhCheckWallEx, bhCheckBox, bhSetDansaLimitAtari, bhCheckDansaAtari,
bhCheckFloorP und bhCheckDansa; `player.c` PlyInfo, bhSetPlayer,
bhControlPlayer und die Standard-/Walk-/Run-/Back-Moden. Diese Quellen
werden weder kompiliert noch zur Erzeugung von Funktionskörpern benutzt.

- Ein geordneter Wanddurchlauf, keine erfundenen Substeps, Sweeps, Skin-
  Margins, Iterationen oder Raumsonderfälle. Typ 0/1: alte Quadranten und
  alte Abstände; runde Ecken. Typ 2/3: radiale Antwort; Typ 4/5:
  vorzeichenbehaftete Diagonale und beide Achsenkanten. Typ 7: alte
  Quadranten, neue Abstände, ursprüngliche Höhenbedingungen. Originalfilter
  und untere inklusive/obere exklusive Grenzen bleiben erhalten.
- Der direkte bhCheckWall-Vertrag enthält Y-Quantisierung und Rampen-
  Projektion; Ex hat ausdrücklich keinen Typ-6-Zweig. Der freie Spieler
  ruft wie bhControlPlayer **Ex** auf, nicht eine frei gewählte Rampenlösung.
- `arp` 0/2/3/-1, Halbierung zu `arn`, Vorfühler auf Y+12,5 mit Höhe 1,
  Korrektur des Körpers, Flag-/Statusrestaurierung, danach Körperpass mit
  Radius 3,5 und PlyInfo-Höhe 16,5/17,5. Statusbit 1 beschreibt den letzten
  Körperpass; ein reiner Vorfühlerstopp darf es nicht künstlich setzen.
- `MotionActor` ist eine Referenzansicht. Produktionsflags kommen direkt
  aus EventVm-Banken 13/14/10/5; Position bleibt SceneSpawn. Nur bisher
  fehlende kollisionsspezifische Felder liegen beim vorhandenen RoomWorld-
  Besitzer. Kein paralleler Work, keine BH_PWORK-Tabelle oder Copy-in/out-
  Engine. Event-Wandmutationen wirken auf dieselbe Raum-ATR-Quelle.
- Stufenlimits: vier Richtungsproben, erste passende ETC-Identität bzw.
  gespeicherte Identität, temporäre ATR nach Raum/Mobil, anschließend
  verworfen. Keine dauerhafte zweite Mobilwandwelt.
- FloorP: sicherer Fühlerpunkt und arn für gerichtete Trigger, originale
  Richtungsbits, Floorindex, Wasser-/Fußeffekt-/Trittschallfelder und fünf
  Stufenkontakte. EventVm delegiert diese Abfrage an den Besitzer; seine
  bisherige eigenständige Test-API bleibt erhalten. Simulation-/Hidden-
  und flg-0x10-Gates sind positiv geprüft.
- Der vormals zusätzliche positionsgleiche World-Tick-Wandpass entfällt:
  freie Bewegung löst nach Auswahl des Kandidaten auf. Der neutrale Push-
  Resolver ist nicht mehr der produktive freie Spielerpfad.

## Abnahme

macOS arm64, eigener Build `build/native-ground-20261009`:

- Vollständiger Build und 48/48 CTest; auch nach dem ergänzten Pushkontakt-
  Test bestehen native_motion_collision, native_ground_world und Policy.
- Komponente: beide Box-/Zylinder-/Dreieckstypen, alle Dreiecksrichtungen,
  Filter, runde Ecken, alte/neue Seiten-/Abstandsregeln, Grand, direkte
  Rampen, Y-Quantisierung, Stufenlimit-/Cache, Bodenfelder sowie Kontakt-
  Zähler/Index/Attributmutation. ASan/UBSan des Mechanismus bestanden.
- Echter RM_0050-Worldtest: originale Wand 11 über Event 0x0a aktivieren,
  Kandidat wird gestoppt; deaktivieren, Kandidat passiert; wieder aktivieren.
  Produktions-Eventflags, Vorfühler und Pause-/Hidden-Gates geprüft.
- `verify_motion_collision.py`: originaler RM_0050-Raum, echte ATR-Wand 11,
  normale Bewegungs-/Input-/Event-/Renderpfade, diagnostische Aufstellung.
  Walk X=-253,51, Run -252,51, Back -254,51; jeweils HP 160, kein Raumwechsel.
  Das direkt vor dem Anschluss gesicherte **kernfreie native** Binary
  überschreitet dieselbe Wand: Walk -304,079, Run -396,327, Back -287,539.
  Drei vorbestehende pending Eventbefehle sind identisch, nicht geschlossen.
  Tangentialer Lauf (50 Frames) und natürlicher POS-Spawn/Input-Smoke ebenfalls
  bestanden. Keine Spieleflags zur Freigabe injiziert. Der frühere oblique-
  Langlauf betrat ein Zombie-Raumereignis; der Nachweis prüft den früheren
  kollisionsfreien Steuerungsabschnitt, nicht eine abgeschaltete Eventwelt.
  Logs/Zustände: `build/validation/native-motion-20261009-r3/`.
- Regressionen `verify_gameover.py`, `verify_pickup_slide.py` und
  `verify_events.py` bestehen unter
  `build/validation/native-motion-20261009-regression/`.
- Quellen-/Build-/Runtime-Binary-Prüfer PASS, 24/24 Policy-Vertragstests,
  git diff --check sauber. port_check ausgeführt; dessen expliziter
  Belegumfang ist kein Gesamtfortschritt des Spiels.

Windows x64: Spielbibliothek sowie Motion-/World-Testprogramme kompiliert
und gelinkt in `build/windows-ground-20261009`, Buildscan PASS. Nicht
ausgeführt; kein eigener Windows-Runtime-/Binaryaudit behauptet.

## Grenzen / nächste Pakete

**B00 bleibt ein Teilpaket**, nicht die vollständige Spielerzustandsmaschine.
Zylinderattr 0x40 verlangt Effekt29 und Hurt-State: der vorhandene
Effektbesitzer hat diese noch nicht. Die native Kollision setzt ihre
belegten Felder, meldet die fehlende Wirkung und lässt das Attribut erhalten;
kein falscher Erfolg oder Ersatz-Effekt. Diese Fortsetzung bleibt offen.

Push/Kistenproduzent (B02/B03), Treppen-/Leiteranimationen (B01), Pferdeschwanz
(B04), geskriptete/Kampf-/Gegnerbewegung und allgemeiner Spieler-Bodenwork-
Abschluss fehlen weiterhin. Die Mobilquellen werden konsumiert, aber ihre
stillgelegten Produzenten werden dadurch nicht implementiert. Der ältere
Gegner-move_walls-Pfad ist kein Originalport und bleibt getrennt offen.
Nicht jede gerenderte Deko besitzt im Original eine kollidierende ATR;
keine neue Möbelgeometrie erfinden. Kein Durchspielnachweis, keine vollständige
Spielbarkeit, keine Behauptung eines neuen Start.command-Paketstands.
