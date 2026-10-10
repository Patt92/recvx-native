# Native ATR-Linien/Reflexion und unabhängige D05-Abnahme

Stand: 2026-10-09. Getrennt von Claudes G01/Türenarbeit. Keine Änderung
der Gameover- oder Türmechanismen; kein Commit, Push oder Startpaketbau.

## D05 / Gameover

Im eigenen Build `build/native-ground-20261009` neu gebaut und unabhängig
geprüft. `gameover_original` besteht. `tools/verify_gameover.py` besteht
vor und nach dem Linienanschluss. Der produktive native Runtime-Pfad zeigt:

- Prompt, gelöschte Timerflags 69/67/70, gesetztes ts_flg 0x100, Fade.
- Ja: Retry und Wiederherstellung des tatsächlichen Checkpoints mit
  identischem HP-Wert, Inventar und Raum.
- Nein: ADV/0008 wird abgespielt, Titelrückkehr, kein Retry.

Nachweise: `build/validation/gameover-independent-20261009-final/`
enthält checkpoint/prompt/retry/decline als JSON, Replay und Runtime-Log.
Der Auslöser ist eine diagnostisch veränderte Eventtabelle in einer
temporären RM_0020-Kopie; Modelle/Spieler/Audio/Texturen sind Originaldaten.
Das belegt D05 nach Auslösung, **nicht** einen natürlichen Tod durch die
noch fehlenden Kampf-/Gegnermechanismen. Die vorher gefundene HP-Regression
wurde nicht durch diesen Test verdeckt: Retry erhält den Checkpointwert.

## Eigenständige native Implementierung

Semantikreferenzen ausschließlich außerhalb der Quarantäne:
`hitchkl.c` (Linienabfragen 1/2/3, ATR-Seiten, L2PL/InOut),
`hitchk.c:2904` (`bhSetWallRefAngle`, dazu `bhCheckBox`),
`ps2_NaColi.c` (mathematische Point/Line/Plane-Verträge).
Keine Originalquelle, generierter Körper oder Emulator im produktiven Link.

- `native/geometry_line.h`: unendliche Punkt-Linienprojektion, Punkt-Ebene,
  Linien-Ebene mit originaler 0,025-Parallelgrenze und Fallback,
  nicht normierte Kreuzproduktnormalen. Kein Host-Epsilon/Segmentclamp.
- `native/effect_collision.cpp`: stabile Raum-vor-Mobil-Prüfung auf der
  vorhandenen Geometrie, originale Flag-/Floor-/Attributfilter getrennt
  für Line1/2/3. Richtung bleibt die ursprüngliche Segmentrichtung;
  erfolgreiche Treffer verkürzen denselben Endpunkt. Gleiche Schnitte
  behalten den ersten Eintrag. Originale Flächenreihenfolge und Endgrenzen.
- Boxseiten, signed Dreiecke, vier Rampen und Bodenplatten; Zylinderseiten
  sind acht flache Polygone mit originalen Winkeln, kein Kreisstrahl.
- Reflexion aller acht ATR-Typen: Top-/Seitenwahl einschließlich Ties,
  radiale Zylinderreaktion, Dreiecksdiagonale und beide senkrechten Kanten
  einschließlich Rundungsmasken, Rampenhöhe und Typ-7-Oberfläche py.
- Der Query-State ist lokal. Endpunkt und Normale werden erst bei einem
  erfolgreichen vollständigen Query übernommen; Miss/Fehler ändern sie
  nicht. Keine statische PLANE-Work oder weitere Gamewelt.
- `RoomWorld::collision_contact` / `collision_line`: bestehende room/mobile
  Quelle. Die vorhandenen EN01-Serviceanschlüsse verwenden nun diese
  Mechanismen statt guessed Probegrößen und analytischem Strahl mit 0,01-
  Toleranz. `line_hit` liefert den vollständigen ATR-Datensatz (36 Bytes).
  Das ist **keine** Aktivierung einer fehlenden Gegnersteuerung.
- Produktion und Komponententest kompilieren die Kollisionsimplementierung
  mit getrennten Float-Multiply/Add-Schritten (`-ffp-contract=off`); die
  Option hängt nicht von BUILD_TESTING ab.

## Explizite Semantiklücke: Zylinderdeckel

