# E03-Basis: gemeinsame Texturen, native Flamme und Feuerzeuglicht

Stand: 2026-10-09. **E03 ist teilweise implementiert, nicht geschlossen.**
Keine quarantänierte Datei gelesen/verwendet, kein Funktionskörper-Generator,
keine Originalquelle kompiliert, kein Originaldispatcher/zweite Workwelt.
Claudes Türarbeit wurde erhalten und nicht bearbeitet. Kein Commit/Push oder
Startpaket-Update durch diesen Auftrag.

## Eigenständige native Mechanismen

Semantikreferenzen außerhalb der Quarantäne: effect.c (ef_info,
bhInitEffect, bhSetEffectTb, bhControlEffect, bhDrawTrsEffect3D), effsub1.c
(bhEff009), player.c (Feuerzeugzweige in bhControlPlayer), pwksub.c
(bhSetLighterFire), light.c (lgttab[1], bhSetLightTab, bhGetLightVector,
RGB-/Clock-Typen 1..6 von bhControlLight). Keine dieser Quellen wird gebaut.

- ef_info als reine Daten: 20 Kategorien, vier modelltragende Kategorien
  0/1/7/13. Der native Leser folgt vier Größenblöcken plus dem zuletzt
  gelesenen Wort und richtet die PVP-Liste auf 32 Byte aus. Tatsächlicher
  SYSTEM/0001-Offset **8448**, **40 Texturen**; Kategorie 8 startet bei 25,
  Kategorie 9 bei 26. Keine Suche nach einer Signatur, kein geratener Offset.
  Der `size_t(-1)`-Guard in World bleibt für fehlende Daten erhalten.
- Eff009 direkt auf EffectSprite: acht Schritte, kleine UV-Bank für Typen
  1/2/3, große für andere Typen. Die Bank wird bei Init gelatcht; ein späterer
  Typwechsel ersetzt sie nicht. Quad, Farbe, Blend, Queue, Cursor-Wrap und
  Ausrüstung/System-/Presentation-/Hidden-Gates entsprechen der Referenz.
  `0x4100000` enthält Billboardbit `0x100000`, nicht Hidebit `0x1000000`.
- Der produktive World-Pass reicht eine Flagansicht als Argument hinein.
  Player-/Status-/Systemflags stammen aus vorhandenen EventVm-Banken 13/14/5;
  ohne EventVm dienen vorhandene World-Felder. Kein globaler Kontext-Work,
  kein Copy-in/out einer zweiten Player-/Systemwelt.
- Spielerflamme benutzt den gemeinsamen ersten freien Slot, Originalskala
  0,5, WeaponTab-Offset und Handknoten 9. Ablegen löscht das Allocationflag;
  Wiederanlegen nutzt denselben freien Slot und startet bei UV-Schritt 1.
  Spielerflag und bisheriger Event-Timer-Spiegel bleiben synchron.
- Event 0xa2 erzeugt Eff009 Typ 3 am tatsächlichen Objekt und bindet
  Roomlight 1 an Objekt/Knoten 0. Ausschalten löscht Systembit 0x20000000,
  Lichtaktivierung und im nächsten Effektpass die Flamme. Keine erfundene
  Objektposition, kein Raumsonderfall.
- Feuerzeuglicht: Typ 4, Geschwindigkeit 30, Handknoten 9, Offset
  (0,3; 0; -1,5), RGB (3,16; 2,06; 0,7), Reichweiten 8/400. SetLightTab
  berechnet aus den Nullwinkeln die Richtung (0; 0; -1).
- Gemeinsame Licht-RGB-Typen 1..6 sind auf den bestehenden Lichtern bestimmt:
  Arbeitsfarbe wird vor dem Clockvorschub berechnet; Typen 4..6 konsumieren
  genau einen Wert des gemeinsamen RNG je aktiver Lichtpass. Simulationbit
  0x80, Aktivierung und Eventkameramaske begrenzen den Vorschub. Draw-Aufrufe
  verändern weder Clock noch RNG. Die 724 aktiven Animationslicht-Datensätze
  der exportierten Räume haben Geschwindigkeiten 2..100, keine negativen.
- Die geportete Flamme zeichnet mit Ninja-Blendpaar 8/6 und unverändertem
  decoded Alpha im 0..128-Bereich, nicht additiv mit verdoppeltem Alpha.
  Native SceneTriangle-Samplerwahl bildet den ursprünglichen per-Draw-
  Point/Linear-Vertrag ab; Linear verwendet Basislevel-Bilinearfilterung,
  nicht den optionalen Host-Trilinear-/Anisotropiefilter. Der gemeinsame
  Zeichenhelfer übernimmt bei nicht-billboardartigen Links die Elternbasis.

## Nachweise

macOS arm64, eigener Build `build/native-ground-20261009`:

