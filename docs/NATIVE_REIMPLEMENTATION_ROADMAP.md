# Roadmap: eigenständige native Implementierungen, originales Verhalten

Stand: 2026-10-09. Diese Roadmap ist ein Arbeitsplan, kein Fertigstellungsnachweis.

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

Vor Arbeitsbeginn den Schutzprüfer ausführen und Befunde melden. Der normale
Build ist derzeit absichtlich gesperrt, weil produktive Alt-Teilports noch
generierte Funktionen und getrennte Works besitzen. Dies ist **kein** Anlass,
den Prüfer zu lockern. Neue Komponenten können separat mit ausschließlich
nativen Eingaben getestet werden; das ist kein produktiver Spielnachweis.

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

## Phase 0 — Besitz festlegen und direkte Originalquellen entfernen

| ID | Kleiner Auftrag / erlaubter Schreibbereich | Abhängigkeit / Fertigkriterium |
|---|---|---|
| P00 | Integrator: bestehende Player-/WorldEntity-/Inventory-/Effect-/Pose-Besitzer dokumentieren; kleine gemeinsame State-Views und Mutations-API definieren. Keine Logik implementieren. | Startpunkt. Feldmapping, Reihenfolge und Eigentümer für jedes Folgepaket; keine zweiten Works. |
| P01 | Native Itemkonstanten als reines Datenmodul `native/item_data.*`; Verbraucher behalten. `item.c` nur Referenz. | Parallel zu P00. Byte-/Wertvergleich aller Tabellen; Integrator entfernt direkte CMake-Quelle. |
| P02 | Disc-/Dateizugriff in vorhandene `asset_io`-/`archive`-Schicht übertragen; eigenes Modul `native/disc_image.*`, nur wenn die Funktion tatsächlich gebraucht wird. | Parallel zu P00/P01. ISO-Index-/Bereichs-/Fehlersemantik prüfen; nicht bloß `ps2_dvd_image.c` verschieben/umbenennen. |
| P03 | Native Geometrieprimitiven für folgende Pakete: zuerst benötigte Box-/Kapsel-/Linien-/Dreiecksoperationen in `native/geometry_*.*`. | P00. Originalfälle und Randfälle, keine neu erfundene Kollision. `ps2_NaColi.c` wird nicht kompiliert. |
| P04 | Kamera-Clipkommandos aus `view_clip.inc` in vorhandene native Clipwerte überführen; kleines Modul `native/view_clip.cpp`. | Parallel zu P01/P02. Opcode-Semantik, Grenzen und Reihenfolge; Generator entfällt. |

## Phase 1 — parallele Paketimplementierungen

Pro Paket genau ein Agent; kleinere Teilaufgaben in der angegebenen Reihenfolge.
Pakete dürfen nach P00 parallel laufen, soweit ihre Abhängigkeiten erfüllt sind.

### A — Kampf (Owner: `native/combat*`, `native/targeting*`)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| A01 | Zielsuche/-wechsel aus `targeting.inc` auf vorhandene WorldEntity-/Player-Views übertragen. | P00/P03; Gate-/Distanz-/Winkel-/Dual-Target-Tabellen exakt. |
| A02 | Ready/Release/Reload/Bullet-Zähler und Inventarübergänge eigenständig portieren. | A01/P01; Flag-/Munitions-/Frameverlauf ohne separate COMBAT_SYS/BH_PWORK-Welt. |
| A03 | Pistol/Burst/Dual-State-Machines in die vorhandene CombatController-Statequelle übertragen. | A02; Motion-/Latch-/Posefolgen und Sideeffects belegen. |
| A04 | Knife-/Trefferzuweisung, Joint-/CPCL-Daten und Damage-Meldung an tatsächliche native Enemy-Works übertragen. | A03/P03/C02; keine zweite Ziel-/Schadensliste. |
| A05 | Weitere Originalwaffen in Tabellenreihenfolge: Scope, Shotgun, Armbrust, Granaten, Launcher. Alle implementieren, anschließend gesammelt prüfen. | A04/E01–E03; Licht/SFX/Blut/Explosion nicht wegstubben. |

`player_combat_core.c` und `player_combat.inc` werden danach recoverable in
die Quarantäne verschoben; keine Funktion wird nur kopiert oder umbenannt.

### B — Player-/Objektbewegung (Owner: `native/player_*`, `native/object_itmbox.cpp`)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| B01 | Treppen-/Kletter-Entry, Motion, Release und Flags auf bestehende native Player-Statequelle übertragen. | P00/P03; `player_stairs.inc` und statische Ersatz-Player-Work entfernen. |
| B02 | Pushkontakt-/Objektinitialisierung und mobile Raumkollision direkt auf WorldEntity-Objekte übertragen. | P00/P03; Reihenfolge und originale Tabellen; keine PUSH_SYS/PUSH_ROOM-Kopie. |
| B03 | Push-State-Machine, Motionwurzel und Move-/Releasepfad übertragen. | B02; `player_push_core.c`/`player_push.inc` ablösen. |
| B04 | Haar-/verknüpfte Objekttransformation auf tatsächliche native Actor-Pose übertragen. | P00/F01; keine eigene Player-/Allocator-Attrappe. |
| B05 | Itembox-Objektablauf/Deckel-/Motionzustände direkt an native Objekt-/Inventar-Statequelle binden. | B02/D01; `object_itmbox.inc` ablösen. |

