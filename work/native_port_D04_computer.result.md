# D04 Computerterminal — nativ portiert

Stand 2026-10-09. **Implementiert und geprüft.** Semantikreferenz
`src/ps2/veronica/prog/sp_evt.c`. Kein Funktionskörper übernommen, kein
Generator beteiligt.

## Was entstanden ist

| Datei | Inhalt |
|---|---|
| `native/computer_tables.c/.h` | Reine Daten: acht Skripte, acht Meldungstabellen, die Zubehör-Quadlisten (`comevt_script_tab`, `comevt_message_tab`, `comevt_acs_tab`) |
| `native/computer_core.c/.h` | Werk, Fensterautomat, Skriptinterpreter, Soft-Keyboard, Textgitter, Zeichenliste, Fade, Boxlinien, Uhr, Balken, Figurenanforderung |
| `native/computer.cpp/.h` | Hostseite: Start/Ende, Pad, Töne, `ev_flg`, Texturen aus SYSTEM/0002, Zeichnen, 3D-Figur |
| `native/world.cpp/.h` | `RoomWorld::computer_character` — der Actor-Besitzer liefert die Figur |
| `tests/native/computer_test.cpp` | Positiver Komponententest (CTest `computer_terminal`) |
| `tools/verify_computer.py` | Vier Laufzeitabläufe im Spiel |

## 1. Datentabellen

Einmalig aus `sp_evt.c` übernommen und als gewöhnliche native Daten
eingecheckt (wie `native/item_tables.c`): 181/141/140/126/41/83/18/40 Skript-
bytes, 27/19/17/23/20/20/20/4 Meldungen, 1/8/9/8/9/9/9/8 Zubehör-Quads. Die
Einträge 4..6 teilen sich Meldungstabelle und Quadliste des 3D-Scanners.

## 2. Werk und Fensterautomat

`com_md0` ruft je Durchlauf genau eine der drei Phasen auf, wie
`bhCtrSpEvtCom_mode0`. Der Abschluss liegt deshalb einen Durchlauf nach
`com_md1 = 30`; in diesem Durchlauf zeichnet der Task nichts — die Runtime
zeichnet den Raum während des Terminals ohnehin nicht (sp_flg 0x30).

Gemessen und im Test festgehalten: Modus 0 fällt in Modus 1 durch und spielt
`0x8000026A`; Modus 1 legt neun Boxlinien an (ct0 0..8, `njSin(ct0*2048)`);
Modus 2 rampt das Hauptfensteralpha in acht Schritten zu je 2048 auf 16384 und
springt nach 10. Dieselbe Form gilt für Schrift- (3/4) und Figurenfenster
(5/6), rückwärts für 18/19 und 21/23/24. Modus 20 verbirgt die Meldung und
wählt je nach offenem Tastaturfenster 21 oder 22; Modus 24 löscht bei
`com_flg 0x8` das Textgitter und startet den Schlussfade (1.0 über 5).

## 3. Interpreter und Tastatur

Ein Opcode je 30-Hz-Durchlauf, Meldungen ein Zeichen je Durchlauf mit
`0x8000026E`. Gebunden sind 224–245, 254 und 0xFF: Text, Wartezeit,
Tastendruck, Farbe (R&0xF8, G&0xFC, B&0xF8 in die Originalbitlagen), Einzel-
zeichen, Worteingabe auf/zu, Textfluss, Wortvergleich mit Bitflagge, Sprung-
marken aus dem Vorlauf, bedingter Sprung, Figur und Motion, `ev_flg`
(Operand little endian), Meldung ein/aus, Flaggenreset, Abbruchsperre,
Zeilenumbruch mit Scroll ab Zeile 11 und Ende. Ein Byte, das kein
Originalopcode definiert, wird gezählt (`unknown_opcode`), nicht still
verschluckt.

Die Tastatur hat die originale Wiederholsperre: die erste Bewegung einer
Richtung wirkt sofort, dann acht Durchläufe Pause, danach jeder Durchlauf.
Spalte 9 trägt die hohe Entertaste; hoch aus 9/3 und runter aus 9/2
überspringen je eine Reihe. Die Seite ist 1234567890 / A–H / I–Q / R–Z mit
ESC, BS und Enter; SE 0x8000026B (Zeichen), C (Löschen), D (Bestätigen),
F (Bewegung).

## 4. Darstellung

Die Zeichenliste trägt die Originalprioritäten: Hintergrund 0.1, Zubehör ab
0.11 in 0.001-Schritten, Uhr und Balken 0.15, Fensterfüllung 0.2, Text 0.21,
Textcursor 0.22, Fensterrahmen 0.25, Figurenfenster 0.26/0.27, Tastatur 0.3
und Tastencursor 0.31; Fade (-0.8) und Boxlinien (-0.85) liegen darüber.
SYSTEM/0002 liefert drei 512×512-Seiten: Hintergrund in zwei Hälften
(`com_flg 0x4` wählt für com_num 1 die untere), Schrift + Hauptfenster +
Tastatur, Zubehör + Ziffern + Balken. Die Textzellen blenden mit +64 Alpha je
Durchlauf ein, der Textcursor pulst mit `curct`, der Tastencursor mit
`curct*2`, und ein Tastendruck zieht ihn für einen Durchlauf um zwei Pixel ein.

