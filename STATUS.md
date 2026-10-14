# Arbeitsindex

Dieser Index ersetzt die Chronik in den Agenten-Pflichtdateien. Keine neue
Gameplay-Abnahme: Angaben aus alten Berichten vor Bearbeitung/Auslieferung mit
aktuellem Code, Arbeitsbaum und installierter Runtime abgleichen.

## Aktive Regeln und Dokumentation

- Verbindliche Policy: [AGENTS.md](AGENTS.md); Prüfmethode: [QUALITY.md](QUALITY.md).
- Geprüfte Funktionsbelege: [Ledger](docs/port_ledger.tsv);
  [Abdeckungsbericht](docs/PORT_STATUS.md) ist kein Semantik-/Durchspielnachweis.
- Aktuelles fachliches Portiert/Offen: [Gameplay](docs/NATIVE_GAMEPLAY.md),
  [Player](docs/NATIVE_PLAYER.md), [Audio](docs/AUDIO.md),
  [Assets](docs/NATIVE_ASSETS.md), [Validierung](docs/VALIDATION.md).
- Ergebnisse/Anschlussverträge bleiben an ihren bisherigen `work/`-Pfaden;
  Tests, Originalreferenzen, Ledger und Policy-Prüfer wurden nicht verschoben.

## Arbeitsbaum und Auslieferung

Gemeinsamer Arbeitsbaum mit laufender Fremdarbeit; aktuelles `git status --short`
und den tatsächlich gestarteten Binary-Stand prüfen. Keine pauschale Freigabe
oder Dateibesitzzuordnung aus historischer Übergabe übernehmen.

**CTest `script_motion` grün (2026-10-14, `build/en09-claude-20261014`, 86/86).**
Ursache war Buchführung, nicht Darstellung: `pose_frame()` liefert für native
Gegnerworks jetzt den Frame, den bhSetMotion vor dem Vorschub abgetastet hat
(`sampled_frame`, gesetzt im Dienst `set_motion`); bhMotionPauseSet (0x2b) schreibt
mode3 auch auf den nativen Work und exp1. Testerwartungen, die dem Original
widersprachen, sind mit Quellbeleg korrigiert: Common 23 ist keine Pause (event.c:11971,
nur flg 0x118), bhMotionPauseSet stoppt einen EN01-Work nicht (bhEne01_SetMtn läuft
unabhängig von mode3, en01.c:2528); die Figur wird nach der Drag-Szene per Originalbefehl
0x24 reaktiviert, der Zombie in RM_0070 erst nach bhEne01_Init per Common 34 gesteuert.
Ältere Testbinärs (z. B. `build/macos-arm64`, `box0090`) vor Verwendung neu bauen.

`Start.command` bevorzugt die Entwicklungs-App unter `build/start/macos-arm64`,
danach das v0.5-Paket, zuletzt das Build-Binary. Die Dokumentationstrennung ändert
keine Runtime, kein Paket, keine Konfiguration und keinen Snapshot.
Nicht aus einem alten „im Paket“-Absatz eine aktuelle Auslieferung ableiten.

**Kartenbild:** MAP zeichnet MRY-Geometrie, Marker und Sprites in der eigenen
Build-Runtime; Fade-Rückweg und Wiedereintritt gezielt geprüft. Constant-Material-
Farbpfad und Typ-41-Normalen korrigiert. **Noch keine Starter-/Paketersetzung.**
Belege und verbleibende Grenzen: [D10 Kartenrenderer](work/native_port_D10_map_render.result.md).

**Pausentaste (Host):** Escape beendet die Runtime nie. Im Menü/Dialog bricht es ab,
im freien Spiel öffnet und schließt es die Pause (`system.c:668/691`, dieselbe
Taste schaltet ein und aus); das Gate bleibt `st_flg 0x1C040208`. Quit liegt auf F10,
und eine Taste, die Cancel ist, zählt nicht als Quit (auch nach Neubelegung; alte
`controls.ini` mit `quit = Escape, F10` behalten F10). Belege:
[keyboard_controller_config mit Negativkontrolle](tests/native/input_test.cpp)
und `main_menu_settings_video_skip` in der eigenen Build-Runtime,
[Controls](docs/CONTROLS.md). **Noch keine Starter-/Paketersetzung.**

