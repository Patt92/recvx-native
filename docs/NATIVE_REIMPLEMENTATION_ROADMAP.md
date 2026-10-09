# Roadmap: eigenständige native Implementierungen, originales Verhalten

Stand: 2026-10-09, aktualisiert nach Wiederherstellung von Build, Runtime-Link
und Raumladen. Stabil, aber spielmechanisch unvollständig.
Diese Roadmap ist ein Arbeitsplan, kein Fertigstellungsnachweis. Task-IDs
bleiben stabil; bestehende native Teile erhalten, nicht erneut portieren.

## Aktueller Stand — Datenport, Rückbau und Gameplay getrennt

Der Abgleich erfolgte an den aktiven Dateien und CMake, nicht an Quarantäne-
Inhalten oder alten Erfolgsmeldungen. **Stillgelegt bedeutet nicht portiert.**
Die kommentierten Neutral-/No-op-Rückgaben sind offene Spielfunktionslücken
und keine zulässige 1:1-Implementierung, auch wenn sie einen Task beenden.

| Bereich / Tasks | Tatsächlicher Stand | Konsequenz für den nächsten Auftrag |
|---|---|---|
| P01 Itemdaten/Header | `native/item_tables.c/.h` ersetzen die direkte `item.c`-Quelle und den nativen Gebrauch von `item.h`. Der eigene Header nutzt `ninja_matrix.h` statt der fehlenden PS2-Ninja-Header. Alle zwölf Tabellen stimmen nach erneutem Kommentar-/Whitespace-bereinigtem Vergleich mit den Originalinitialisierern überein. | Daten-/Headerablösung auf Quellenebene erledigt; nicht nochmals als `item_data.*` bauen. Regression der Werte und Layouts auf beiden Plattformen ergänzen; keine Mechanismus-/Runtime-Abnahme. |
| P02 Raumindex | `native/room_table.c/.h` und `RoomArchive::name` ersetzen die direkte `ps2_dvd_image.c`-Quelle. Alle 205 RDX-Namen sind in Originalreihenfolge identisch. | Quellenablösung erledigt. Kein neues Disc-/ISO-Subsystem erfinden; Index-/Archivregression nachtragen. |
| A01–A05 Kampf/Zielwahl | `targeting.cpp` und `player_combat_core.c` sind stillgelegt: keine automatische Zielwahl, kein aktiver Kampf/Schuss/Messer, keine Armsteuerung. | Mechanismen eigenständig neu implementieren; nicht nur Includes/zweite Works entfernen. WeaponFlash und reine Waffendaten erhalten. |
| B00–B04 Bewegung | `player_push_core.c`, `player_stairs.c`, `player_hair.c` sind stillgelegt: keine dortige Wand-/Bodenauflösung, Push-, Treppen-/Leiteraktion oder Haarsimulation. | Gemeinsame Geometrie/Kollision zuerst, danach vollständige Zustandsabläufe. Neutrale Positionsrückgabe ist kein Kollisionsport. |
| B05/D01 Itembox | `control_item_box_object` ist No-op; der Welt-Deckel öffnet kein Menü. Der Itembox-Transfer in `inventory.cpp` ist inzwischen ebenfalls stillgelegt. | Weltobjekt und Inventarmenü getrennt bearbeiten, dann auf denselben Bestand binden. |
| C01/C05 Spawn/Subplayer | `activate_enemy_spawn` liefert immer false; `recvx_is_subplayer` immer 0, Init/Tick tun nichts. | Fallenaktivierung und tatsächliche Subplayer-ID-/Statebindung wieder vollständig portieren. Normale Raumplatzierung ist nicht derselbe Mechanismus. |
| C03/C04 EN01/EN04/EN00 | Auch `en01_damage.c` ist inzwischen stillgelegt: nur Hostwerte werden in `EN01_STORE` gespeichert/zurückgespiegelt. KI, Kapselhit, Schaden, Tod, Griff/Biss und Sprengfass fehlen. | Keine funktionierenden Zombies/Hunde behaupten. Gemeinsame native Enemy-Statequelle statt Durchleitung schaffen, alle Abläufe einschließlich EN00 portieren. |
| D02/D03/D05/D06 Inventar | Auch Pickup/Use/Box, Filefolder, Gameover und Event-Inventarmutationen sind jetzt stillgelegt. Die Event-VM meldet die fehlenden Mutationen als pending; übrige APIs sind weitgehend neutral. Native Tabellen-/Matrix-/Darstellungshelfer bestehen weiter. | Aufnahme, Gebrauch, Sortierung, Reload, Untersuchung, Leser, Tod/Retry und Eventmutationen vollständig implementieren. Keine Tests an die Neutralantworten anpassen. |
| D07 Kombinationen | `combinations.inc` und seine Originalbrücke sind nicht mehr eingebunden. `Inventory::combine` meldet Fehlschlag und lässt Slots unverändert; der native Tabellen-Suchhelfer bleibt vorhanden. | Rückbau erledigt, sechs Kombinationsmechanismen fehlen. D07 implementiert Originalwirkung direkt auf derselben Inventory, keine Neutralantwort als Fertigkriterium. |
| D08 EKG | `status_pulse.cpp` enthält keine `Pulse*`-Körper mehr. `tick` liefert keine Quads, `heal_mode` bleibt 0; Condition-API und Renderer-Schnittstelle bestehen weiter. | Rückbau erledigt; Kurve, Zustands-/Heil-/Gift-/Fadeverlauf und Quad-Ausgabe eigenständig implementieren. |
| P05/D09 Item-UI | Daten aus dem gemischten 1160-Zeilen-Include liegen als `native/ui_tables_data.inc` im Baum. `BulletSet`, `TrigerSet` und `original_command_parts` fehlen: keine Mengenanzeige und leeres gezeichnetes Itembefehlsfenster. | Datenaufteilung erhalten, nicht erneut extrahieren. D09 implementiert Ziffern/Farbbank und originale Befehlsausgabe; vorhandene Menüeingabe ist kein Nachweis der fehlenden Darstellung. |
| D04 Computer | `computer_core.c` liefert leeren Frame und sofort `finished`; Terminalskripte, Tastatur, Scanner-/Passwortflags und SFX fehlen. | Vollständigen Ablauf neu portieren; sofortige Taskfreigabe nicht als erfolgreichen Computerablauf zählen. |
| E01–E03 Effekte | Ground/Wall/Line-Abfragen sind leer bzw. liefern -1000; Partikel werden nicht erzeugt; Sprite-/Explosions-/Feuerzeugsteuerung ist stillgelegt. Native Feldübernahme/Slotwahl/Zeichenausgabe bestehen teilweise weiter. | Gemeinsame ATR-Abfragen, Effektdaten/Atlasbindung und vollständige State-Machines portieren; bestehende native Ausgabe erhalten. |
| P04/G01 Clip/Tür | Beide Clip-Abfragen liefern nur den Ninja-Standard (near 2, far 20000, ts_flg 0x200), kamerabezogene Bereiche fehlen. `door_core.c` hat keine Parametergrößen/Prozeduren und meldet sofort Abschluss 0x40. | Clip- und MPAC-/Türabläufe einschließlich Zeitpunkten/SFX implementieren, keine neuen Abschluss-Stubs. |
| F01–F05 Actor/Face/Morph | Native Actor-/Basic-/Skin-/Pose-Schicht vorhanden. Morph-Events schreiben Felder, Vertexinterpolation/Draw bleibt offen; aktive Lip-Streamquelle liegt getrennt vom neu gewählten Stream auf `WorldEntity`. | Bestehende Decoder und Lip-Cursor-Semantik behalten; produktive Besitzer-/Vertex-/Face-/Shadowbindungen vervollständigen. Alte Core-Morph-Erfolge gelten nicht. |
| Math/WeaponFlash/BGM | Native Math und sechs WeaponFlash-Typen bestehen. BGM-Discregel ist an beiden Raumladewegen gebunden. | Nicht neu anfangen. Typ-3-Wandbindung, Point-Filter und Enemy-SFX-Reihenfolge bleiben Abnahme-/Integrationsaufgaben. |