## 5. Figurenpfad — bearbeitet, nicht weggelassen

Im Kern liegen Werk und Transform vollständig: Translation (0 / 0.1 / -1.4),
Startwinkel cax 16384 mit dem Schritt aus Befehl 238, Skalierung 0.05,
`njSetScreen(600, 608, 416, 528, 336)` und die Beschneidung auf
(448,256)-(608,416) aus `njUserClipping(2, {14,8}, {18,12})`. Die Figur selbst
kommt vom Actor-Besitzer: `RoomWorld::computer_character(chr_no, mtn_no,
frm_no)` schreibt — wie das Original in die Gegnerarbeit — Motion, Frame und
die Nullpose, baut die Szene und beleuchtet sie mit dem Easy-Light des
Originals (weiss, 1.0 diffus, 0.2 ambient, Richtung 4/6/10). Der Host rendert
sie in ein eigenes 160×160-Bild (dessen Mittelpunkt genau der Originalmittel-
punkt 528/336 ist) und kopiert nur die bedeckten Pixel ins Fenster.

Fehlt dem Raum die Figur oder der Motionbibliothek die Nummer, meldet der Frame
eine Lücke (`character_gaps`); es wird nichts ersetzt.

### 5a. Werk gegen Objektbaum — Nachtrag, die Abweichung ist geschlossen

Die erste Fassung schrieb `mtn_no/frm_no` in die Actor-Arbeit und zeichnete
sofort daraus: die Motion wirkte während der Steuerungspause, im Original aber
erst danach. Das ist jetzt **am gemeinsamen Actor-Besitzer** behoben, nicht am
Terminal.

`WorldEntity` trennt die beiden Dinge, die der Host vorher vermischt hat:

| Feld | Original | gelesen von |
|---|---|---|
| `motion`, `room_motion`, `frame`, `has_motion` | `pp->mtn_no`, `frm_no` — das Werk | der Steuerungsdurchlauf, Eventbedingungen (`bhCmpW`) |
| `pose_mtn`, `pose_room`, `pose_frm`, `pose_motion_valid` | `pp->mlwP->objP` — der Objektbaum | alles, was zeichnet oder eine Pose abfragt |

`commit_pose()` läuft genau einmal je `RoomWorld::tick()`, am Ende der
Motionfortschreibung — genau dort, wo das Original `bhSetMotion`/`bhCalcModel`
ruft. Die Posekonsumenten (`append`, `model_pose`, Gelenkmatrizen in
`enemy_capsule_hit`, Linkmatrizen, die Knotenüberschreibung im Tick, der
Flinch-Knoten) gehen über `posed_motion_clip` / `posed_frame()` /
`posed_has_motion()`. `entity_motion` bleibt das Werkclip und wird weiter für
die Fortschreibung selbst benutzt.

Damit gilt allgemein: **ein Werk, das geschrieben wird, während der
Steuerungsdurchlauf nicht läuft, bewegt den gezeichneten Baum nicht.** Das
Terminal braucht dafür keine Sonderregel — `RoomWorld::tick()` wird vom Host
nicht gerufen, solange der Task den Schirm besitzt, also commitet niemand.

Im normalen Spiel ändert sich dadurch nichts: `events->tick()` läuft vor
`world->tick()`, und gezeichnet wird danach — jede Skriptmotion ist im selben
Bild sichtbar wie vorher. Eine Stelle in `script_motion_test` las eine Pose
ohne Steuerungsdurchlauf zwischen Befehl und Abfrage; sie hat jetzt den
originalen `world.tick()` dazwischen (der `sampled_frame` vor dem Vorschub
nimmt, sodass derselbe Halbbildwert geprüft bleibt).

Positiv geprüft, Komponententest mit Originaldaten (RM_0020, Gegner 0 per
`bhEneSetCk` stillgelegt wie ein Darsteller):

* Fingerabdruck der gezeichneten Szene bleibt über mehrere Terminaldurchläufe
  **identisch**, obwohl Motion 31 ins Werk geschrieben wurde,
* `motion == 31` und `posed_motion() != 31` während der Pause,
* nach `tick()`: `posed_motion() == 31` und der Fingerabdruck ändert sich,
* ein weiterer Terminaldurchlauf friert wieder ein.

