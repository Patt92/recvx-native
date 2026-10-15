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

**RM_1090 Standuhr (2026-10-15):** Sichtloch stand 22,5° neben 12 Uhr — Common_controll 9 schrieb
für den nativen Uhr-Work in einen ungelesenen Override, jetzt wie 8/18 auf `enemy_obj`
(event.c:12136). Escape bestätigte Skriptauswahlen mit Auswahl 0 („Links“); Cancel bestätigt
keine Auswahl mehr, das Raumskript liest ihn selbst (bhPadCheck, Event 13). Der Reset-Freeze
nach L/R war die Dev-App vom 10.10. ohne 0xb1 (HEAD hat den Port). Raumdaten: Tafeln
1/3/4/5/6/7/8/Emblem, L=+180°, R=−45°; L,R,LR,RRR zeigt 3,1,7,1 — „1974“ ist nicht darstellbar.
Last: nur die Schreibtischkamera (Glasdach, bildfüllendes Alpha) kostet ~2× CPU, kein Fehler.
Nachweis `tools/verify_clock_puzzle.py` (Build und Dev-App grün);
[Bericht](work/rm1090_clock/README.md). `./build.sh` gelaufen. **Kein Commit/Push.**

**Semantik-Audit Flags/Bedingungen (2026-10-15):** behoben in `world.cpp`: bhLoadWork2 Fall 2
(Bytes 3..5, Fall 1 behaelt model_cno), Common 15/16/28-33/44/45 auf `enemy_obj` bzw. work
px/ax, Common 40 frm_no (+exp1), Common 50 Worktyp, Sub 0x8f/0x92 nur ct2-Lowbyte, 0x93 ct2
unberuehrt, 0x43 stflg 0x1000000, 0xd2 sx=sxb. Dialoge: Cancel beantwortet keine Auswahl
(auch GetItem/Inspektion), schliesst nur die fertige letzte Seite; Timerseiten (FF02 n) nehmen
keine Taste, FE08 blaettert nach Ablauf (nur RM_7240); kein Systemklang beim Textschliessen
ausser GetItem-Antwort (SE 3); Inspektionsauswahl ohne Hoch/Runter. `bhDrawEneObject`:
bhCalcModel fuer en_obj (RM_9290 EN25). Widerlegt: IF-Yield, Sleep/Next, Skip-Set,
Hoehenpruefung/break in bhCheckExmAtari, metcp-Typ 3/4, Pad-Reihenfolge, ZXY/BREAK in
bhCalcTree. Danach: Scheduler laeuft nur mit sp_flg 0x10 (event.c:13024); die Raeume setzen
bzw. loeschen es selbst im scd1 (bhSet 6,27), nach einer Tuer setzt `runtime.cpp` wie
bhStartDoorDemo Fall 1 sp_flg -1 (room.c:1006; RM_7050 Event 8 lief sonst nicht weiter).
Event_exec belegt bei vollen Slots 0..14 Slot 15 neu. Leere FFFF-Seiten warten wie im Original
auf OK/Cancel (1507 leere Raumtexte, z. B. RM_0000 Event 32 zur Stimme). Synthetische
Ereignistabellen in verify_doors/gameover/view_clip setzen sp_flg 0x10 im scd1; verify_doors
gruen, gameover/view_clip konnten wegen Auto-Mode-Sperre nicht laufen. Raum-Sweep (205 Raeume,
600 Bilder) vorher/nachher: nur RM_6100/RM_7050 (Tor) bzw. offene leere Texte.
`MessageDialog` ist jetzt bhControlMessage pro Durchlauf: Aufbau ueber mes_ct (Item-Namen
zaehlen wie bhDispItemName, FF04/FE00 sofort, OK gehalten +4), Timer erst nach dem Aufbau,
FE08/FE09 wie message.c. Schliessen mit OK/Cancel nur bei st_flg 0x4000|0x1000 des vorigen
Durchlaufs (game.c:264); Auswahl-/Blaetter-OK und Links/Rechts verarbeitet EventVm::tick vor
dem Skript (Statusmenue: sofort nach seinem Tick); Klang 2/3 aus bhControlMessage/GetItem.
Suite 98/100 unveraendert, Sweep unveraendert. **Ungeprueft (Auto-Mode-Sperre):**
verify_clock_puzzle, verify_doors, verify_gameover, verify_view_clip nach diesem Umbau.
Danach: bhIfelFlg/G_Sp wie event.c (globaler Zeiger +4/-4 statt Stapel; jede 0-Rueckgabe im IF
springt, auch Sleep/EvtNext/EvtEnd/EWHILE-Ende, die nativ 2 lieferten; RM_0090 Ev33 wartete
nativ). Opcode-Zuordnung korrigiert: 0xd1 = bhEffSSizeSet (sx=sxb), 0xd2 = bhEffLinkOffsetSet
(lox/loy/loz; war faelschlich Groesse). Neu gebunden: 0x2f bhInitSetKage (bhSetShadow einmal je
Work, 51 Raeume), 0x7e bhKageSwSet (stflg 0x8; obwp/itwp-Faelle bleiben Luecke). Sweep:
keine neuen Luecken. Item-Glanz portiert (objitm.c:404-450 nach bhControlEffect: Pause
60..89 aus EE-rand, 8 Bilder mdflg 0x80; bhDrawObjItm ersetzt Licht 1 durch
bhGetLightVector(57344,ayp,0) mit 0.1*aspd); Items laden aspd (short), 0x42 bhItmAspdSet
gebunden. 144 Items in 69 Raeumen; RM_1090 waehlt danach ein anderes Zufallsevent (EE-rand
wie im Original verbraucht). Lichtzustand je Zeichengruppe wie bhAllDrawModel/bhDrawObjItm:
Objektlisten ob_nlg/ob_hlg/ob_spc je bhControlObjItm-Durchlauf (sp_flg 0x4, pt_flg 1/2/4,
cb_flg 0x40000000 = nur 4 Objekte); Spieler, Gegner und Objekte id >= 1000 (347 in 70 Raeumen)
mit bhSetHalfLight (Licht 1 und Punktlichter 0.5, amb_chr); ob_spc draw_tp 0 mit rom->lgtp[aspd]
als Licht 1, erster ob_spc in RM_1130/3060/7060/7100/9110 ungezeichnet; en_obj (bhDrawEneObject)
mit amb_rom und vollem Licht. Spielerschatten (player.c:791/905: bhSetShadow(NULL,plp,1,
4.5,4,3.5) je Raum, bhEff001 liest plp flg/stflg/owP). 0x6c bhObjAlphaSet mit
bhControlAlphaFadeObject (Startalpha = plist-Byte 7 des Knotens, dauerhaft je Knoten; RM_7050,
RM_8030). 0x23 war schon portiert (nur Eintraege hinter rom->etcp bleiben gemeldet).
Einzellicht-Pfad (ps2_NinjaCnk.c:252-276): draw_tp 2/3 und mit gm_flg 0x80 jedes Modell
(Game-Over-Kamera, gameover.c:323) mit dem Easy-Zustand der Gruppe (bhSetLight/HalfLight,
bhDrawSpObject lgtp 0.2, bhSetEasyDirLight (0,0.3,0.7) im Kameraraum, Augenkamera 0.6 je
Gegner). Gebunden: 0x88 bhEffUVSet, 0x80 bhInitPonySet (plp->flg2 0x2 setzt den Zopf in
bhObjClpn Fall 1 zurueck), 0x2d/0x30 bhInitMotionPause(Ex), Sub 148 bhEne11_LightControl
(EN11-Dienste ausserhalb des Gegnerpasses), pl_sleep_cnt (RM_9302). RM_9100 Event 0 laeuft jetzt
durch. A02 `native/player_event.c`: pl_evt.c bhCPM0_event/pl_smove00..08 (Test player_event);
Player_controll 131 setzt plp mode0 7/mode1/mode2 (mode3-Regel event.c:9405-9420), die Runtime
fuehrt den Pass ueber den einen Spielerbesitzer (auch waehrend eines Texts), 128/139 geben frei.
Erreicht ohne Eingabe nur RM_7010/RM_3220 (pl_smove01 Fall 0). Spielerschatten-Test und EN02-Test
an den neuen Besitzer angepasst; ausserdem die durch den Dialogumbau roten Tests (erst mit
`./build.sh --test` gebaut) an bhControlMessage angepasst. Suite 99/101 (47/69 fremd).
RM_7010 Event 37 DoorCall-Spawn 1 nach RM_7030 blockiert auch auf HEAD (Vorherstand belegt).
0xce bhSyukanModeSet setzt/loescht die gm_flg-Bits wie event.c:8873 (erreicht RM_5500 mit 1).
Augenkamera in RoomWorld::tick_camera: bhInitPlEyeCamera (gm_flg 0x2000; owP[5], pe_ax/pe_pers aus
Kampfkern bzw. 0xce, ohne hidden_objects-Maske, lgtp[4..] flg 2 aus, rom->fog_* ROM 0x108..0x110,
njClipZ 1.0/1.1), bhSetPlEyeCamera unter gm_flg 0x40 (Kopf +1.5/-1, wax, Zoom st_flg 0x800000 in
364er-Schritten), Rueckweg ueber bhControlPlEyeCamera mode0 1/2 mit CAM_WRK.spd. Sonde RM_0010:
Augenpose und Rueckkehr zur Schnittkamera. Kampfkern liefert plp->wax/mode1/at_flg. Ungeprueft
im Spiel (Zielfernrohr-Replay). Offen dort: cam.flg-0x4-Einstieg aus anderen Kameras (Event/fest).
Gemeldet: mdflg 0x400 npGetMatColor, lgtp[0] Typ 100/101 in bhSetEasyDirLight, 0x91 nur bei
nicht portierter Effektprozedur.
**Kein Commit/Push.**