**Technische Basis wiederhergestellt:** Claude meldet erfolgreiches Configure,
Build und Link von `recvx-runtime`, 205/205 ladende Räume und keine blockierten
Eventbefehle. Der Schutzprüfer wurde für diese Aktualisierung zusätzlich gegen
Quellen, `build/gapcheck-claude` und dessen Runtime-Binary ausgeführt: **PASS**.
Der Raumlauf wurde hier nicht erneut ausgeführt. Keine Original-C-Datei ist
Kompilierquelle; Referenz-DEPENDS bleiben erlaubt. D07–D09-Verbraucherrückbau
ist erledigt, nicht erneut durchführen. Diese Nachweise gelten nicht als
Spielbarkeit, Gegnerabnahme oder Windows-Runtime-Nachweis.

CTest war bei Claudes Meldung **34/41**, sieben Fälle noch in Bearbeitung.
Das ist ein Zwischenstand, kein endgültiges Testergebnis. Für jeden übrigen
Fehler tatsächliche Regression, fehlenden Mechanismus oder fehlerhaften Test
unterscheiden; fehlende Wirkung nicht durch Neutral-Erwartungen wegtesten.

Der Guard in `world.cpp` für `common_effect_textures_offset == size_t(-1)`
behebt einen echten Zugriff auf einen ungültigen Datei-Offset. Er bleibt
erhalten. Gemeinsame Effekttexturen sind trotzdem offen (E03): weder Offset 0
noch leere/falsche Texturen als Ersatz einsetzen. Fehlender Block und gültiger
Block benötigen getrennte Regressionen beim Integrator.

**R02 bleibt offen, mit engerem Scope:** Für `adx_tables.inc` ruft CMake
weiter `generate_native_tables.py` auf. Dieser erzeugt zusätzlich die drei
gesperrten Funktionsoutputs und CMake entfernt sie anschließend per `rm -f`.
Nicht mehr eingebunden heißt hier nicht rein datenbasierter Generator.
Die Quelle muss ohne Funktionsausgabe ADX-Daten liefern; nachträgliches
Löschen darf weder die Erzeugerprüfung noch den Output-Audit ersetzen.
Der aktuelle statische PASS erkennt diesen Mischgeneratorrest nicht.

Die Tests für unter anderem Munition und StatusPulse wurden inzwischen auf
dokumentierte Lücken umgestellt (kein Pickup/Reload, keine EKG-Quads).
Das sind Rückbau-/Lückenverträge, keine ursprünglichen Gameplay-Regressionen.
Ihre Erfolge nicht als Portabdeckung oder 1:1-Abnahme zählen. R03 führt die
fehlenden Wirkungsfälle wieder gegen die eigenständigen nativen Ports ein.

## Verbindlicher Auftrag für jeden Agenten

Ein eigenständig laufendes natives Game auf macOS arm64 und Windows x64,
mit originalem Verhalten und allen Gegnern in der produktiven World. Keine
PS2-/CPU-/VU-/GS-Nachbildung, kein Originalkern und keine Ersatz-Spiellogik.