**ID-Karte RM_1020 (Fix A):** Der Freeze nach der Wegwerf-Frage ist behoben. Der Spielerpass
läuft trotz st_flg 0x4 unter den Originalbedingungen von bhCPM0_action, und bhControlPlayerPad
gibt das Bit frei (player.c:1838). Geprüft in der eigenen Build-Runtime
`build/discard-claude-20261014`. Offen ist B: das Original-Pad-Gating statt Host-Gate.
**Noch keine Starter-/Paketersetzung.**
[Bericht](work/native_port_discard_question.result.md).

**Effektprozeduren (Übersetzer abgelöst, weitere Gruppen):** alle 66 vormals
regex-übersetzten Prozeduren sowie 68 nie übersetzte (effsub0, effsub1, effsub1b,
effsub5 und effsub6, zuletzt die Augenlichter 145/146, die Gaswand 127, der Schneefall
219 mit der Schneebibliothek aus effsub5 und die Nachbilder 135/136) sind eigenständig in
`native/effect_sprite_procedures.cpp` portiert; 18 Prozeduren dieser Dateien und die
Familien aus effsub2/3/4 bleiben offen; `native/effect_procedures.cpp`, `native/effect_core.*` und
`tools/generate_effect_procedures.py` sind entfernt. Dazu sind
`njGetPlaneNormal`/`njGetPlaneNormal2`, `njProjectScreen` und der Bildschirmzustand
(`njSetScreen`/`njSetAspect`/`njSetPerspective`/`njViewScreenMatrix`) in
`native/ninja_matrix.c` portiert und tragen das Rückseitentor `flg 0x20000` des
Effektzeichenpfads. Zufalls- und Versatzfehler des
Generats sind in [QUALITY.md](QUALITY.md) dokumentiert. Belege, Offenliste der nie
übersetzten Prozeduren mit dem Scan aller 205 Raumdateien:
[Ergebnis](work/native_port_effsub0_handport.result.md), Differenzläufe
`build/validation/effproc-handport-20261014/`, Gruppen 7/8
`build/validation/effproc-backface-20261014/`, Gruppe 9 `build/validation/effproc-snow-20261014/`, Gruppe 10 (inkl. `bhSetEffectLink` beim
Raumladen) `build/validation/effproc-afterimage-20261014/`.
Nachbild-Nachprüfung: belegte DifAmb-Lichtrechnung, committed Spieler-/Zusatzmodellaufnahme
und Skin-FM; **88/88** Tests. Diagnose-A/B mit Originalraumgeometrie, keine natürliche
Sichtabnahme. [Belege](work/native_port_effsub0_handport.result.md).
**Start.command-Dev-App durch `./build.sh` aktualisiert; kein Paket ersetzt.**

**Gegner-KI EN21:** Albinoid-Jungtier im nativen WorldEntity-/Dispatchpfad
angebunden, mit Route/Spielmodus, Spielergriff, Audio und Effekten 396/399 auf der
Modelltexturliste. Gemeinsame Routeninitialisierung ohne Zombie-Ressource und
Spieler-Griffseiteneffekte ergänzt. Eigene arm64-Build `build/en21-bind-claude-20261014`.
**Tests auf Nutzerwunsch zurückgestellt; keine Gameplay-Abnahme oder Starter-Ersetzung.**
[Umfang und spätere Abnahme](work/native_port_EN21_world.result.md).

**Bandersnatcher (EN09):** Stillstand behoben — Identität 9 fehlte im Dispatch, die
Zustandsmaschine war nur Gerüst. KI, Motion/Fußfixierung, Arme, Griff, Schaden, Tod
und Rauch portiert; RM_1060 A/B und `tests/native/en09_world_test.cpp` grün.
Eigene Build `build/en09-claude-20261014`. **Noch keine Starter-/Paketersetzung.**
[Bericht](work/native_port_EN09_world.result.md).

