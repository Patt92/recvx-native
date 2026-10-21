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
- `work/` enthält nur laufende Arbeit (Gegner-Auftragsbrett `work/enemies/`, Briefings,
  Inventarvertrag, Leistungsplan, Quarantänemanifest). Erledigte Ergebnisse, angewendete
  Patches und die Anweisungshistorie liegen seit 2026-10-21 unter `archived/work/`;
  Verweise in Ledger, Docs, Code und Tests sind nachgezogen.

## Arbeitsbaum und Auslieferung

Gemeinsamer Arbeitsbaum mit laufender Fremdarbeit; aktuelles `git status --short`
und den tatsächlich gestarteten Binary-Stand prüfen. Keine pauschale Freigabe
oder Dateibesitzzuordnung aus historischer Übergabe übernehmen.

**Portierungsrunde nach dem Audit (2026-10-21, Arbeitsbaum):** StartVibrationEx(0, 8) beim Schieben/Drehen
(psh/rpsh) an die Vibrationseinheit gebunden, flr_no bei Common/Player 7, bhCheckSubTask-Tor plp->flg 0x6
(Statustaste/Skriptanforderung). Kistenklettern (bhObj001) und bhControlPlayer :1540-1572 als gebunden
belegt; Audio-Bank-3-Meldung im Scan ist der Direktstart ohne bhRoomSoundCase (kein Portfehler). Snapshot-Format 2
speichert die Raumklang-Variante (Test `snapshot_room_sound`, Format 1 weiter lesbar). [Bericht](work/port-batch-20261021.result.md). **Kein Commit.**

**EventVm-Audit und Rest-Opcodes (2026-10-21, Arbeitsbaum, 212/212 grün):** alle 223 Skriptbefehle
gebunden (0x82 bhSetDebugLoopEx, 0xCF bhExGameItemInit, 0xD3 bhRankingCall; dm0-Plätze halten wie
event.c:437 mit Konsolenmeldung statt Hostfehler). Statischer Skript-Walker
`tools/walk_event_scripts.py`: Player_controll 132/140/141/143/145 und 0x82 in keinem Raum benutzt.
Ledger-Altlasten aus der Generatorzeit (bhItemGetGet/Ex, bhPlItemLost/Ex, bhArmsItemSet,
bhPlayerPoisonCk, bhNext) am Original neu belegt (02d12c1). Danach: Exm-Tor wie player.c:1678
(mode0 1, Passtor, ohne st_flg 0x4), bhCheckCut(1)-Augenzweig (cut.c:24-45), Common-Selektor 0
nur auf dem Spieler (Walker). Offen: ranking.c ControlRanking.
[Bericht](work/eventvm-audit-20261021.md). **Kein Commit** fuer den zweiten Teil.

**IOP-Modell: Stimmenpool, ATick, SE-Timer (2026-10-21, Arbeitsbaum):** native/audio_iop.* bildet TSNDDRV-
Befehlsthread, TQ-Warteschlange, MODMIDI-Sequencer, MODHSYN (2x24 Stimmen, Raub, Key-On-Stapel) und die 48
SPU2-Stimmen nach; der Mixer fuehrt nur noch EE-Handles und ADX/Host-PCM. Audiotests auf die originale
IOP-Latenz umgestellt (Req am sdSysServer, Ton im ATick, Stopp zwei Bilder spaeter). Hostpolitik: ersetzte
Bank stoppt ihren Port. **Kein Commit.**

**MIDI-Pegel und MidiHandle[2] (2026-10-21, Arbeitsbaum):** Sequenznoten rechnen das MODHSYN-Register
(MODMIDI-CC7 x Portlautstaerke, HsMsg-Pan je 240-Hz-ATick, Pitchbend jetzt ausgewertet, Kanalzustand je Port
ueber Sequenzen); Hintergrund-Slot 2, System-MIDI und Tuer teilen die Portrotation 2->8->9; Hintergrund-Timer,
Hintergrund-Stopp und Raum-Fade-In laufen als MidiInfo-Fades (Host-Rampen und finish_door_sound entfernt);
Tuer-Skip stoppt nur Tuerlisten. Stimmenpool 24/Kern gemessen (max. 24, RM_0100) und als Meldung eingezogen,
Raub/ATick-Batching offen. Tests audio_levels/audio_gaps/audio_movie/core_continuity angepasst. **Kein Commit.**

**SE-Shot-Pegel TSNDDRV/MODHSYN (2026-10-21, Arbeitsbaum, `./build.sh --test` 205/205):** eigene
Disassemblierung von TSNDDRV/MODHSYN/MODMIDI.IRX (nur Lesereferenz). SE-Shots rechnen jetzt das
SPU2-Register VOLL/VOLR (`sony_voice_levels`): Notenpegel inkl. Sample +0x10 (251 Samples mit 100
fehlten), CC7/CC10, SdrSeReq/SdrSeChg-Regeln (ungueltige Lautstaerke, Chg-Pan 64, Chg-Bend 8192).
Neuer Test `audio_levels` (5 Gegenproben rot). Ledger: Get3DSoundParameter, CallPlayerFootStepSeEx,
CallPlayerWeaponSeEx, CallYakkyouSe done (ARMS-SPQ-Gate: nativ synchroner Transport). Offen:
CallNativeEventSe/MIDI-Pegel (MODMIDI-CC7, HsMsg-Pan, Kanalstart) und Stimmenpool, siehe docs/AUDIO.md.

**Audio-Lücken sdfunc.c (2026-10-21, Arbeitsbaum):** 11 partial-Zeilen am Original nachgeprüft und
auf done gesetzt: `ss_flg 0x4000000` hat keinen Originalschreiber (PlayBgmEx2/ExecAdxFadeManager
immer offen), `sdMidiSetFxLev` ist auf PS2 leer (CallDoorSe FxLevel ohne Wirkung, -2-Delays = Pegel 0/
Mitte/Pitch 0), Set3dSoundFlag/SetUserSoundVolume 6/7/8, Objekt-SE (Type stets 0), Gegner-SE-Manager
und 60-Hz-Fades, VolDownTbl 8..15 (Aufruferrahmen, kein Literal erreicht ihn), PlayVoiceEx2-Pan
(reiner Zustand). Neuer Test `audio_gaps` (5 Gegenproben rot). Offen bleibt nur die geschlossene
IOP-Pegel-/Pan-/Pitchsynthese (Get3DSoundParameter, Weapon/Yakkyou/Footstep, CallNativeEventSe
Stimmenpool). **Kein Commit.**

**RM_1090 Computer, falsches Passwort (2026-10-21, Arbeitsbaum):** Task 8 (bhSysCallEvent) läuft im
Original während des Terminals weiter (nur Task 7 gestoppt, system.c:2121); nativ ruhte der
Ereignistick, Ereignis 4 verdrängte das schlafende Ereignis 5, die Monitorkamera blieb stehen.
Fix runtime.cpp, Test `rm1090_terminal_wrong_password`. **Wesker-Absturz** („player motion 9
missing“): Motion 9 hat 20 Kanäle auf 22 Knoten; Motion.c:131 schreibt nur obj_num Knoten,
`has_motion` jetzt `<= bones`. Test `player_short_motion`. **Kein Commit.**

**Nutzerbefunde 2026-10-21 (Dev-App, 203/203 grün):** (1) Himmel schwarz beim Inventaröffnen: njSetBackColor(0)
kam schon beim Tab statt im Monitor nach dem Abblenden (system.c:1987); (2) Calico-Mündungsfeuer bei zwei Zielen:
PlyPchMain-Armknoten fehlten im committeten owP-Baum (beides in de1fac7 mitcommittet); (3) RM_3091-Wurmsound:
SE-Stimmen ignorierten VAG-Loopmarker (70400 brach nach 1 s ab) – Mix folgt jetzt den Markern, dazu AllStopEnemySe
(system.c:602/617); (4) Stufenaktion lief am Passtor player.c:1365 vorbei weiter, wenn ein Ereignis die Steuerung nimmt
(RM_3100-Verdacht, Szene nicht nachgestellt). Autoaim auf tote Gegner nicht reproduziert. **Kein Commit.**

**Gegnerzeilen-Audit und EN21-Abnahme (2026-10-21, Arbeitsbaum):** EN21 am echten Raum
(RM_3120/RM_3140) mit `enemy_en21_world` belegt, sechs Ledger-Zeilen done. Veraltet waren
EN26 Kopfplatzen, EN29 Blut, EN19 Schatten/Trefferzuweisung/Kamerawackel, EN12-Effekte;
neu portiert `bhSetGunSplash` (combat.cpp, Kampf- und Effektpass) und das Zeichnen der
EN01-Kinder mit eigener Modellreihe (Kappe/Wurm/Bom/Scope, 30 Räume). Tests `gun_splash`,
`enemy_en01_child_draw`, `rm5840_tyrant_gun`. Berichte `archived/work/enemy_rows_audit.result.md`,
`archived/work/en21_acceptance.result.md`. **Kein Commit.**

**RM_0090 Sicherheitsbox mit hochstehendem Deckel (2026-10-20, Arbeitsbaum, 197/197 grün):**
Raumobjekte übernahmen jede Common-24/34-Motion im nächsten Takt; im Original schreibt nur
bhSetMotion (v0 0/2/5/6, bhSetObjMotion) objP. Auslöser: scd1 setzt das Work der Ebene 15 auf
obwp[14], das Fensterzombie-Common 34 (Raummotion 2) trifft danach die Box. Test `object_motion_commit`.
Zusätzlich zeichnet der Headless-Lauf jeden Takt (rand() in bhEff106), `verify_enemy_ai` wieder grün.

**Review der letzten 10 Commits (2026-10-20, Arbeitsbaum, 196/196 grün):** GPU-Batches über
Schattenphasengrenzen (Metal/D3D11), doppeltes bhStandPlayerMotion beim Kistenschließen,
Klauen-Blendzustand, triviale Prüfungen im Stand-Test, ISO-Rohabbilder/PVD-Lage/Namen behoben.
Offen: `verify_enemy_ai` rot seit 2de4dad (Zombie-Zeitachse, Originalreferenz nötig).
[Bericht](build/validation/review-last10-20261020/README.md). **Kein Commit.**

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

**Leistung/Energie:** Der Software-Rasterer ist ≈ 99 % der CPU-Last (≈ 62–70 ms CPU je
Raumbild bei „High“, Simulation ≈ 0.26 ms/Takt). Plan und Messbasis:
[Plan](work/performance_optimization_plan_20261020.md),
[Phase A + Schnitte](archived/work/performance_phaseA_scalpel_20261020.result.md)
(`bilinear` inline −8 % bitidentisch, ein Zeichenpass je Takt, `--render-threads` für
`powermetrics`-A/B am Starter). Workerzahl per `powermetrics` entschieden (Automatik bleibt).
[GPU-Machbarkeit](archived/work/performance_gpu_feasibility_20261020.result.md) bestanden: dieselbe
Setup-Liste auf Metal statt 62–73 ms CPU. **[GPU-Rasterer im Spiel](archived/work/performance_gpu_raster_d2_20261020.result.md)**
(macOS, Dev-App): `video.ini renderer = gpu` (Standard), `--renderer gpu|software`; Punktabtastung
bitgleich inkl. Schatten (Test `gpu_raster_parity`), CPU je Bild ≈ 0,3–0,5 ms. Offen: Wattmessung
am Starter. Schattenmasken laufen ebenfalls auf der GPU (ein Befehlspuffer je Bild).
Windows: D3D11-Backend über Rasterizer Ordered Views geschrieben, Crossbuild grün, **auf Hardware
noch ungeprüft**; GPUs ohne ROV behalten den Softwarepfad.
**OpenGL (2026-10-21, Arbeitsbaum):** optionales Core-4.1-Backend, Linux-Standard;
`--renderer opengl`/INI/Debugwahl, Metal/D3D-Defaults erhalten. Mac-arm64 GL an/aus und
Windows-x64-Crossbuild grün; 11 gezielte Tests bestanden, 3 GPU-Prüfungen wegen
Sandbox ohne Display übersprungen. Dev-App aktualisiert, Titel/Raum nur im
Softwarefallback geprüft. GL-Shader-/Pixelparität und Linux-Hardware offen.
[Bericht](archived/work/opengl_backend.result.md).

**Kartenbild:** MAP zeichnet MRY-Geometrie, Marker und Sprites in der eigenen
Build-Runtime; Fade-Rückweg und Wiedereintritt gezielt geprüft. Constant-Material-
Farbpfad und Typ-41-Normalen korrigiert. **Noch keine Starter-/Paketersetzung.**
Belege und verbleibende Grenzen: [D10 Kartenrenderer](archived/work/native_port_D10_map_render.result.md).

**Pausentaste (Host):** Escape beendet die Runtime nie. Im Menü/Dialog bricht es ab,
im freien Spiel öffnet und schließt es die Pause (`system.c:668/691`, dieselbe
Taste schaltet ein und aus); das Gate bleibt `st_flg 0x1C040208`. Quit liegt auf F10,
und eine Taste, die Cancel ist, zählt nicht als Quit (auch nach Neubelegung; alte
`controls.ini` mit `quit = Escape, F10` behalten F10). Belege:
[keyboard_controller_config mit Negativkontrolle](tests/native/input_test.cpp)
und `main_menu_settings_video_skip` in der eigenen Build-Runtime,
[Controls](docs/CONTROLS.md). **Noch keine Starter-/Paketersetzung.**

**bhLoadWork-Fall 0 (Spielerwork):** `event.c:bhLoadWork`/`bhLoadWorkEx` Fall 0
schreiben jetzt `mode0 7`, `mode2 0`, mtn_no-Lowbyte 42, `stflg |= 0x10000`,
`stflg &= ~0x80` und `flg |= 0x10000`; `bhLoadWork2` Fall 0 nur `mode0 7` sowie
`flg`/`stflg 0x10000` (jeweils mit Originalfundstelle). Damit greift das
Spielerpad-Gate `player.c:1804` und die portierte Freigabe in
`system.c:736-744` löscht echte Schreiber. Die rmthp-Lücke datP NULL
(`Motion.c:29`) ist für den Spielerwork wie für Entities behandelt (kein
Schritt, kein frm_no-Fortschritt, zuletzt geschriebener objP/owp wird
gezeichnet). Setzen und Freigabe liegen in einem Tick; Endbild vorher/nachher
gleich, Belege: [Messungen](build/validation/loadwork-case0/README.md),
`event_lifecycle_control_release` und `script_motion` (Ctest 124/124).
**Noch keine Starter-/Paketersetzung.**

**ID-Karte RM_1020 (Fix A):** Der Freeze nach der Wegwerf-Frage ist behoben. Der Spielerpass
läuft trotz st_flg 0x4 unter den Originalbedingungen von bhCPM0_action, und bhControlPlayerPad
gibt das Bit frei (player.c:1838). Geprüft in der eigenen Build-Runtime
`build/discard-claude-20261014`. Offen ist B: das Original-Pad-Gating statt Host-Gate.
**Noch keine Starter-/Paketersetzung.**
[Bericht](archived/work/native_port_discard_question.result.md).

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
[Ergebnis](archived/work/native_port_effsub0_handport.result.md), Differenzläufe
`build/validation/effproc-handport-20261014/`, Gruppen 7/8
`build/validation/effproc-backface-20261014/`, Gruppe 9 `build/validation/effproc-snow-20261014/`, Gruppe 10 (inkl. `bhSetEffectLink` beim
Raumladen) `build/validation/effproc-afterimage-20261014/`.
Nachbild-Nachprüfung: belegte DifAmb-Lichtrechnung, committed Spieler-/Zusatzmodellaufnahme
und Skin-FM; **88/88** Tests. Diagnose-A/B mit Originalraumgeometrie, keine natürliche
Sichtabnahme. [Belege](archived/work/native_port_effsub0_handport.result.md).
**Start.command-Dev-App durch `./build.sh` aktualisiert; kein Paket ersetzt.**

**Gegner-KI EN21:** Albinoid-Jungtier im nativen WorldEntity-/Dispatchpfad
angebunden, mit Route/Spielmodus, Spielergriff, Audio und Effekten 396/399 auf der
Modelltexturliste. Gemeinsame Routeninitialisierung ohne Zombie-Ressource und
Spieler-Griffseiteneffekte ergänzt. Eigene arm64-Build `build/en21-bind-claude-20261014`.
**Tests auf Nutzerwunsch zurückgestellt; keine Gameplay-Abnahme oder Starter-Ersetzung.**
[Umfang und spätere Abnahme](archived/work/native_port_EN21_world.result.md).

**Bandersnatcher (EN09):** Stillstand behoben — Identität 9 fehlte im Dispatch, die
Zustandsmaschine war nur Gerüst. KI, Motion/Fußfixierung, Arme, Griff, Schaden, Tod
und Rauch portiert; RM_1060 A/B und `tests/native/en09_world_test.cpp` grün.
Eigene Build `build/en09-claude-20261014`. **Noch keine Starter-/Paketersetzung.**
[Bericht](archived/work/native_port_EN09_world.result.md).

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
[Bericht](archived/work/native_port_enemy_gap_scan.result.md). **Kein Commit/Push, keine Paketersetzung.**

**EN02 Gulp Worm (2026-10-14 nachts):** vollständig portiert (`native/enemies/en02.c`, Dispatch
`jump[2]`, 18 Kindworks auf Slot 31, mobiler ATR, Sandsprites 253/254, Effektstimme 127).
RM_5830 headless: Angriffskette läuft, Spieler-HP 160→0; CTest `enemy_en02_world` grün, Gegner-/
Effekt-Suite 32/32. Scan jetzt **35 Räume** mit Lücken. Belegte Original-UB: `ang` in
`SetSandEffectMain` Fall 8 ist `$s3` des Aufrufers von `bhControlEnemy` (Disassembly), nativ gemeldet.
Offen: Skript-Opcodes 0x98/0xC3 (world.cpp fremd in Arbeit), Sprite 255, RM_3091/7220 skriptgehalten.
[Bericht](archived/work/native_port_EN02_world.result.md). **Kein Commit/Push, keine Paketersetzung.**

**Effekt 355 (2026-10-14 nachts):** `bhEff_Sub355` portiert (Zielfernrohr-Marke des EN01-Kindes
bhEne01Scope; globale Texturgruppe 4 aus SYSTEM/0001.bin, 16-Pass-Alpha). **Korrektur:** die frühere
Meldung `eff[-1]` in RM_5550/5630/8000 war eine Fehldiagnose — `spawn_effect` liefert -1 für
**nicht portierte** Effekte, und der Aufrufer meldet dann die Originalbedingung; der Pool war nie
voll. Beide Meldungen (`bhEff355`, `eff[-1]`) sind mit dem Port verschwunden. Scan jetzt **32 Räume**
mit Lücken. [Bericht](archived/work/native_port_enemy_gap_scan.result.md). **Kein Commit/Push, keine Paketersetzung.**

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
[Bericht](archived/work/native_port_player_owp_effect_pass.result.md). **Kein Commit/Push.**

**RM_1090 Standuhr (2026-10-15):** Sichtloch stand 22,5° neben 12 Uhr — Common_controll 9 schrieb
für den nativen Uhr-Work in einen ungelesenen Override, jetzt wie 8/18 auf `enemy_obj`
(event.c:12136). Escape bestätigte Skriptauswahlen mit Auswahl 0 („Links“); Cancel bestätigt
keine Auswahl mehr, das Raumskript liest ihn selbst (bhPadCheck, Event 13). Der Reset-Freeze
nach L/R war die Dev-App vom 10.10. ohne 0xb1 (HEAD hat den Port). Raumdaten: Tafeln
1/3/4/5/6/7/8/Emblem, L=+180°, R=−45°; L,R,LR,RRR zeigt 3,1,7,1 — „1974“ ist nicht darstellbar.
Last: nur die Schreibtischkamera (Glasdach, bildfüllendes Alpha) kostet ~2× CPU, kein Fehler.
Nachweis `tools/verify_clock_puzzle.py` (Build und Dev-App grün);
[Bericht](archived/work/rm1090_clock/README.md). `./build.sh` gelaufen. **Kein Commit/Push.**

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
[Bericht](archived/work/native_port_enemy_gap_scan.result.md) Abschnitt 9. **Kein Commit/Push.**

**Effekt 267/269 + EN14-Dienste (2026-10-15):** `bhEff_E13_Fluid` (Erzeuger ist EN30, Gruppe 81
freigeschaltet) und `bhEff_E14_Fire` (Kind von 256 type 6; `fire_light`-Haken um Farbe/Reichweite
verallgemeinert) von Hand portiert; EN14 `joint_flags`/`node_angles`/`motion_angles`/`joint_matrix_set`/
`floor_enemy`/`game_mode`/`find_beam`/`beam_line` (O_WRK +0x10/+0x64 nach `RECVX_EN16_EFF`) gebunden.
Test `effect_e14_fire`, 37/37 grün; RM_9302/RM_5810 zeigen 256/257/267 sichtbar. Scan `run11` **31 Räume /
46 Einträge** (50 → 46; neu sichtbar `bhEff250`/`bhEff301` in RM_9302, weil der Kampf jetzt weiter kommt).
Offen: `bhEff130` (Spielerstrahl, 1120 Z.) — `find_beam` findet nativ nie einen Strahl. `./build.sh`
gelaufen. [Bericht](archived/work/native_port_enemy_gap_scan.result.md) Abschnitt 10. **Kein Commit/Push.**