`archived/FORBIDDEN_DONTUSE` darf von keinem Task gelesen, durchsucht,
kopiert, importiert, wiederhergestellt oder verwendet werden. Auch keine
Umwege über alte Archive, Patches, Bundles, Tasks oder Build-Caches.
Semantik aus `src/ps2/veronica/prog`, den Originaldaten und Disassembly
herleiten; diese Referenzquellen nicht kompilieren. Eigenständige native
Implementierung mit nachvollziehbarer Quellenzuordnung, nicht umbenannter
Original-Funktionskörper oder ein generierter Körper in einer neuen Datei.

Algorithmen, Reihenfolge, Konstanten, Flags, Tabellen, Zustandsübergänge und
Seiteneffekte 1:1 erhalten. Keine Raum-/Gegner-Sonderfälle, Heuristiken,
stille No-ops, Fake-Rückgaben oder "Korrekturen" am Original ohne Beleg.

Vor Arbeitsbeginn und jedem Build den Schutzprüfer ausführen und Befunde
melden. Die technische Baubarkeit ist wieder belegt; eine Gameplay-Freigabe
ist es nicht. R02 muss weiterhin die unnötige Funktionsausgabe des
ADX-Mischgenerators und die entsprechende Prüflücke schließen. Der aktuelle
PASS hebt diesen konkret belegten Rest nicht auf. Stillgelegte Bereiche
bleiben fehlende Spielfunktionen; weder Prüfer noch Wirkungsansprüche lockern.
Isolierte native Komponententests sind kein produktiver Spielnachweis.

## Besitz, Parallelität und gemeinsame Abnahme

- Ein Integrator besitzt `CMakeLists.txt`, `native/world.cpp/.h`,
  `native/runtime.cpp`, `native/events.cpp`, globale Work-/Asset-Schnittstellen,
  Packaging und gemeinsame Dokumentation. Andere Agents liefern kleine neue
  native Module und Anschluss-Patches als getrennte Übergabe, statt diese
  gemeinsamen Dateien gleichzeitig zu editieren.
- P00 legt den bestehenden nativen Besitzer jedes Zustands fest und friert
  minimale Schnittstellen ein. Keine neue parallel laufende Player-, Enemy-,
  Room- oder Effect-Work. Views/Referenzen auf den vorhandenen Speicher sind
  erlaubt; Spiegelwelten und Copy-in/Copy-out-Dispatcher sind es nicht.
- Jeder Teilauftrag bekommt einen eigenen Quellen-/Disassembly-Beleg und
  Zustands-/Ausgabevergleich in `work/native_port_<ID>.result.md` sowie einen
  eng begrenzten Test. Gemeinsame Dateien nur durch den Integrator ändern.
- Erst alle Module eines Pakets implementieren und integrieren, dann einen
  gemeinsamen Regressionstestblock durchführen. Gemeinsame Ursachen global
  beheben; nicht für jeden Gegner/Raum eine Sonderlösung bauen.
- CMake-/Compiler-/Link-/Binaryprüfung müssen vollständig grün sein, bevor
  produktive Runtime-Abnahme oder Paketveröffentlichung erfolgt. Kein alter
  Kern als Vergleichsruntime; Originaldaten/Disassembly sind die Referenz.

## Phase 0 — Integrationsreste bereinigen, Besitzer und Geometrie festlegen

R00/R01/R02/R03/P00 gehören dem Integrator. P01/P02/P05 sind keine neuen
Implementierungsaufträge mehr. Quellenport und Datenvergleich liegen vor;
die verankerten Daten-/Layoutregressionen fehlen noch. Der macOS-Runtime-Link
ist wieder belegt, nicht mehr als unerledigter Rückbauauftrag führen.

