# Gameplay-Portstand

Diese Seite beschreibt je Spielsystem, welche Originalmechanismen portiert
sind und was noch fehlt. Fundstellen beziehen sich auf
`src/ps2/veronica/prog/`. Prozentwerte je Bereich stehen in
[PORT_STATUS.md](PORT_STATUS.md), Prüfwerkzeuge in [VALIDATION.md](VALIDATION.md).

„Portiert“ heißt: Eine eigenständige native Implementierung erhält die
originalen Tabellen, Flags und Aufrufreihenfolgen. Originalkörper oder daraus
generierte Funktionen dürfen nicht gebaut oder gelinkt werden. „Offen“ heißt: Der
Mechanismus fehlt oder ist nur teilweise gebunden. Fehlende Dienste werden
gemeldet, nicht stillschweigend als erledigt behandelt.

Ältere Aufzählungen außerhalb des folgenden Kollisionsabschnitts enthalten
noch Nachweise aus der Zeit vor dem Kernrückbau. Sie sind keine aktuelle
Gameplay-Freigabe; der aktuelle Lückenplan steht in
[NATIVE_REIMPLEMENTATION_ROADMAP.md](NATIVE_REIMPLEMENTATION_ROADMAP.md).

## Spielstart und Ablauf

**Portiert**
- Titelbildschirm aus ADV-Ressourcen; *Neues Spiel* spielt ADV/0003 wie
  `adv.c` TitleCall und startet die Event-VM.
- Der Prolog läuft vollständig über die originalen Raumskripte: Film,
  Zellenszene, Feuerzeughinweis, automatisches Öffnen des Statusbildschirms,
  Ausrüsten, Rodrigo-Szene und Steuerungsfreigabe.
- Cinematic-Skip wie `bhSysCallGame`/`bhSysCall` für alle CineSet-Modi,
  gesperrt durch die originalen Statusbits `0x1C040008`.
- Ereignis-Countdown `bhEvtTimerSet` mit allen drei `bhSetEventTimer`-Modi und
  Ablaufzweigen aus `player.c`; Retry/Game Over über `bhGameOverSet`.
- Disc-Wechsel: Archivsatz 1/2 folgt `sys->ss_flg & 1`.

**Offen**
- Speichern/Laden des Originals (Schreibmaschine, Speicherkarte),
  Schwierigkeitswahl und vollständige Titelübergänge.
- `bhSysCallMonitor` als vollständige `mn_md`-Zustandsmaschine; Raumladen und
  -wechsel laufen nativ in Runtime/World.
- Der Host-Snapshot (F5/F9) ist ein Entwicklungswerkzeug, kein Originalspeichern.

## Events und Skripte

**Portiert**
- Szenario-Interpreter mit Event_init, Tasks, Bedingungen, Schleifen
  (`FOR`/`WHILE`/`NEXT` mit gemeinsamem Stack), Wartebefehlen und
  10-Bit-Flagmaske von `bhCk`/`bhSet`.
- 147 von 223 Event-Befehlen gebunden, darunter Kameras (inklusive
  Eventkamerafahrten per `njOverhauserSpline` aus `cut.c`), Dialoge,
  Türaufrufe, Bodentrigger, Item-Befehle (`bhPlItemLost`, `bhItemGetGet`,
  `bhArmsItemSet` u. a.), Objektverknüpfungen, Modellknotensteuerung,
  `bhMotionPauseSet`, Lichter, Sound- und BGM-Befehle.
- Unbekannte Befehle blockieren ihren Task an der Originaladresse und werden
  gemeldet; bekannte, aber noch ungebundene Befehle werden mit korrekter Länge
  übersprungen und als *pending* protokolliert.
- Alle 205 Räume laden. `tools/verify_rooms.py` startet jeden Raum und
  berichtet blockierte Befehle.

**Offen**
- 76 Event-Befehle: 46 nur mit korrekter Länge konsumiert, 30 ohne Parser.
  Liste: `python3 tools/port_check.py --domain Events`.
- `Common_controll` (event.c) ist nur teilweise gebunden.
- Gesichts-/Lippenmorphs (0x3c–0x3f, 0x90, 0x9a), Kamerapause, Schatten.

## Spieler, Bewegung und Kollision

**Vorhandene native Mechanismen und Teilports**