**Effekt 250 + `model_texp_index` + Morph-Zeichenpfad (2026-10-15):** `bhEff_E00_Mince` portiert; der
Erzeuger brauchte `RECVX_ENEMY_EXT.model_texp_index` (Blut-Texturliste), jetzt gebunden — RM_5810/RM_9302
melden nur noch `bhEff301`. Dabei gefunden: `actor.cpp` warf „morph object has no vertex stream" für
EN14 MV02 (Objekt 1 evalflags 0xE), das `npCalcMorphing` überspringt; Quellzugriff jetzt hinter
`morph_blend`. HEAD rendert dort nur, weil EN14 MV02 vorher nicht erreichte (Worktree-Messung). Scans sehen
nur das Endbild — Endbild-Sweeps nach Gegnerports nötig. Scan `run12`/`run13` **31 Räume / 44 Einträge**,
37/37 Tests, `./build.sh` gelaufen. Letzte EN14-Raumlücke `bhEff301` hängt an der fehlenden 2D-Effektliste
`sys->ef_trs2d` (Subsystem). [Bericht](archived/work/native_port_enemy_gap_scan.result.md) Abschnitt 11.
**Kein Commit/Push.**

**EN05/EN13/13B/EN23/bhStandPlayerMotion (2026-10-14 abends):** Hunter/Sweeper (EN05 +
bhEne05s), Alexia II (EN13/13B), Riesenspinne (EN23 + bhEne03s/EN24-Kinder) portiert und
im Dispatch; bhStandPlayerMotion an Kiste/Karte/Status/Computer/Player_controll 52/bhEff134.
Runtime rechnet plp vor Event_init (player.c:840) — schliesst die Erst-Pass-Lücken
`plp->mlwP->owP[n]`. CTest 89/89 grün (`enemy_en05_world`, `enemy_en13_world`,
`enemy_en23_world`). Offen: Effekte 238/256/267, EN14-Lücken, plp->txp[2], keine
Fensterabnahme, Windows nicht gebaut. Nur Entwicklungs-Start.command aktualisiert.
[Bericht](archived/work/native_port_en05_en13_en23_stand.result.md).

**Gegner-Auditbefunde (2026-10-14):** `bhCheckWall` für Gegner, `bhControlMask` im
Gegnerpass, Baum beim Raumaufbau (room.c:438), modellose Works, EN11-Dienste,
Effekte 261/270 mit `bhCollisionCheckL2MDL` (plp mdl[3]) und Eckalpha gebunden; CTest
`enemy_beam_effects`. Zombie-`natural` ist Originalverhalten (RM_0080 pos_no 0 →
Sub_controll 0x8f Typ 8), Verifier korrigiert. Offen: EN56/bhEff134, EN16-Folgeeffekte
(EN05/EN13/EN23 und EN12-Feuereffekte siehe oben).
[Bericht](archived/work/native_port_enemy_audit_fixes.result.md).

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
Speicherabschluss am 2026-10-18 nachgeprüft: Continue-Punkt erst nach der Tippanimation
(SaveScreen.c:1408), nicht beim Dateischreiben; einmalige Abschlussmeldung von Typewriter.
Die vertauschten Flag-56-Operationen berichtigt: Schreibfehler setzt, Sonderspeichern löscht.
`verify_typewriter_save.py` prüft RM_0010 mit isolierten Benutzerdateien: letzter Tipp-Pass
noch alter Continue-Punkt, Erfolg neuer Punkt; echter Host-Schreibfehler, wiederholter Fehler,
Abbruchrückfrage und erneutes Speichern in anderem Slot ohne weiteren Band-/Zählerverbrauch.
Kartenverwaltung bleibt entfernt. CTest 122/122 grün; derselbe Prüflauf über `Start.command`
grün, Erfolgs-/Fehler-/Rückfragebilder angesehen. Belege: `build/validation/typewriter-completion/`.
**Gemeldeter Fehler „Speichermenü nur per Speichern verlassbar" (2026-10-18):** Escape ist im Host
zugleich `cancel` und `pause`; das Pausetor `pause_allowed` prüfte die laufende Schreibmaschinen-
aufgabe nicht und verschluckte die Taste, bevor sie die Slotliste erreichte (Tab ist dort keine
Originaltaste). Das Original sperrt genau diesen Fall: `system.c:668` verlangt zusätzlich
`(ts_flg & 0x3DE00) == 0x3DE00`, und Bit 15 ist das Taskbit der Schreibmaschine (`main.c:195`,
Task 15). Jetzt gebunden; Pause in freier Kontrolle unverändert (gemessen: mit Escape 61 Pässe
und Spieler steht, ohne Escape 356 Pässe und z −35.9 → 52.5). Prüfung mit echten SDL-Tasten:
`tools/verify_typewriter_save.py --sdl-cancel` (Slotliste und Speicherrückfrage verlassen, kein
Band-/Zählerverbrauch, keine Speicherdatei; Vorher-Binary rot), auch über `Start.command`.
Offen: Ladeabschluss (`typ_flg 0x10`), Type_Space bei Räumen ohne RoomNameSwitch-Eintrag
(Original liest Index −1); keine interaktive Fensterabnahme oder Windows-Lauf.
**Start.command-Dev-App durch `./build.sh --test` aktualisiert, arm64/Signatur geprüft;
kein Release-Paket ersetzt, kein Commit/Push.**

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
Gegnerpaket 2026-10-15 (Arbeitsbaum): `rySetShadow`/`bhEff304` gebunden (Modifier-Schatten ueber
`sys->efm[1]`; drei Gelenke, Lage/ay/sx/sz/Sichtgate nach effsub3.c:1683) und EN29 Tentakel
portiert (`native/enemies/en29.c`+`en29.h`: Mode0/BrainType, `En29ActTbl[40]` im EA_WORK-Layout,
Mv00-04/20 mit 74498/74499, TargetAnalyze, DmgCheck, AttackHitCheck gegen `plp->watr`,
PlySetDamage/PlyMoveMain/PlyAction*, SetDmgEffect, fuenf Schadenskapsel-Kinder als Ring).
Tests `effect_ry_shadow` (RM_5840) und `enemy_en29_world` (RM_9000, Angriffskette bis
PlyMoveMain); CTest 102/103 (47 fremd). Lueckenmessung alle 205 Raeume: 31/44 -> **26/37**
(`build/validation/enemy_scan_20261014/run15`). Ledger und
`archived/work/native_port_enemy_gap_scan.result.md` (Abschnitt 12) fortgeschrieben.
**Naechster Posten:** EN17 (Monster Steve; Vertrag en17.h und en54/en55 liegen fertig),
EN17 Monster Steve inklusive zwei Kettenwerken (Arme) ist portiert (`native/enemies/en17.c`);
damit laufen auch die gepanzerten Ritter EN54/EN55 (beide jetzt in Build und Dispatch). Test
`enemy_en17_world` (RM_9280); CTest 103/104 (47 fremd). Messung: 26/37 -> **25/34**
(`run16`); `bhEne17`/`54`/`55` aus RM_9280 weg. Offen und benannt: Partikel `bhEff_SetPtcl*`,
Nachbild 364, `rom->lgtp[2]`, Spielerblut. [Bericht](archived/work/native_port_enemy_gap_scan.result.md)
Abschnitt 13. **Naechster Posten:** das `sys->wt_*`-Wassertiefenfeld (`bhObj005`) fuer `bhEff108`
(4 Raeume).
Wasserflaeche `bhObj005` (`native/world_water.cpp`: Hoehenkarte `sys->wt_*`, Eckpunkt-y/Normalen
nach den ELF-geprueften Asm-Bloecken und `VU0_WAVE_INIT/CALC`, 4x4-Kachel-UV ueber `npSetOffsetUV2`,
Abklingen) und `bhEff108` (Wasserringe) sind portiert; Zeichenpfad ueber
`ModelGeometryOptions.local_vertices/local_normals/uv_*`. Befund: `sys->st_flg 0x40` setzte nativ
nie jemand, alle wassergebundenen Pfade (Zombie-Wassercheck, Spritzer, Wasserschritte) waren in den
13 Wasserraeumen stumm; EN01 `bhCheckWater` jetzt gebunden. Test `object_water_wave`
(RM_3060/3210/7060/9130); CTest 104/105 (47 fremd). Messung: 25/34 -> **32/42** (`run17`):
`bhEff108` (4) weg, neu die eine belegte Original-UB "Nachbarzelle vor `wt_wvp`" (Randeckpunkte
x/z = 0) in 12 Raeumen. [Bericht](archived/work/native_port_enemy_gap_scan.result.md) Abschnitt 14.
**Naechster Posten:** `bhEne03` (RM_6090/7080/7250), danach EN53 `child_ambient`. **Kein Commit/Push.**
Schwarze Witwe `bhEne03` (`native/enemies/en03.c`, rund 4100 Zeilen) ist portiert und im Dispatch
(`jump[3]`): Init samt acht `bhEne03s`-Beinkindern und fuenfzehn EN24-Kindern, verstecktem
Anfuehrerwork, Schatten 258; Brain BR00-02, Move MV00-19, Nage NG00/01, Damage DG00-12, Die
DD00-05, CollisionWalls/BoxEdge2/Line/Line2, DiveSpace, DamageInit, CheckJumpSpace, PlayerControl,
CallSE, Acid, HitMark. Befunde: `CheckClimbWall`/`CheckClimbDownWall`/`AvoidWall` sind im Original
parameterlos und geben 0 — die Zweige `mode2 6/7/8` sind damit unerreichbar (Originalverhalten).
Ein absichtlich leerer MDL-Record **innerhalb** eines Modellsatzes ist im Original ein ML_WORK mit
`obj_num 0`; `load_models`/`create_enemy_child` unterscheiden ihn jetzt von einem gescheiterten
Lesevorgang (`model_failed`). `plp->dvx/dvy/dvz` hat keinen verbrauchenden Spielerbesitzer (Dienst
meldet es); `EXP0_INT(pno,980)` schreibt im Original hinter den Kindblock (nicht nachgebildet),
`EXP0_BYTE(...,12/14/15)` bleibt als `recvx_en03s_leg_mark` erhalten. Test `enemy_en03_world`
(RM_6090; Init, Kinder, Anfuehrer, MV00-Zweig nach 96 Passen, Gelenkmatrix weicht von der Bind-Pose
ab, GoAHead bewegt, Gegenprobe ohne Gegnerpass); CTest 105/106 (47 fremd). Messung: 32/42 -> **30/40**
(`run18`); `bhEne03` (3) weg, neu die eine, einmalige Spielerlookup-Meldung nach dem Griffende in
RM_7250. [Bericht](archived/work/native_port_enemy_gap_scan.result.md) Abschnitt 15.
Nosferatu `bhEne15` (`native/enemies/en15.c`, `jump[15]`) ist portiert: Bewegung, drei Hiebe
(Fluegelquader `njCollisionCheckBC2`, Kapsel), Spritzer, Wurf, Schaden/Kerntreffer, Tod, alle acht
Spielerfuehrungen und das Gift 397 (`bhEff_E15_Poison`). Mechanismusweite Befunde:
`bhSetMotion(plp)` aus dem Gegnerpass (`player_set_motion_now`), Rueckweg fuer `plp->spd`
(`take_player_speed`), Giftbits `stflg 0x80000/0x200000` erreichen jetzt das Inventar (damit
vergiftet auch die Saeure 256 wie im Original), Effektsaetze tragen `exp0 = epw`. Test
`enemy_en15_world` (RM_5800: Wurfkette, Sturz ueber den Plattformrand, Gift); CTest 106/107
(47 fremd). Messung: 30/40 -> **30/38** (`run19`), `bhEne15` (2) weg.
[Bericht](archived/work/native_port_enemy_gap_scan.result.md) Abschnitt 16.
EN53 `child_ambient` ist am vorhandenen Modellbesitzer gebunden: je `mbp[k]` ein Live-Strom,
Patch nur auf Ambient-Gruen, geordnete Materialweitergabe und Multi-Lichtauswertung.
`enemy_en53_world` prueft drei Originalraeume, Puls, diagnostischen Tod, Patchgrenzen und den
produktiven Zeichen-/Lichtpfad; neun gezielte Tests gruen. Start.command-Dev-App neu gebaut,
arm64/Signatur geprueft; RM_5800/6040/6100 bei 1/8/30/60/600 Bildern ohne Enemy gaps und
Posenabbruch. Kein neuer Gesamtscan; Kampf/Fensterabnahme offen.
[EN53-Bericht](work/enemies/EN053.result.md), Scanbericht Abschnitt 17.
`bhSetRapEff 310` erledigt (2026-10-16): die ganze Effektanlage des Tyrant laeuft nativ auf dem
vorhandenen `eff[512]`-Satz. Neu von Hand portiert: 310 `bhEff30a` Kamerawackel, 349 Rueckruf,
308 `bhSetEffSpark` (eigene Tabelle `Eff308Prm[2]`, `gnd_hgh` aus `bhGetGroundPosition`) in
`native/effect_rap_camera.cpp`; 307 Klauenflaeche + 312 gespiegelte Klaue in
`native/effect_rap_claw.cpp`; 309/311 Blutstrahlen in `native/effect_rap_blood.cpp`; 305 Staub +
306 Rauch in `native/effect_eff30x.cpp`. Kamera/Licht ueber die Haken `camera_offset_set/add` und
`spark_light`, Bereichsueberschreitungen bleiben benannte Luecken. Tests `effect_rap_camera`,
`effect_rap_claw`, `effect_rap_blood`, `effect_eff30x` und `enemy_en19_world` gruen
(RM_5840: `310=1 349=0 312=3`, Typ 2 hat kein 349). Headless 600 Bilder: RM_5010 **+310/349**,
RM_5840 **+305/307/309/310/312**, Kamerapose und Tyrant-Zustand vorher/nachher gleich, keine
Effektluecke. Tabellen byteweise am ELF belegt (u. a. `Tex5Buv[12]` liest Sprungtabellenworte =
Denormale ~3.4e-39, nativ `{0,0,0,0}`). Offen: `bhEff298`/`bhEff303` unportiert, `bhEne19`
Mode0-Index 4 (im Original NULL), kein natuerlicher Durchspielnachweis (Spieler stirbt im Lauf).
[EN19-Bericht](work/enemies/EN019.result.md). **Kein Commit/Push.**

**`effect_room_sprites` wieder grün (2026-10-16), zwei vorbestehende Befunde geklaert:** (1) seit
`7d1f366` meldet `step_eff219` die Kameraluecke bewusst nicht mehr, weil `room.c:33`
`bhInitCamera` das ganze `cam`-Objekt beim Raumladen nullt und `game.c:50` vor `:58` laeuft – die
Box liegt im ersten Pass am Ursprung; drei veraltete Pruefungen nachgezogen. (2) Der danach
sichtbare Nachbildfehler stammt aus `f7f6c67` und ist **gemessen** eingegrenzt (`7b81215` gruen,
`7d1f366` + 219-Korrektur gruen, erst `f7f6c67` rot): die fuenf Nachbildalphas sind korrekt
(je 2704 Dreiecke, Materialphase 6), aber `f7f6c67` gibt Objektlistenwerken
(`kind==2 && object_list>=2`) `light_category = 1` – **belegt richtig**, denn `objitm.c:518-529`
zeichnet `ob_hlg`/`ob_spc` nach `bhSetHalfLight()` mit `amb_chr`. Zu grob war der Test; die
Alphapruefung laeuft jetzt auf `material_phase == 6`, dazu gleiche Dreieckszahl je Stufe und eine
ausdrueckliche Pruefung, dass Phase 3/4 auf halbem Licht liegt. Gegenprobe mit abgeschaltetem
Alphaschreiben schlaegt fehl. **Voller CTest 112/112 gruen.**
Scanbericht Abschnitt 18. **Kein Commit/Push.**

**`bhEff298`/`bhEff303` portiert (2026-10-16), die EN19-Effektkette ist damit vollstaendig:**
298 `bhEff_E00_AcidGenerator` (effsub2.c:623) in `native/effect_sprite_procedures.cpp`, 303
Blutfleck (effsub3.c:1507) in `native/effect_rap_blood.cpp`; `enemy_en19_world` laesst **keine**
Effektluecke mehr zu. Zwei Besitzerfragen mitgeklaert: `sys->bl_ct` liegt jetzt bei
`effect_blood_counter()` (ein Zaehler fuer Partikelpfad und bhEff301/303, Schnappschuss
erhalten), und `RoomWorld::EnemyCallbackFrame` bindet im Effektpass Weltzeiger und
EN19-Dienste, weil `bhEff349` dort Gegnercode ausfuehrt (`bhEne19_TyBloodSet` ruft
`bhGetGroundPosition`/`objP[18]`/`bhSetEffectTb`) — **ohne den Rahmen stuerzt der Weltest ab**.
Belegte Eigenheiten: 303 zaehlt `bl_ct` erst und nutzt es dann (umgekehrt zu bhClrEff_RY/301),
`sy` waechst gegen `szb` statt `syb`, `flg = 0x200001` ist eine Zuweisung; 298 haelt bei
`stflg 0x1000000` des Erzeugers auch den Wartezaehler an. Voller CTest **112/112** gruen,
vier Gegenproben greifen; RM_5010/5840/3230 headless je 600 Bilder mit identischen Satz-IDs,
Kamerapose, HP und Motion (kein natuerlicher Lauf erreicht die beiden Saetze).
Gemessen offen: RM_5010/3230 halten den Tyrant in `mode0 5`, RM_5840 ist Typ 2 ohne 349,
RM_1122 nicht im Kampf; der 303-Nachweis laeuft ueber das gekennzeichnete `status`-Bit, und der
Erzeugerzweig von 298 braucht einen Treffer mit Waffe 15 (`comb_wep` aus en03_lib.c:1314).
Scanbericht Abschnitt 19, [EN19-Bericht](work/enemies/EN019.result.md). **Kein Commit/Push.**

**Ledgerbereinigung (2026-10-16):** `docs/port_ledger.tsv` hatte zehn doppelte Schluessel, und
`tools/port_check.py` wertete still die jeweils letzte Zeile. Drei verfaelschten den Bericht:
`rySetShadow` (veraltetes `missing` ueberdeckte den belegten Port), `bhEff219` (`partial`
ueberdeckte `done`) und `bhDrawTrsEffect3D` (eine falsch einsortierte `eff[-1]`-Notiz hob den
Zeichenpass auf `done`; jetzt wieder `partial`, die Notiz steht bei `bhSetEffect`). Alle zehn
zusammengefuehrt, und `port_check.py` lehnt doppelte Schluessel jetzt ab. `rySetShadow`/`bhEff304`
gegen effsub3.c:1640/1683 geprueft (Gegenprobe Vorzeichen `ay` greift) -> `done`. `bhEff219`
Bildabnahme im Schneehof RM_5840 (natuerlicher Lauf, einzige Flockenquelle) -> `done`; in RM_9090
schaltet das Raumskript alle drei Boxen per `0x43` aus, das leere Bild dort ist Originalverhalten
(Laufzeitspur `build/validation/snow219_20261016/`). Effekte jetzt 176/190, kein offener Posten.
CTest 112/112. **Kein Commit/Push.**

**Granatwerfer und der 298-Erzeugerzweig (2026-10-16):** Waffe 15 ist der Saeure-Granatwerfer
(sub1.c:4489-4492). Der Erzeugerzweig war nicht erreichbar, weil `CombatController` die Waffen
14..17 still abwies (`return false` ohne Meldung). Jetzt: nicht freigegebene Waffen melden
`Combat gaps` (auch in der Runtime-Ausgabe); der Abschuss player.c:4990-5120 ist im Kern portiert
(vorher benannte Luecke), das Geschoss `bhEff130` (effsub1.c:9001, alle Typen) in
`native/effect_eff130.cpp`, `bhCheckFlyAtari`/`bhSetBowDamage` in `native/weapon_fly.c`; Waffen
14..17 freigegeben. `enemy_en19_world` belegt die **ganze Kette ueber einen echten Treffer**:
Saeureschuss -> Geschoss -> `bhCheckFlyAtari` (wpnr_no 15) -> `comb_wep 15` -> 298 -> 257
(Tyrant-HP 900 -> 860). Mitgeklaert: `bhEff019`/`bhEff130` schreiben `rom->lgtp[0]` ueber einen
Lichtbesitzer (`room_light0`, bhEff019 jetzt `done`); der Spielerkampf haengte Effekte aus
`bhSetEffectTb(..., NULL, ...)` (pwksub.c:2810/2919) faelschlich an den Spieler — `link_player`
entscheidet jetzt wie im Gegnerpfad. Tests `effect_eff130` (neu), `enemy_en19_world`,
`effect_room_sprites`; Gegenproben greifen; nach erzwungenem Neubau **CTest 113/113**,
`verify_combat` gruen, RM_5010/5840 vorher/nachher identisch. Offen: `sys->ef_ntx2d`,
`bhCheckL2Water` (beide inzwischen portiert, s. u.), Waffen 18/20, Mündungsrauch nicht bildlich abgenommen.
**Kein Commit/Push.**

