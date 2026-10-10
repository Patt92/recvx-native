# G01 Türsequenzen — nativ portiert

Stand 2026-10-09. **Implementiert und geprüft.** Der bisherige sofortige
Abschluss (`status |= 0x40` im ersten Durchlauf) ist ersetzt: alle 105
originalen Türpakete fahren jetzt ihre eigene Sequenz.

## Quellenbelege (Semantikreferenz, kein Körper übernommen)

| Gegenstand | Fundstelle |
|---|---|
| Passtreiber, zwei Soundgruppen, Zeichnen | `src/ps2/veronica/prog/door.c` 386–568 (`bhControlDoor`, `ControlSoundTiming`, `SetSoundTiming`) |
| Blende | ebd. 569–652 (`FadeProc1`) |
| Kamera 1–7 | ebd. 653–1126 (`ViewProc1`…`ViewProc7`) |
| Türflügel 1–7 | ebd. 1127–1875 (`DoorProc1`…`DoorProc7`) |
| Vergleiche, VectorMove | ebd. 1929–2025 (`CompareSint32`, `CompareFloat`, `VectorMove`) |
| Prozedurtabellen | ebd. 19–39 (`FadeProcTbl`, `ViewProcTbl`, `DoorProcTbl`) |
| Statusbits der Anforderung | ebd. 184–348 (`bhSetDoor`), 375–384 (`AnalyzeDoor`) |
| Paketlayout | `include/ps2/veronica/prog/door.h` 12–290 (die elf WORK-Layouts), 397–417 (`PT_WORK`, `PH_WORK`) |
| Takt des Demos | `src/ps2/veronica/prog/system.c` 939–1060 (`bhSysCallDoordemo`, Modus 5) |

Aus diesen Fundstellen sind **Datenlayouts und Semantik** übernommen, keine
Funktionskörper. `native/door_core.c` ist neu geschrieben; es ist kein
Generator beteiligt und die Quarantäne wurde nicht gelesen.

## MPAC-Parameterlayout