**Offene Punkte, Stand 2026-10-14 nachmittags:** ID-Karte Variante B (Original-Padsperre statt
st_flg-0x4-Hostsperre), UFO-Kran bhEff134 + EN56-Dienste + Anzeige, Funken 308, Blutstoss 296/297,
EN16-Folgeeffekte, Regen (Linien je Bild geleert), RM_1100 ENDIF ohne IF erledigt; EN12: Dienste und Feuereffekte 262-265/274-276 gebunden (Alexia kaempft in RM_9321); offen
EN12-MV04 (Original-UB bei stehendem Spieler, Nutzerentscheidung), Effekte 256/257/266/269.

**Gegner-Lückenscan (2026-10-14 abends):** Statt die Ledger-Texte zu übernehmen, meldet
`build/validation/enemy_scan_20261014/scan_enemy_gaps.py` die tatsächlichen `Enemy gaps:` aller
205 Räume (600 Frames, ohne Eingabe). Vorher 51 Räume mit Lücken, nach vier Fixes **38**:
`bhEne01_FlyingCap` `owP[0].flg |= 3` (7 Räume, Dienst war längst gebunden),
`bhCheckWallRefAngle` als `wall_ref_angle` mit beiden Originaltoren (7 Räume),
`eff[sdw].mdlver` (4 Räume) und `bhEne03_Collision` in EN08 (3 Räume, der Dienst war ebenfalls
gebunden). 31/31 Gegner-/Effekt-Tests grün. Die alte Ledger-Zeile zu EN04
(„MV-Zustandsmaschine fehlt") war veraltet: mv00..mv12, ng/dg/dd und der Brain-Dispatch sind da,
RM_1000 meldet keine Lücke. Die verbleibenden Lücken sind **Subsysteme und Gegnerfamilien**, keine
ungebundenen Einzeldienste: `bhEne02` Gulp Worm (6, Struktur bestimmt: 18 Kindworks, mobiler
ATR-Eintrag, EXP0-Layout), `bhEne19` (4), `bhEff108`/`bhEff219` (4), `bhEne03`/`bhEne29` (3),
`bhEff355` (3), EN53-`child_ambient` (3).
[Bericht](work/native_port_enemy_gap_scan.result.md). **Kein Commit/Push, keine Paketersetzung.**

**EN02 Gulp Worm (2026-10-14 nachts):** vollständig portiert (`native/enemies/en02.c`, Dispatch
`jump[2]`, 18 Kindworks auf Slot 31, mobiler ATR, Sandsprites 253/254, Effektstimme 127).
RM_5830 headless: Angriffskette läuft, Spieler-HP 160→0; CTest `enemy_en02_world` grün, Gegner-/
Effekt-Suite 32/32. Scan jetzt **35 Räume** mit Lücken. Belegte Original-UB: `ang` in
`SetSandEffectMain` Fall 8 ist `$s3` des Aufrufers von `bhControlEnemy` (Disassembly), nativ gemeldet.
Offen: Skript-Opcodes 0x98/0xC3 (world.cpp fremd in Arbeit), Sprite 255, RM_3091/7220 skriptgehalten.
[Bericht](work/native_port_EN02_world.result.md). **Kein Commit/Push, keine Paketersetzung.**

**Effekt 355 (2026-10-14 nachts):** `bhEff_Sub355` portiert (Zielfernrohr-Marke des EN01-Kindes
bhEne01Scope; globale Texturgruppe 4 aus SYSTEM/0001.bin, 16-Pass-Alpha). **Korrektur:** die frühere
Meldung `eff[-1]` in RM_5550/5630/8000 war eine Fehldiagnose — `spawn_effect` liefert -1 für
**nicht portierte** Effekte, und der Aufrufer meldet dann die Originalbedingung; der Pool war nie
voll. Beide Meldungen (`bhEff355`, `eff[-1]`) sind mit dem Port verschwunden. Scan jetzt **32 Räume**
mit Lücken. [Bericht](work/native_port_enemy_gap_scan.result.md). **Kein Commit/Push, keine Paketersetzung.**

**Effekt 219 (2026-10-14 nachts):** Wartebedingung auf `camera_ready` entfernt — Fehllücke, denn
`room.c:33 bhInitCamera` nullt bei jedem Raumladen `cam` (camera.c:8) und `game.c:50 bhControlEffect`
läuft vor `:58 bhControlCamera`; im ersten Effektpass ist die Kamera also 0, wie die nativen
Defaults. Betroffen waren 4 Räume (RM_5800/5840/6100/9000); der Zähler „Räume mit Lücken" bleibt
bei 32, weil diese Räume weitere Lücken haben — die **Lücken-Einträge** sanken 58 → 48 seit run5.
**Kein Commit/Push, keine Paketersetzung.**

**EN11/EN16-Bindung (2026-10-14 nachts):** RM_9100 meldete drei EN11-Lücken mit einer Ursache —
`en11_services` war ab dem zweiten Gegnerpass NULL, weil der Anbieter nur in `init_enemy_works`
gebunden und am Ende jedes Passes in `control_enemy_works` wieder gelöst wurde (EN21/EN09/EN05
binden sich dort korrekt je Pass). Beide Blöcke liegen jetzt im Pass-Scope. RM_9100 scan-lückenlos
(3 → 0), Scan **31 Räume**. Test `enemy_en11_world`. **Kein Commit/Push, keine Paketersetzung.**

**Spieler-owP im Effektpass (2026-10-15, Subagent, geprüft):** die Test-Meldung „owP vor
bhCalcModel" war ein Fixture-Fehler (kein `commit_player_joints` wie runtime.cpp:450/493 =
player.c:840/1688); EN11-Fixture korrigiert, Test `player_owp_order` belegt beides. **Offen als
echte Lücke:** der Effektpass liest nativ den Spielerbaum des vorigen Bildes (Runtime committed
nach `world->tick()`), das Original den des selben Bildes (game.c:49 vor :50) — ein Bild Latenz
für 261/270-Treffer, Spielerlinks, Nachbild 136, E12-Feuer. Fix = Spielerpass-Haken in
`RoomWorld::tick` (world.cpp:1777-1778), nicht chirurgisch, offen.
[Bericht](work/native_port_player_owp_effect_pass.result.md). **Kein Commit/Push.**