**Zwei Fremdbefunde geprueft und behoben (2026-10-16):** (1) `bhLoadWork` 0x65 nahm fuer alle Arten
Byte 3 als Knoten; das Original setzt ihn fuer Spieler/Item/Effekt auf 0 (event.c:12733/12785/12799).
Reale Folge in RM_4010/4011/8010: `65 03 03 01` + Unterbefehl 18 drehte Item 3 um Knoten 1 statt um die
Wurzel (anderer Drehpunkt). Korrigiert in `native/world.cpp`; `script_motion` spielt die echten
Befehlsfolgen der drei Raeume ab (vorher rot gemessen) plus Gegenfaelle Spieler/Effekt/Objekt. `bhLoadWorkEx`
0xB5 (gleiche Regel, bhEtask) ist weiterhin nicht portiert und jetzt als `missing` gefuehrt. (2) Das
Computer-Figurenfenster tastete eine Motionpose neu ab; das Original zeichnet den vorhandenen lokalen
objP-Baum nach npRetSkinConvert und nullt objP[1].ang dauerhaft (sp_evt.c:1478-1500). Neu
`PlayerAsset::sample_tree` (direkter Baum-Zeichenweg mit npRetSkinConvert-Translationen),
`computer_character` liest enemy_obj/evalflags/live_stream bzw. committed Pose + node_overrides und
schreibt mtn_no/frm_no/px..az ans Gegnerwerk. Betroffen nur Skript 3 (Albinoid-Datenbank); Scanner 4-6
nutzen das Fenster nicht. Tests actor/computer_terminal/script_motion, Gegenproben rot, **CTest 113/113**,
`verify_computer` gruen. **Kein Commit/Push.**

**`bhLoadWorkEx` 0xB5 portiert (2026-10-16):** derselbe Auswahlzweig wie 0x65, Ziel `bhEtask[0]`
(Task-Ebene 0) statt des laufenden Tasks; Knotenregel identisch. Test `script_motion` (Zielkontext
messbar getrennt, Gegenprobe rot). Fall 0 teilt weiter die bekannte Luecke mit `bhLoadWork`.
**Baufalle gefunden:** `./build.sh` ohne `--test` konfiguriert `BUILD_TESTING=OFF`; danach sind alle
Testziele im Makefile leer (`make: Nothing to be done`, rc 0) und ein Testlauf trifft das alte Binary.
Ein vorheriger "gruener" Lauf von 0xB5 war dadurch ungueltig; nach `cmake -DBUILD_TESTING=ON` neu gebaut
und mit `strings` am Binary belegt. Vor jedem Testlauf nach build.sh: Cache pruefen oder
`./build.sh --test`. **Kein Commit/Push.**

**Granatwerfer-Rest (2026-10-16):** (1) **Kein Nachladen im Original** — `bhSearchBullet` (sub1.c:8907)
laedt nur Combi-Zeilen mit Flag `0xF000`; alle Granatenzeilen tragen `0x4/0x5`, nur Bogenmunition `0xF005`.
Das zuvor hier vermutete "Nachladen mit Munitionswechsel" ist kein Portposten; der Port prueft dasselbe
Tor. Festgenagelt in `ammunition` (leerer Werfer laedt/feuert nicht, Armbrust laedt). (2) **`sys->ef_ntx2d`
portiert:** `bhEff130` type 3 reiht den Schirmsatz je Pass ein (`ntx2d_queued`), `RoomWorld::draw_ntx2d_effects`
= effect.c:905 `bhDrawNtxEffect2D` -> `njDrawPolygon` (Alpha `col>>25`/128 unter 8/6) zeichnet ihn im
Runtime-Zeichenpass vor HUD und Fade. Tests `effect_eff130` (Eintrag erst ab ct0 > 7, Farben exakt) und
`effect_room_sprites ntx2d_room` (RM_5840, Pixelnachweis ueber die ganze Leinwand, Tore pt_flg/stflg);
zwei Gegenproben rot. `./build.sh --test`: **CTest 113/113**, Dev-App 16:58. **Kein Commit/Push.**

**Wassereinschlag portiert (2026-10-16):** `bhCheckL2Water` (hitchk.c:5561) = `RoomWorld::l2_water` auf
denselben Flaechen/derselben Reihenfolge wie `bhCheckWater`; `bhSetWaterSplash4` (pwksub.c:2643) =
`effect_detail::water_splash4` (im Effektpass ueber `submit_template`, sonst direkt). Gebunden und damit
geschlossen: Kampfkern Schuss (weapon.c:872-958, id 15 + Splash4, Waffe 11 mit 32 Saetzen) und Messer
(:1035-1069, zwei id-11-Saetze), `bhEff130`, EN01 `water_line` (4 Stellen), T3 `water_line`. Mitbehoben:
T3 uebergab einen **Endpunkt** statt des Vektors (0,20,0) (zonzon.c:921); der Kampfkern zog `rand()` aus der
libc statt aus dem EE-Generator (auch player.c:4950). `RECVX_CB_EFFECT.sys_ef_keep_ax`: Wasserzweige
schreiben `sys->ef`, ax bleibt Vorlagenwert. Tests `object_water_wave` (RM_3060/3210/7060/9130 echte
Flaechen) und `combat_player_state test_water_impact`; vier Gegenproben rot. `./build.sh --test`:
**CTest 113/113**, `verify_combat` gruen, Dev-App 18:56. Nicht belegt: Waffe 11 (32 Saetze) nur im Code,
kein Bild-/Durchspielnachweis im Wasserraum. Befund: Skriptbefehl 0x7c aendert nur die Event-Flaechenliste,
nicht `floor_sounds_` (Quelle von bhCheckWater/L2Water) — moeglicher zweiter Besitzer, ungeprueft.
**Kein Commit/Push.**

**Bodenflaechentabelle rom->flrp vereinheitlicht (2026-10-16):** Der 0x7c-Befund war echt. `RoomWorld` hielt
drei Kopien (`floor_sounds_`, `enemy_areas_`, `effect_water_room_`); 0x7c (bhFlrAtariSet2) erreichte keine,
0x0c nicht die Effekt-/Gegnerboden-Kopie (bhCheckFloorEnemy, Effektboden). Jetzt eine Tabelle
`room_floors_`, `enemy_areas()` als Sicht, 0x7c an die Welt weitergeleitet. Realfall **RM_0070**: das
Init-Skript setzt bei Flag 1/0x74 den Bodenklang von Flaeche 2 auf 2 — vorher blieb nativ Klang 1.
Ebenso RM_4020 (Flaeche 2 -> Klang 3, nicht einzeln geprueft). Test `world_floor_table` (neu), zwei
Gegenproben rot. `./build.sh --test`: **CTest 114/114**, Dev-App aktualisiert. Weiter getrennt: die
Eventsicht `EventVm::floors_` (beide Schreiber spiegeln jetzt), `activate_enemy_spawn` ignoriert sys->mflrp
(ungeprueft). **Kein Commit/Push.**

**Waffen 13/18/20 und Zielfernrohr (2026-10-16):** Befund: `WpnTab.flg 0x20` schickt Waffe **13** (bisher als
"unterstuetzt" gefuehrt) und 18 nach mode2 71 `bhCPM2_act_scp` (playpch2.c:13) — dort stand im Kern nur eine
Luecke, Waffe 13 schoss nativ also nie. Portiert: Ich-Perspektive (gm 0x80000/st 0x800000, pt_flg 0x1 aus),
Schuss (13: Gun-Atari ab owP[5]; 18: Rakete bhEff130 type 4, ef_flg 0x2), 30-Schritt-Rueckstoss, Zoom
cam.ppers 3640 auf pad 0x80, Zielen ueber pad_dx/pad_dy, Freigabe nach ct0 > 5. Waffe 20: bhSetGunFire-Funken
bhEff027 (pwksub.c:3126). CombatController gibt 18/20 frei. Mitbehoben: sys->pad_dx/pad_dy jetzt ein
Besitzer (Runtime, nur mit sp_flg 0x20 wie bhSysCallPad); pt_flg und sys->ef_flg im Kampf gebunden (vorher
Luecke bzw. immer 0, das Raketen-Nachladetor player.c fuer 18 griff nie). Tests `combat_player_state`
test_scope/test_linear_launcher, drei Gegenproben rot; `./build.sh --test` **CTest 114/114**, `verify_combat
--unit/--runtime` gruen. Nicht belegt: kein Bild der Ich-Perspektive in der Runtime, kein natuerlicher Erwerb
von 13/18/20 durchgespielt. Offen/Befund: der Kampf-Pad bildet "vorwaerts" nur auf 0x20 ab, das Original
setzt 0x1|0x20 (pad.c pad_tab: Steuerkreuz oben = Bit 0 und Bit 5) — wirkt auf bhControlPlayerPad
(player.c:1818); Analogstick pad_ax/pad_ay fehlt im Host. **Kein Commit/Push.**

**plp->stflg im Kampf vereinheitlicht (2026-10-16):** Bei der Pad-Pruefung gefunden: die Kampfsicht hielt
plp->stflg als eigene Kopie. Das Zielbit 0x400 (player.c:1789), das en05/en09/en17 lesen, erreichte die
Gegner nie; das Item-Kisten-Tor 0x10000 sperrte das Zielen nicht. Jetzt liest/schreibt der CombatController
Eventbank 14 um jeden Kernaufruf. Kampf-Pad: Steuerkreuz oben/unten = 0x21/0x42 wie pad_tab (Wirkung im Host
nicht beobachtbar, s. Ledger). Test `combat_player_work` (neu), Gegenprobe rot; `./build.sh --test`
**CTest 115/115**, `verify_combat --runtime` gruen. OFFEN: plp->flg (Bank 13) ist weiter privat in der
Kampfsicht. **Kein Commit/Push.**

**Waffe 11 und plp->flg-Pruefung (2026-10-16):** `bhSetGunFire` Waffe 11 (pwksub.c:2981) portiert: fuenf
bhEff014- und acht bhEff002-Saetze ueber sys->ef mit der Vorlagenvererbung des Originals (type 3 nimmt die Lage
von Satz 4). CombatController gibt 11 frei. Test `test_weapon11_fire`, zwei Gegenproben rot; `./build.sh --test`
**CTest 115/115**, `verify_combat` gruen. plp->flg: geprueft und **bewusst nicht** an Bank 13 gebunden — kein
externer Leser eines Kampf-flg-Bits ausser dem Passtor 0x10000, und eine Bindung wuerde nach einem Menue/Event
(Kern wird verworfen) 0x10000 stehen lassen und den Bewegungspfad sperren. Voraussetzung waere, den Kern bei
Sperren zu behalten wie das Original den Spielerwork (Ledger `player.c:BH_PWORK.flg:Kampf`). **Kein Commit/Push.**

**Bogengewehr (Waffen 10/19) portiert (2026-10-16):** `bhEff013` (effsub1.c:3352) in `native/effect_eff013.cpp`
mit Flug, Zielverfolgung, Wand-/Gegner-/Wassertreffer, Stecken (121 Paesse) und Sprengpfeil-Explosion; gezeichnet
ueber bhDrawPolEffect mit `efm[13].objP[mdlver]` (vierter Modellblock aus SYSTEM 0001). Dabei geschlossen:
**`bhCheckCapCol2Capsule`** (weapon.c:1496, CPCL-Gelenkkapseln) war im Kern nur eine Luecke — **auch Waffe 13
konnte deshalb keinen Gegner treffen**. `bhLinkBlood` ist im Original leer, EN01 meldete es faelschlich als
Luecke. Neu: `recvx_combat_check_gun_atari`/`recvx_combat_search_near_enemy` fuer Effekte, bhEffDmy 139..142,
Laufzeitanhaengen eines Effekts an einen Gegnerwork. CombatController gibt 10/19 frei. Test
`effect_eff013_world` (RM_0080, drei Faelle, drei Gegenproben rot); `./build.sh --test` **CTest 116/116**,
`verify_combat` gruen, Dev-App 20:29. Nicht belegt: Bild, Wasserraum, natuerlicher Erwerb. **Kein Commit/Push.**

**Semantikabgleich Treffer (2026-10-16):** `bhCheckGunAtari` (weapon.c:473-870) und `bhCheckKnifeAtari`
(:1002-1146) Zeile fuer Zeile gegen den Kern: Filter, Kegeltest, Gelenkkapsel-/Kapselzweig, Sichtwand, naechster
Treffer, Durchschlagszweig (nimmt wie das Original px/nr des bisher naechsten), Gelenkwahl, Schadensstufen und die
Gelenkkoordinaten-Rueckgabe stimmen ueberein. Einzige Restluecke dort: Wandzerstoerung attr 0x1000000
(:552-566, bhSetExplosion/obwp[prm3].mdflg/ahtp->flg) ist im Spielerkampf ungebunden — Befund: nur `bhObj012`
(objitm.c:2069) erzeugt solche Waende, und **kein Raum fuehrt ein Typ-12-Objekt** (alle RDX geprueft); bhObj012 ist
nativ nicht portiert. **Kein Commit/Push.**

**Spieler-Bewegungsmischung (SetMtn*Hokan) portiert (2026-10-16):** plp->mlwP->objP ist jetzt dauerhaft
(`RoomWorld::player_obj_`); Aktion/Event, Kampf, Schaden, Griff und Schieben schreiben den Knotenteil von
bhSetMotion mit dem Stand vor dem Vorschub ueber das vorhandene `set_motion`, alle Abtaststellen legen objP unter
ihre Bearbeitungen. Motionwechsel blenden wie im Original (Protokoll: Lauf->Stand ueber 5 Paesse, Ziehen/Wegstecken
ueber 4/12); die gezeigte Pose ist das Bild vor dem Vorschub (vorher eins voraus). Kampf-Luecke `SetMtn*Hokan`
und Push-Bit HOKAN entfallen. Test `player_motion_blend` (neu, zwei Gegenproben rot). `./build.sh --test`
**CTest 117/117**, `verify_combat` gruen (gleiche 6 Schuesse/4 Treffer; Aufschaltwinkel 3521 -> 3366, weil der
Kern jetzt die objP-Pose liest), `verify_push`, `verify_stairs`, `verify_lighter` gruen. Rot, aber mit
abgeschalteter Mischung identisch rot gemessen: `verify_events` (window sequence), `verify_enemy_ai` (Hunde-
Todesbiss bei Bild 3000); `verify_crate_edge` erwartet veraltet Opcode 0x8b als offen (seit 7023885 gebunden).
Offen: Treppe, Gameover, skriptgefuehrte Spielermotion fuehren objP noch nicht. Dev-App 21:10. **Kein Commit/Push.**

**2026-10-16, drei gemeldete Fehler (RM_1050 / Inventar), Ursachen belegt und behoben:**
(1) *Ameisen-Juwel, blaue Flaeche:* `RoomWorld::set_fixed_cut` schlug die Nahgrenze mit dem vorigen Raumschnitt
nach (Schnitt 1: 20/20000) statt mit `cam.ncut = cno` (cut.c:486 vor bhChangeViewClipRM :537); die Vitrine lag
vor der Nahgrenze, es blieb die Hintergrundfarbe. Jetzt Schnitt 5 -> 2/20000, Bild zeigt die Vitrine.
(2) *Steve-Schalter ohne Bilder:* die Meldungsbits von `sys->st_flg` (0x1000 Seite fertig, 0x4000, 0x8000, 0x800,
0x400, 0x80000) lebten nur im `MessageDialog`; das Skript (Ereignis 34, @2ba2) wartet per bhFlagCk Typ 5 auf
0x1000 und dessen Fall beim Umblaettern von Meldung 24 und setzt erst dann bhCamSet 4 (Schaltertafel mit den
sechs Waffenbildern). `EventVm` spiegelt `MessageDialog::status()` jetzt nach jedem Pass in flags[5]; Kamera 4
und die Bilder erscheinen. Dabei festgestellt: der Monitor (bhEff111, Rendertextur aus Schnitt 6, nur RM_1050)
ist nicht gebunden und bleibt schwarz (Ledger `effsub1.c:bhEff111`, missing).
(3) *Inventartext vor der UI:* GetItem laeuft im Original erst im stehenden Aufnahmebildschirm (sub1.c:3452,
StatusMain case 8 nach SpriteH ohne statusflg 0x40); `begin_pickup` entschied sofort, die Frage 157 stand an
ihrer festen Position (mes_spos[1] 44/342) ueber den einfahrenden Sprites. Die Entscheidung wartet jetzt auf den
Statustask und das Ende der Einfahrt. Der Itemname selbst faehrt mit cen_pos[6] mit (unveraendert).
Tests: `native_ground_world` (Clip), `event_lifecycle` (RM_1050 Ereignis 34 bis bhCamSet 4),
`native-gameplay-bindings` (RM_0000 Messer); alle drei mit Gegenprobe rot. Headless-Bilder: Vitrine, Schaltertafel.

**2026-10-16, RM_0080 Rolltor-Zombies (Nutzerbefund "bewegende Dummies"):** Der Schalter (Interaktion 8, nach
geloester Sperre `ev_flg 110`) startet Ereignisse 5-11/40/41; 7 und 8 sind die zwei Zombies: Common 25 Lauf,
`Common_controll 53` (@132a/@1360), dann `Sub_controll 0x8f` Modus 1. Die native VM kannte Befehl 53 nicht,
der Task blieb stehen ("unimplemented Common_controll command 53"), die Zombies blieben in mode0 5. Ursache
ist das Parsing: die 0x69-Laengentabelle war ein Teilstand. Jetzt `EventVm::common_controll_length` mit allen
49 Originalbefehlen aus event.c:9963-12710 (Zeigervorschub je Fall, Selektorabhaengigkeit 24/25/34/35,
Achsenabhaengigkeit 10/22); nativ gebunden 0, 53, 10/22, 43, 35 Selektor 1; benannte Luecken 51/52
(`sys->evt_posno`, Opcodes 0xb3/0xb4 ohne Besitzer) und 35 Selektor 0/Spieler. Test `event_lifecycle`
(Tabelle gegen die Originalvorschuebe; RM_0080 Rolltor bis beide Zombies im Brain laufen; Gegenprobe rot).
Headless-Nachweis (`--enemy-ai on`, Snapshot mit ev_flg 110): Bild 300 Rolltor oeffnet, 500 beide laufen
auf die Spielerin zu (mtn 125/117), 800 Griff (mode 1/0/5/1). CTest 117/117.

**2026-10-17, neun Nutzerbefunde (Diagnose durch Unteragenten, Fixes geprueft):** (1) Leerlauf->Laufen:
pad_old nach dem Pass (pad.c:146), `tools/verify_idle_walk.py`. (2) RM_4050 Tasche: `bhCheckSubPack`.
(3) RM_3091 Wurm: `njPtclDrawPolygon` in Bildschirmpunkten. (4) RM_4020 Kranschatten: `bhObj008`.
(5) Leiter im ersten Uebergangsbild: Tuermodell nur ohne Status 0x40 (door.c:395). (6) RM_1080
Lugertuer-Kamera: gm_flg-Maske beim Raumladen (system.c:185); Lugertuer ohne Einsetzfrage und RM_3030
Kartenleser offen sind originalgetreu. (7) RM_3140 Kameraevent: `bhEff137` (Monitoranzeige bhDraw137
offen). (8) RM_1050 Monitor: `bhEff111` + Rendertextur (Headless-Bild zeigt Steve). (9) Steve: bhSetDispObj
mdflg 0x1, bhEneRenderSet am Work, Subplayer-Motionswechsel nach Original. Details im Ledger. CTest 117/117,
alle Gegenproben rot.

**2026-10-17, Nachtrag:** bhDraw137 (RM_3140 Monitoranzeige) portiert; der gemeinsame 2D-Rasterer der
ef_fnc-Anzeigen rechnet Texel normiert auf 256 (GS-ST, ps2_NaDraw2D.c) -- das korrigiert auch die Kran-Anzeige
bhDraw134 (Gruppe 44 ist 128x128, vorher nur ein Ausschnitt). gm_flg 0x20 (Schnittwechsel) hat jetzt Schreiber
und die bhCheckCut-Loeschung; der RM_1050-Monitor frischt bei Schnittwechseln auf. Offen bleibt nur die
PS2-Framebufferkopie (PS2_Render_Tex_Sub), die ohne GS-Nachbildung nicht bestimmbar ist.