- Bewegung mit originalen Clips und Geschwindigkeitskurven bei 30 Hz,
  Rückwärtsschritt und 180°-Wende.
- Eigenständige ATR-Bodenabfrage nach `bhGetGroundPosition`: Boxen,
  Zylinder, vorzeichenbehaftete Dreiecke, alle vier Rampenrichtungen und
  Flächen. Raum- und mobile Tabelle werden in Originalreihenfolge geprüft;
  die getroffene ATR-Identität bleibt über Tabelle/Index nachvollziehbar.
  Der vorhandene `RoomWorld::combat_ground`-Pfad verwendet diesen Dienst.
- Eigenständige Wandkontaktabfrage nach `bhCheckWallType`: alle acht
  ATR-Typen, Radius-/Höhenüberlappung und originale Abfrageflags.
  Sie erkennt einen Kontakt, löst aber keine Spielerbewegung auf.
- Eigenständige Wandreflexion nach `bhSetWallRefAngle` für alle acht
  ATR-Typen und Linienabfragen nach `bhCollisionCheckLine/2/3`: originale
  Filter, Seitenreihenfolge, Segmentverkürzung und unnormierte Polygonnormalen.
  Zylinderseiten sind Achtecke, keine analytischen Kreisstrahlen. Der
  abwärts gerichtete Zylinderdeckeltest bleibt ausdrücklich unportiert:
  das Original liest dort uninitialisierte Polygonpunkte. Solche Abfragen
  melden die Lücke, ohne Teilergebnisse an den Aufrufer zu schreiben.
  Die bestehenden World-Liniendienste verwenden dieselbe Raum-/Mobilgeometrie;
  ein echter Raumtest prüft auch Änderungen durch Eventbefehl 0x0a.
- Interaktionsreichweiten und Richtungsmasken aus `bhCheckExmAtari`.
- Eigenständige Spieler-Wandauflösung nach `bhCheckWallEx` im freien
  Bewegungsweg: erst geglätteter Vorfühler, danach Körper, mit Originalradius,
  Höhen, Flags, Vorpositionsregeln und Rundungen. Raum-/Mobil-ATR und
  temporäre Stufenkanten werden in Originalreihenfolge geprüft. Wandänderungen
  durch Events wirken auf denselben Besitzer. `bhCheckFloorP` verwendet den
  sicheren Fühlerpunkt für gerichtete Trigger und setzt Boden-/Wasser-/
  Trittschallfelder; die eigenständige direkte `bhCheckWall`-Abfrage enthält
  auch deren gesonderten Rampen- und Y-Rundungszweig.
- Zufallsfolge des EE-libc (`state * 0x41C64E6D + 0x3039`).

**Offen**

- Die allgemeine `bhControlPlayer`-Work mit allen Zustandsflags und Modi.
- Vollständige Spielerzustandsmaschine, Schiebe-/Treppen-/Leiteraktionen und
  deren mobile Geometrieproduzenten. Die freie Spieler-Wandauflösung ist
  angebunden, nicht sämtliche geskripteten, Kampf- und Gegner-Bewegungspfade.
- Die Semantik des abwärts gerichteten Zylinderdeckeltests und die echte
  `bhCheckL2Wall`-Bindung (anderer Kapsel-/Inset-Vertrag als die Linienabfrage).
  Die weiterhin analytischen Kampfstrahl-/Gegnerbewegungspfade sind damit nicht
  portiert und ersetzen diese Mechanismen nicht.
- Vollständige Spieler-Bodenwork und alle effekt-/gegnerseitigen Verbraucher.
  Eine gültige Bodenabfrage allein erzeugt noch keine Partikel oder Effekte.
- Wandkontakt-Effekt 29, Pferdeschwanz und Feuerzeugflamme mit Raumlicht.
- Vibration und Arm-IK (`bhArmIkMdk`).

## Kampf

**Portiert**
- Messer, Pistole, M93R, Custom Handgun, Magnum, M-100P, Dual-MPs,
  Sturmgewehr und Scharfschützengewehr mit originalen Modellen, Clips,
  Munitionsregeln, Nachladen und Waffenklängen.
- Zielwahl aus `pwksub.c`/`playpch.c` (`bhSearchNearEnemy`,
  `bhSearchNextEnemy`, `bhCalcLockEneYR`, Dual-Waffen-Teilung).