### C — Gegner und Subplayer (Owner: neue `native/enemy_*`-Module)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| C01 | Originale Raum-/Eventspawn-Daten und Initialisierungsreihenfolge für alle IDs aufnehmen; `enemy_spawn.inc` auf vorhandenen RoomWorld-Spawnweg übertragen. | P00; kein künstliches Spawn-Injizieren. |
| C02 | Gemeinsame native Enemy-State-/EXP-/Child-/Motion-/Damage-Felder ausschließlich am vorhandenen Entity-Besitzer. | P00/F01; Arena/Lebenszyklus nativ, keine BH_PWORK-Spiegel-/Globalswelt. |
| C03 | EN01 Init/Move/Skript-/Damage-/Death-/Childmechanismen eigenständig portieren. | C01/C02/A04; `en01_damage.inc` ersetzen, alle Zweige vor Sammeltests. |
| C04 | EN04 Init/Move/Attack/Damage/Death auf dieselbe native Statequelle übertragen. | C02/C03; kein fester EXP-Ersatz und kein zweiter Dispatchbesitzer. |
| C05 | Subplayer Init/Move/Skript-/Motion-/Releaseabläufe eigenständig portieren. | C02/F01; `subplayer.inc` entfernen, EventVm und Draw lesen dieselbe Work. |
| C06 | EN07 vollständig portieren: originalbelegter Spawn, Init, Animation, KI, Attack/Grab/Damage/Death, SFX und Draw. | C01/C02/E/F/G; verbindliche produktive Abnahme RM_0130 (fünf originale ID-7-Einträge), kein Direktdispatch-Test als Ersatz. |
| C07+ | Restliche fehlende IDs in disjunkten kleinen Gruppen; erst nach gemeinsamen Schnittstellen auf andere Agents verteilen. Für jede Gruppe vollständige Originalfälle, dann Sammeltest. | C02/F/E/G. Gruppierung aus Original-Dispatch-/Abhängigkeitsdaten bestimmen, nicht nach vermuteten Gegnernamen. |

### D — Inventar/Menü/Computer (Owner: `native/inventory*`, `inspection*`, `file_folder*`, `computer*`, `gameover*`)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| D01 | Itembox-Bestand, Auswahl und Transfers eigenständig portieren (`box_engine.inc`). | P00/P01; eine Inventory-Statequelle. |
| D02 | Pickup, ItemUse00 und übrige Benutzungs-/Kombinationsfälle übertragen. | D01; `pickup_engine.inc`, `item_use*.inc` entfernen; originale Mengen-/Flagfälle. |
| D03 | Inspection-/Filefolder-Dialog, Eingabe-, Frame-/Releasezustände übertragen. | D02; `inventory_engine.inc`, `file_folder.inc` ablösen. |
| D04 | Computerzustände, Texteingabe und ursprüngliche Erfolgs-/Abbruchseiten auf native Dialog-/Eventstates. | P00/D03; kein eigener SYS-/ROM-Wrapper; `computer.inc` entfernen. |
| D05 | Gameover-Zustände/Timing/Fade/Release in bestehenden nativen Ablauf übertragen. | P00; `gameover.inc` entfernen. |
| D06 | Event-Inventarkommandos für Integrator vorbereiten; Anschluss ausschließlich an dieselbe Inventory. | D01/D02; `event_inventory.inc` entfernen; keine Event-Schattenbestände. |

### E — Effekte/Sound/Licht (Owner: `native/effect_*`, kleine native Service-Module)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| E01 | Effekt-Wall-/Ground-/Reflexionsmechanismen inklusive echter `bhCheckL2Wall`-Semantik eigenständig übertragen. | P00/P03; `effect_collision.inc` ablösen. Keine vereinfachte Strahlheuristik. |
| E02 | Partikellebenszyklus, Zufall, Bewegung, Farbe/Atlas/Line-/Billboardausgabe direkt im vorhandenen EffectPool. | E01; `effect_particles.inc` ersetzen, keine zweite Effekttabelle. |
| E03 | Sprite-/Custom-/Explosion-/Blood-/Linked-Effectfälle in vorhandene Scene/EffectPool-Statequelle übertragen. | E01/E02/F01; `effect_sprites.inc` ersetzen, keine eigenen sys/plp/rom-Attrappen. |
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

### G — Raum/Tür/Kamera (Owner: `native/door_core*`, `native/doors*`, neue Raumservices)

| ID | Aufgabe | Abhängigkeit / Nachweis |
|---|---|---|
| G01 | Türvergleichs-/Sortier-/Init-/Steuermechanismen aus `door_procedures.inc` eigenständig portieren. | P00; nicht die bestehenden Generator-"Korrekturen" übernehmen, Original/Disassembly prüfen. |
| G02 | Raumlade-/Eventstart-/Playerinit-/Enemyinit-/SFX-Reihenfolge am produktiven RoomWorld-Einstieg belegen und integrieren. | C01/F01/E04; Integrator besitzt gemeinsame Dateien. |
| G03 | Kamera-/Clip-/Licht-Work an bestehende native Datenquelle binden, originale Übergänge schließen. | P04/G02; keine globale Kamera-Spiegelwelt. |

## Phase 2 — Integration und gemeinsame Abnahme

1. Nach Integration aller Ersatzmodule: sämtliche Funktionsgeneratoren und
   Wrapper aus dem aktiven Build entfernen und recoverable quarantinieren.
   Nur überprüfte Daten-/Layoutgeneratoren bleiben. Vorhandene API-Namen
   können für kompatible Aufrufer bleiben (nicht die verbotenen Original-
   Dispatcher-/Kern-APIs); die Implementierung muss trotzdem
   eigenständig sein, keinen Originalkern erreichen und keine Work duplizieren.
2. Schutzprüfer: statische Quellen/CMake, frische Kompilierdatenbank,
   transitive Linkeingaben und Symbolprüfung jedes ausgelieferten Binaries.
   Keine alten Buildartefakte oder archivierten Engines für fehlende Symbole.
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