| ID | Kleiner Auftrag / erlaubter Schreibbereich | Abhängigkeit / Fertigkriterium |
|---|---|---|
| R00 | Bekannte Funktionsincludes einschließlich Kombinationen, EKG und UI sind nicht mehr eingebunden; keine direkte Original-C-Quelle mehr. Integrator prüft finalen Diff und Daten-/Layout-/Funktionsklassifizierung. | Verbraucherrückbau auf Quellenebene erledigt, nicht wiederholen. ADX-Mischerzeugung ist R02; fehlendes Gameplay bleibt D07–D09. Keine Codeentfernung als Port-Erfolg zählen. |
| R01 | Integrator: Fehlfunktionsliste der stillgelegten APIs an tatsächlichen Aufrufpfaden pflegen. Gameplay-/Abnahmetests dürfen Neutral-/Abschlussantworten nicht als erfolgreich portierte Wirkung zählen; getrennte Lückentests gehören in R03. | Parallel zu R00/P00. Kampf, Kollisionshit, Trapspawn, Subplayer, Terminal-/Türabschluss, Itembox, Inspection und Effektanforderungen bleiben sichtbar unvollständig. Prüfer-grün allein ist keine Spiel-Freigabe. |
| R02 | Kleiner erster Restauftrag: ADX-Daten über eigenständigen Daten-only-Erzeuger oder geprüftes natives Datenmodul bereitstellen. CMake darf den Mischgenerator nicht mehr ausführen und muss keine Funktionsoutputs nachträglich löschen. Schutzprüfer um Mischgenerator-/Bodyoutput-Erkennung ergänzen. | D07–D09-Verbraucher sind bereits stillgelegt; nicht nochmals entfernen. Original-ADX-Werte exakt erhalten, pure Combat-/Chunk-/UI-Daten nicht pauschal sperren. Synthetische Negativtests auch für „erzeugen, danach löschen“ und Teilkörper; Daten-only-Ausgabe positiv prüfen. Keine Ausnahme und keine Generatorausführung zur Klassifizierung. |
| R03 | Integrator: Testabdeckung als Daten-/Rückbau-/Lückenvertrag oder tatsächlichen Mechanismus-/Runtime-Nachweis kennzeichnen; verlorene Wirkungsregressionen pro Task erfassen und nach vollständiger Paketimplementierung wieder gegen den nativen Besitzer prüfen. | Parallel zu P00, gemeinsame Tests gebündelt nach Integration. Keine Rückkehr zu Kern-/Generator-Tests. Lückentests dürfen existieren, aber kein „0 Quads/kein Treffer/keine Mutation“ als 1:1-Erfolg; alte Gameplayfälle anhand Originalreferenzen und zulässiger Testhistorie herleiten, Quarantäne nicht lesen. |
| P00 | Integrator: bestehende Player-/WorldEntity-/Inventory-/Effect-/Pose-Besitzer dokumentieren; kleine gemeinsame State-Views und Mutations-API definieren. Keine Logik implementieren. | Startpunkt. Feldmapping, Reihenfolge und Eigentümer für jedes Folgepaket; keine zweiten Works. |
| P01 | Bereits vorhanden: `native/item_tables.c/.h` samt Verbrauchern behalten. Regression für zwölf Tabellen, gepackte Werte sowie native ITEM_WORK-/DSP_WORK-/NJS_POINT3-Größen, Alignment und Feldoffsets hinzufügen. | Erneuter Quellvergleich bestanden; eigenen Header nicht durch `item.h`/fehlende PS2-Ninja-Header ersetzen. macOS-arm64/Windows-x64-Layout prüfen; Itemmechanismen bleiben D02/D03/D07. |
| P02 | Bereits vorhanden: `native/room_table.c/.h` und `RoomArchive` behalten. Regression der 205 Namen/Indizes, Bereichsfehler und tatsächlich zugeordneten Raumdaten hinzufügen. | Geordneter Quellvergleich bestanden. Keine unbelegte Behauptung eines Disc-/Dateimechanismusports und kein neues ISO-System. |
| P03 | Native Geometrieprimitiven für folgende Pakete: zuerst benötigte Box-/Kapsel-/Linien-/Dreiecksoperationen in `native/geometry_*.*`. | P00. Originalfälle und Randfälle, keine neu erfundene Kollision. `ps2_NaColi.c` wird nicht kompiliert. |
| P04 | `bhChangeViewClip` / `bhChangeViewClipRM` aus Originalreferenz als eigene native Kamera-Clipmechanismen in `native/view_clip.cpp` implementieren. | Sofort parallel zu P00/P03 möglich. Der alte Include ist bereits entfernt, Verhalten aber nicht portiert. Opcode-Semantik, Grenzen und Reihenfolge; keine geratenen Kamera-Sonderwerte. |
| P05 | Bereits vorhandene UI-Daten `native/ui_tables_data.inc` erhalten; PARTS-/NJS_TEXANIM-/Font-/Positionsinitialisierer und native Deklarationen gegen Originalreferenz prüfen und Datenregression nachtragen. | Keine erneute Extraktion und keine Funktionen in die Datei einsetzen. Werte/Reihenfolge/Dimensionen sind Datenabnahme, nicht BulletSet-/Befehls- oder EKG-Abnahme. |

## Nächste konfliktarme Arbeitswelle

Der konkrete Folgeauftrag für Claude steht in
[`CLAUDE_NEXT_TASKS.md`](CLAUDE_NEXT_TASKS.md). Keine weitere breite
Stilllegungsrunde; zuerst den kleinen Erzeugerrest schließen, danach die
fehlenden Inventar-/Status-/UI-Mechanismen als vollständiges Paket bauen.

1. Integrator: R02 mit Claude als erstem abgegrenztem Auftrag abstimmen;
   R00/R01/R03/P00 sowie Datenregressionen P01/P02/P05 weiterführen.
   Eigentümer und Schnittstellen festlegen, keine zweite Player-/Enemy-/Effect-Welt.
2. Geometrie-Agent: P03 → E01 (gemeinsame ATR-Boden/Wand/Linie/Hit-Identität)
   → B00. Primitive und Services einmal portieren; Bewegung, Zielwahl,
   Gegner, Partikel und Typ-3-Flash konsumieren dieselbe Quelle.
3. Actor-Agent: F01/F02 und gemeinsamer C02-Asset-/Pose-Vertrag; vorhandene
   Decoder weiterverwenden. Danach F03/F04/F05 nach gemeinsamen Verträgen.
4. Ablauf-Agent: P04 und G01 in getrennten vorhandenen Dateien. Echte
   Clip-/Türparameter und Zustandsübergänge, keine sofortigen Erfolgsrückgaben.
5. Claude: D02a/D02b → D02c/D02d → D07 → D01 → D06-Anschlussübergabe
   → D09 → D08. Inventar nicht zwischen Agents aufteilen; die gesamte
   Mutation läuft auf derselben `Inventory`. Danach gemeinsame Tests und
   Ursachenfixes für das Paket. D03/D04/D05 sind nachfolgende getrennte Pakete,
   nicht stillschweigend Teil dieses ersten Auftrags.

Nach P00 sind A, C und D unabhängig vorbereitbar; Integration erst gegen
die freigegebenen Verträge. D07/D08/D09 sind nun reine Implementierungs-
aufträge für fehlende Mechanismen, keine erneuten Rückbauaufträge. Sie lassen
sich nach P00 in getrennten Dateien bearbeiten: D07 `inventory.cpp`,
D08 `status_pulse.cpp/.h`, D09 `ui.cpp`; D01/D02 dürfen nicht zeitgleich mit
D07 dieselbe Inventory editieren. Zusätzliche Agents können D04 und D05 nach
explizit getrennter Dateizuständigkeit übernehmen. R00/P00/G02 sowie gemeinsame
World-/Runtime-Dateien bleiben beim Integrator. Die Aufteilung ist ein Plan;
hierdurch werden keine Tasks gestartet oder laufende Zuständigkeiten geändert.