- Mündungsfeuer (`bhEff003`), Hülsen (`bhEff000`) mit Boden-/Wandabprall.

- Granatwerfer (Waffen 14–17: Spreng-, Säure-, Brand-, BOW-Gas-Munition): Abschuss
  (`player.c:4990`), Geschoss `bhEff130` mit Flug, Wand-/Gegnertreffer
  (`bhCheckFlyAtari`), Einschlägen und `bhSetBowDamage`.

**Offen**
- Schrotflinte, Armbrust, Waffen 18/20, Zielfernrohr. Nicht freigegebene Waffen melden
  ihre Lücke beim Zielen namentlich (`Combat gaps`).
- Granatwerfer: Schirmfärbung des BOW-Gases (`sys->ef_ntx2d`), Wassereinschlag
  (`bhCheckL2Water`), Nachladen mit Munitionswechsel ungeprüft.
- Weitere Mündungsvarianten, Rauch, Waffenlichter, Gelenkkapseln für
  Waffen mit `WpnTab.flg & 8`.

## Gegner

**EN21 (Albinoid-Jungtier): implementiert, angebunden und am Originalraum geprüft.**
Der vorhandene native Zustandsautomat läuft im WorldEntity-Dispatch, mit Routen,
Spielmodus, Spielergriff und Audio. Funken/Spuren (399/396) verwenden die vom
Gegner gesetzte Modelltexturliste statt eines gemeinsamen Ersatzatlasses.
Die gemeinsamen Raumrouten werden auch ohne Zombie-Motionbibliothek geladen.
`tests/native/en21_world_test.cpp` prüft in RM_3120/RM_3140 Init (Typ 0/1),
Bewegung, Verfolgung bis zum Griff, Schaden 9, Funken 399, Spuren 396, Tod mit
Blutpfütze und die Verteilung von Sperrzeit/Sterbemotion.
Offen: natürliche Skriptfreigabe in RM_3120 (Test gibt per 0x24 frei), echter
Waffentreffer statt Trefferfeldern, Beobachtung von Stimme/SE/`plp->dpx`.

**Portiert**
- Zombies (`en01.c`): Wahrnehmung, Wegsuche, Verfolgung, Fußfixierung,
  Trefferreaktionen, Niederschlag, Tod, Blut, Gegnerstimmen.
- Greifen/Beißen mit Spielerschaden und Hebelrütteln (`EatCheck`,
  `bhEne01_Nage`, `PlyDG00/01`) laufen im normalen Spiel (Standard an;
  `debug.ini enemy_grab = off`, Debugmenü oder `--enemy-grab off` schalten
  ab). Die Kette ist seit 2026-10-12 geschlossen: Spielersuche
  (`bhSearchPlayer/2`), Bereichs-/Bodeneffektflächen
  (`bhEne_EnemyAtariCheck`, `bhCheckFloorEffect`), Wegzonen
  (`bhCheckRouteID/bhCheckRoute` aus `rom->rutp/ruttp`), Laufmotionswechsel
  (`bhGetFrameNum`, `bhCalcFixOffset`, `bhEne_GetTranslateMtn/2`,
  `bhAddSpeed`), Treppenflächen (`bhKaidanAtrCheck`), Spielerkapsel
  (`plp->watr`, `npCollisionCheckSC`) und der Gegner-Spieler-Kontakt
  (`bhCheckPlayer`, Bedingung `plp->flg & 8` war invertiert). Bisse ziehen
  direkt von der Inventar-HP ab; HP < 0 führt in das native Game Over.
  Nachweis: `tools/verify_enemy_ai.py` (RM_0080: natürlicher Spawn,
  Diagnoseaufstellung, Hebelrütteln, Tod).
- Fässer (`bhEne00`) mit Explosion und Kettenreaktion.
- Hunde: Aktivierung über die originalen Bereichs-/Fallenregeln.

**Offen**
- Alle weiteren Gegnerfamilien (en02 ff.), vollständige Hunde-KI.
- Zombie-Treppen, Kopf-/Gliedmaßenabtrennung, Feuer, Krabbler-Zweige,
  gegenseitiges Wegschieben.