**2026-10-17, weiter offene (Arbeitsbaum gebaut 09:01, CTest 117/117; Dev-App noch 02:34, nicht
ausgeliefert):** (1) Opcodes 0xb3 `bhEtcAtariEnePosSet` (event.c:7794) und 0xb4 (event.c:7865) mit
`sys->evt_posno` (Snapshot, Raumwechsel) und rom->posp; Common 51/52 gebunden. Test `event_lifecycle`
(RM_0110). (2) Skriptgefuehrte Spielermotion: `pl_smove08` (Common 24/25/34 Selektor 0) fuehrt objP ueber
`set_motion`; Test `player_motion_blend`. (3) `verify_events` gruen: cb_flg 0x4000 verbraucht GetItem
selbst (sub1.c:3830-3857), Replay-Takt fuer Meldung 158. (4) `verify_enemy_ai`: die Erwartung mode0 6 beim
Hundetod hatte keine Originalquelle (EN07 setzt nur mode0 4, Bisse klemmen hp auf 1; der Tod kommt aus dem
Sprungangriff, Schadensmodus 2, hp 6 -> 0 -> -6 bei Bild ~2420/~2497). Portiert: player.c:1405 (hp < 0 ->
mode0 3, mnwP = mnwPb) und `bhCPM0_die` (Fallmotion 25/27 mit objP ueber set_motion, Bodenlage, Stimme 1025,
Game-Over-Start beim Bildwechsel ueber ts_flg 0x4000, Liegemotion 26/28; laeuft in gov_md1 0 weiter).
Dabei zwei Host-Fehler: der Todesmelder las plp->flg 0x2 in jedem Modus (nur bhCPM0_enedie/mode0 6 liest es),
und ts_flg 0x4000 fehlte im nativen Raumladen. Der RM_0050-Teil des Pruefers war mit dem Baseline-Binary
ebenfalls rot (feste Lockposition -142, rm_flg 9 wuerfelt die Falle) und folgt jetzt dem Wurf.
`verify_enemy_ai` gruen; verify_events/event_freeze/stairs/push/lighter/continuity/doors/idle_walk gruen;
CTest 117/117; Gegenproben rot (kein Uebergang -> kein Game Over; altes Flag -> enemy_work rot). Dev-App 09:53.

**2026-10-17, Treppe auf objP:** die sechs Stufenroutinen fuehren den bhSetMotion-Knotenteil (Mischung,
hokan_count-Abzug) jetzt auf plp->mlwP->objP, auch im Abschlussbild; bhCalcFixOffset liest objP dieses
Passes gegen owP des letzten bhCalcModel (vorher ungemischte Rekonstruktion). hokan geht an act_std zurueck.
Lueckenbit HOKAN entfaellt (0x280 -> 0x200). `verify_stairs` mit POSETRACE-Pruefung gruen, Endlagen wie vorher;
`core_continuity` mit Knotenteil-/owP-Fixture (Gegenprobe rot). CTest 117/117, Nachbarlaeufe gruen. Dev-App.

**2026-10-17, Todesszene (gov_md1 10) auf objP:** bhSetMotion-Knotenteil je Pass beim Spielerbesitzer,
frm_no schiebt auch waehrend der Mischung (vorher gehalten), Eintritt mit mnwP = mnwPb, bhDrawGameOver zeichnet
den objP-Baum. Test `gameover_original` mit SYSTEM/0010 (Gegenprobe rot), CTest 117/117, alle Prueflaeufe gruen,
Dev-App 10:23. Offen: Laufzeitmessung der Selbstzerstoerungsszene (kein headless erreichbarer Ausloeser).

**2026-10-17, Task 7 in gov_md1 0:** das Spielbild laeuft in den 70 Bildern vor der Game-Over-Maske weiter
(main.c:197, nur Task 8 ueber ts_flg 0x100 gesperrt), der Game-Over-Pass folgt am Bildende. Dabei: NG00 mode3 7
ist im Original eine Leerstelle (keine Luecke), bhCPM0_enedie loescht ts_flg 0x4000. `verify_enemy_ai` prueft
die Hundebewegung in md1 0 (Gegenprobe rot); alle Prueflaeufe gruen, CTest 117/117, Dev-App 10:56.

**2026-10-17, Griff-Tod RM_0080 geprueft:** Spieler mode0 6 / mode3 7 mit Motion 412 ist PlyDG00 case 7, Zombie 1
in MV16 -- originalgetreu. Fehler war der native Game-Over-Start: in mode0 6 schon bei hp < 0 statt mit plp->flg 0x2
am Ende der Todesmotion (player.c:6711, en01.c:7748); jetzt Start bei Bild ~1072 statt ~1008. `verify_enemy_ai`
death-1050 (Gegenprobe Baseline rot), alle Prueflaeufe gruen, CTest 117/117, Dev-App 13:19.

**2026-10-17, Treppe mtn_attr:** plp->mtn_attr hat im Original keinen Leser (nur Motion.c:76 schreibt es); der
Knotenteil fuehrt es mit, das Lueckenbit 0x200 entfaellt, die Stufenaktionen melden keine offenen Dienste mehr.
CTest 117/117, Dev-App 14:00.

**2026-10-17, Selbstzerstoerungsszene gemessen:** `tools/verify_self_destruct.py` (Diagnose-Snapshot RM_4040 mit
ev_flg 257/35 gesetzt, 259/36 frei; Untersuchung von Eintrag 1 loest scd1 @0452 aus: EvtStart 5 + 0xcb Modus 1; die
Tuer in Event 5 fuehrt nach RM_3230, deren Fluchtszene per SELECT uebersprungen wird, dann RM_3091). Gemessen: evt_tim
16202 bei Bild 1100, Ablauf bei Bild 9200 (ev_flg 67 frei, gm_flg 0x400, ts_flg 0x4000 frei, sp_flg 0x20 -> kein
Spielerpass mehr, player.c:1365), md1 0 71 Paesse, md1 10 ab Bild 9273 mit genau einem Knotenteil je Pass: Motion 4
(20 Bilder) ohne Mischung, fuenfte Explosion in Pass 96 -> Motion 25 ab frm_no 0 mit hokan 8..1 bei schiebendem frm_no,
Umbruch nach 40 Bildern -> Motion 26 ab Pass 137, md1 1 nach Pass 150; mit ply_hp 100 Motion 7 (zweite PlMtnAct-Zeile).
Drei native Fehler behoben: der Zaehler evt_tmd/evt_tim ging beim Raumwechsel verloren (system.c:178 laesst ihn stehen;
`inherit_progress`), bhCheckEvtTimer verlangte ts_flg 0x4000 nicht (player.c:1272), die Todesszene nahm wpntp/dmlvl 0/0
aus begin() statt exp0 beim Eintritt (gameover.c:289) und schrieb plp->px/py/pz/ay nicht (:303-308). `--state` traegt
jetzt `event_deadline_mode/ticks`. Tests game_test/gameover_original (Gegenproben rot), CTest 117/117,
verify_self_destruct/enemy_ai/stairs/events gruen. Dev-App 14:43.

**2026-10-17, RM_3230 Fluchtszene:** kein Enginefehler. Die Szene laeuft nativ durch, sobald der Gegnerpass laeuft:
Event 2 (rel 0a22) haelt EN19 (Art 1 Index 1) per Common_controll 24 (event.c:11299-11500: mnwP = sys->rmthp, mtn_no 1,
mtn_add 0x10000, mode0 5) 569 Bilder auf frm_no 0, danach warten die Tasks 0/1/2/4/5 mit bhFrameCheck (event.c:3237,
Befehl 0x38) auf frm_no 35/80/463; den Vortrieb liefert bhEne19_Event (en19.c, bhEne19_Mode0[5] -> bhSetMotion) im
Gegnerpass bhControlEnemy, der nur bei sys->sp_flg 0x2 laeuft (eneset.c:352, nativ enemy_dispatch.c:144). Der alte
Befund (frm_no 0, alle Tasks in 0x38) ist die Headless-/Replay-Vorgabe `--enemy-ai off` (runtime.cpp, world_enemies.cpp
maskiert sp_flg 0x2), die `verify_self_destruct.py` nicht ueberschrieb. Gemessen mit `--enemy-ai on --enemy-grab on`:
Raumeintritt RM_3230 Bild 450, Schleife bis 1018, Warten 35/80/463 enden bei 1053/1098/1481 (Skriptarithmetik
450+569+463), EN19 frm_no 82 bei Bild 1100 und 464 bei 1500, EEVT der Tasks 1/2/4 bei 1482, 5/7 bei 1572, Tuer ab 1573,
RM_3091 bei Bild 1869 ohne Eingabe (mit Tuer-Skip-Confirm 1580). Pruefer ohne SELECT-Zeile, mit Szenenmessung und
`--enemy-ai on`; Ablauf jetzt bei Bild 9956, md1 10 ab 10029, Motion 4/25/26 wie vorher (Gegenprobe ohne `--enemy-ai on`
rot: frm_no 0). Native Quellen unveraendert; `./build.sh --test` CTest 117/117, verify_self_destruct/enemy_ai/events/
stairs gruen, Dev-App 14:59. **2026-10-17, bhSetExplosionEffect:** weapon.c:1812 (23 Art 1 Groesse 6 an pos - 4 * cam.v, zwei 24 mit
Zufallsfolge sx/ay/ax, Versatz (0,0,-2) gedreht) war in `native/effect_sprites.cpp` bereits portiert, aber ohne
Weltbesitzer: `RoomWorld::explode_effect` (cam.v aus effect_context, unter cam.flg 0x40 aus der direkt
geschriebenen Lage) legt die Saetze an, `GameOver::death_blast` bindet gameover.c:539 (gov_etb, sechs Aufrufe bei
gov_ct 40/54/68/82/96/110, Test gameover_original). Dazu die Szene selbst: gameover.c:278 bhClearEffect
(`RoomWorld::clear_effects`), :296-297 pt_flg 0x15, :301-302 sp_flg 0x6C (Task 7 laeuft: `world->tick()` in md1 10
vor Task 14), :313-327 Kamera cam.flg 0x40 0/10/35 pers 11832 (`set_monitor_camera`, effect_context liest sie),
game.c:318 Effektzeichnen in das Bild der Todesszene (`death_scene_append`, Szene des Raumbesitzers uebernommen),
:313 txp[0] Spielertexturen (`death_body_textures`), :567 pt_flg 0. Messung `verify_self_destruct`: eff[512] bei
gov_ct 10 leer (vorher Raumeffekt 122 belegt), bei gov_ct 100 Saetze 23/24/25 = 4/18/86, keine bhEff23/24/25- oder
bhSetExplosionEffect-Luecke im Protokoll, Endbilder mit 2582/30020 warmen Pixeln bei gov_ct 45/100 (Vorherstand
0/0 mit dem Binary 14:42). Gegenprobe Bindung entfernt: rot bei den Effektsaetzen, zurueck gruen. `./build.sh --test`
CTest 117/117, verify_self_destruct/enemy_ai/events/stairs/combat gruen, Dev-App 15:33. Keine neue Effektprozedur
noetig (23/24/25 waren done). **2026-10-17, Todesszene Spielerkoerper/Nebel:** Der Koerper stand in md1 10 an der Kameralage statt am Ursprung:
native/gameover.cpp verschob die Koerperecken wie das cmat-Netz um (0,10,35), game.c:357 bhPutModel zeichnet aber den
bhCalcModel-Baum (MdlPut.c:240-264, px/py/pz 0, ay 35499) im Weltraum unter cam.mtx (camera.c:28-37, cmat * T(0,-10,-35));
Versatz entfernt. Messung 640x480 bei gov_ct 36: vorher Fragment x 48..134 / Zeilen 0..31 (oben beschnitten), nachher
x 278..356 / Zeilen 141..394 (Fuesse 394 gegen 383.5 fuer y 0, Kopf 141 = 16.3 Einheiten). Nebel: kein Nebel im Original
(gameover.c:242 njFogDisable, :323 gm_flg 0x200000 -> sync.c:143 bhControlEOR njFogDisable je Bild); nativ laeuft der
EOR-Nebelteil in md1 10 jetzt mit (`world->end_of_render()`), fog.enabled false bei gov_ct 10 (vorher true). Tests
gameover_original (Koerperpixel-Differenz mit/ohne PlayerAsset; Gegenprobe Versatz zurueck rot), CTest 117/117,
verify_self_destruct/enemy_ai/events/stairs/combat gruen, Dev-App 15:52. **2026-10-17, Event-Explosion und RM_3230 Event 3:** Skriptbefehl 0xc8 = event.c:8722 bhEffBakuDrmSet (bhScenarioJmpT[200],
Vorzeichenbyte + 3 u16/100) war in der nativen VM unimplementiert (Task blockierte mit "unimplemented opcode 0xc8"); jetzt
native/events.cpp case 0xc8 (8 Bytes) + RoomWorld::bind op 0xc8 -> explode_effect (weapon.c:1812, cam.v aus effect_context).
Nutzer laut Disassembler ueber alle 205 Raumskripte (Laengentabelle vervollstaendigt, keine Abbrueche): RM_5010 Ereignis 8 @09ea
(Katapulthebel mit Tyrant in TY_BR0_PINCH/hp < 0, scd1 @026c = ELSE von IF rm_flg 0x4, en19.c:1068) und RM_9370 Ereignis 5
(10 Aufrufe). Headless RM_5010 (Snapshot ev_flg 269, 3700 Bilder bis ct_pinch 0, zwei Hebelzuege): vorher Task 8 @0x9ea
blockiert und Szene eingefroren, nachher @0x9fb bei Bild 5360 mit 2x23/3x24/14x25 im Pool, Szene endet bei 5600, einzige
Luecke der vorbestehende bhEasySESet owP[0]. RM_3230 Event 3: Originalverhalten, kein Fix. Ereignis 1 pausiert enep[2]
(bhSubpl id 96, subpl.c:357 mv00_subpl0_1 ohne mode3-4-Zweig) per bhMotionPauseSet 0 sobald frm_no >= 123; bhEwhile
(event.c:9147) gibt beim Schleifenende 0 zurueck, der naechste Gegnerpass (bhSysCallGame vor bhSysCallEvent, main.c:195)
schiebt noch um 0x8000 auf 123.5, danach steht der Work; 124<<16 wird nie erreicht, der Raumwechsel beendet die Task.
Nativ gemessen: 123.0 bei Bild 509, 123.5/Schleife verlassen bei 510, mode3 4 ab 511, Task 3 bei 0afd bis RM_3091.
Tests event_lifecycle (RM_5010 Ereignis 8 mit 23/24, Gegenprobe ohne Bindung pending/leer; RM_3230 Haltepunkt 123.5),
CTest 117/117, verify_self_destruct/enemy_ai/events/stairs/event_freeze gruen, Dev-App 16:58. **2026-10-17, Event-VM 0x86/Obj_controll:** 0x86 = event.c:6117 bhEasySESet
(16 Bytes, Art 0 plp/1 ene[enep.wrk_no]/2 obwp/3 itwp, Knoten v7) ruft sdfunc.c:1279 PlayGameSe4Event; Typ 7 =
CallNativeEventSe Mode 1 liest pPos nie (sdfunc.c:1861). Nativ verlangte auch Typ 7 eine owP-Lage und verwarf den Ruf;
RM_5010 Ereignis 8 @0908 nennt obwp[0] (Waffe, room.c:210 Raumobjekte erst ab 4, nativ kein Waffen-owP) -> Luecke
`bhEasySESet owP[0]` und fehlender Ton. Jetzt Typ 7 ohne Lageaufloesung (native/world.cpp). 2002 Aufrufe in 205 Raeumen,
Art 2 nur mit Typ 7, obwp[0] nur RM_3070 @06fc/RM_5010 @0908. Headless RM_5010 (Aufstellung wie 0xc8): vorher 1 Luecke,
Bank-2-Folge in Ereignis 8 seq2/3/7/11/8; nachher 0 Luecken, seq2/3/6/7/11/8, 0 blockiert. 0x66 = event.c:9708
bhObjCtr/Obj_controll: nur Faelle 1 (2 Bytes) und 20 (4 Bytes), Default ohne Vorschub und ohne Wirkung, Rueckgabe 1 -
das Kommandobyte laeuft als naechster Opcode (event.c:13047). `EventVm::obj_controll_length` + Default `advance(1)`
statt Ausnahme. Einziger Nicht-1/20-Fall aller Raeume: RM_3020 Ereignis 6 Kommando 12 -> 0x0c bhFlrAtariSet @040f ->
bhEnd @0413 (Task bleibt dort wie im Original); kein Skript in RM_3020 startet Ereignis 6. Tests event_lifecycle
(Obj_controll-Laengentabelle, RM_3020 Ereignis 6 explizit ohne Fehler bei 0x413, RM_5010 Ereignis 8 ohne
bhEasySESet-Luecke mit Bank 2 seq6 genau einmal; beide Gegenproben rot/gruen), CTest 117/117,
verify_events/event_freeze/enemy_ai/self_destruct/stairs/doors gruen, Dev-App 17:37.
**2026-10-17, RM_9370 Ereignis 5 / Licht der Todesszene:** Ereignis 5 hat keinen eigenen Ausloeser: scd0 @0054 loescht
ev_flg 458, scd1 @0430 startet bei 458 frei mit allen Szenentasks @044c Ebene 5 Ereignis 5 (Eintritt aus RM_9091 Ereignis 2
@036e, Tuer 09/25); es wartet mit bhCamCheck (event.c:4881) auf die Kameras von Ereignis 2 und mit bhFrameCheck auf
enep[0/1] (bhSubpl 92/97, --enemy-ai on). Headless lief es schon durch (10x 0xc8, bhEvtEnd @23f2), liess aber alle 22
0x6d = event.c:5028 bhCyodanSet (Treffer, bhSetEffParticle effsub3.c:911) pending; jetzt RoomWorld::bind op 0x6d ->
EffectParticles::spawn. Nachher: 10x 0xc8, 22x 0x6d, Ende ~Bild 4760, blocked 0, keine Luecke, lebende 23/24/25 (neues
Zustandsfeld effects[].flg) Bild 3620 7/12/110, 3800 0, 4700 4/10/48. Test event_lifecycle RM_9370 (natuerlicher
scd1-Start, genau 10 Anforderungen/10 Feuerbaelle 23 Art 1, keine pending Befehle von Ereignis 5, Partikel > 0; Gegenproben
ohne 0xc8-Bindung bzw. ohne 0x6d-Bindung rot/gruen); Testgeruest laedt Diskwechsel/Raumbank jetzt aus RoomIdentity wie
runtime.cpp. verify_events prueft 0x6d jetzt als gebunden. Todesszene: md1 10 setzt gm_flg 0x80 (gameover.c:323), daher
bhSetHalfLight = bhSetEasyDirLight(0.6) (light.c:992/279, MdlPut.c:33); der Koerper geht jetzt ueber death_scene_append
durch RoomWorld::append (Kategorie 1, kein zweiter Lichtweg) statt der Renderer-Hostschattierung; Koerper gov_ct 36
Mittel RGB 21.0/14.2/17.0 -> 16.8/11.5/14.1, Licht grau 0.2..0.657. Netz: njDrawTriangle3D ohne Licht (Eckfarbe -1),
jetzt world_lit ohne Hostschattierung (md1 2 Mittel R 18.1 -> 24.9). Test gameover_original (RM_3091-Besitzer; Gegenprobe
rot/gruen). CTest 117/117, verify_events/event_freeze/enemy_ai/self_destruct/stairs/doors gruen, Dev-App 18:22.
**2026-10-17, bhEffClearEvt/bhEff085:** 0xca = event.c:8801 bhEffClearEvt (2 Bytes) -> effect.c:159 bhClearEventEffect,
eine selektive Freigabe (nicht bhClearEffect): bhClrEff_YT (effect.c:166: 5/11/113/114/136/159/225/398, dazu 22/24/25/27 mit
Hilfssatz exp0), bhClrEff_RY (effsub3.c:117: 301, Partikelwurzel 302 mit exp0/exp1), bhClrEff_YG (effsub2.c:4793:
250/253/256/257/272/296-298, 254/260 mit exp0), je nur flg & 1. Jetzt RoomWorld::bind op 0xca -> RoomWorld::clear_event_effects
(+ EffectParticles::clear_event). 51 Aufrufe in 27 Raeumen. Test event_lifecycle RM_9360 Ereignis 14 (Eintritt mit ev_flg 458,
Aufstellung Explosion + Treffer): um 0xca 3x24/31x25/2x302 und 29 Raumeffekte 159 frei, 33 Hilfssaetze frei, 23 und alles andere
unveraendert; Gegenprobe rot/gruen. Headless RM_9370 -> RM_9360 (7000 Bilder): pending 1 -> 0. 0x91 bhEffModeSet war schon
gebunden und scheiterte nur an der fehlenden Prozedur: bhEff085 (effsub6.c:18) jetzt handportiert - zeichnet nie, mode1 1 tauscht
ene[rom->effp[type].lkno].mlwP->texP->textures[3].texaddr gegen sys->ef_tn[85] (Sicherung in exp0, alle Werke derselben
et_lp-Liste), mode1 2 stellt her. In RM_9370 laufen 0x91 2 und 0 (Ereignis 0 @07ee/@07f2) im selben Schedulerpass vor dem
naechsten bhControlEffect, daher bleibt der Tausch wie im Original stehen. Tests effect_eff085 (neu) und event_lifecycle RM_9370
(1 @Pass 3646, 2/0 @4551, Texturtausch geteilt mit ef_tn[85]); Gegenprobe rot/gruen. Headless RM_9370 4700 Bilder: pending 7 -> 4
(nur 0x86). CTest 118/118, verify_events/event_freeze/enemy_ai/self_destruct/stairs/doors gruen, Dev-App 19:02.
**2026-10-17, 0x86 Raumsoundbank:** bhEasySESet (event.c:6117) gibt nach PlayGameSe4Event immer 1 zurueck (event.c:6205);
Typ 7 -> CallNativeEventSe (sdfunc.c:1830) -> ExPlayMidi. MODHSYN.IRX (nur gelesen): Programmwechsel .text 0x75c behaelt kein
Programm fuer Eintrag 0xffffffff / Nummer > letzter Index / 0 Splits, Note-On 0x5f90 startet dann keine Stimme; die Splitschleife
(0x6098) vergleicht Tastengrenzen &0x7f, ungedeckte Taste = keine Stimme. Die Sequenz laeuft weiter. Kein Formatfehler der Basis:
MULTSPQ2/0086 Bank 2 Programme 23/24/27..30 sind leere Eintraege, Programm 11 hat nur Taste 60 (seq 11 spielt 60 und 61). Nativ
warf decode_system_sound fuer MIDI-Noten und brach die ganze Sequenz ab; jetzt stumme Note (native/audio.cpp), und op 0x86 gilt
immer als ausgefuehrt, ein nicht bedienter Ruf ist audio_unavailable 'bhEasySESet type T slot S SeNo N' (native/world.cpp).
Messung aller 205 Raumskripte gegen das Raumpaket Fall 0: 9 Typ-7-Aufrufe betroffen (RM_3020/3130/6080x2/7100/9370x4), nachher 0;
RM_3150 @0cae/@0cc8 (seq 116/117) liegen hinter der Tabelle von Paket Fall 0 (der Raum setzt per 0xb6 Fall 1, Paket 3151 fehlt
im IDX; Bankwahl dort offen) und waeren jetzt Luecke statt pending. Tests audio_movie und
event_lifecycle (RM_9370 Ereignis 11), Gegenproben rot/gruen. Headless RM_9370 4700 Bilder: pending 4 -> 0, keine Luecke.
**2026-10-17, MIDI Mehrfach-Splits:** MODHSYN.IRX Note-On (0x5f90, Splitschleife 0x6098..0x64a0) startet fuer jeden Split, der
die Taste deckt, eine eigene Stimme (kein Abbruch nach Treffer; Stimme 0x6264, Key-On 0x63d0). Nativ jetzt `decode_system_note`
(native/audio.cpp/.h): je Split ein Clip mit eigener Lautstaerke/Pan, im Sequenzer je Stimme ein Notenlayer. Messung: 80 Paket-
Sequenzen, 1757 Noten mit >1 Stimme (Stimmen 13347 -> 15104); 7 Typ-7-Aufrufe (RM_0100/1100/1130x2/1140/2000/4050), Stimmen 88 -> 104.
RM_1100 seq 0: rechter Kanal vorher stumm (RMS R 0 -> 831, L 833 unveraendert). Test audio_movie (Summe der Einzelsplits), Gegenprobe
rot/gruen; CTest 118/118, verify_audio_state/bgm_rooms/events/event_freeze/enemy_ai/doors gruen, Dev-App 19:46. Offen: Stimmenpool/Stealing (24 je Kern) nativ unbegrenzt.
**2026-10-17, vier Nutzerbefunde (Dev-App 21:19, CTest 121/121):** (1) Fassexplosion ohne Schaden: der Gegnerteil von
bhCheckBombAtari (weapon.c:1420) lief ueber die stillgelegte EN01-Fassade mit leerem recvx_en01_bomb; jetzt am echten
Gegnerwork (`RoomWorld::bomb_enemy_damage`) mit watr-Kapsel, bhCheckC2WallN 0x400 (neu `check_c2_wall_n`, Kapselradius 0.1
statt Strahl), Gelenkwahl und dam[]/comb_flg. Test `native_barrel_blast` (RM_0080 ueber den echten Trefferpfad).
(2) RM_1130 Wasserspritzer: der Zeichenpass nahm die Startgroesse der Raumtabelle statt der von bhEff249 fortgeschriebenen
(effect.c:1374 zeichnet op->sx/sy/sz); Bild 350 vorher bildfuellende Flaechen, jetzt kleine Tropfenwolken (Gegenprobe rot).
(3) Schreibmaschine: die Tipptoene (0x80000242-244) haben Bit 31 und gehen ueber ExPlayMidi auf MIDI-Slot 2
(sdfunc.c:1344-1385); `play_system` nahm sie als COMMON-SE-Programm und verwarf sie. Gemessen RM_0010: 24 Sequenzen 2/66..68.
(4) Ueberlappender Text im Aufnahmebildschirm: er erbte den Zustand des vorigen Statusbildschirms (FILE/Examine), und die
Befehlsbeschreibung 180 - maincsr zeichnete ueber die Frage; sub1.c:3308 startet ihn mit StatusInit. Test `pickup_status_reset`.