Der vierte MPAC-Abschnitt ist `PH_WORK` („PMB" + `prm_num`) gefolgt von
`prm_num` × `PT_WORK` zu je 20 Byte: **fünf** Prozedurplätze (Blende, Kamera,
Tür, Licht, Rüttler) als je ein `short`-Paar aus Nummer und Offset, und die
Offsets zählen ab dem Blockanfang. Der Host las bisher nur die ersten drei
Paare; jetzt werden alle fünf gelesen, die ersten drei gebunden und die
Nummern der beiden übrigen im Zustand festgehalten.

Blockgrößen (`recvx_door_parameter_size`, im Test gegen diese Zahlen geprüft):

| Familie | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| Blende | 0x34 | — | — | — | — | — | — |
| Kamera | 0x50 | 0x5C | 0x94 | 0x38 | 0x5C | 0x58 | 0x50 |
| Tür | 0x50 | 0x1C | 0x40 | 0x30 | 0x34 | 0 | 0x50 |

Türtyp 6 hebt nur ein `evalflag` und trägt deshalb keinen Block; Typ 0 hat gar
keine Prozedur.

## Verkettete Viewvalidierung

Kameratypen 4, 5 und 6 enden auf `chg_vew` und übergeben an
`ViewProcTbl[chg_vew]`, dessen Parameterblock **unmittelbar hinter** dem
eigenen liegt. `recvx_door_validate_view` läuft diese Kette ab: jeder
Folgeblock muss vollständig in den Parameterbereich passen und eine definierte
Prozedur nennen; `chg_vew` 0 oder > 7 scheitert, und eine Kette, die sich im
Kreis dreht, scheitert an der Tiefenschranke statt endlos zu laufen.

## Portierte Verfahren

**FadeProc1** mit seinen sechs Abschnitten: Wartezeit, erste Farbe/Rate, Lauf
bis zum Vergleichsziel, zweite Wartezeit, zweite Farbe, und am Ende
`status 0x20` (gibt die zweite Soundgruppe frei) plus `wait2` bis `status 0x40`
— **das ist das tatsächliche Sequenzende**, nicht mehr der sofortige Abschluss.

**ViewProc1–7**: Startposition je nach Druck-/Zugseite (`status 0x4000`),
Spiegelung über `status 0x8000`, `VectorMove` entlang Gier/Neigung, die
Zielverfolgung von ViewProc2 über `atan2`, die Hangschleife von ViewProc3 mit
`slope_ax`, die Übergaben von ViewProc4/5/6 an die nächste Kamera und die
Teilinitialisierung von ViewProc7, die danach in ViewProc1 weiterläuft.

**DoorProc1–7**: Knauf und Flügel mit ihren Vorzeichen aus `status 0x800`/
`0x2000`, die Spiegelung des zweiten Flügels bei `status 0x1000`, das
Rolltor von DoorProc2 (sieben Lamellen zu je sieben Knoten, Modellübernahme
vom ersten Segment, Winkel aus `asin`), der Schiebeflügel von DoorProc3 samt
`type 1`-Mitnahme, der Deckel von DoorProc4 mit seinem `njCos`-Anstoß, die
Klappe von DoorProc5, das reine Ausblenden von DoorProc6 und die
Zweiachsentür von DoorProc7.

**Soundzeitpunkte**: die unmittelbare Gruppe (`status 0x10000`…`0x80000`) und
die zurückgehaltene Gruppe (`0x100000`…`0x800000`), die erst nach
`FadeProc1`-Abschnitt 4 bedient wird, dazu die Klangbefehle aus `SetFlag`
(`0x1000000`, `0x2000000`, `0x4000000`, `0x10`, `0x20`).

**LightProc1/2 und DokiProcLgt** (`door.c` 1876–1928, 2027–2137): Der
Lichtblock wird über `LightSubAmb`/`LightSubPnt` gesetzt; `bhInitDoor` räumt
das Werk und legt Punktlichtrate `{1,1,1}` und Umgebungsversatz `{0,0,0}` an,
`DokiProcLgt` bewegt sie danach. Die Blöcke liegen im Paket **kurz** (Kopf
0x10 + `lgt_num` × 0x24 `LGT_WRK`), nicht in den deklarierten 0x7C/0xF8 —
`recvx_door_light_extent` misst sie deshalb, statt sie anzunehmen; LightProc2
schreitet für seinen zweiten Block trotzdem einen ganzen 0x7C weit. Alle 105
Pakete wurden gegen diese Messung geprüft (`over-running blocks: none`).
Gezeichnet wird über `njCnkSetEasyMultiAmbient` plus die Punktlichter der
Plätze 2–4, ausgewertet wie das Ninja-Chunklicht: je Vertex über eigene Normale
und Position mit der linearen Nah-/Fernblende.

**PuruProc1 und DkiPru** (`door.c`, Rüttler): nach `wait_time` hebt der
Durchlauf `CallSystemSe(0, PruSndTbl[snd_no])` und `StartVibrationEx(2,
pru_no)`; der neu bewaffnende Durchlauf hebt das Muster zweimal. `dmo_atr 0x40`
ersetzt das Paketrütteln durch das eingebaute `DkiPru` (Muster 13, Klang 10,
alle 21 Durchläufe ab dem ersten, 768 Durchläufe). `pdVibMxStart` hat keinen
nativen Treiber; die Anforderung wird gemeldet, nicht nachgebaut.

**`bhSetDoor`/njClipZ** (`door.c` 184–348, `dmo_clip`): ein einziger Fall —
Türtyp 1 aus der MRY-Partition, deren Fall `ViewProc4` fährt, zieht den
Demo-Clip auf near 10 / far 34; das Spiel selbst bleibt bei near 2 / far 20000.

## Gemessene Originalwerte

Türtyp 0 fährt 165 Durchläufe. Seine Zeitpunkte (im Komponententest
festgeschrieben):

| Durchlauf | Ereignis |
|---|---|
| 1 | Klangbefehl 8 (Startflagge `0x10`) |
| 41 | Türgeräusch 0 (Öffnen) |
| 72 | Klangbefehl 5 (Flügel läuft) |
| 76 | Türgeräusch 2 |
| 149 | Klangbefehl 6 (Flügel steht) |
| 164 | Türgeräusch 1 (Schließen) und Klangbefehl 9 |
| 165 | Abschluss |

Das deckt sich mit dem früher protokollierten Stand der generierten Fassung
(„Schließ-SE 1 bei Türtyp 0/5 auf Tick 163 vor Ende 164", dort ab 0 gezählt,
hier ab 1).

Türtyp 7 (MRY/0078, Zellentür, Bank 7) fährt 165 Durchläufe. Sein Blendenblock
liest sich als `wait0 0`, `rate0 1.0`, `speed0 −0.03`, Ziel 0, dann `wait1 96`,
dann `rate1 0`, `speed1 +0.03`, Ziel 1: **34 Durchläufe ausblenden, 96 warten,
34 einblenden**. Im laufenden Abzug gemessen: Durchlauf 9/19/24/29
Rate 0.73/0.43/0.28/0.13 → 0, ab 35 Durchlaufen hält die Blende (`status 0x100`
weg, `0x20` noch nicht gesetzt), ab 140 läuft sie wieder ein (0.27 → 0.87),
bei 159 ist `0x20` gesetzt, bei 165 Ende. Genau diese Haltephase ist der Grund,
warum der Laufzeitabzug die Blende in **zwei** Läufen prüft: einen kurzen im
ersten Rampenabschnitt und einen langen in der Haltephase.

Über alle 105 Pakete: **105 wählen eine Lichtprozedur, 18 zusätzlich einen
Rüttler** (gemessen im Komponententest, der jedes Paket einzeln fährt).

## Tatsächliche Tests

**`door_transition` (CTest, mit Originaldaten).** Spielt **alle 105
originalen Paket-/Fallkombinationen** bis zu ihrem eigenen Ende (149…815
Durchläufe). Je Lauf geprüft: Abschlussstatus gesetzt, Blende und Türmodell am
Ende wieder aus, Blende lief, zurückgehaltene Gruppe freigegeben, Klangbefehl 8
zuerst und 9 zuletzt, Kamera bewegt, und — je nach Türtyp — entweder
Knotenbewegung oder das gehobene `evalflag` von Typ 6. Dazu die Blockgrößen,
die chg_vew-Kette (vollständig, zu kurz, 0, > 7, Ringschluss), die
Prozedurbindung, Türtyp 0 gegen seine eigenen Zeitpunkte oben, die
Druck-/Zugseite und die Spiegelattribute, der Abbruch und ein gezeichnetes
Bild.

**`tools/verify_doors.py`** fährt echte Abläufe im laufenden Spiel. Der
DoorCall kommt aus einem eingespielten Originalbefehl (Opcode 0x33) in einer
Kopie von RM_0020; Raum-, Spieler-, Audio- und Texturdaten sind die originalen.

| Ablauf | Messung |
|---|---|
| Blendenrampe (Bild 20) | aktiv, 19 Durchläufe, `status 0x100` gesetzt, Rate 0.43 **im offenen Rampenintervall**, `0x20` noch nicht gesetzt — die zurückgehaltene Soundgruppe ist also noch gesperrt |
| Demo läuft (Bild 60) | aktiv, 59 Durchläufe, Raum noch RM_0020, Blende aus, `0x200` (Türmodell) noch gesetzt, Lichtprozedur 1 an mit eingeschaltetem Punktlicht, Rate `{1,1,1}`, keine Rüttleranforderung, Clip `{2, 20000}`, Knoten bewegt, nur das Öffnungsgeräusch — das Schließgeräusch ist noch **nicht** da |
| Demo zu Ende | 165 Durchläufe, Raum RM_0010, Geräusche `…/0000` dann `…/0001` aus derselben Paketbank, Steuerung frei (kein Cinematic/Skript/verdeckter Spieler) |
| Herzschlaglicht und ‑rüttler (`attribute 0x60`) | an derselben Anforderung: `0x40000000`/`0x20000000` gesetzt, Punktlichtrate pulsiert um die Paketrate, Umgebung steigt auf den 0,4-Anschlag, Punktlichtfarbe weicht ab, Rüttler 1, Klangzahl `1 + (Durchläufe−1)/21`, Muster = 2 × Klänge, `COMMON/0010` gespielt, `StartVibrationEx(2, 13)` als fehlend gemeldet, Ziel und **Laufzeit identisch** mit dem Lauf ohne Attribut |
| Abbruch mit OK | 39 Durchläufe, als Abbruch gemeldet, **gleiches Ziel** RM_0010 |
| Direktübergang (Typ 255) | 1 Durchlauf, kein Demo, keine Türgeräusche |
| Gehen im Gang | Originalschritte, kein Übergang ausgelöst |

Damit prüft `verify_doors` Kamera/Blende/Bewegung/Licht/Rüttler indirekt über
den Sequenzverlauf und direkt über Dauer, Geräuschreihenfolge, Raumwechsel,
Abbruch und Steuerungsfreigabe — bloßes Raumladen genügt nicht mehr. Der
Herzschlaglauf ist gleichzeitig die Probe, dass die beiden Zusatzprozeduren nur
den Attributpfad ändern und die Sequenzlänge unangetastet lassen.

## Offene Bindungen

* **Rüttler-Wirkung**: `PuruProc1`/`DkiPru` laufen vollständig — Klangzeitpunkt,
  Muster, Intervall —, aber `pdVibMxStart` hat keinen nativen Treiber. Die
  Anforderung steht im Zustand unter `audio_unavailable`
  (`StartVibrationEx(<attribut>, <muster>)`) und wird **nicht** nachgebaut; ohne
  angeschlossenes Gerät ist die Wirkung ohnehin nicht messbar. Ein Treiber für
  SDL-Haptik wäre eine eigene Aufgabe.
* **Rüttler-Hüllkurve**: Das Original hält die Stärke über `pru_wait`/`loop_time`
  als Muster; ein Amplitudenverlauf ist im Dekompilat nicht belegt und wird
  deshalb auch nicht erfunden.
* **Der Abbruch mit OK** ist weiterhin eine **Host-Zugabe**: `bhSysCallDoordemo`
  kennt keinen Skip, das Demo läuft dort bis `bhControlDoor` 0 meldet. Er darf
  nicht als Originalverhalten ausgegeben werden; der Bericht und `verify_doors`
  nennen ihn getrennt.
* **Türtyp 54** (MRY/0125) ist kein MPAC-Paket (256 Nullbytes); 20 weitere
  Typnummern haben in den ausgepackten Daten keine Datei. Beides ist
  Datenumfang, keine Codelücke — die Tests überspringen sie sichtbar.
* **Nicht Teil dieses Pakets**: Die Türen hängen an keiner Stufen-/Treppenaktion
  (B01) und treiben keine Gegner- oder Effektlogik. Was ein Türpaket auslöst,
  ist hier gebunden; was ein Etagenwechsel darüber hinaus braucht, gehört B01.

## Getrennte Befunde macOS / Windows

* **macOS arm64**: gebaut und gelaufen. **49/49 CTest** in
  `build/doors-claude-20261009`, darunter `door_transition` mit den 105 Paketen
  und `native_lighter_effect`. `verify_doors` grün, ebenso events, progression,
  gameover, scanner, computer, item_catalog, file_pickup, file_folder,
  snapshot, frontend, debug, audio_state. Prüfer PASS (`check_no_original_core`).
* **Windows x64**: Die dort liegenden Binaries
  (`build/windows-x64/recvx-runtime.exe`, 2 255 360 Byte; `recvx-door-tests.exe`)
  stammen vom 2026-10-09 23:11 und sind damit **älter als die hier
  abgeschlossenen Bindungen** für Licht, Rüttler und Clip — sie enthalten das
  MPAC-Zweiphasen-Demo, aber nicht die zuletzt geschlossenen Prozeduren. In
  diesem Durchgang wurde **weder neu gebaut noch gebaut und ausgeführt**; ein
  Windows-Lauf hat weiterhin nie stattgefunden, und die Auditimplementierung
  braucht eine unabhängige Prüfung.

## Noch rot, ausserhalb dieses Pakets

`verify_detector`, `verify_pickups`, `verify_ammunition`, `verify_continuity`
und `verify_combat` bleiben offene Projektaufgaben (Effekte,
Inventaraufnahme, Treppen, Kampf). `verify_doors` und `verify_gameover` sind
aus dieser Liste herausgefallen. Die beiden Kollisionstests, die in einem
frischen Buildverzeichnis scheitern (`wall_collision`,
`effect_collision_debris`), bleiben unberührt und unverifiziert — Kollision
bearbeitet Codex.

## Nicht angefasst

`world.*`, `events.*`, CMake und die gemeinsamen Dokumente sind unverändert.
Geändert wurden `native/door_core.c/.h`, `native/doors.cpp/.h`,
`tests/native/door_test.cpp`, `tools/verify_doors.py` und dieser Bericht; in
`native/runtime.cpp` kam nur der abgestimmte Anschluss-Patch dazu (zwei Zähler
für die Rüttleranforderungen, der `door`-Block im `--state`-Auszug). Die
Effektdateien sind unberührt. Kein Paketupdate.

Gebaut wurde ausschließlich in `build/doors-claude-20261009` (eigenes
Verzeichnis, SDL2 offline über
`-DFETCHCONTENT_SOURCE_DIR_SDL2=$PWD/build/dependencies/SDL2-2.32.10`).

**Ein Rotlauf, der sich als Artefakt erwies:** Im ersten CTest-Lauf fiel
`native_lighter_effect` mit „explicit base-level linear filter/GS alpha
ignored“ durch. Derselbe Test bestand im eigenen Buildverzeichnis des
Parallelagenten (`build/lighter-asan-20261009/recvx-lighter-tests`), und nach
einem vollständigen Neubau meines Verzeichnisses bestand er dort ebenfalls —
die Objekte waren gegenüber einer zwischenzeitlichen Änderung an
`scene.h`/`effect_sprites.*` veraltet. Danach 49/49. Die Testdatei ist zudem zum
Zeitpunkt dieses Berichts noch nicht versioniert und gehört zur Arbeit des
Parallelagenten, nicht zu diesem Paket.