- Auf dem Angriffspfad noch gemeldet: Blutsprite `bhEff005` (Partikel laufen),
  Controller-Vibration (`StartVibrationEx`), `bhSetUseKaidanFlag` für
  `sys->metcp`-Kisten, der `nlxb`-Zweig von `bhCheckPlayer` beim sterbenden
  Spieler. Originaldefekt nachgebildet: `bhEne01_ChgWalkMtn` verschiebt den
  rohen `bhGetFrameNum`-Wert um 16 Bit, der Startframe kann `frm_num`
  erreichen; dieser eine Pass behält die Vorpose und wird gemeldet.

## Inventar und Items

**Portiert**
- Statusbildschirm mit originalen Sprites, Ein-/Ausfahren (`SpriteH`,
  `SpriteUV`, `StatusCancel`), Fensterläden, Befehlsleiste
  EXIT/FILE/MAP/ITEM (`MainCommand`) und Cursorlogik (`CursorMove`).
- Itemtabelle `itemdata[156]` mit Typen, Mengen und Kombinationen; alle
  117 gerichteten Rezepte über `combidata`/`Combi_*`.
- Kontextmenü: Sprites und Sichtbarkeit aus `ItemCommand`/`TrigerSet`.
- Untersuchen mit Startwinkeln aus `dsptbl`, Modellwechseln, Öffnen von
  Koffern und Namensänderungen (`ItemModelCheck`, `MdlAction00–02`).
- Inventarmutationen über `GetItem`, `ItemSearch`, `EraseItem` und
  `ItemSort` inklusive Korrektur des Ausrüstungsindex.
- Zustandsanzeige/EKG (`PulseInit`, `PulseMain` u. a.) mit originaler
  Zeichenpriorität.
- Dateiordner (`SearchTag`, `SelectFile`, `SelectTag`, `FileSyu`) und
  Leseansicht; Dateinummern über `FileNumberSwitch`.
- Item-Box: Öffnen über `bhCheckExmAtari` (Typ 4, Kistenbit) mit
  Deckelanimation `bhObjItmBox`; Tauschen, Stapeln, Restriktionen
  (`ItemBoxChange`), Ein-/Ausfahren und EXIT-Knopf.
  Der Deckelcontroller ist eigenständig nativ: 41 Öffnungsschritte,
  Menüanforderung im folgenden Durchlauf, einmaliger Klang und Rückkehr
  auf demselben Objekt. Pause-/Playerflags und Wiederöffnung sind geprüft;
  allgemeine reservierte/verknüpfte Objektbesitzer bleiben getrennt offen.
- Benutzen über `ItemUse` für alle Itemtypen, Heilen und Entgiften über
  `Use_00` (inklusive Sprayzähler und Gift-Statusbits).
- Aufnahme vom Boden und durch Skripte mit Ja/Nein, voller Tasche und
  Abschluss über `ItemTaskCheck` (löscht GetItem- und GetFile-Anforderung).
- Menüklänge nach `CallSystemSe`: Bestätigen 3, Abbrechen 0, Cursor 2,
  Fehler 1.

**Offen**
- `StatusMain` und `ItemCommand` als vollständige Zustandsmaschinen; Teile
  der Eingabe- und Übergangsphasen sind noch Host-Steuerung.
- `ItemCombination`-Menüablauf und -Animation (die Regeln selbst sind
  original).
- Vollständige analoge Padbindung des Kartenbildschirms (`pad_ax/ay/al/ar`)
  und gemeinsame Systemtask-/Drawreihenfolge außerhalb des geprüften MAP-Pfads.

### Kartenbildschirm

**Portiert:** `MapScreen` besitzt den Automaten aus `map.c`, Flag-/Etagenwahl,
Zoom-/Nachbarzustände und Original-Zeichenliste. `MapRenderer` dekodiert MRY-
Index und Etagenmodelle mit dem vorhandenen MDL/PVP-Pfad und zeichnet Gitter,
Rahmen, Sprites, Marker, Etagenobjekte und Kompass. `pri` und Modellgeometrie
teilen den Tiefenpuffer. Karten-CNKs verwenden den von `njControl3D(0x810)`
gewählten Constant-Material-Farbpfad, nicht den Normalenlichtshader; Typ-41-
Normalen bleiben im gemeinsamen Assetdecoder erhalten. Spritefarbe und
Texturmodulation behalten die originale 128er-Faktordomäne.