**2026-10-17 spaet, RM_4020 Greifarm und RM_1050 Steve (Dev-App 23:32, CTest 122/122):** (5) Greifarm: dieselbe lkono-Regression
wie bei den Wasserspritzern (Lader setzte bei lkflg 0 lkono 0 statt efp->lkono, room.c:877); bhEff134 prueft die Greifentfernung
gegen sys->obwp[op->lkono] (effsub1b.c:453-471) und las obwp[0] statt der Kiste obj[4]. Mit dem Fix von 21:18 misst der Lauf
Klammern px 13/pz 10, Abstieg py -40.5, nn 1.97 < 4 -> ev_flg 59, EN56 mode0 2; Fehlgriff px 27 ohne Flag. Test `crane_ufo_event`
(Gegenprobe rot), Replay build/validation/crane-rm4020-20261017. (6) Steve-Haenger: Opcode 0xb0 bhHEffectSet2 (event.c:7472) war
nicht implementiert; Ereignis 12 blieb @0x144a stehen und erreichte CineSet 1 (@0x14b0) nie - daher lief das Cinematic bis SELECT.
Die e.frame/frm_no-Vermutung ist widerlegt. 0xb0 schreibt sys->ef (22 Byte) und belegt bhEff211 (Monitorrauschen) mit lkono v4.
(7) Monitor aus der Ferne schwarz: njDrawTextureSub3DH schreibt XYZF2.F = 128 fest (ps2_NaDraw.c:443, FGE an), der Port nebelte
den Thl-Quad distanzabhaengig (aus 85 Einheiten blieb in Schnitt 1 nur Nebelfarbe). Jetzt SceneTriangle::fog_fixed, gemessen
Schnitt 1: 497 px, mittleres RGB 95/64/64 bei grauem Testbild (= Cs*128 + ocol*127 >> 8). Beides in `event_lifecycle_control_release`.
Runtime-Endbild Schnitt 1 vorher RGB 54/6/6, nachher 42/13/12. Gegenproben: 0xb0 entfernt -> rot (unimplemented @0x144a), F=128 sabotiert -> rot (128/128/128).
Offen: Headless-Replay des Steve-Ereignisses ueber den Dialogpfad abgebrochen (Auswahl wechselt per right/left, nicht down); bhEff110 (Vollbildansicht) unportiert.

**2026-10-18, Gameover-Retry (Dev-App 01:18, CTest 122/122):** Nutzerbefund „nach Retry sterbe ich am respawnten Punkt direkt wieder, der zweite Retry klappt".
Ursache: `ts_flg` Bit 0x4000 ist die Einmal-Eintrittskarte des Game-Over-Starts (player.c:6709-6714 verlangt `plp->flg 0x2` UND das Bit und
verbraucht es; gameover.c:516/593 gibt es erst vor `bhPopGameData` zurueck). Der Port loeschte das Bit, pruefte es aber nicht, und weil
`plp->flg 0x2` nach dem Tod stehen bleibt (Riegel `player_dead_`, world.h:394, faellt erst bei hp > 0), forderte jeder Pass der 70 Bilder von
`bhMainGameOver` md1 0 den Start neu an -> nach `bhPopGameData` sofort ein zweites Game Over. Gemessen RM_0080 Griff-Tod: Bild 1800 hp -20/md1 2,
nach Retry 1 hp 160/Lage sauber, aber md1 0->1->2 erneut; erst Retry 2 frei. Dieselbe Luecke im Skriptbefehl `bhGameOverSet` (event.c:8681-8689).
Beides in `native/runtime.cpp` behoben. Gegenprobe am sabotierten O3-Binary gegen dieselben Archive: rot; `verify_gameover` und
`verify_self_destruct` weiter PASS. Neuer Pruefer `tests/native/gameover_retry_death_check.py`.
Nebenbefund Retry-Punkt geprueft und **widerlegt**: `bhInitPlayer` (player.c:848, bhPushGameData) laeuft nur in mn_md0 1 (Spielstart/Laden,
system.c:1397); Raumwechsel laufen ueber mn_md0 4 mit `bhResetPlayer` (system.c:1776) ohne Push. Weitere Pushes: Figurenwechsel (system.c:1824),
Speichern (ps2_SaveScreen.c:1408), Skript 0xbd (event.c:8176); ss_flg 0x400 (system.c:1901) hat in den Quellen keinen Setzer. Der Port bildet
genau diese Stellen ab (runtime.cpp `!retry_point || player_change_push`, 0xbd, Schreibmaschine).

**2026-10-18, Treppen (Dev-App 01:41, CTest 123/123):** (1) Kamera stockt auf langen Treppen (RM_3080): `tick_camera` folgte immer
`plp->px/py/pz`, das in Stufenaktionen nur beim Motionumlauf nachzieht (player.c:3258-3271); das Original zielt unter `plp->stflg 0x30` und
in mode0 7 auf `owP[0]` (cut.c:1197-1217). Vorher 42/80 Bilder Stillstand und 3.3-Einheiten-Spruenge, jetzt 0.07..0.34 je Bild. Test
`camera_follow_target` (Gegenprobe rot), `verify_stairs` gruen. Offen: freie Steuerung cut.c:1207-1209 (EXP_WORK plx/plz) ohne Besitzer.
(2) RM_1000 Treppeneinstieg "nur im 90-Grad-Winkel": nicht reproduziert; Typ 1 hat im Original keine Winkelpruefung (hitchk.c:4677),
Port float-genau gleich, Einstiegsfenster 271 Grad an der natuerlichen Standlage.

**2026-10-18, RM_3130-Absturz und Kamera-Eintritt (Dev-App 02:01, CTest 124/124):** (1) RM_3130 "Native runtime error: vector" nach dem
Raum-verlassen-Ereignis: Ereignis 2 setzt per Common 24 (Sel. 1) mtn_no 5 auf das Subplayerwerk, der Raum hat aber nur 3 Raummotionen;
`room_motions_.at(5)` warf. Original: `sys->rmthp` sind 512 genullte MN_WORK (room.c:700-706), `bhSetMotion` tut bei datP NULL nichts
(Motion.c:29). Test `room_motion_empty_rmthp_slot`. Nachgestellt ueber it_flg 232 + Bodentrigger 0, nicht im natuerlichen Spielablauf.
(2) RM_1000 Tuer links: Kamera schwenkte von ausserhalb herein. Spawn 2 liegt laut Tabelle knapp ausserhalb jeder Schnittflaeche,
`follow()` liess dann die rohe CAM_WRK-Lage stehen; cut.c:663-710 rechnet die Verfolgungspose ohne Bereichsgrenze. Vorher 24 Einheiten
Schwenk, jetzt keiner. `tests/native/room_entry_camera_check.py` (Gegenprobe rot).
(3) Third-Person "wechselt an Ecken und beim Raumeintritt auf einen festen Punkt": Ursache ist die Ausweichlogik der Host-Mod
(native/third_person.cpp `consider`): Wand direkt hinter dem Spieler -> Arm schwenkt bis 180 Grad nach vorn (Frontalansicht), nach einigen
Schritten zurueck. Belegt RM_1000 Spawn 1 (Tor 2 Einheiten hinter Claire, Gang beidseits eng): Bild 2/20 frontal, ab 45 hinten. Die Mod schaltet
dabei nicht ab (Raumscan: Abschalten nur bei st_flg 0x4 Treppe/Stufe und Ereigniskameras). Drei Versuche (Schwenk <= 90 Grad, Blick in
Laufrichtung, Anheben vor Zurueckziehen) loesen die Engstelle nicht: dort ist hinten und seitlich kein Platz, Claire faellt aus dem Bild.
Auf HEAD zurueckgesetzt; Versuchsstand $TMPDIR/tpd/third_person.experiment.cpp. **Vom Nutzer 2026-10-18 als Luecke zurueckgestellt.**
**2026-10-20 behoben (nur Arbeitsbaum/Build, nicht ausgeliefert):** Ausweichschwenk entfernt; Arm bleibt hinter dem Spieler, wird
an Waenden gekuerzt und angehoben, Schulterversatz schrumpft mit, Spieler/Haare/Waffe blenden bei nahem Auge aus (Alpha bis 0.2).
Raumeintrittsscan (jeder 6. Raum, Spawn 0/1, 30 Takte): Kamera vor Claire 10 -> 0, hinter 8 -> 52 von 53 freien Faellen.
Test `third_person_camera_mod` (Wand hinter Spawn; Gegenprobe gegen alten Stand rot), CTest 173/173.

**2026-10-18, Kamerafilter (Dev-App 09:46, CTest 125/125):** Raumeffekt-Luecken neu vermessen: von 89 Raumtabellen-IDs ohne Handport
sind 77 im Original `bhEffDmy` (ids 30-84, loeschen sich) oder id >= 400 (bhControlEffect setzt flg 0, effect.c:846); echte Luecken 12.
Groesste: `bhEffFil` 90..92 (effsub1.c:28), der Vollbildfilter, den 140 Kameraschnitte in 61 Raeumen per CAM_WRK flg 0x8000 einschalten
(cut.c:412-421). Jetzt portiert (Vignette, Laufzeit RM_0070 Bildrand 14.9 -> 10.0). Test `effect_camera_filter` (Gegenprobe rot).
Rest: 110 (3 Raeume), bhEffBG 93-97 (je 1-3 Raeume), 273 E14_Mucus, 112, 133, 138.

**2026-10-18 11:09, Startabsturz behoben (Dev-App 11:09, CTest 125/125):** "Native runtime error: active camera outside table" sofort
beim Start. Verursacht vom Kamera-Eintrittsfix 02:01: `follow()` rechnete die Verfolgungspose jetzt auch ohne Schnitttreffer, beim Titel
gibt es aber keinen Raum und keine Kameratabelle. Alle Pruefungen liefen mit `--in-game` und haben den Titelpfad uebersprungen. Jetzt nur bei
vorhandener Kamera; `tests/native/room_entry_camera_check.py` startet zusaetzlich ohne Raum (Gegenprobe rot mit genau dieser Meldung).
Am installierten App-Binary geprueft: Titel, Neues Spiel bis RM_0000, LOAD GAME aus dem Titel, Raumeintritt RM_1000.

**2026-10-18 12:3x, Flammenstreifen Friedhof (Dev-App, CTest 126/126):** Nutzerbefund RM_0020 Kinoszene: fliegende Flammenstriche um den
brennenden Zombie und seinen Kopf. Quelle eindeutig die Gelenkflammen `bhEff_Sub352` (ohne diese Sprites keine Striche). Ursache: der
Renderer kachelte jede Textur (REPEAT); bilinear holte an den 48-px-Zellraendern des Effektatlas 8 Flammentexel der Gegenkante. Das Original
setzt GS CLAMP_1 aus dem TIM2-Kopf: `Ps2TexLoad` (ps2_dummy.c:1650) uebergibt ClampFlag, `Ps2CheckTextureAlpha` (:1749-1844) setzt bei
ClutChange 0 `0xFF000FF5` (WMS/WMT CLAMP), nur Gindex 20500/15205..15209 `0xFF000FF0` (REPEAT); sonst bleibt der Dateiwert (0xCC). Jetzt
`SceneTexture::clamp_u/clamp_v` im einzigen TIM2-Decoder (scene.cpp), im Renderer (gefiltert und ungefiltert) und erhalten bei TGA-/HD-/
Mod-Ersatz. Beleg: Bild 650 vorher/nachher 366 Punkte Unterschied, genau die drei Striche; 37 Raeume Bild 45 sonst hoechstens 22 Punkte.
Test `gs_texture_clamp` (synthetisch + echte RM_6050-Textur Gindex 20500), Gegenprobe dreifach rot (Renderer, Gindex-Ausnahme, ClutChange).
Am installierten App-Binary: Bild 650 identisch zum Pruefbild, Titelstart, Neues Spiel bis RM_0000, LOAD GAME. **Offen:** der
Texturcache-Pfad `Ps2DrawOTagSub` (ps2_dummy.c:1420) setzt fest CLAMP/CLAMP, auch fuer die sechs REPEAT-Gindices (RM_6050/5600/7000/3020),
falls diese ueber die OT gezeichnet werden - nicht zugeordnet. Ebenfalls offen aus demselben Befund: grauer Balken unten (Kinobilder ~410-426,
Kamera 12) und schwarzes Flackern (Bilder 358-378). Achtung Gegenprobe: `make` vergleicht Sekunden; Zurueckholen + touch in derselben Sekunde
wie der Sabotagebau liess das sabotierte Objekt stehen (einmal so ausgeliefert, sofort neu gebaut) - vor touch mindestens 1 s warten.

**2026-10-18, Friedhof Kistenszene (Ereigniskamera 12), Flackern und grauer Balken - kein Portfehler gefunden:** Messung je Bild
(Proberuntime in $TMPDIR, Produktquellen unberuehrt). (1) Schwarzflackern Bilder ~383-406: im 2-Bild-Takt ist das ganze Bild ~30 %
dunkler. Allein Effekt 119 (Rauch, Blend 11/3 = Cd*(1-As)) verursacht es; ausgeblendet bleibt die Helligkeit gleichmaessig. Erzeuger
`bhEff103` Art 2 (effsub1.c:6080) setzt jeden zweiten Pass eine Rauchwolke 119 Art 1; die jeweils neueste sitzt 14.5 Einheiten vor der
Kamera, Ecken +-1*sx (effect.c:506-520, sx 33) - sie deckt das ganze Bild. 119 (effsub1.c:7890), 103, Ecken, bhDrawTrsEffect3D
(effect.c:1287), njDrawTextureSub3D-Verwerfen (ps2_NaDraw.c:300-358, _Clip_Screen ps2_Vu1Strip.c:1202) und Tiefentest stimmen mit dem
Port ueberein. Ohne Originalaufnahme nicht weiter entscheidbar. (2) Grauer Balken unten (Bilder ~407-428): Hintergrundfarbe 0x181E2D
(rom->bak_col; bhChangeBackColor ps2_event.c:8 hat fuer Stage 0 Raum 2 keinen Eintrag). Der Boden fehlt dort, weil bhChangeViewClip
(event.c:13205, ViewClipSt0) fuer Raum 2 evc 12 near -9.0 setzt; die Kamera steht ~8 ueber dem Boden. Nativ ebenso. Nebenbefunde (Ledger):
bhChangeClipVolume/bhChangeClipVolumeRM nicht portiert (andere Stages), PRIM-Lesart in effect_particles.cpp vermutlich vertauscht.