**EN05/EN13/13B/EN23/bhStandPlayerMotion (2026-10-14 abends):** Hunter/Sweeper (EN05 +
bhEne05s), Alexia II (EN13/13B), Riesenspinne (EN23 + bhEne03s/EN24-Kinder) portiert und
im Dispatch; bhStandPlayerMotion an Kiste/Karte/Status/Computer/Player_controll 52/bhEff134.
Runtime rechnet plp vor Event_init (player.c:840) — schliesst die Erst-Pass-Lücken
`plp->mlwP->owP[n]`. CTest 89/89 grün (`enemy_en05_world`, `enemy_en13_world`,
`enemy_en23_world`). Offen: Effekte 238/256/267, EN14-Lücken, plp->txp[2], keine
Fensterabnahme, Windows nicht gebaut. Nur Entwicklungs-Start.command aktualisiert.
[Bericht](work/native_port_en05_en13_en23_stand.result.md).

**Gegner-Auditbefunde (2026-10-14):** `bhCheckWall` für Gegner, `bhControlMask` im
Gegnerpass, Baum beim Raumaufbau (room.c:438), modellose Works, EN11-Dienste,
Effekte 261/270 mit `bhCollisionCheckL2MDL` (plp mdl[3]) und Eckalpha gebunden; CTest
`enemy_beam_effects`. Zombie-`natural` ist Originalverhalten (RM_0080 pos_no 0 →
Sub_controll 0x8f Typ 8), Verifier korrigiert. Offen: EN56/bhEff134, EN16-Folgeeffekte
(EN05/EN13/EN23 und EN12-Feuereffekte siehe oben).
[Bericht](work/native_port_enemy_audit_fixes.result.md).