Solange die Karte den Schirm besitzt, steht **Task 7** still: der
Spieleinstieg setzt `ts_flg 0x180` (system.c:777), der Statusmenü-Einstieg hat
Task 7 schon gestoppt und `sp_flg 0x30` hinterlassen (system.c:608), sodass
`bhControlPlayer` (player.c:1365, `sp_flg & 0x1`) nicht läuft. Der Host wacht
Welt-, Kamera- und Spielerpass mit derselben Bedingung; eine gehaltene Richtung
bewegt die Figur während der offenen Karte nicht mehr.

`ScreenFade` bildet Setzen, Vorlauf und Abschluss aus `screen.c` ab. Die
MAP-Rückkehr wartet auf das live gelesene `cb_flg & 0x2`, auch wenn der Fade
erst im selben Kontrollpass startet. Statusmenü → Karte → Statusmenü → Spiel
und Wiedereintritt sind im nativen Runtime-Pfad geprüft.

**Offen:** analoge Kartensteuerung, globaler Fade-/Taskbesitzer für sämtliche
anderen Spieltasks, vollständige originale Primitive-/Material-Drawreihenfolge
und Alpha-/Texturfunktion außerhalb dieser nativen Abbildung. Kein pauschaler
1:1-Nachweis des vollständigen Kartenbildschirms.

Die vollständige Item-, Untersuchungs- und Rezeptabdeckung erzeugt
`recvx-inventory-engine-tests` als [INVENTORY-COVERAGE.md](INVENTORY-COVERAGE.md).

## Räume, Kameras und Türen

**Portiert**
- Raumnamen und -identität nach `system.c` (dezimal), Spawns, Kameraschnitte
  mit Etagenvergleich (`bhCheckCut`), Clip-Ebenen (`bhChangeViewClip`).
- Native Clip-Abfragen mit originalen Event-/Raumtabellen, Erstrefferregeln,
  Displayflag-abhängigen Eventstandards und Monitor-Tabellenbypass. GameOver
  behält seinen ausdrücklich installierten Bereich, unabhängig vom Kameraflag.
- Türsequenzen aus MRY-Paketen über eigenständige native `FadeProc1`, `ViewProc1–7`
  und `DoorProc1–7`; Überspringen mit OK; Abschluss der Raumskripte vor
  `bhStartDoorDemo`.
- Inventar, Flags, HP und Spielzeit bleiben über Raumwechsel erhalten.
- 3D-Scanner-Terminal (`sp_evt.c`) mit Skriptinterpreter und Bildschirmtastatur.

**Offen**
- Spezielle Türbeleuchtung, Vibration.
- 3D-Figur im Terminal (`bhDrawSpEvtComCharacter`).
- Eff137 als weiterer Monitor-Erzeuger, vollständige Kamerazustandsübergänge
  und Abnahme der sichtbaren Eisraum-Tiefensortierung; keine PS2-OT-Nachbildung.

## Modelle und Effekte

**Portiert**
- Skinning nach `npCalcSkin`/`npCalcSkinFM`, MASK-Knotenlayout,
  zusammengesetzte Ober-/Unterkörper-Motions der Zombies.
- Effektslots `eff[512]` mit `bhSetEffectTb`; einzelne Effekte: Flammen und
  Funken (Eff200/201/218), Treffer (5), Blutpartikel (`bhEff302`),
  Blutlachen (`bhEff300`), Explosion (16/23/24), Feuerzeug (9), Hülsen (0).

**Offen**
- Der allgemeine Effektlebenszyklus (`bhInitEffect`, `bhControlEffect`,
  Zeichenwarteschlange) und die große Mehrheit der Effekt-IDs.
- `bhControlLight`, exakte EasyLight-Schattierung, Gesichtsmorphs.

## Audio

Siehe [AUDIO.md](AUDIO.md). Kurz: ADX-Musik und -Stimmen, Sony-Soundbanken,
SQ-Sequenzen, Schritt-, Tür-, Waffen-, Gegner- und Ereignisklänge sowie Filme
laufen nativ. Tonhöhe, Hüllkurven, Hall und bitgenaue SPU-Mischung fehlen.