**2026-10-18 nachmittags (Unteragenten, jede Aenderung gegen das Original nachgelesen, Dev-App via `./build.sh --test`, CTest 127/127):**
(1) Speicherbildschirm "weisses Overlay": `draw_typewriter` loeschte die Leinwand nicht; das Original loescht jedes Bild schwarz
(bup_00.c:299-304, ps2_dummy.c:89). In den schwarz gekeyten Loechern des Hintergrunds blieb das Raumbild stehen und das Fenster
0x18808080 mischte Bild fuer Bild erneut (Grau ~118 statt 12). Headless unsichtbar (nur letztes Bild gezeichnet). Test
`typewriter_slot_list_draw` (Gegenprobe rot). (2) `bhChangeClipVolume`/`...RM` portiert (view_clip.cpp, Renderer-Streifenregel);
gemessen bytegleiche Bilder in RM_0070/0090/0150/0020, nur nicht mehr verworfene Pakete ausserhalb des Bildes. Offen: ClipDispW der
Negativzeilen (nur DoubleMain-Leser), Tuer-/Gameover-Kamera behalten 320/240. Tests `view_clip`, `native_ground_world` (Gegenprobe rot).
(3) RM_0100 Kistensprung-Ruck: dnd setzt plp->flr_no vor py (player.c:3601); der Host nahm die Etage aus py und schrieb den
Flagstand vom Aktionsbeginn zurueck, das Abschlussbild baute so die Kantenwand der Kiste und schob Claire 5.0-5.9 Einheiten weg.
Jetzt `stairs.floor` waehrend Stufenaktionen und Neulesen der Flags vor jedem Tick. `tests/native/box_jump_landing_check.py`
(alle drei Seiten 0.000, Gegenprobe rot 5.045), `tools/verify_stairs.py` (RM_2050-Erwartung korrigiert), verify_push gruen.
(4) GS-PRIM ab Bit 47 (ELF-belegt): Blutlache jetzt Dreiecksstreifen statt acht Rechtecke, Partikel-Sprites als GS-Sprites, n/2 Sprites
je njPtclDrawSprite und Breite /1.174 (ps2_NinjaPtcl.c:100-163). Tests `original_effect_particles`, `effect_room_sprites` (gegen HEAD rot).
Am installierten App-Binary: Titelstart, RM_1000-Eintritt, Kistensprung, Treppen, Schieben.
Punch-through-Nebenbefund nachgezogen: bhEff023 mdlver 0 zeichnet jetzt ohne Überblendung mit Alpha-Lochtest und Tiefe;
RGB-Filterung unabhängig von Alpha, auch im optionalen Host-Mipfilter. Gegenproben für Liste, Coverage und RGB rot,
CTest 129/129 grün; Kistensprung und Titel-/Raumeintritt jetzt regulär in CTest. `Start.command`-Titel/Neues Spiel
und LOAD GAME am App-Binary geprüft. Einzelheiten und Grenzen: [Prüfbericht](archived/work/2026-10-18-punch-through.md).
Danach integriert (Unteragenten, jeder Diff gegen das Original nachgelesen, eigene Gegenprobe je Port rot, CTest 139/139,
Dev-App, `Start.command` Titel/Neues Spiel, LOAD GAME, Kistensprung, Treppen, Schieben am App-Binary):
(a) bhChangeBackColor/Evt (`native/back_color.*`, 132 Tabellenzeilen, EOR-Timer sync.c:114, 0xab sofort in rom->bak_col;
ts_flg startet jetzt mit 0x20000 wie system.c:453). RM_7070 Kamera 3: Tabellenfarbe 0x205070 durch das Gitter. partial: globale
njSetBackColor anderer Tasks, Zielalpha/SSAA. (b) Nebelfarbe ohne R/B-Tausch (Leinwand ist ARGB; alte AABBGGRR-Kommentare falsch;
`ui.cpp` direct_message hat denselben Tausch noch, offen). (c) UV-Scrolls bhObj010, bhEff120 (ELF: -floor(az/32)),
bhEffectSensyaSet 0x96 und OT-CLAMP nur an der belegten bhPutModel-Route (Panzerkette Tex 9). (d) bhEff110 Vollbild-Monitor und
bhEff133 Thermometer (RM_1050/4030/8050, RM_7050). (e) Tiefe GEQUAL: spaetere gleich tiefe Flaeche gewinnt, Kantenregel fuer alle,
Material-Alpha 0 verworfen; 10-Raum-Vergleich nur Kantenpixel bzw. +-1. (f) bhEff138/bhDraw138 Oel-Raetsel RM_8001 (Test `native_oil_puzzle_world`, Gegenprobe ev64 rot; bhDraw138 partial: gemeinsame OT
anderer Zeichner). (g) pt_flg-Zeichentore Raum 0x20/Objekte 0x4/Items 0x8/gelinkt>=1210 0x1 (game.c:328, objitm.c:503/550/639), Test
`native_presentation_gates`. (h) UI-Farbkanaele (direct_message, FE01..05 message.c:161/401), Test `ui_color_channels`.
(i) ein Besitzer fuer Db.clear ueber Tuer/Gameover/Karte/Schreibmaschine/Film/Terminal/Status (back_color.*). CTest 142/142,
Dev-App, verify_doors gruen. Offen: Wasserflaeche bhDrawGeneralPurposeWater ohne Zeichenpfad, bhEff112 (Spiegelpass laeuft),
gefilterte Alphatest-Quantisierung, GS-16-Bit-Z.
Nachtrag: (j) Spiegelpass/bhEff112/njMirror (RM_2040, Test `mirror_pass`; offen: gespiegelter bhDrawEffect game.c:91).
(k) Kisten-Ausruestungsflaeche ArmsSet center 9 (sub1.c:4975-5105, Test `item_box_equipment_slot`). (l) Muendungslicht Typ 100/101
(light.c:673-699, Test `native_lighter_effect`). (m) EN01-Blutlache bhSetBloodPoolLnk (en01.c:9990/9994/10122/10149, en01b.c:763,
Tests `enemy_en01_blood_pool`, `zombie_blood_pool`). (n) GetItem-Bildschirm ohne Statuspad (sub1.c:3447-3460, Test `pickup_gap_pad`).
RM_0050 Rueckgleiten nach Zombie-Event ist Originalverhalten (event.c:12034 Common 7 ohne spx, player.c:2127).
(o) Zopf als obwp[2] am Linkwork (event.c:2177/2501, objitm.c:310), Test `player_hair_link`. (p) RM_3060 Wasser-SE ListNo char
(types.h:1754), Test `rm3060_water_action_se`. (q) RM_1140 EOR nach Eventpass, Test `rm1140_lever_back_color`. (r) Kampf-Wandkapsel
bhCheckC2WallN (pwksub.c:2172), Test `combat_target_rules`; Treffer/Auto-Aim sonst Original. (s) EN26 Anatomist Zombie RM_0110
(en26.c), Test `rm0110_event_zombie`. (t) Muendungsfeuer Aufrufclip (ps2_NaDraw.c:327), Blutlache lkwkp/pt_flg-Tor (effsub3.c:389).
(u) bhDrawGeneralPurposeWater (objitm.c:471, Test `object_water_wave`). (v) bhEff362/370/371-373 (effsub4.c). (w) Lichttypen 7-12
und gm_flg-0x80-Ruecksprung (light.c:367/486-657, Test `light_types_7_12`). (x) bhStartDoorDemo-Blende und Tuertask-Modi
(room.c:949-1009, system.c:939-1061, verify_doors). (y) Gespiegelter bhDrawEffect (game.c:87/427, effect.c:1225, Test `mirror_pass`).
(z) Framefolge (game.c:48-59/289-296): "Kamera vor Spieler" war Fehlbefund (tick_camera = bhCheckCut, cut.c:12). Portiert: Gegner ->
Spieler -> Effekt/ObjItm/Licht, Kopf/bhCalcModel nur mit sp_flg 0x1 (player.c:1365), bhCheckCut nach dem Zeichnen, Kameraschnappschuss
game.c:58 fuer Zeichnen/Effekte. Tests player_head_pass_gate, effect_after_player, event_camera_first_frame, camera_control_order.
(aa) bhSetDispObj Spieler (op 0x20): plp->mdflg 0x1 jetzt als player_model_hidden_ (nur Modell, sperrt den Pass nicht) statt player_hidden;
pl_sleep_cnt-Sonderfall von op 0x20 nach bhInitModelSet (op 0x24) verschoben. Test event_lifecycle_control_release (Gegenprobe rot).
(ab) bhInitCamSet (0x5b) ruft jetzt bhControlLight (event.c:4745): Lichtpass als RoomWorld::control_lights() ausgekoppelt, in
tick_after_player (game.c:59) und im 0x5b-Zweig der Runtime nach control_camera. Test light_types_7_12 (RM_1020 Typ 9: Pass + Extra-Aufruf
= 2 Schritte; Gegenprobe rot "ausserhalb des Passes"). (ac) Lichtlinktod light.c:719-748: je Licht mit flg 0x1|0x2 wird das Zielwerk
(lkflg 1 plp, 2 ene, 3 obwp, 4 itwp; 5 eff ohne Besitzer, >5 lkflg/lkno = 0) auf flg 0x1 geprueft, sonst flg &= ~0x3 dauerhaft. Befund:
das Werk-flg-Bit 0x1 kommt aus dem ETTY-flg und hat im Original fuer ene/obwp/itwp/plp keinen Schreiber; alle 33 gelinkten Lichter der
Raumdaten (lk 2: 19, lk 3: 14) zeigen auf aktive Werke, die Regel ist mit Spieldaten unerreichbar und nur ueber die Pruefnaht
set_work_flags belegt (Test light_types_7_12 RM_3230 Licht 5 -> obwp[7], Gegenprobe rot "link death") (ad) Wasserlicht draw_tp 0 (RM_9110) ohne Codeaenderung belegt: bhDrawGeneralPurposeWater ->
bhDrawSpObject setzt Licht 1 = lgtp[aspd] selbst (objitm.c:483/696-703), Punktlichter bhSetHalfLight 0.5 (game.c:352), amb_chr (game.c:344);
bhDrawEnemy/bhPutModel(plp)/bhDrawEffect schreiben keine EasyMulti-Farben - identisch zum nativen ob_spc-Glint-Satz. CTest 158/158, je Fix Gegenprobe
rot/gruen, Dev-App Raumeintritt/Hebel/RM_0110/Blutlache/Kiste/LOAD GAME/Tuer gruen.
(ae) Spiegel-bhDrawMdfEffect (game.c:427 -> effect.c:866/1076, zweite Modifierstufe unter cam.mtx*njMirror): SceneShadow::mirror,
Renderer-Stufe zwischen Spiegelraum (Phase 2) und Spiegeleffekten (Phase 3) mit mirror_view; world.cpp markiert die im Spiegelaufruf
erzeugten Modifier statt sie zu verwerfen. Befund: die Passwahl bleibt das Bildschirmvorzeichen von njCnkCoP3 (ps2_NinjaCnk.c:1684,
lCnkModClipFace), game.c:425 njControl3D(0x100) traegt vor dem Spiegel-bhDrawEffect kein 0x10000 mehr -> ein gespiegeltes geschlossenes
Volumen tauscht Vorder-/Rueckseiten und ergibt dieselbe (leere) Maske wie ein vorab gespiegeltes Volumen im Normalpass (Original-
semantik, keine Korrektur erfunden). Test shadow_modifier_pixels: Gleichheit Spiegel-Modifier == vorgespiegeltes Volumen, M*M*T sichtbar
nur ueber mirror_view, Spiegelraum empfaengt / Spiegeleffekte danach, Ablehnung ohne njMirror-Matrix; drei Gegenproben rot (view statt
mirror_view, Stufe vor dem Spiegelraum, Stufe nach den Spiegeleffekten). CTest 158/158.
(af) bhDraw134-Zweitlauf im Spiegel ohne Codeaenderung geschlossen: der Spiegelpass haengt an Effekt 112 (Raumtabellen nur RM_2040/9230,
bhEff112 nur Stufe 2), bhDraw134 an Effekt 134 (nur RM_4020/8020); kein Raum traegt beide, Skripte spawnen weder 112 noch 134
(bhHEffectSet2/bhSetEffect aller 205 Raumdisassemblies). Der ef_fnc-Zweitlauf (effect.c:929 unter game.c:427) ist damit fuer 134 unerreichbar.
Offen: Bildabnahme Wasserraeume/Kopfplatzen/Spiegelraum RM_2040 (Sichtpruefung im Spiel).
(ag) Raumscan 205/205 mit --enemy-ai on (600 Bilder, Dev-Binary 2026-10-19): 0 Laufzeitfehler. Der aeltere Scan ohne Gegner-KI meldete 94
unbindbare Befehle (Common 24/25/34 auf uninitialisierte Werke) - Artefakt der KI-Abschaltung, mit KI 0. Verbleibende Meldungen: bhEffModeSet
0x91 auf Texturhalter (RM_7140, jetzt gebunden, Test event_lifecycle, Gegenprobe rot); 0x23 hinter rom->etcp (RM_3100/3140/7030, Original-UB);
bhSetMotion frm_no 9226 auf 2-Bild-Raummotion (RM_3120/3220, versteckter Gegner 2, Original-UB, Ledger); bhObj005-Nachbarzelle (12 Raeume, UB);
unportiert: bhEff_Sub350 (RM_1120/5580), bhEff301 (RM_5810/9302), bhEne06 (RM_6030), bhEne22 (RM_7100), plp mnwPb[mtn_no] (RM_7250).
(ah) bhEff_Sub350/351 portiert (Giftspeichel der EN01-Giftzombies, en01.c:11190 bhEne01_PoisonEffect; effsub4.c:319/538): Bahn, Wartezeit,
Bodenaufprall (bhCollisionCheckLine2 0x400, Lage +0.5), Spielertreffer gegen plp->watr (Schaden 10, mode0 2/mode2 1/mode3 0, flg 0x4, mode1 aus
bhDGCdirCheck) mit drei 351 am plp->djnt_no (neuer Effekthaken player_hit_check_parts = zonzon.c:1376 bhEne_HitCheckParts auf dem Spielerbaum),
SE 8961 ueber bhEne01_SePlay(lkwkp) mit flg-0x10000-Tor, Spritzer 351 (Wartezeit, Link loesen, Ausblenden, 14 Bilder). Vorher taten die Speichel
nativ nichts (Luecke 'bhEff350'). Test effect_eff350 (Gegenproben rot), Headless RM_1120/5580 mit Gegner-KI ohne Luecke, 350/351 lebendig.
(ai) bhEff301 portiert (Blutfleck aus zonzon1.c:301 bhEne_SetBloodstain, viermal je Gegnertreffer; effsub3.c:404-808): Streuung/Strahl,
Boden (Fall 4-6), Wand (8-10), Typ-6-Satz (16-17), Wasser (12-14, bhCheckWater mit h ueber neuen Dienst set_effect_water_area), Linse
(bhCheckCamWall2D effsub3.c:1611 gegen cam.mtx, Fall 24/25 als ef_trs2d, gm_flg 0x20 und cam.flg 0x10 = ci->flg 0x200), ATR flg 0x10,
sys->bl_ct-Staffelung, gm_flg 0x2000000. EffectContext traegt jetzt cam.mtx (camera_view_matrix) und cam.flg 0x10. Vorher entstanden nativ
keine Blutflecken an Waenden/Boden (Luecke 'bhEff301'). Test effect_eff301 (Gegenproben rot), Headless RM_5810/9302 mit Gegner-KI ohne
Luecke. CTest 160/160, Dev-App via `./build.sh --test`. Verbleibend aus dem KI-Scan: bhEne06 (RM_6030), bhEne22 (RM_7100), plp mnwPb[mtn_no]
(RM_7250). **Kein Commit/Push.**
(aj) bhEne22 portiert (Albinoid Adult, RM_7100; native/enemies/en22.c, jump[22], Dienste RECVX_EN22_SERVICES): Init/Brain/MV00-06/DG/DD,
Spielerschaden, Kollision, Wassereffekte, Stromschlag (354), Funkenangriff mtn 9 mit bhEne22_ChgDengekiColor/GetDengekiColorAddr auf den
beschreibbaren Stroemen mdl[1..6].objP[1], Bodenlicht 353, Blitzmodelle 364 und rom->lgtp[2] (SetLight/CtrLight, Light.ct1). Neu dazu
bhEff_Sub353/354/364 (effsub4.c:837/958/1774; 354 als bhEff_3DSpriteDraw-Billboard mit Gegnertextur) und RoomWorld::water_splash2
(pwksub.c:2572, jetzt auch fuer EN01 gebunden). **Offen:** bhEff_ModelDraw (effsub4.c:3306) zeichnet die 364-Blitzmodelle noch nicht
(njCnkSimpleMultiDrawModel-Farbpfad ohne Hostpfad, benannte Luecke im Zeichenpass). Test enemy_en22_world (fuenf Gegenproben rot),
Headless RM_7100 mit Gegner-KI ohne neue Luecke, CTest 161/161, Dev-App via `./build.sh --test`. **Kein Commit/Push.**
(ak) bhEne06 portiert (Motte, RM_6030; native/enemies/en06.c, jump[6], Fragmente bhEne06s auf jump[31] per SetCallFunc, verdeckter
Anfuehrer fuer den Spielergriff): Flug, Wandanlehnen, Griff mit Spielermotion 20 aus der Mottenbibliothek, Mottenei (id-27-Work + plp->stflg-
Marke), Treffer/Absturz/Zerfetzen mit zwoelf fallenden Fragmenten auf den owP-Matrizen der Motte. Neu bhEff_E06_Rinpun (260) mit
Teilchenzeichnung aus mdl[1].texP. Test enemy_en06_world laeuft ueber das echte Raumskript (Ereignis 16 gibt Motte 3 bei Flag 4/0 frei),
sechs Gegenproben rot. **Offen:** (1) Staub-Spielerberuehrung: das Original mittelt mit nie gesetzten FPU-Registern $f20-$f22 (ELF belegt),
benannte Luecke; (2) EN27 (Mottenei) ist weltseitig ungebunden und meldet seine Dienste; bhCheckMothEgg im Spielerbesitzer fehlt.
CTest 162/162, Dev-App via `./build.sh --test`, Titelstart geprueft. **Kein Commit/Push.**
(al) RM_7250 Spinnengriff: Ursache war ein zweiter Schreiber von plp->mnwP — der EN03-Anfuehrer haengt die Spinnenbibliothek per
Dienst ein und schreibt danach seinen alten Spielerschnappschuss zurueck, der mnwP wieder auf mnwPb setzte (Griffmotion 62/63 fehlte
dort). enemy_player_set uebernimmt die Bibliotheksfelder jetzt nur, wenn die Sicht sie selbst geaendert hat. Check rm7250_spider_grab.
(am) EN27 Mottenei angebunden: Ei auf plp->mdl[6]/[7] mit Spielermotionen 93..97, Wurzel am Spielergelenk 3 (neues lk_player fuer
lkwkp = plp, Tore lesen plp->stflg), bhCheckMothEgg vor dem ersten Gegnerpass, plp->stflg & 0x78280000 ueber den Raumwechsel (Marken
gingen bisher verloren), Spielerreihe ohne Raum-Texturrueckfall. Test enemy_en27_world (Init, Zeichnung, Schlupf, Griff, Schaden/Gift,
Freigabe, Fall), Gegenproben rot; EN06-Test prueft jetzt das abgelegte Ei ohne Luecke. Frischer Raumscan mit Gegner-KI (205 Raeume):
uebrig nur benannte Original-Undefiniertheiten (bhObj005, bhCheckRoute, bhSetMotion-Kanaele/RM_3120 Common 34 frm_no 0x240A,
en02 ang, ef_tn vor Rendercut). CTest 164/164. **Kein Commit/Push.**
(an) bhEff_ModelDraw (effsub4.c:3306) fuer die Albinoid-Blitzmodelle (364) gezeichnet: njCnkSimpleMultiDrawModel ist der Ninja-Modus
der Gegnerzeichnung (MdlPut.c:182), die davor gesetzten Einzellichter (Modus 2) wirken nicht; Lichtkategorie 4 wie bhEff30cDrw,
Materialfarben aus dem von bhEne22_ChgDengekiColor beschriebenen Strom. EN22-Test prueft Bild und Farbe (zwei Gegenproben rot).
Spiegelpass bleibt benannte Luecke. CTest 164/164. **Kein Commit/Push.**
(ao) Fuenf nativ schon angeforderte, aber unportierte Effektprozeduren portiert: 356 Zombie-Halsblutung nach Kopfplatzen (Rueckrufe in
EN01 aus dem Effektpass; EnemyCallbackFrame bindet dafuer T3-Ergaenzung und EN01-Kontext), 363 Fleischstuecke der Bein-/Wurmexplosion,
252 Flamme am Gegnergelenk (bhEne_SetFireEffect), 259 Hunterblut, 26 Schirmschlieren beim Explosionstod. Tests effect_en01_exp,
effect_e00_fire (Gegenproben rot). progression_test fuehrte 26 als ungebunden; Waechter jetzt auf 6 (bhEff006, echt unportiert).
Naechstes: EN17 (Steve) hat ungebundene Effektdienste (SetPtcl 365/366, Beil-Nachbild 364, lgtp[2], SetBlood2(plp)). CTest 166/166.
**Kein Commit/Push.**
(ap) EN17 (Monster Steve): alle bisher ungebundenen Effektdienste angeschlossen. Neu portiert sind die Erzeuger bhEff_SetPtcl/SetPtcl1
(effsub4.c:19/35) sowie bhEff_SetPtcl2/SetPtcl2V (:117/:199) als RoomWorld::set_ptcl/set_ptcl1/set_ptcl2/set_ptcl2v mit ihren
bhEff_AllocOwork-Hilfsarbeitssaetzen (EffectPtcl365Work/EffectPtcl366Work) und die Prozeduren bhEff_Sub365 (:1812) und bhEff_Sub366
(:1884) samt Zeichenteil bhEff_LineDraw (:3090, untexturierte Dreiecke mit eigenen Eckfarben). Gebunden sind ausserdem
bhEne17_AfterimageAxEffect (Effekt 364, Beil-Nachbild auf epw->mdl[5]), bhEne17_SetLight (rom->lgtp[2]), bhEne_SetBlood2(plp) und
mlwP->objP[0].pos[0/2] = 0 am Gegner- wie am Spielerbaum. Fuer das Nachbild hat bhEff_ModelDraw jetzt seinen flr_no-Zweig:
npSetAllMatColor (njplus.c:1375) faerbt die Materialien des gezeichneten Objekts, sodass die sieben Nachbilder eines Bildes ihre
eigenen Alphastufen behalten; der Port faerbt dafuer eine Kopie des Modellstroms statt den gemeinsamen Strom (gleichwertig, weil
mdl[5] nur dieser Effekt zeichnet und jeder Zeichner vorher selbst faerbt). Drei Abweichungen in native/enemies/en17.c mitkorrigiert:
plp->djnt_no = 2 mit plp->dpx..dpz = cap.c1 beim Treffer und plp->djnt_no = 3 vor bhEne_SetBlood2 fehlten ganz (en17.c:1594/1653),
bhEne_SetVibration(1) ist StartVibrationEx(1, 0xA) und stand als (1, 11), und eine doppelte Zuweisung von cap.c1 mit falschem Index
ist entfernt. **Offen und benannt bleibt die Anzeige von Effekt 365:** bhEff_PtclSpriteDraw uebergibt njPtclDrawSprite n = 1, und
Ps2AddPrim3D gibt genau n Eckpunkte je GIF-Tag mit neu gesetztem PRIM aus — ein GS-Sprite braucht zwei. Der Satz laeuft deshalb,
legt aber keine Flaeche ins Bild; ohne Hardwarebeleg wird hier nichts erfunden. Test enemy_en17_world um Partikel, Nachbild, Licht,
Spielerblut, Gelenk und Vibration erweitert; die bisher zugelassenen Lueckennamen bhEff369/370/301 sind ersatzlos entfallen,
weil diese Prozeduren laengst portiert sind und auf den gelaufenen Wegen gar keine Luecke mehr auftritt — der Test laesst jetzt
keine einzige zu. Vierzehn Gegenproben rot. 205/205 Raeume laden, Gegner-KI-Sweep ueber alle Raeume meldet unveraendert nur die sechs bekannten
Original-Undefiniertheiten. CTest 166/166. **Kein Commit/Push.**
(aq) Magazin beim Nachladen (bhEff007) portiert und zwei Fehler im Zeichenweg sys->ef_pol behoben. bhEff007 (effsub1.c:2335) war
nicht portiert, obwohl der Kampfkern es bei jedem Nachladen anfordert (pwksub.c:3372 bhSetMagazine, nativ cb_set_magazine): der Satz
entstand gar nicht und der Port meldete bhEff007. Jetzt faellt das Magazin vom Handgelenk, prallt mit CallPlayerWeaponSeEx(259) auf,
dreht sich im Fall und bleibt eine halbe Einheit tiefer liegen; im Wasser bremst es und setzt den Spritzer 15.
Auch das Magazin IN der Waffe hat jetzt seinen Besitzer: bhEff007 schreibt bei mdlver != 0 ueber RoomWorld auf den
Waffenassetbesitzer CombatController; WeaponAsset::append laesst nur das rechte Knotenmodell 2 aus, nicht Kinder
oder das linke Modell. player.c:6075 loescht das Flag bei act_ct1 NUR fuer Waffe 12; der bestehende Clear-Auftrag
wird einmal verbraucht. Waffe 13 bleibt wie im Original verborgen. Kampf-Abbruch behaelt das Flag, neues
Waffenasset setzt es zurueck (dread.c:200-210). Test effect_magazine mit echten Reloadpfaden 12/13 und diagnostisch
leerem Inventar, Zeichenmengen, Abbruch/Neu-Laden; sechs weitere Gegenproben rot. Auch Materialschreiber des
verdeckten Knotenmodells entfallen, Positionen der verbleibenden Schreiber bleiben korrekt.
[Pruefbericht](build/validation/magazine-owner-20261019/README.md). Abschliessend ./build.sh --test: 169/169 gruen,
Dev-App gesichert/aktualisiert/signiert; Start.command-Titel und Reload geprueft (Motion 116, Effekt 7, keine
blockierten Befehle/offenen Weltbindungen), arm64/Signatur/Policy gruen. Runtimebilder angesehen, aber Kamera
hinter der Figur: keine getrennte visuelle Abnahme des Magazinwechsels. Kein Commit/Push.
Beim Anschluss fielen zwei Abweichungen in bhDrawPolEffect (effect.c:949) auf, beide per Disassembly belegt: (1) die Textur ist
sys->ef_tlist.textures[tex_id + ani_ct] OHNE sys->ef_tn (ELF 0x0021D3F8 gegen 0x0021DEBC in bhDrawTrsEffect3D) — der Port suchte die
Kategorie, wodurch die Patronenhuelse das gemeinsame Bild 19 statt 5 und die Markierung 25 statt 8 trug; (2) gezeichnet wird nur
objP[op->mdlver], der Port zeichnete das ganze efm[0] und damit beide Huelsenobjekte zugleich. Beides ist korrigiert, die vier
Modellbloecke aus bhInitEffect (efm[0]/[1]/[7]/[13]) liegen dafuer einzeln vor. Neuer Test effect_magazine, dazu der Block
pol_effect_draw in effect_room_sprites; elf Gegenproben rot. Ausserdem: die Lueckenliste im EN17-Test liess bhEff369/370/301 zu,
obwohl diese Prozeduren portiert sind und auf den gelaufenen Wegen gar keine Luecke mehr auftritt — der Test laesst jetzt keine zu,
Ledger und Eintrag (ap) sind entsprechend berichtigt. 205/205 Raeume laden, Gegner-KI-Sweep unveraendert bei den sechs bekannten
Original-Undefiniertheiten. CTest 167/167. **Kein Commit/Push.**
(ar) Die beiden Sand-Skriptbefehle des Gulp Worm portiert: 0x98 bhEffectSandSet (event.c:4110) und 0xC3
bhEffectSandSetMatsumoto (:8577) fehlten in der Event-VM, obwohl ihre Ziele bhEne02_SetSandEffect und
bhEne02_SetSandEffectEV nativ schon vorlagen (die EN02-Zeile nannte beide als offen). 0x98 ist 12 Byte lang:
Vorzeichenbits fuer drei u16-Betraege in Hundertsteln, eine Lageart (Weltlage oder Versatz, der nur x und z zur
Gegnerlage addiert) und ein Typbyte, hinter dem der Skriptzeiger um zwei rueckt — per Disassembly 0x0015CAF0
geprueft, weil eine falsche Laenge den ganzen Skriptstrom verschieben wuerde. 0xC3 nimmt den Werkplatz von
sys->ene, nicht den Raumindex. Weil die EN02-Dienste nativ nur waehrend des Gegnerpasses gebunden sind, merkt der
Skriptpass den fertig aufgeloesten Auftrag vor und der Gegnerpass desselben Bildes fuehrt ihn vor den Gegnern aus —
dieselbe Reihenfolge wie im Original. Unbekannte Ziele bleiben ungebunden statt geraten. Damit sind 197 von 223
Skriptbefehlen gebunden (vorher 195). Test enemy_en02_world um drei Bloecke erweitert; sieben Gegenproben rot.
205/205 Raeume laden, Gegner-KI-Sweep unveraendert. CTest 167/167. **Kein Commit/Push.**
(as) Blickbefehle des Spielerkopfes portiert und die Lage bei den restlichen Skriptbefehlen geklaert. 0x40
bhLookGsetPlStart (event.c:3488) und 0x41 bhLookGsetPlStop (:3539) schreiben den Kopfzustand plp->exp1, den der Port
mit RECVX_HEAD_STATE schon fuehrt: Start setzt die beiden Blickflags, waehlt die Zielart gespeicherter Punkt und
traegt den Punkt aus dem Skript ein, Stop loescht nur die beiden Flags. Damit sind 199 von 223 Skriptbefehlen
gebunden. Wichtiger Befund dabei: ein unbekannter Skriptbefehl wirft in der Event-VM und haelt den Raum an, die
verbleibenden sieben sind also moegliche harte Stopps, sobald ein Skriptzweig sie erreicht — in 205 Raeumen x 600
Bildern Initlauf wird keiner erreicht. Ihre Laengen und Wirkungen sind jetzt in der Ledgerzeile
event.c:bhScenarioJmpT:offen festgehalten, damit sie einzeln nachgezogen werden koennen; der teuerste ist 0x1E
bhAdxTimeCk, eine Bedingung, die die Abspielzeit eines ADX-Stroms braucht und dafuer nativ keinen Besitzer hat.
Ausserdem geprueft: von den 17 als 'nur konsumiert' gemeldeten Befehlen sind alle in Wahrheit am Raumbesitzer
gebunden (die Objektbindungen ueber eine Tabelle, die der Zaehler nicht sieht). Neuer Weltblock im Test player_head
(der jetzt das Datenverzeichnis bekommt); sieben Gegenproben rot. 205/205 Raeume laden, Gegner-KI-Sweep unveraendert.
CTest 167/167. **Kein Commit/Push.**
(at) Sechs der sieben verbliebenen Skriptbefehle halten den Raum nicht mehr an, und drei bisher wirkungslose sind
gebunden. Ihre Wirkung hat nativ keinen
Besitzer (Spieler als Kind einer Objektbindung, die beiden Handobjektwerke, sys->efid, die Eventaufgaben-Werke,
die Debugkamera, die Battle-Game-Rangliste), aber ihre Laenge steht im Original fest: 0x6F 12, 0x82 4, 0x97 8,
0xAA 10, 0xAF 4, 0xD3 2 Byte. Sie werden jetzt genau so weit uebersprungen und als offene Bindung gemeldet, statt
den Lauf mit `unimplemented opcode` zu beenden. Uebrig bleibt 0x1E bhAdxTimeCk als einziger harter Stopp — das ist
eine Bedingung, die die Abspielzeit eines ADX-Stroms an ein IF liefert; sie zu ueberspringen hiesse, den Zweig zu
erfinden. Neuer Test event_opcode_lengths: er legt jeden der zehn in diesem Stand behandelten Befehle an den Anfang
eines Eventskripts, setzt dahinter ein bhSet und prueft, dass genau dieses bhSet ausgefuehrt wird — eine falsche
Laenge verschoebe den ganzen Strom dahinter. Acht Gegenproben mit falscher Laenge rot, dazu eine Gegenprobe der
Vorrichtung selbst (ein um ein Byte verschobenes bhSet darf nicht laufen). Ausserdem gebunden: 0x3A bhMutekiSetPl (Spieler unverwundbar, plp->flg 0x4 mit Spiegelung
nach player_flags), 0x2E bhMotionPauseSetPly (sp_flg 0x1, das Tor des Spielerpasses) und 0xA1 bhTrapDamageSet
(plp->stflg 0x1000, im Original mit dem Wert 0 gesetzt). Alle drei schreiben nur Flagbits, die die Event-VM selbst
fuehrt — sie wurden bisher gelesen und an den Raumbesitzer gereicht, der sie nicht band, liefen also ins Leere.
Dazu 0x46 bhSetNextRoomBgm und 0x47 bhSetNextRoomBgSe, die Musik- und Raumtonvorgaben fuer den naechsten Raum schreiben. Dabei ein Befund, der meine Vermutung aus dem letzten Bericht widerlegt: die Zielstruktur NextSoundInfo (sdfunc.c:90) hat im Original ueberhaupt keinen Leser — ueber alle 154 Quellen steht sie nur in diesen beiden Befehlen und in ihrer Definition. Sie sind also auch im Original ohne hoerbare Wirkung; nach einer Tuer kann deshalb keine falsche Musik laufen. Der Port fuehrt den Zustand trotzdem, damit die Befehle gebunden sind und ein spaeterer Leser die richtigen Werte vorfindet. Damit sind 204 von 223 Skriptbefehlen gebunden. 205/205 Raeume laden, 0 blockierte Befehle, Gegner-KI-Sweep
unveraendert. CTest 168/168. **Kein Commit/Push.**
(au) Kein Skriptbefehl haelt mehr einen Raum an. 0x1E bhAdxTimeCk (event.c:1523) war der letzte ohne nativen Fall.
Er ist eine Bedingung auf die Abspielzeit eines ADX-Stroms; die Modi 0..3 liefern 0, WENN die Bedingung zutrifft,
die Modi 4 und 5 liefern 1, und ein Modus ueber 5 gibt die rohe Zeit zurueck (kein default im Original). Die
Zeitquelle ist neu: AudioMixer::adx_play_time rechnet die Kanalposition des Mischers nach derselben Formel um wie
adxwrap.c:650 (SampleCount / SamplingRate * 100). Channel::clip_rate ueberlebt das natuerliche Clipende, damit ein
zu Ende gelaufener Strom seine Endzeit behaelt — sonst haengt eine Warteschleife im Skript an einer Zeit 0.
NEBENBEFUND, selbst gefunden beim Nachlesen: die Ausfuehrungsschleife der Event-VM lief bei Rueckgabewert == 1
weiter, das Original aber bei JEDEM Wert ungleich 0 (event.c:13046). Beobachtbar bei Befehlen, die etwas anderes
als 0/1 liefern koennen — bhAdxTimeCk mit Modus ueber 5 und bhCkFlg mit `mode ^ flag`. Korrigiert; CTest unveraendert
gruen, also kein ausgeliefertes Skript war bisher betroffen. Test event_opcode_lengths deckt jetzt sechzehn Befehle
ab; sechs weitere Gegenproben rot. Damit: 205 von 223 Skriptbefehlen gebunden, 18 mit korrekter Laenge uebersprungen
und gemeldet, keiner mehr unbekannt. 205/205 Raeume laden, 0 blockierte Befehle, Gegner-KI-Sweep unveraendert.
CTest 168/168. **Kein Commit/Push.**
(av) Verdeckungsmasken der Ereigniskamera am Spurende korrigiert. cut.c:1803 bhSetEventHideObjLgt schreibt die
Objekt- und Lichtmaske eines Keys EINMAL; sie bleibt bis zum naechsten Aufruf stehen. Gerufen wird sie beim Setzen
der Kamera, im Initpass und nach jedem Keywechsel — NICHT bei dem Keywechsel, der in das Endhalten fuehrt
(cut.c:2124 bricht dort mit `break` vor dem Aufruf ab). In der Haltephase steht die Kamera also auf dem letzten Key,
die Masken aber noch auf dem vorletzten. Der Port las die Masken je Bild aus dem aktuellen Key und zeigte am Ende
jeder Zwischensequenz die Verdeckung des LETZTEN Keys. RoomWorld::event_camera_mask_key_ haelt jetzt den Key fest,
mit dem das Original sie zuletzt geschrieben hat. Eigener Scan ueber alle Raumdaten: 383 von 1362 endenden
Kameraspuren in 82 Raeumen haben dort verschiedene Masken; in RM_0000 Spur 33 verdeckt der vorletzte Key eine ganze
Objektgruppe, der letzte nichts — diese Objekte wurden am Ende der Sequenz sichtbar. Neuer Test event_camera_hide
(RM_0000 Spur 33); drei Gegenproben rot. Der Befund stammt aus einer der fuenf parallelen Untersuchungen und wurde
vor der Umsetzung Zeile fuer Zeile im Original und mit einem eigenen Raumdatenscan nachgeprueft.
Auch die Wechselwirkung mit bhControlLight ist geschlossen: set_event_camera_masks schreibt die Lichtmaske
an den Originalaufrufstellen einmal in Light::flags Bit 0x2; Lichtpass und Zeichnung lesen diesen Zustand.
Typ 100/101 bleiben nach dem Erloeschen aus bis zum naechsten Maskenaufruf, ohne Uhr-/Modusreset; Typ 13
und Linktod verlieren Aktivbit 0x1, das die Maske nicht wiederherstellt (light.c:669/:680/:696/:749).
Erweiterter Test event_camera_hide: diagnostisch veraenderte Lichtfelder/Keymasken in RM_0000, vier Typen,
Lichtbeitrag am Raumdreieck, letzter Lichtpass, Wiederholung, Keywechsel, Endhalten und FixedCut.
Drei weitere Gegenproben rot; [Prueflogs](build/validation/event-camera-light-20261019/).
Frueherer Stand: 205/205 Raeume laden, 0 blockierte Befehle, Gegner-KI-Sweep unveraendert, CTest 169/169.
Aktuell CTest 169/169, Policy/Ledger gruen; Dev-App ueber ./build.sh --test gesichert, aktualisiert und signiert.
Start.command geprueft: Titel und RM_0000 mit Gegner-KI je 60 Bilder kopflos, Exit 0, keine blockierten Befehle/
offenen Weltbindungen; arm64/Signatur geprueft. [Bericht](build/validation/event-camera-light-20261019/README.md).
Keine visuelle Abnahme. **Kein Commit/Push.**
(aw) EN01 njRandom-Vorzeichen aus der Worktree-Sitzung "Check EN01 njRandom sign" in main uebernommen und
nachgeprueft: macros.h:105 mit RAND_MAX+1 = -2^31 und Disassembly 0x1882C8/0x1885E8 (rand -> negu -> cvt.s.w ->
div.s durch 0xCF000000) ergeben einen POSITIVEN Wert in [0,1]; en01_core.c rechnete negativ, Groesse und
Geschwindigkeit der ExpLeg-/ExpWorm-Splitter lagen dadurch unter dem Originalbereich. Alle anderen Gegnerports
waren schon positiv. Neuer Test enemy_en01_exp_effect (428 Pruefungen), Gegenprobe alter Ausdruck rot.
Der Worktree selbst ist verworfen. **Kein Commit/Push.**
(ax) 0xAF bhPlyHandChange (event.c:7378) gebunden statt nur gemeldet: zwei Raumobjekte haengen an den Haenden
(lkono 9/13, flg 0x80, stflg 0x1000000) und die Waffenmodelle obwp[0]/[1] verschwinden per mdflg 0x1 aus der
Zeichenliste (neuer Besitzer CombatController, Ruecksetzen durch gameover.c:304 und beim Laden einer Waffe).
Ablegen laesst das Objekt an der letzten Handlage. Daten: RM_9110, RM_9140, RM_9260. Offen und benannt: das
Parken eines gezogenen Feuerzeugs, weil plp->wpnr_no nativ nicht vom Inventarplatz getrennt ist. Test
player_hand_change mit echten RM_9110-Bytes, acht Gegenproben rot. **Kein Commit/Push.**
(ay) Waffe in Zwischensequenzen und beim Gegnergriff korrigiert: der Port zeichnete bei Skriptsteuerung das
Modell von Waffe 0 (leere Haende) statt der ausgeruesteten Waffe — Ersatzlogik vom 2026-10-05, schon damals
ohne Originalbeleg vermerkt. Das Original wechselt das Waffenmodell nur ueber bhReadWeaponData (Ausruesten,
bhArmsItemChange, Spielstart) und zeichnet es unter pt_flg 0x1 (objitm.c:364/:648); dieses Tor fehlte nativ
ebenfalls. Test player_hand_change erweitert, zwei Gegenproben rot. Offen: 0x27 bhArmsItemChange nativ
falsch abgebildet (loescht den Inventarplatz), braucht einen eigenen plp->wpnr_no-Besitzer. **Kein Commit/Push.**
(az) Gezogene Waffe plp->wpnr_no vom ausgeruesteten Platz pip[0] getrennt (Inventory::wpnr_no). Vorher leitete
der Port die Waffe aus dem Platz ab. Jetzt schreiben WeaponSet/GetItem/ItemBoxChange beides, bhArmsItemSet 0xC1
und bhSv 22 nur den Platz, bhArmsItemChange 0x27 / bhSv 18 / 0xAF nur die Waffe; Spielerwechsel und Spielstand
fuehren sie ueber ply_wno. Damit behoben: 0x27 mit 0 loeschte den Inventarplatz, andere Werte wirkten nicht; und
das Feuerzeug-Parken aus 0xAF ist portiert (0xAF jetzt vollstaendig). Grenze: bhSv 18 laedt im Original kein
neues Waffenmodell, nativ schon (im Raumscan nie erreicht). Vier Tests setzten nur pip[0] in der Aufstellung
und sind ergaenzt bzw. auf das Original korrigiert. CTest 172/172, fuenf waffenrelevante Laufzeitpruefungen
gruen, acht Gegenproben rot. [Logs](build/validation/wpnr-no-20261020/). **Kein Commit/Push.**
(ba) 0x97 bhEffectKokuenSet korrigiert und 0xAA bhEffUVSet2 portiert. 0x97 stand ungeprueft im Arbeitsbaum und
setzte den Rauch an die Weltlage des Taskworks; das Original nimmt die lokale Lage von objP[0]. 0xAA schreibt
UVs als 28-Byte-H-Eckpunkte nach op->pv; bei Effekten ohne H-Eckpunkte reicht das in tv hinein. Einzige
Datenstelle RM_9180 (Vollbildfilter bhEffFil): dort aendert das Original damit tv[1].u - nativ jetzt ebenso.
Test event_kokuen, sechs Gegenproben rot. Damit noch uebersprungen: 0x6F, 0x82, 0xD3. **Kein Commit/Push.**
(bb) 0x6F bhObjLinkSetObjPly portiert: das Skript haengt die Spielerin an einen Objektknoten, bhCalcModel(plp)
baut ihre Wurzel dann aus obwp[lkwkp].owP[lkono] * T(lox,loy,loz) * RXYZ(ax,ay,az) (MdlPut.c:242). Nativ
PlayerObjectLink + PlayerAsset::sample_root in RoomWorld::sample_player. Einzige Datenstellen RM_4011 (Objekt 6,
Knoten 3, skriptgesteuert). Offen und benannt: Bindung im freien Spielerpass (in den Daten nicht vorhanden).
Test player_object_link mit echten Bytes, sechs Gegenproben rot. Uebersprungen bleiben nur 0x82 (Debugkamera, in
den Daten unbenutzt) und 0xD3 (Battle-Game-Rangliste). **Kein Commit/Push.**
(bc) Offene Lichtluecken und parallele Aufgaben (2026-10-20). (1) Bildschirm-Clipping der Punktlichter (light.c:789-808)
portiert: ohne Ereigniskamera und mit cutp[cam.ncut].cam[0].flg 0x10 faellt ein Punktlicht ausserhalb des 640x480-Fensters
mit Rand 2*(u8)ci->frame (CAM_WRK Byte 2, SceneCamera::key_frame) weg, ohne einen der drei Plaetze zu belegen; 14 Schnitte in
11 Raeumen tragen das Flag. Das Monitorbild zeichnet unter gm_flg 0x80 (screen.c:947) mit dem Einzellicht und ist nicht
betroffen. Linktod lkflg 1..4/default war schon in control_lights, die Ledgerzeile sagte es nicht. Test light_clip
(RM_0000-Bytes, eigene Projektionsrechnung), fuenf Gegenproben rot. (2) bhAllEasyDrawModel (game.c:391-403) ruft kein
bhDrawEneObject: der Monitorpass laesst en_obj-Works (flg 0x80000000) jetzt weg; kein Datenfall (en_obj nur RM_7120/9290,
Monitore nur RM_1050/7030), kein Test. (3) Parallel durch Unteragenten, nachgeprueft: bhEff199 (effsub0.c:3478) als
native/effect_eff199.cpp mit Test effect_eff199, partial (Ps2_ptcl_spr_flag, Steuerpass-Zeichnung); in Daten/Skripten
unerreichbar. Effekt 298 bhEff_E00_AcidGenerator ist seit 2026-10-16 portiert — der Hinweis "bhEff298 unportiert" oben
(EN19-Eintrag) ist veraltet, ebenso "EN14-Luecken": Inventur ergab alle EN14-Dienste gebunden, EN14 nur in RM_5810/9302
(Typ 0), neuer Test enemy_en14_world; dessen erste Erwartungen widersprachen dem Original (en14.c:297 loescht flg 0x10,
bhSetMotion laeuft im selben Pass) und wurden korrigiert. (4) bhDrawObject/bhDrawObjItm mdflg 0x10 (ETTY flg 0x80000000,
objitm.c:164/:228) portiert: unter st_flg 0x2 zeichnen diese Objekte/Items ohne Nebel (:664-676/:565-567; nicht ob_spc);
23 Objekte und 5 Items in 23 Raeumen tragen das Bit (u. a. RM_0090 Objekt 20, RM_0000 Item 0). mdflg 0x8 njControl3D(0x2500)
ist wirkungslos (ps2_NinjaCnk.c:205-215 liest 0x2000/0x400 nicht). Test object_cut_restriction Abschnitt 3, zwei Gegenproben
rot. [Logs](build/validation/light-clip-20261020/), [eff199](build/validation/eff199-20261020/),
[Nebel](build/validation/object-fog-20261020/). **Kein Commit/Push.**