## Phase 1 — parallele Paketimplementierungen

Pro Paket genau ein Agent; kleinere Teilaufgaben in der angegebenen Reihenfolge.
Pakete dürfen nach P00 parallel laufen, soweit ihre Abhängigkeiten erfüllt sind.

### A — Kampf (Owner: `native/combat*`, `native/targeting*`)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| A01 | Fehlende Zielsuche/-wechsel direkt auf vorhandenen WorldEntity-/Player-Views implementieren; Wand-Sichtprüfung und Armwinkelsteuerung einschließen. | P00/P03/E01; Gate-/Distanz-/Winkel-/Dual-Target-Tabellen exakt. `Targeting::select/arms` sind momentan neutral. |
| A02 | Ready/Release/Reload/Bullet-Zähler und Inventarübergänge eigenständig portieren. | A01/P01; Flag-/Munitions-/Frameverlauf ohne separate COMBAT_SYS/BH_PWORK-Welt. |
| A03 | Pistol/Burst/Dual-State-Machines in die vorhandene CombatController-Statequelle übertragen. | A02; Motion-/Latch-/Posefolgen und Sideeffects belegen. |
| A04 | Knife-/Trefferzuweisung, Joint-/CPCL-Daten und Damage-Meldung an tatsächliche native Enemy-Works übertragen. | A03/P03/C02; keine zweite Ziel-/Schadensliste. |
| A05 | Weitere Originalwaffen in Tabellenreihenfolge: Scope, Shotgun, Armbrust, Granaten, Launcher. Alle implementieren, anschließend gesammelt prüfen. | A04/E01–E03; Licht/SFX/Blut/Explosion nicht wegstubben. |

Der derzeit inaktive `player_combat_core.c`-Vertrag muss dabei entweder direkt
an dieselbe native Combat-Statequelle gebunden oder mit den Aufrufern ersetzt
werden. Nicht die neue Implementierung pauschal quarantinieren: Nur tatsächlich
ersetzte Altartefakte recoverable verschieben. Keine Funktion nur kopieren oder
umbenennen; kein Wiederaktivieren des alten Funktionsgenerators.

### B — Player-/Objektbewegung (Owner: `native/player_*`, `native/object_itmbox.cpp`)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| B00 | Player-Wand-/Boden-/Dansa-Auflösung und dynamische ETC-/FLR-Bindungen aus der stillgelegten Push-Brücke auf vorhandene Player-/RoomWorld-Daten portieren. | P00/P03/E01. Flags, ATR-Typen, `grand`, Reihenfolge ROM vor mobile Work und tatsächliche Hit-Identität. `native/collision.cpp` auf belegte Originalsemantik prüfen; keine Substep-/Polygonheuristik als Ersatz übernehmen. |
| B01 | Fehlende Treppen-/Leiter-/Stufenaktionen, Motion, Etagenwechsel, Fußfixierung, SFX und Release auf bestehender nativer Player-Statequelle implementieren. | P00/B00; sofortiges Ende in `recvx_stairs_tick` ersetzen. Include und Ersatz-Player-Work sind bereits entfernt. |
| B02 | Pushkontakt-/Objektinitialisierung und mobile Raumkollision direkt auf WorldEntity-Objekten implementieren. | P00/P03/B00; Reihenfolge und originale Tabellen; keine PUSH_SYS/PUSH_ROOM-Kopie. |
| B03 | Push-State-Machine, Motionwurzel und Move-/Releasepfad eigenständig implementieren. | B00/B02; vorhandenen inaktiven `player_push_core.c`-Aufrufvertrag auf dieselbe native Quelle binden oder geordnet ersetzen. Keine zweite Positionsbuchführung als Spiellogik. |
| B04 | Haar-/verknüpfte Objekttransformation auf tatsächliche native Actor-Pose übertragen. | P00/F01; keine eigene Player-/Allocator-Attrappe. |
| B05 | Fehlenden Itembox-Objektablauf/Deckel-/Motionzustände, Playerflags, Menüanforderung und SFX direkt an native Objekt-/Inventar-Statequelle binden. | P00/D01; `control_item_box_object` ist No-op, der Include bereits entfernt. Keine unnötige Abhängigkeit von der kompletten Push-Kiste. |