- Build einschließlich Runtime-Link erfolgreich; **49/49 CTest**.
- `native_lighter_effect`: Typen/Bänke/Flags, 25 UV-Schritte, gelatchte Bank,
  alle Pause-/Hide-/Löschegates, RGB-Typen 1..6 über jeweils 40 Schritte,
  exakte RNG-Verbrauchszahl, PVP/Kategorien und Originalatlasalpha.
- Echter RM_0050-World-Besitzer: native Playerflags, Actual-Handmatrix und
  Lampenoffset, Billboardanker bei zwei Yaws, sichtbare gerenderte Pixel,
  Shared-Pool-Reuse, Hidden-Status/Lichtrelink sowie Objekt-Ein/Aus über 0xa2.
  Der Objektfall ist ein diagnostischer Bindungstest, kein Durchspielen
  seines gesamten Originalraumskripts.
- `verify_lighter.py`: echte Eröffnung, Film/Dialog/Inventar und normales
  Ausrüsten/Ablegen, ohne eingesetzte Items/Spawns/Spieleflags. Genau eine
  sichtbare Typ-2-Flamme, alle acht Phasen, HP 160. Gegenprobe mit dem direkt
  vorher gesicherten kernfreien nativen Binary: Room/Position/Yaw/Weapon/
  Items/Events/HP/Blocked identisch, dort keine sichtbare Flamme.
  BMP visuell geprüft: tatsächliche Flamme an Claires Hand, keine zweite
  künstliche Welt. Die fehlende angehobene Armhaltung ist sichtbar und offen.
- `verify_rooms.py`: **205/205**, je **600 Ticks**, **0 blockierte Befehle**.
  Das ist ein Lade-/Runtime-Smoke, kein Gameplay-/Gegner-Abschlussnachweis.
- Runtime-Regressionsprüfungen Gameover (Yes/No), Snapshot und Events grün.
- Homogener Debug-Neubau aller Test-Abhängigkeiten mit ASan/UBSan in
  `build/lighter-asan-20261009` besteht. Ein vorheriger Mischlink aus
  instrumentierten Dateien und Releasebibliotheken meldete Container-
  Overflow; er ist kein gültiger Sanitizer-Nachweis. Der maßgebliche Neubau
  instrumentiert alle Abhängigkeiten und benutzt keinen Sanitizer-Bypass.
- Prüfer mit eigenem Build-Verzeichnis und Runtime-Binary **PASS**;
  **24/24 Policy-Vertragstests**, git diff --check sauber.

Logs/Zustände/Bilder: `build/validation/native-lighter-20261009-final/`.
Gameover/Event-Regressionslogs: `build/validation/native-lighter-20261009/`.

Windows x64: eigener Build `build/windows-ground-20261009`, Spiel-/Scene-
Bibliotheken und `recvx-lighter-tests.exe` erfolgreich kompiliert/gelinkt.
**Keine Windows-Ausführung und kein neuer Windows-Runtime-Audit beansprucht.**

## Offene Grenzen / nächste Teilaufgaben

1. **F01/P00: originale Armhaltung.** player.c:1515..1537 friert die Winkel
   von Knoten 7/8/9 für die Motion ein, bestimmt das Ziel aus Knoten 4,
   ruft `bhArmIkMdk(plp,6,...,35498)` auf und setzt Handwinkel Y auf -910.
   Das fehlt im nativen Playerpose-Besitzer. Die jetzige Flamme folgt der
   tatsächlichen vorhandenen Handpose; keine Winkelheuristik wurde ergänzt.
   Den Mechanismus inklusive vorheriger Objektbaum-Matrizen und Reihenfolge
   portieren, nicht einfach die Hand/Flamme an eine hübsche Stelle setzen.
2. **P00: vollständige Presentation-/Modelldraw-Verträge.** World besitzt
   pt_flg mit ursprünglichem Initialwert ~0u und expliziter Mutations-API;
   allgemeine Untersuchungs-/Systemübergänge dieses Feldes sind noch nicht
   vollständig angebunden. Fehlende Mirror-/Modelldraw-Gates nicht als
   erledigt zählen. Player_hidden ist der vorhandene native Model-Hide-Gate.
3. **E04:** restliche Lichttypen, ursprüngliche Lichtbereich-/Clipprüfung,
   Specular sowie vollständige Positions-/Arbeitscache-/Rendersemantik.
   Der vorhandene native Beleuchter wurde nicht als vollständiger Lichtport
   neu ausgegeben.
4. **E03/E02:** weitere Effekt-State-Machines, Explosion/Trümmer/Rauch,
   Blut/Partikel und Detektor. Sie bleiben ausdrücklich ungebunden; ihre
   bestehenden Lückentests wurden nicht zu Wirkungsnachweisen umbenannt.
5. Vollständige Realisierung der übrigen ef_info-Modellworks/Verbraucher ist
   noch offen. Gemeinsame Daten allein sind kein vollständiger Effektport.

Claudes getrenntes Folgepaket bleibt **B01 Treppen/Leitern/Stufen** nach G01;
Effekt-/Pose-Lücken dieses Berichts sind nicht dessen Türabschluss.