(bd) Waffenmodell-Ladeauftrag von plp->wpnr_no getrennt (2026-10-20): bhSv 18/0xAF aendern nur den
Spielerwert; nur mn_mode0 3 (WeaponSet/GetItem/ItemBoxChange/0x27) und Spielstart/Spielerwechsel/Laden
ersetzen das Waffenwerk. Ein 0x27 mit identischer Nummer laedt erneut und nullt mdflg, bhSv 18 nicht.
ply_wno wird bei bhSv 18/0x27 sofort mitgeschrieben. Test player_weapon_number erweitert, drei Gegenproben
rot; Zwischenstand CTest 177/177. [Beleg](build/validation/weapon-reload-20261020/). **Kein Commit/Push.**
(be) Drei Lichtluecken parallel (drei Unteragenten, vom Hauptprozess integriert und nachgeprueft, 2026-10-20). (1) Effekt-
modelle 364 (Steve-Nachbilder, Albinoid-Blitz) zeichnen jetzt auch im zweiten bhDrawEffect des Spiegelpasses, mit dem mdflg
des Effekts als Tor; zugleich behoben: sie wurden bisher gar nicht beleuchtet (Lichtlauf endete vor ef_fnc). (2) bhSetEasyDirLight
Typ 100/101 schreitet im bhControlLight-Aufruf unter gm_flg 0x80 fort; die Zeichenpass-Aufrufe und getrennter Easy-/Simple-Satz
bleiben benannte Luecke. (3) Lichtlink lkflg 5 auf eff[efid] portiert (in den Raumdaten unbenutzt); Nebeneffekt: bhDrawPolEffect
zeichnete bei fehlender Elternmatrix mit Einheitsmatrix (Ersatz entfernt, jetzt Luecke). CTest 180/180, 12 Gegenproben rot,
Starter: Titel, RM_7100/9280/1020/0000 ohne blockierte Befehle und ohne neue Luecken, arm64/Signatur ok. Berichte
work/{light_effect_link,easy_dir_light,effect_model_mirror}.result.md, Logs build/validation/light-link-easydir-20261020/ und
build/validation/effect-model-mirror/. Keine Sichtabnahme. **Kein Commit/Push.**
(bf) Zwei Wellen mit je fuenf Unteragenten (2026-10-20), jede Integration vom Hauptprozess am Original nachgeprueft,
CTest 188/188, Dev-App ausgeliefert, Titel/RM_0000/0080/0090/2050/3130/5810/7030/9280 ohne blockierte Befehle und ohne neue
Luecken (RM_7030 hat 3 offene Weltbindungen, am HEAD-Stand gemessen identisch), arm64/Signatur ok. Welle 1: verify_combat-
Erwartung korrigiert (Steve schiesst in RM_0030 dreimal ueber plp->wpnr_no, fuenf ARMS-Klaenge sind originalgetreu);
Kopfsuche bhSearchNearEnemy2 aus ene[]/owP[lok_jno]; 15 Effektprozeduren (u. a. 273 Alexia-Schleimfaeden RM_5810/9302 mit
eigenem Weltest, 357, 398). Welle 2: Skriptbefehle 0x6E bhHEffectSet und 0x4A bhYakkyouSet gebunden; Kamerawackeln (EN17
nullte ofx/ofz, Raumwechsel liess Wackeln stehen); Item-Licht (Glanztor, Monitor Stufe 7 blau, Licht-1-Nachwirkung);
Gegnergelenke der Maskenfiguren id 91..99 logisch statt physisch (Effektlinks lkono 28..30, EN16-Treffer, dam[27..30]);
Koerper-Wandpass mit plp->pxb; Common_controll 10 schreibt kein pxb mehr. RM_0090-Box: keine Regression gefunden
(17.10./18.10./HEAD bytegleich); belegter Kandidat: Untersuchen und sofort abbrechen laesst die Klappe von Box 5 offen,
weil Ereignis 19 waehrend seiner 30 Bilder vom Boden-4-Neustart verdraengt wird, sobald st_flg 0x8 faellt - nativ nach
~19 Bildern; die Haltedauer des Originals ist nicht vermessen (Statustask-Durchlaeufe sub1.c:3103/system.c:599-611 offen).
Der Deckel wird von keinem Skript geschrieben. Test rm0090_security_box_nodes. Gegenproben build/validation/wave1-20261020/,
wave2-20261020/. Keine Sichtabnahme. **Kein Commit/Push.**