**Schreibmaschine / Spielstände (2026-10-14 abends):** Speicher- und Ladebildschirm nach
`ps2_SaveScreen.c`/`ps2_LoadScreen.c` auf dem nativen Besitzer `native/typewriter.*`/`ui.cpp`:
Kartenauswahl, 15 Slots im 5-Zeilen-Fenster (ITEM1/0144), Rückfragen 482/483/459, leerer
Ladeslot 458, Tippanimation im Originaltakt (McSaveFile.c:571–584), Erfolg 485, Fehler 486/461,
Sonderspeicher-Rückfrage 253 bei ev_flg 56. Gegen das Original korrigiert: mcWriteStartSaveFile
**löscht** ev_flg 56 (McSaveFile.c:168; vorher gesetzt und so in den Spielstand geschrieben),
kein erfundener Inventar-Rollback bei Schreibfehler (stattdessen ev_flg 56, :206),
bhPushGameData nach dem Speichern (SaveScreen.c:1408). CTest `typewriter_save_slots`,
94/94 grün. Offen: `typ_flg 0x10` nach dem Laden, Type_Space bei Räumen ohne
RoomNameSwitch-Eintrag (Original liest Index −1), Fensterabnahme der Ansicht.
**Start.command-Dev-App durch `./build.sh` aktualisiert; kein Paket ersetzt.**

## Gezielter Zugang zu bisherigen Befunden

Vollständiger [Abschnittsindex](work/history/2026-10-13-instructions/README.md)
zu den unveränderten früheren Anweisungsdateien. Nicht vollständig laden; bei
Bedarf den betreffenden Abschnitt lesen. Folgende Links sind **historische
Befunde**, keine aktuelle Offen-/Erledigtliste:

| Thema | Historischer Einstieg |
|---|---|
| Tür-Skip / gezielter Türton-Stopp | [CLAUDE vor Trennung:26](work/history/2026-10-13-instructions/CLAUDE.before.md:26); aktive Entscheidung in AGENTS.md |
| SELECT-Skip / Standin-Claire | [CLAUDE vor Trennung:947](work/history/2026-10-13-instructions/CLAUDE.before.md:947) |
| Hauptmenü / Host-Abweichungen | [CLAUDE vor Trennung:1039](work/history/2026-10-13-instructions/CLAUDE.before.md:1039) |
| Friedhof / Skript-Gegner-Motion | [CLAUDE vor Trennung:1094](work/history/2026-10-13-instructions/CLAUDE.before.md:1094) |
| Ereigniskamera / Pause | [CLAUDE vor Trennung:1131](work/history/2026-10-13-instructions/CLAUDE.before.md:1131) |
| Dialog-Padflanke | [CLAUDE vor Trennung:1213](work/history/2026-10-13-instructions/CLAUDE.before.md:1213) |
| Objekt-Cut-Gate / Leuchten | [CLAUDE vor Trennung:1285](work/history/2026-10-13-instructions/CLAUDE.before.md:1285) |
| Körperpass / Treppen / Hundstoß | [CLAUDE vor Trennung:1346](work/history/2026-10-13-instructions/CLAUDE.before.md:1346), [Reihenfolge:1506](work/history/2026-10-13-instructions/CLAUDE.before.md:1506) |
| Renderer-Worker / Nebelkosten | [CLAUDE vor Trennung:1421](work/history/2026-10-13-instructions/CLAUDE.before.md:1421) |
| Karte: Eingabesperre und damaliger Draw-Vertrag | [CLAUDE vor Trennung:1539](work/history/2026-10-13-instructions/CLAUDE.before.md:1539); spätere Kartenrenderer-Arbeit nicht aus dieser alten Offenmeldung zurückbauen |
| Audio / Gegner / Schatten / Inventar | [AGENTS-Abschnittsindex](work/history/2026-10-13-instructions/README.md); passende `work/*.result.md` gezielt lesen |

## Pflege

Nur aktuelle, auftragsrelevante Kurzstände mit Beleg-/Ergebnislink aktualisieren.
Abgeschlossene Detailprotokolle nach `work/` beziehungsweise `build/validation/`,
nicht in diese Tabelle oder die Pflichtdateien kopieren. Fachseiten bleiben
undatierte Mechanismusbeschreibungen. Historische Kern-/Generatorfreigaben sind
verworfen und heben die aktuelle Originalkern-/Emulationspolicy niemals auf.