Im Spiel (verify_computer, Testskript): während der Pause trägt das Werk
Motion 31 und der Baum weiter Motion 0; nach der Rückkehr folgt der Baum dem
Werk wieder. Dass ein KI-geführter Gegner seine Motion sofort selbst
überschreibt, ist das Verhalten des Originals (`bhControlEnemy` ruft
`bhSetMotion` mit seinem eigenen `mtn_no`); deshalb zeigt der Komponententest
den wirksamen Fall am stillgelegten Darsteller.

## 6. Zwei Grenzen des Originals, nicht des Ports

`bhDispMessage` indexiert `rom->mesp` ohne Schranke. RM_0090 hat für die
Indizes 5..14 leere Meldungen (Offsets 960, 962, 964 … liegen zwei Bytes
auseinander, also nur der Terminator): der Scanner zeigt dort keine
Bildunterschrift. Ein Index ausserhalb der Raumtabelle wird gezählt
(`message_gaps`), statt die Runtime abzubrechen.

`ce->word` hat zwei Plätze. Ein drittes Wort würde im Original über das Array
hinausschreiben; der Port begrenzt den Schreibzugriff. Keines der acht Skripte
fordert ein drittes Wort an.

## 7. Prüfung

**`computer_terminal` (CTest).** Fährt alle acht Originalskripte bis 0xFF.
Geprüft: Passzahlen des Automaten (9 Boxlinien, 8 Alphaschritte), der
gedruckte Text von Skript 4 samt Befehl 228 und Meldung 5/7, die gesperrte
Abbruchtaste (245) und ihre Freigabe (244), ev_flg 0x3d bei „1971" und 0x3e bei
„1972", die Wiederholung nach abgelehntem Code mit anschliessendem Abbruch,
Tastaturseite (acht Eckzeichen), Wiederholsperre, Spaltenumbruch, die hohe
Entertaste, Backspace, wordmax, ESC, der Scroll ab Zeile 11, beide Wörter des
Testskripts mit `lcflg == 3`, die Figurenanforderung (chr_no 0, Motion 31,
Transform, Schirm und Beschneidung) und die Austrittsflaggen samt
`bhStandPlayerMotion`.

**`tools/verify_computer.py`** (vier Abläufe im laufenden Spiel):

| Ablauf | Ergebnis |
|---|---|
| Passwort „1971" | `ev_flg 0x3d` gesetzt, 0x3e nicht; Terminal beendet, Steuerung frei |
| Passwort „1972" | `ev_flg 0x3e` gesetzt, 0x3d nicht |
| Security-Terminal | abgelehnter Code → Fehlermeldung → Skript fragt erneut → 244 gibt Abbruch frei → Abbruch ohne `ev_flg 4` |
| Testskript (RM_0020) | „JHON"/„ASTOR" über die Tastatur, `lcflg 3`, „Ok!", 3D-Figur im Fenster gezeichnet (`character_gaps 0`), Ende mit Steuerungsfreigabe |

**`tools/verify_scanner.py`** prüft zusätzlich zum bisherigen Ablauf, dass
Skript 4 wirklich auf dem Schirm steht (erste Zeile, Quadzahl, Meldung 5,
gesperrter Abbruch) und dass der Abschluss gm_flg/ts_flg setzt — RM_0090 vom
Eagle-Plate-Ereignis bis zum Deckelöffnen.

Das Debug-Menü hat einen Eintrag „Computer-Terminal" (com_num 0..7). Das ist
ein Diagnosezugang über die originale `bhEvtComSet`-Anforderung, keine Aussage
darüber, welcher Raum welches Skript fährt.

## 8. Stand

Mac arm64: **44/44 CTest**, Prüfer PASS, 24/24 Policy. Windows x64 neu gebaut,
Audit bestanden; ein tatsächlicher Windows-Lauf fehlt weiterhin und bleibt ein
eigener Nachweis.

**Nicht grün:** `verify_detector`, `verify_pickups`, `verify_gameover`,
`verify_ammunition`, `verify_continuity`, `verify_combat`, `verify_doors`. Das
ist gemessen und nicht vermutet: mit zurückgenommener D04-Arbeit (git stash der
zehn geänderten Dateien, `computer_tables.*` entfernt, neu gebaut) scheitern
dieselben sieben genauso, und in keinem ihrer Zustände lief der Terminal-Task
(`computer: null`). Sie liegen in den getrennten Paketen Effekte,
Inventaraufnahme, Gameover, Kampf, Treppen und Türen. Grün: `verify_events`, `verify_progression`,
`verify_item_catalog`, `verify_file_pickup`, `verify_file_folder`,
`verify_snapshot`, `verify_frontend`, `verify_debug`, `verify_audio_state`,
`verify_scanner`, `verify_computer`.

## Abgrenzung

D05 Gameover folgt getrennt. Kollision, Spielerkampf, Effekte und Treppen
bleiben eigene Pakete und sind hier nicht angefasst.