**2026-10-19, Greif-und-Dreh-Statue (bhObj007 + bhCPM2_act_rpsh) portiert (Dev-App via `./build.sh --test`, CTest 158/158):**
Nutzerbefund "ich kann die Statue nicht drehen" (als RM_1050 gemeldet). Ursache: der Greif-und-Dreh-Gegenstand ist **unportiert** — bhObj007
(objitm.c:1648, bhJumpObject[7], 100%-matching) und die zugehoerige Spieleraktion bhCPM2_act_rpsh (player.c:4336, mode2 22). Der Objektpass
dispatchte nur bhObj005/008/010, der Spielerpass kannte mode2 22 nicht. RDT-Scan aller 205 Raeume: einziges Typ-7-Objekt ist **RM_0150 Objekt 5**
(ident 46, Eisenstatue; Ereignis 16 hebt sie auf py 10) — **RM_1050** (Stufe 1 Raum 05, Steve/Monitor, 18 Objekte alle Typ 0) **hat keine Statue**.
Port: `control_turn_objects`/`control_turn_object` (bhObj007, objitm.c:1648-1845 mit Originalzeilen) + `tick_turn` (rpsh), mode2-22-Zweig in
runtime.cpp analog bhCPM2_act_psh, Pad-Bit-Korrektur 0x21/0x42 (pad.c Analog-vorwaerts/-zurueck statt nur 0x20/0x40). Test `turn_object`
(RM_0150, Greifen -> Drehen bis |ay|<182 -> ev_flg 2 -> Freigabe, Gegenprobe rot). StartVibrationEx als benannte Host-Luecke.
**Nachkontrolle des Unteragenten-Diffs (2026-10-19, jede Zeile gegen objitm.c:1648-1845 / player.c:4324-4490 nachgelesen), vier Abweichungen behoben:**
(1) Die Statue drehte nur in `angles[1]`, die Zeichenwurzel liest aber `transform.yaw` (RoomWorld::append) -> sichtbar erst der Endsprung auf 0;
jetzt je Drehbild `transform.yaw = angles[1]` (Test prueft die Mitdrehung, Gegenprobe rot "Zeichenwurzel folgt op->ay nicht").
(2) `ABS((short)plp->ayp - ayn)` mit `short ayn`: int-Differenz zweier shorts ohne Nachtruncation; der Port truncierte die Differenz
auf int16 (ein Bild Unterschied an der Wrap-Grenze). (3) Kopf-Latch `head_push_owned` wurde zwischen Schiebe- und Drehzweig zurueckgesetzt,
EXP1_I(0)-Freigabe lief so jedes Bild statt nur in Fall 0 (idempotent, aber nicht Original); Latch jetzt geteilt. (4) `plp->px = PEXP0_F(72) = ...`
(spx/spz = collision_safe_) in Fall 1 (:1716) und 2/3 (:1786/:1826) fehlte -> nach dem Loslassen haette der Settle-Schritt von act_std
zur alten sicheren Lage zurueckgezogen. Annahme bleibt: plp->ar = 3.5 (PlyInfo[*][0]); en19.c:501 (ar_bak) nicht gleichzeitig. CTest 158/158.
**Nachgezogen (dieselbe Ursachenkette, Dev-App via `./build.sh --test`, CTest 158/158):** Player_controll 128 (event.c:9640) war im Port
unvollstaendig — world.cpp op 0x64 0x80 behandelte 128 wie 139 und liess `mtn_add=65536`, `flg|=0x10` (Koerper-Wandpass, player.c:1600),
`mnwP=plmthp`, `mode0=1`, `stflg&=~0x10000`, `flg&=~0x210000` weg. Folge: nach dem Freigabe-Cinematic blieb der Spieler in mode0 7 / flg
0x10000 haengen und konnte nicht zur Statue laufen (alle Raeume mit Spieler-Freigabe-Cinematics). Jetzt voll portiert; Test
`event_lifecycle_control_release` (bhLoadWork 0 -> Player_controll 128 -> mode0 1, flg/stflg 0x10000 weg, flg 0x10 an; Gegenprobe rot
"mode0 bleibt 7"). **Kein Commit/Push.**

**Zu verifizieren: RM_0090 Security Box verformt** (Nutzerbefund 2026-10-18): nach Abholen der Adlermedaille an der *anderen* Box ist die
vordere Box (obj[5], Kamera 9) auf dem Rueckweg verformt (Klappe offen + Deckel hochgeklappt), ohne dass sie benutzt wurde; beim Untersuchen
bewegt sich die Klappe nicht. Laut Nutzer am 2026-10-17 noch nicht vorhanden -> Regression aus den Aenderungen seit gestern. Nutzerverdacht:
3D-Scanner/Fraese (Ereignis 23) oder Zombie-Fensterspawn. Headless nicht nachgestellt: alle 99 Ereignisse und 20 Interaktionen von RM_0090
einzeln (mit Gegner-KI) lassen obj[5] unveraendert; Raumskripte schreiben nur Knoten 1 (Ereignis 31 +75 Grad relativ, 7/19 absolut 0),
nie den Deckel. Naechster Schritt: F5-Snapshot vom Nutzer im kaputten Zustand, dann Knotenwinkel direkt auslesen; alternativ Differenz
gegen den Stand vom 2026-10-17 (HEAD-Worktree) im selben Ablauf.

**RM_9370 Raumeffekte 35/50/54/70-76/80/400/402/405/426 (2026-10-19 geprueft, keine Luecke):** das sind Texturhalter, keine Prozeduren:
room.c:306 laedt fuer Tabelleneintraege mit id 30..99 und >= 400 den Atlas (sys->ef_tn[id]); in bhJumpEffect (effect.c:33) stehen 35/50/54/70-76/80
auf bhEffDmy, 400+ liegen hinter bhJumpEffect4[50]. Alle 16 Halter dieses Raums sind in native ported_effect_atlas (effect_sprites.cpp:1070)
enthalten; die Prozeduren, die sie nutzen, sind separat gefuehrt. Weiter offen: sys->obwp[0..3] (Waffe rechts/links, Zopf) ueber
Art 2 nicht aufloesbar (Waffe ohne eigenen owP-Baum, Zopf nur als Nachbildmodell) - Typen 1/3/4/5 von 0x86 mit Art 2
Index 0..3 (in den Daten nicht genutzt); Art-1-Knotenlagen von 0x86 nicht je Raum gemessen. **Kein Commit/Push.**

**Effekt 006 + leere Prozeduren 4/28/129/174/197 (2026-10-20, Worker T01/T02, geprueft):** `bhEff006`
(effsub1.c:2214) von Hand portiert (ef_opq bei mdlver 0, sonst ef_trs; Alpha-Abbau type != 0; Hoehenende
mtx[13] < -100 ueber `mtx_world_y` wie 352). Die leeren Originalprozeduren sind als `step_eff_empty`
gebunden (Platz bleibt belegt, ungezeichnet; nicht bhEffDmy). Erzeuger: nur Skriptbefehle 0x2C/0x6E/0xB0;
Byte-Obermengenscan aller Ereignisbereiche der 205 Raeume findet keinen davon (einziger Treffer RM_6070
+1306 liegt im Operanden eines 0x7c). **367 bleibt Luecke** (kein exp0-/func-Erzeuger im Original).
Test `effect_eff006`; `engine_progression_resources` nutzt jetzt 367 als ungebundenes Beispiel.
`./build.sh --test` 190/190 (gpu_raster_parity uebersprungen), Dev-App aktualisiert. Offener Nebenbefund:
`initialize_effect_sprite` setzt die UV-Vorgaben von bhSetEffectTb (effect.c:522-532) nicht. **Kein Commit/Push.**

**Ablauf/Speichern finalisiert (2026-10-20):** Ledger-Audit des Bereichs. Sechs Eintraege auf done, weil die
damals offenen Punkte belegt sind: `bhAllDrawModel`/`bhEtcMirrorDrawModel` (Spiegel-Effektpass unter gm_flg 0x4000,
mirror_pass/effect_model_mirror/claw_model_light), `bhAllEasyDrawModel` (Monitorpass, event_lifecycle),
`bhControlEOR` (BackColor nativ ueber back_color.cpp; der GS-Palettenupload ist die Konsolen-Fadedarstellung, nativ
Host-Fade), `bhSysCallMain` (Pad-Gating; `ss_flg 0x40000000` hat keinen Schreiber, `tk_flg 0x1000` ist der
Host-Taskautomat) und `bhSysCallMonitor` (Raumlade-/Taskfreigabe, A/B-Messung). Fuenf weitere Eintraege
sind als **Host-ersetzt** gewertet (Nutzerentscheidung 2026-10-20): `pad.c:bhSetPad` (SDL-Eingabe statt Keytype-/Analogtabellen),
`ps2_LoadScreen.c:ExecuteStateLoadScreenLoadExit` (Typewriter-Slotliste statt typ_flg/tk_flg), `system.c:bhCheckSubTask`
und `system.c:bhSysCall` (Host-Menuezugaenge statt tk_flg-Taskautomat) sowie `system.c:bhSysCallEvent:Pausetor`
(Host-Pause statt ssv_md0 10). Bereich 22/36 -> **28/31** (95.2 % gewichtet, 90.3 % voll belegt); Gesamt
88.1 %/76.8 %. Offen bleiben nur `ps2_McSaveFile.c:mcSetStringSaveFile` (Original-UB saveroom 0),
`system.c:bhSysCallDoordemo` (Nutzerentscheidung Tuer-Skip) und `system.c:bhSysCallMap` (Drawreihenfolge/Padbindung).
**Kein Commit/Push.**

## Gezielter Zugang zu bisherigen Befunden

Vollständiger [Abschnittsindex](archived/work/history/2026-10-13-instructions/README.md)
zu den unveränderten früheren Anweisungsdateien. Nicht vollständig laden; bei
Bedarf den betreffenden Abschnitt lesen. Folgende Links sind **historische
Befunde**, keine aktuelle Offen-/Erledigtliste:

| Thema | Historischer Einstieg |
|---|---|
| Tür-Skip / gezielter Türton-Stopp | [CLAUDE vor Trennung:26](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:26); aktive Entscheidung in AGENTS.md |
| SELECT-Skip / Standin-Claire | [CLAUDE vor Trennung:947](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:947) |
| Hauptmenü / Host-Abweichungen | [CLAUDE vor Trennung:1039](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1039) |
| Friedhof / Skript-Gegner-Motion | [CLAUDE vor Trennung:1094](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1094) |
| Ereigniskamera / Pause | [CLAUDE vor Trennung:1131](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1131) |
| Dialog-Padflanke | [CLAUDE vor Trennung:1213](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1213) |
| Objekt-Cut-Gate / Leuchten | [CLAUDE vor Trennung:1285](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1285) |
| Körperpass / Treppen / Hundstoß | [CLAUDE vor Trennung:1346](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1346), [Reihenfolge:1506](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1506) |
| Renderer-Worker / Nebelkosten | [CLAUDE vor Trennung:1421](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1421) |
| Karte: Eingabesperre und damaliger Draw-Vertrag | [CLAUDE vor Trennung:1539](archived/work/history/2026-10-13-instructions/CLAUDE.before.md:1539); spätere Kartenrenderer-Arbeit nicht aus dieser alten Offenmeldung zurückbauen |
| Audio / Gegner / Schatten / Inventar | [AGENTS-Abschnittsindex](archived/work/history/2026-10-13-instructions/README.md); passende `work/*.result.md` gezielt lesen |

## Pflege

Nur aktuelle, auftragsrelevante Kurzstände mit Beleg-/Ergebnislink aktualisieren.
Abgeschlossene Detailprotokolle nach `archived/work/` beziehungsweise `build/validation/`,
nicht in diese Tabelle oder die Pflichtdateien kopieren. Fachseiten bleiben
undatierte Mechanismusbeschreibungen. Historische Kern-/Generatorfreigaben sind
verworfen und heben die aktuelle Originalkern-/Emulationspolicy niemals auf.