### C — Gegner und Subplayer (Owner: neue `native/enemy_*`-Module)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| C01 | Originale Raum-/Eventspawn-Daten und Initialisierungsreihenfolge für alle IDs aufnehmen; fehlende ATR-Fallenaktivierung, Etagen-/Richtungswahl und EN04-Kontaktfälle auf vorhandenem RoomWorld-Spawnweg implementieren. | P00, Kontaktfälle P03/E01/B00; `activate_enemy_spawn` ist derzeit immer false. Normale Raumplatzierung getrennt belegen; kein künstliches Spawn-Injizieren. |
| C02 | Gemeinsame native Enemy-State-/EXP-/Child-/Motion-/Damage-Felder ausschließlich am vorhandenen Entity-Besitzer. | P00, abgestimmter F01-Vertrag; State-/Asset-Teams können parallel arbeiten. Arena/Lebenszyklus nativ, keine BH_PWORK-Spiegel-/Globalswelt. |
| C03 | Fehlende EN01 Init/Move/Skript-/Damage-/Death-/Child-/Griff-/Bissmechanismen eigenständig portieren. | C01/C02/A04; Includes sind bereits entfernt. `EN01_STORE`-Durchleitung durch tatsächliche native Entity-Zustandsfortschreibung ersetzen; alle Zweige vor Sammeltests. |
| C04 | EN04 Init/Move/Attack/Damage/Death auf dieselbe native Statequelle übertragen. | C01/C02/A04 sowie gemeinsame Helper-Verträge aus C03; keine unnötige Wartepflicht auf den kompletten EN01-Port. Kein fester EXP-Ersatz oder zweiter Dispatchbesitzer. |
| C05 | Tatsächliche Subplayer-ID-Tabelle sowie Init/Move/Skript-/Motion-/Releaseabläufe eigenständig portieren. | C02/F01; `recvx_is_subplayer` erkennt momentan keine ID, Init/Tick sind leer. EventVm und Draw lesen dieselbe Work; keine pauschal geratenen ID-Bereiche. |
| C06 | EN07 vollständig portieren: originalbelegter Spawn, Init, Animation, KI, Attack/Grab/Damage/Death, SFX und Draw. | C01/C02/E/F/G; verbindliche produktive Abnahme RM_0130 (fünf originale ID-7-Einträge), kein Direktdispatch-Test als Ersatz. |
| C07+ | Restliche fehlende IDs in disjunkten kleinen Gruppen; erst nach gemeinsamen Schnittstellen auf andere Agents verteilen. Für jede Gruppe vollständige Originalfälle, dann Sammeltest. | C02/F/E/G. Gruppierung aus Original-Dispatch-/Abhängigkeitsdaten bestimmen, nicht nach vermuteten Gegnernamen. |
| C08 | EN00-Sprengfass: Zustand, bewegliche Wand, Treffer-/Explosions-/Verschwindepfad eigenständig portieren. | C02/A04/E01/E03; bislang ebenfalls Teil der nun entfernten EN01-Generatorbrücke. Kein verlorener Nebenfall des Gegnerpakets. |

### D — Inventar/Menü/Computer (Owner: `native/inventory*`, `inspection*`, `file_folder*`, `computer*`, `gameover*`)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| D01 | Fehlende Itembox-Bestands-/Auswahl-/Transferzustände eigenständig auf derselben Inventory portieren. | P00/P01; `box_engine.inc` ist entfernt, Transfer neutral. Weltobjekt B05 separat anschließen. |
| D02 | Fehlende Pickup-/Kapazitäts-/Stack-/ItemSort-/Equip-/Search-/Erase-/Reload-/CountBullet- und ItemUse00-/übrige Gebrauchsfälle portieren. | P00/P01 und A02-Vertrag; gemeinsame Bestandshelper zuerst, D01 konsumiert sie anschließend. Includes sind entfernt, Mengen-/Flag-/Slotfolgen fehlen. Kombinationen sind ein eigener Restauftrag D07. |
| D03 | Fehlenden Inspection-Automaten, Fragen, Itemumwandlung, Zoom-/Aktionspose, Hidden-Nodes, Namens-/Dateizuordnung sowie Filefolder-Auswahl/-Drehung/-Leserübergang implementieren. | D02; beide Includes entfernt. Native Tabellen-/Matrix-/Foldergeometriehelfer erhalten und semantisch prüfen. |
| D04 | Terminal-Skriptinterpreter mit acht Skripten, Fensterautomat, Softkeyboard, Text/Quads, Originalflags, SFX und Abbruchpfad auf native Dialog-/Eventstates portieren. | P00 und vorhandener Inventory-/Message-Vertrag; D03 nicht vollständig abwarten. Kein eigener SYS-/ROM-Wrapper; sofortiges `finished` in `computer_core.c` ersetzen. |
| D05 | Fehlende Todesszene, 16x16-Netz-/Texturdaten, Timing/Fade, Retry, Release und Titelrückkehr in bestehenden nativen Ablauf übertragen. | P00, Texturvertrag E03; `gameover.inc` entfernt. `begin`/`draw` sind leer, `active` false; keine erfolgreiche Rückkehr behaupten. |
| D06 | Sieben fehlende Event-Inventarmutationen für Integrator vorbereiten; Anschluss ausschließlich an dieselbe Inventory. | D01/D02; Include entfernt, VM meldet pending. Original-Slotbereiche, Ersttreffer, Ausrüstungs-/Munitionsflags; keine Schattenbestände. |
| D07 | Fehlende sechs Kombinationsmechanismen eigenständig direkt auf Inventory-Slots, Itemdaten und Messagezustand implementieren; vorhandenen Tabellen-Suchhelfer prüfen/behalten. | P00/P01, Sortier-/Equipvertrag D02. Originalbrücke entfernt, `combine()` aktuell immer erfolglos. Mengenprüfung, Produktwechsel, Restmenge, Flags, Slots und Meldungen 1:1; gültige Kombination muss tatsächlich wirken. Keine kopierten Körper. |
| D08 | Fehlenden Status-/Heil-/Gift-EKG-Automaten, Kurven-/Fade-/UV-/Farbausgabe eigenständig auf bestehendem `StatusPulse` implementieren. | P00, Heil-/Giftvertrag D02. Keine Körper mehr eingebunden; aktuell keine Quads und heal_mode 0. Originalzustandsfelder/Flagbits/Framefolge und vier Quads je relevantem Ausgabeschritt; keine Ersatzkurve, kein thread-local Spiegel mit Copy-in/Copy-out. |
| D09 | Fehlenden Mengen-Ziffernaufbau/Farbbank sowie Triger-/ItemCommand-Auswahl, Sichtbarkeit, Höhe und Beschriftung direkt auf nativen UI-/Inventory-Zuständen implementieren. | P00/P01/P05, A02-/D02-Verträge. Daten stehen bereits in `ui_tables_data.inc`, Funktionskörper entfernt. Mengenanzeige und Befehlsfenster fehlen; Eingabemenü weiterhin vorhanden. Originale Item-/Cursor-/Waffenfälle, keine Teilkörperkopie. |

D02 wird in kleine, aufeinander aufbauende Teilaufgaben geteilt, ohne einen
zweiten Inventarbesitzer anzulegen:

| Teil-ID | Umfang | Grenze / Anschluss |
|---|---|---|
| D02a | Aufnahmefrage, Antworten/Modi, Kapazität, breite Items, Stack-Maxima, Geschenk-/Skript- und Sofortgebrauchsfälle. | `pickup`/Bestandsmutation; Gebrauch über denselben D02d-Vertrag, nicht als Sonderfall doppeln. |
| D02b | Originalsortierung, Suche/Löschen, Ausrüstungsindex und Nachführung ausgewählter Slots. | Gemeinsame Helper für Aufnahme, Kombination, Kiste und Events; vorhandene Host-Helper semantisch prüfen, nicht blind übernehmen. |
| D02c | Inventarseitiges Nachladen und Munitionszählung einschließlich gepackter Flags, signed Zähler und Slotfolge. | A02 erhält einen kleinen Vertrag auf dieselbe Inventory; keine Combat-State-Machine im Inventar neu erfinden. |
| D02d | Alle ItemUse-/Use_00-Fälle, Heilung/Gift/Spray, Trigger, Meldung, Schließen und Pulseanforderung. | Bestehende Player-/Event-/Statuszustände verwenden; D08 konsumiert dieselbe Heil-/Giftwirkung. |

D06 umfasst `bhPlItemLost`, `bhItemGetGet`, `bhPlItemLostEx`,
`bhArmsItemSet`, `bhItemGetGetEx`, `bhPlItemChangeM`, `bhPlItemTamaSet`.
Claude liefert native Mutationsverträge und belegte Anschlussänderungen;
der Integrator besitzt `events.cpp` und integriert sie ohne Schattenbestände.

### E — Effekte/Sound/Licht (Owner: `native/effect_*`, kleine native Service-Module)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| E01 | Fehlende gemeinsame ATR-Wall-/Ground-/Line-/Reflexionsmechanismen einschließlich echter `bhCheckL2Wall`-Semantik und Hit-Eintragsidentität implementieren. | P00/P03; `effect_collision.inc` ist bereits entfernt. ROM/mobile Reihenfolge, Typen 0..7/Rampen und Flagfälle; keine vereinfachte Strahlheuristik. |
| E02 | Fehlende Partikelemitter, Stufenverlauf, Zufall, Bewegung, Farbe/Atlas, Line-/Billboardausgabe und Blutlache auf bestehender nativer Effekt-Statequelle implementieren. | P00/E01; `effect_particles.inc` ist entfernt. Bestehenden Besitzer zuerst in P00 bestimmen, keinen zusätzlichen O_WRK-/Effect-Pool voraussetzen oder erfinden. |
| E03 | Fehlende Sprite-/Custom-/Explosion-/Blood-/Linked-Effectfälle, Effekt-ID-/Atlasdaten, gemeinsame Texturblockwahl und Feuerzeuglicht auf vorhandener Scene/Effekt-Statequelle implementieren. | E01/E02/F01; `effect_sprites.inc` ist entfernt. Native Feldübernahme/Slot-/Drawhelfer erhalten, vorhandene additive Näherungen anhand Originalsemantik prüfen; keine sys/plp/rom-Attrappen. |
| E04 | Gemeinsame originale Enemy-/Player-SFX-/Voice-/Light-Reihenfolge an AudioMixer/OriginalLight binden. | C02/E03; keine fehlenden Aufrufe durch Null-/No-op-Rückgaben verdecken. |
| E05 | Waffen-Typ-3-Wandbindung und native per-Draw-Samplerwahl schließen. | E01; vorhandene WeaponFlash-Implementierung behalten, keine GS-/GIF-Transportschicht. |

### F — Actor/Asset/Draw (Owner: `native/actor*`, `native/face*`, Renderer nur nach Integration)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| F01 | Vollständige MDL-/MTN-/Skin-/Basic-/Child-Assetdaten auf die vorhandene native Actor-/Assetquelle portieren. | P00; Slots, Eigentümer, Motionverzeichnis und mutable Daten direkt gebunden. |
| F02 | Originale Motion-/Interpolation-/Flag-/Node-/Vertex-Seiteneffekte im selben nativen Actor fortschreiben. | F01; keine zweite Pose oder separat laufende Original-Work. |
| F03 | Morph Push/Calc/Draw/Pop nativ für alle Besitzer: Player, Enemy, Subplayer, Object. | F02; kein Vertexdrift, Originalobjektwahl und Event-/KI-Flags; nicht aus Quarantäne kopieren. |
| F04 | Face/Lip und Ownerbindungen eigenständig vervollständigen; aktiver LIP-Stream bleibt bis zur originalen Aktivierung unverändert. | F02; Originaldaten/Disassembly, Cursor-/Signed-MASK-/Lebenszyklusnachweise. |
| F05 | Shadow vollständig in vorhandenen nativen Zeichenpfad portieren. | F02; Originalgeometrie-/Clip-/Blendalgorithmus statt GS/VU-Simulation. |

F04 ist kein erneuter Rückbau von `face.*`: der vorhandene Port ist
policy-konform. Zwei belegte Abweichungen als getrennte Teilaufgaben beim
Actor-/Face-Besitzer schließen: **F04a** Winkelübernahme von
`toothsrc`/`tangorg`/`eyesrc` auf die tatsächlichen Poseknoten
`objP[7/9/11/13]` (`face.c:232–239`), **F04b** Augenwinkel aus der durch
`eyemat` transformierten Blickrichtung (`_fmCnkCalcEye`). Keine zweite Pose,
keine zusätzliche Rotation desselben Ergebnisses und kein Originalkern.
Aktiver Lip-Cursor, Signed-MASK-Daten und Pose-/Vertexfolge bleiben gemeinsame
Regressionen; Face/Lip gehört nicht zum neuen Inventarauftrag für Claude.