Der Originalcode ruft den Achtpunkt-Polygontest innerhalb der Füllschleife
auf, bevor alle Punkte gesetzt sind. Nicht als Decompilerfehler geraten:
am zulässigen Original-ELF `data/files/SLUS_201.84` geprüft.

- `bhCollisionCheckLineMain` beginnt bei 0x269870; sein Stackbereich für
  die Polygonpunkte wird nicht initialisiert.
- Seiten schreiben nur die vier Punkte ab sp+0xB0 (bis 0xDC).
- Deckelschleife 0x26A418 schreibt jeweils nur den aktuellen Punkt.
- 0x26A484: Polygonadresse sp+0xB0; 0x26A488: Anzahl 8;
  0x26A49C: Aufruf `bhCollisionCheckL2PL`; 0x26A4CC: Schleifenrücksprung.
  Die Prüfung geschieht vor der vollständigen Initialisierung.

Deshalb ist der abwärts gerichtete Zylinderquery in seiner XZ-Breitphase
als offene Semantik diagnostiziert und wirft, ohne Teilergebnis. Kein
geratener Deckel, Nullfüllung als Spielverhalten oder emulierter Stack.
Zylinderseiten sind für horizontale/aufwärts gerichtete Queries portiert;
ein vollständiger Zylinder-Linienport wird ausdrücklich nicht behauptet.
Weiterer Originalbefund: Das widersprüchliche Typ-7-Rechtsseitengate ist
bei 0x26A150..0x26A19C bestätigt und erhalten, nicht still korrigiert.

## Abnahme und Grenzen

- macOS arm64: vollständiger eigener Build erfolgreich, 47/47 CTest.
  Log: `build/validation/native-lines-20261009-ctest.log`.
- Neuer `native_line_reflection`: sechs Boxseiten, Line1/2/3-Filter,
  signed Flooroperand, Tabellen-/Tie-Reihenfolge, Bodenober-/unterseite,
  alle Dreiecksrichtungen/Kanten, Rampen, Achteck statt Kreis, positive
  Reflexionsprüfungen und atomarer Lückenfehler am Zylinderdeckel.
- `native_ground_world` mit echten Daten: RM_0050, tatsächliche ATR-
  Trefferidentität, Liniennormale/Oberfläche/vollständiger Datensatz,
  Disable/Enable durch die produktive Eventbindung 0x0a. Kein Fake-World-
  Geometriebesitzer. Der gleiche Test erhält die HP-Regressionsprüfung.
- ASan/UBSan-Linien-/Reflexionstest erfolgreich.
- 24/24 Policy-Vertragstests; Source-/Build-/Runtime-Binary-Prüfer PASS.
- Windows x64: librecvx_game.a, recvx-line-reflection-tests.exe und
  recvx-ground-world-tests.exe im eigenen Cache gebaut und gelinkt.
  Nicht ausgeführt; kein eigener Windows-Runtime-/Binaryaudit-Nachweis.

CTest enthält weitere neutrale Lückenverträge und teils keine Datenhälfte;
47 grüne Tests sind kein Durchspiel- oder vollständiger Kollisionsnachweis.
Der im Bodenbericht bereits belegte datenhaltige `recvx-game-tests data`-
Fehler am Inventaraufnahmefall ist durch diesen CTest-Lauf nicht behoben.

Noch offen: Zylinderdeckelsemantik, echte `bhCheckL2Wall` (Kapsel-/Inset-
Vertrag, **nicht** Line1/2/3), weitere P03-Kapselprimitiven, B00-Spieler-
Wandauflösung, Push/Treppen, Effekte samt Erzeugern und Gegnermechanismen.
Die analytischen Kampfstrahldienste bleiben als noch unportierte Verbraucher
sichtbar. E01/P03 nicht geschlossen; keine Prozent-/Ledgeraufwertung.

## Reproduktion

```sh
python3 tools/check_no_original_core.py
cmake --build build/native-ground-20261009 -j 4
ctest --test-dir build/native-ground-20261009 --output-on-failure
python3 tools/verify_gameover.py --runtime build/native-ground-20261009/recvx-runtime --data data --output build/validation/gameover-independent-20261009-final
python3 tools/check_no_original_core.py --build-dir build/native-ground-20261009 --binary build/native-ground-20261009/recvx-runtime
python3 tests/port_policy_test.py
```