**EN19 Tyrant T-078 portiert (2026-10-15, Subagent, vom Hauptagenten geprüft):** `jump[19]`, CMake,
Test `enemy_en19_world`; 2353 Z. `en19.c`, Tabellen exakt wie das Original (50/21/17/28/26 …), alle
21 Mv-Routinen. In RM_5840 läuft er (Motion 14/Bild 28, Spieler-HP 160→120); RM_1122/3230/5010 sind
skriptgehalten. Spot-Checks (`head_turn` zeilengleich, `bhCtrPly_mode0[5] == bhCPM0_enedam`) bestanden.
**Offen:** die Restlücken der 4 Räume sind unportierte `effsub3.c`-Effekte (303–312, 349) und hängen am
fehlenden Subsystem **`sys->efm`** (globale Effektmodelle aus `bhInitEffect`, nativ 0 Treffer).
Effekt-*Einträge* im Scan stiegen dadurch 45 → 52 (aus 4× „bhEne19" wurden 10 benannte Effektlücken).
**Kein Commit/Push.**

**Effekt 256/257 Säure + EN14-Säuredienste (2026-10-15):** `bhEff_E03_Acid`/`bhEff_E00_Acid` von Hand
portiert (`effect_sprite_procedures.cpp`); dafür neu am Besitzer: `pxb/pyb/pzb` am `EffectSprite`
(effect.c:794), Haken `player_watr` (plp->watr, player.c:1758), `effect_detail::collision_line`
(bhCollisionCheckLine2 kürzt die Lage auf den Treffer). EN14 `acid_alloc`/`acid_setup` in
`control_enemy_works` gebunden. Test `effect_e03_acid`, 36/36 grün; Scan `run10` **31 Räume / 50
Einträge** (52 → 50: `bhEne14_Acid` aus RM_5810/RM_9302 weg, sonst identisch). `./build.sh` gelaufen
(Start.command-App aktualisiert). Offen: 269 `bhEff_E14_Fire` (Feuerkind, EN14 Typ ≠ 0).
[Bericht](work/native_port_enemy_gap_scan.result.md) Abschnitt 9. **Kein Commit/Push.**

**Effekt 267/269 + EN14-Dienste (2026-10-15):** `bhEff_E13_Fluid` (Erzeuger ist EN30, Gruppe 81
freigeschaltet) und `bhEff_E14_Fire` (Kind von 256 type 6; `fire_light`-Haken um Farbe/Reichweite
verallgemeinert) von Hand portiert; EN14 `joint_flags`/`node_angles`/`motion_angles`/`joint_matrix_set`/
`floor_enemy`/`game_mode`/`find_beam`/`beam_line` (O_WRK +0x10/+0x64 nach `RECVX_EN16_EFF`) gebunden.
Test `effect_e14_fire`, 37/37 grün; RM_9302/RM_5810 zeigen 256/257/267 sichtbar. Scan `run11` **31 Räume /
46 Einträge** (50 → 46; neu sichtbar `bhEff250`/`bhEff301` in RM_9302, weil der Kampf jetzt weiter kommt).
Offen: `bhEff130` (Spielerstrahl, 1120 Z.) — `find_beam` findet nativ nie einen Strahl. `./build.sh`
gelaufen. [Bericht](work/native_port_enemy_gap_scan.result.md) Abschnitt 10. **Kein Commit/Push.**

**Effekt 250 + `model_texp_index` + Morph-Zeichenpfad (2026-10-15):** `bhEff_E00_Mince` portiert; der
Erzeuger brauchte `RECVX_ENEMY_EXT.model_texp_index` (Blut-Texturliste), jetzt gebunden — RM_5810/RM_9302
melden nur noch `bhEff301`. Dabei gefunden: `actor.cpp` warf „morph object has no vertex stream" für
EN14 MV02 (Objekt 1 evalflags 0xE), das `npCalcMorphing` überspringt; Quellzugriff jetzt hinter
`morph_blend`. HEAD rendert dort nur, weil EN14 MV02 vorher nicht erreichte (Worktree-Messung). Scans sehen
nur das Endbild — Endbild-Sweeps nach Gegnerports nötig. Scan `run12`/`run13` **31 Räume / 44 Einträge**,
37/37 Tests, `./build.sh` gelaufen. Letzte EN14-Raumlücke `bhEff301` hängt an der fehlenden 2D-Effektliste
`sys->ef_trs2d` (Subsystem). [Bericht](work/native_port_enemy_gap_scan.result.md) Abschnitt 11.
**Kein Commit/Push.**

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
15 Slots im 5-Zeilen-Fenster (ITEM1/0144), Rückfragen 482/483/459, leerer
Ladeslot 458, Tippanimation im Originaltakt (McSaveFile.c:571–584), Erfolg 485, Fehler 486/461,
Sonderspeicher-Rückfrage 253 bei ev_flg 56. Die Memory-Karten-Auswahl (SelectCard, Port 0/1) ist
als Konsolenverhalten entfernt — der Host hat genau einen Speicherort, der Bildschirm startet
direkt in der Slotliste; Cancel dort führt in den Verlassen-Zustand. Panel-Fundament
mcDispWindowFoundtion korrigiert (Argumente rechts/unten; feste GS-Alpha 96/255 in den Wirtsfarbwert
überführt, vorher unsichtbar gezeichnet). Gegen das Original korrigiert: mcWriteStartSaveFile
**löscht** ev_flg 56 (McSaveFile.c:168; vorher gesetzt und so in den Spielstand geschrieben),
kein erfundener Inventar-Rollback bei Schreibfehler (stattdessen ev_flg 56, :206),
bhPushGameData nach dem Speichern (SaveScreen.c:1408). CTest `typewriter_save_slots`,
94/94 grün. Offen: `typ_flg 0x10` nach dem Laden, Type_Space bei Räumen ohne
RoomNameSwitch-Eintrag (Original liest Index −1), Fensterabnahme der Ansicht.
**Start.command-Dev-App durch `./build.sh` aktualisiert; kein Paket ersetzt.**

Gemeldete Fehler Schreibmaschine/Aufnahme/Scanner (Arbeitsbaum, 2026-10-15): Panel-Fundament
mit Originalfaktor — `njDrawPolygon` (ps2_NaDraw.c:8-58) schreibt Vertexalpha `col>>25` (12 bei
0x18808080), Kontext 1 mit dem ALPHA-Register des vorangehenden Fonts (`bhDispFont` 8/6,
message.c:374); die bisherigen 96/128 stammten aus PS2_swaps Kontext 2. Tippzeile:
Raumname `CountMes-(14-ply_id)` (McSaveFile.c:707; vorher `n-15`, Zählwert 0 = ganzer Text
im ersten Bild) und Zeichnen mit dem Zählerstand vor der Erhöhung (McSaveFile.c:569-586,
`Typewriter::drawn_count`). Scannerabsturz: `sample_motion_pose` bricht bei mehr
Motionkanälen als Modellknoten nicht mehr das Spiel ab (Original schreibt hinter `objP`,
Motion.c:131); überzählige Kanäle werden wie `native/motion_set.cpp:63-67` verbraucht und
gemeldet (`bind_pose_gap`, für statische Objekte mit Werk/Identität/Motion in `object_pose`).
Ursache **nicht nachgestellt**: Nutzerstand save-2 (RM_0010) + Debug-Teleport RM_0090 +
Item 56/59 + Panel/Material, Headless mit `--validate-poses`, auch mit der 20:41-App,
ohne Fehler; in RM_0090 binden nur Event 22 (Objekt 8, MTN 6) und die Zombieevents 60-98
Raummotions. Fehlender Klick nach der Aufnahme: **nicht behoben** — sub1.c:3070 ist der
Öffnungsklick (ItemTaskCheck-Start), GetItem spielt SE 3 nur bei der Antwort (:3879/:3928),
die Schlussmeldung 158 schließt stumm (message.c), dann SE 9 (SpriteUV :2940); nativ gleich.
`values[21]=identity.variant` beim Raumladen bleibt (system.c:1623: Datei folgt aus rcase).
`native-gameplay-bindings`: sys->mes_ip gebunden, Effektlücke jetzt id 258 (E03_Shadow;
256 ist portiert). `verify_scanner` Prüfbilder 230/1000 (Meldungsaufbau je Durchlauf):
grün. CTest 100/101; `effect_room_sprites` auf HEAD-Worktree ebenso rot.
`verify_typewriter_save` auf HEAD-Worktree ebenso rot (Titel-LOAD-Replay ohne Kartenschritt).
Dev-App per `./build.sh` aktualisiert (23:10); kein Paket ersetzt.
**Kein Commit/Push.**

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