### G — Raum/Tür/Kamera (Owner: `native/door_core*`, `native/doors*`, neue Raumservices)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| G01 | MPAC-Parameterlayouts/-größen, verkettete Viewvalidierung, FadeProc, ViewProc1..7, DoorProc1..7 und deren Timing/SFX eigenständig portieren. | Vorhandenes `DoorState` behalten, Aufrufvertrag mit P00 abstimmen. `door_core.c` ist stillgelegt, nicht fertig. Original/Disassembly prüfen; Abschluss 0x40 erst am belegten Sequenzende. Bestehende Tür-/Inventarsortierung separat auditieren. |
| G02 | Raumlade-/Eventstart-/Playerinit-/Enemyinit-/SFX-Reihenfolge am produktiven RoomWorld-Einstieg belegen und integrieren. | C01/F01/E04; Integrator besitzt gemeinsame Dateien. |
| G03 | Kamera-/Clip-/Licht-Work an bestehende native Datenquelle binden, originale Übergänge schließen. | P04/G02; keine globale Kamera-Spiegelwelt. |

## Phase 2 — Integration und gemeinsame Abnahme

1. Verwaiste Generatoranforderungen bereits in R00 bereinigen. Verbleibende
   aktive Altpfade nach Integration ihres vollständigen Ersatzes aus dem
   Build entfernen und recoverable quarantinieren. Nur überprüfte Daten-/
   Layoutgeneratoren bleiben. Vorhandene API-Namen
   können für kompatible Aufrufer bleiben (nicht die verbotenen Original-
   Dispatcher-/Kern-APIs); die Implementierung muss trotzdem
   eigenständig sein, keinen Originalkern erreichen und keine Work duplizieren.
2. Schutzprüfer: statische Quellen/CMake, frische Kompilierdatenbank,
   transitive Linkeingaben und Symbolprüfung jedes ausgelieferten Binaries.
   Keine alten Buildartefakte oder archivierten Engines für fehlende Symbole.
   Zusätzlich R01-Wirkungsabnahme: Keine Spielfunktion gilt als portiert,
   weil ein Include verschwunden ist oder ein Task sofort endet. Keinen
   grünen Raum-Sweep über ausgelassene Kampf-/Terminal-/Tür-/Effektabläufe
   als Originalverhalten ausgeben.
   R03 unterscheidet dabei ausdrücklich grüne Lückenverträge von
   wiederhergestellten Wirkungsregressionen. Kombinationen, EKG, Mengen-
   anzeige und Befehlsfenster müssen positive Originalwirkung zeigen;
   unveränderte Slots oder leere Ausgabe genügen nicht.
3. Alle Komponententests, danach produktiv RM_0130 über Originalraumladen:
   Spawnzeit, vollständiges sichtbares Modell, fortschreitende Originalpose,
   KI und SFX gemeinsam. Anschließend Attack/Grab/Schaden/Tod prüfen.
4. Rodrigo-/Event-/Lip-/Kampf-/Türregressionen und alle Räume/IDs gesammelt
   prüfen; globale Ursachen beheben. Ein Raum ohne Prozessfehler beweist
   weder vollständige Gegneraktionen noch Durchspielbarkeit.
5. Windows x64 zuerst vollständig linken, danach tatsächlichen Runtime-Pfad
   und Spielabläufe prüfen. Auditierten Ninja-Build mit vollständiger
   `compile_commands.json` und COFF-fähigem `llvm-nm` verwenden; fehlende
   Prüfevidenz ist kein Grund für eine Ausnahme. macOS-Paket und Start.command erst mit geprüftem
   Binary aktualisieren; keine Veröffentlichung solange Schutzprüfung rot.
   Quarantinierte Starter nicht automatisch restaurieren: neues geprüftes
   Paket erstellen, Daten und lokale Einstellungen erhalten.

## Kopierbarer Teilauftrag

> Übernimm ausschließlich Aufgabe `<ID>` aus dieser Roadmap und ihren dort
> angegebenen Schreibbereich. Ziel: eigenständig natives macOS-arm64/Windows-
> x64-Game mit originalem Verhalten, kein Originalkern, keine PS2-Nachbildung.
> Lies AGENTS.md und führe den Schutzprüfer aus. Quarantäne nicht lesen oder
> verwenden. Leite Mechanismus, Reihenfolge, Flags, Tabellen und Seiteneffekte
> aus Originalquellen/Originaldaten/Disassembly her und implementiere sie
> eigenständig auf den vorhandenen nativen Besitzern. Keine Schatten-Works,
> kopierten/generierten Funktionskörper, Heuristiken oder Sonderfall-Fixes.
> Bei fehlender Schnittstelle dem Integrator den konkreten Vertrag melden,
> nicht selbst eine zweite Welt bauen. Implementiere die vollständigen Fälle
> des Teilauftrags, danach gebündelte Tests. Liefere Quellenbelege, Tests,
> offene Bindungen und einen kleinen Integrations-Patch. Kein Commit/Push,
> kein Eingriff in fremde Änderungen, keine Runtime-Abnahme behaupten, wenn
> nur der isolierte Komponentenpfad getestet wurde.

Bei allen Aufgaben, die oben als **stillgelegt** markiert sind, lautet der
Auftrag nun: den fehlenden Mechanismus implementieren, nicht einen noch
laufenden Generatorwrapper ersetzen. Alte `.inc`-Namen in den Detailaufgaben
bezeichnen nur die historische Herkunft. Sie sind keine Lese-/Importfreigabe
und kein Nachweis, dass dieser Verbraucher noch eingebunden ist. Kein
Task darf eine neutrale Rückgabe als Erledigungskriterium übernehmen.
