# Event-Parser-Audit gegen event.c (2026-10-14)

Abgleich `native/events.cpp` (EventVm) und der Weltbindung `RoomWorld::bind`
(`native/world.cpp`) gegen `src/ps2/veronica/prog/event.c` (Semantikreferenz) und
gegen alle 205 Raumskripte in `data/rooms` (scd0, scd1, alle Events; linear
dekodiert mit den Originallängen). Werkzeuge nur in `$TMPDIR`, nichts gebaut/gelinkt
aus dem Original.

## Warum ständig Fälle nachgereicht werden mussten

Der Parser selbst (Opcode-Längen, Sprünge, Schleifen) ist weitgehend korrekt. Die
Fehler entstehen an drei Nähten, die kein Test systematisch abdeckt:

1. **Seiteneffekte in aufgerufenen Engine-Funktionen.** Viele Befehle ändern
   Systemflags nicht selbst, sondern über `bhSetEventCamera`, `bhSetFixedCut`,
   `bhCheckCut`, `bhSetEventFixedCut` usw. (cut.c). Nativ geht der Befehl als
   `world()`-Bindung an `RoomWorld`, die die Kamera stellt, aber die Flags in der
   EventVm nicht. Beispiele: 0x2a (`gm_flg 0x1000`/`fixcno`, RM_0090-Boxen) und
   0x13 (`st_flg 0x1`, RM_1002, siehe unten). Offen derselben Art:
   `gm_flg 0x20` aus `bhSetEventCamera`/`bhSetFixedCut`/`bhSetEventFixedCut` hat
   keinen Besitzer (Leser `effsub3.c:738/773`, `screen.c:930`).
2. **Zwei Zustände für dieselbe Work.** `Common_controll` schreibt im Original
   `bhCetask->work`. Nativ schrieben 7/11/2/4 nur den Host-Transform, nicht die
   native Work (`w.px/py/pz`, `w.ax/ay/az`), und Common 24/25/34 rechnete bei
   Subplayer-Works den Baum (`v0`: bhSetMotion/bhCalcModel) nicht sofort. Jede
   Folgebefehl, der `owP` liest (bhMotionPosSetEnePly, Links, Effekte), sah dann
   einen veralteten Baum.
3. **Abbruch statt Lücke.** Ein unbekannter Opcode/Unterbefehl wirft und blockiert
   die Task dauerhaft (`t.blocked`). Ein ganzes Event bleibt dann stehen, ohne
   dass es im Spiel sichtbar gemeldet wird.

## In dieser Sitzung behoben (Arbeitsbaum, nicht installiert)

| Fehler | Original | Nativ vorher | Fix |
|---|---|---|---|
| bhCamSet setzt `st_flg 0x1` nicht | cut.c:1690 / :1764 | fehlte → `bhCamCheck` (event.c:4903) immer 1, RM_1002-Kinoszene hing ohne Skip ewig | `RoomWorld::set_event_camera` setzt `flags[5] |= 1` |
| Common 34/24/25 `v0` bei Subplayer | event.c:11940-11967, :11069-11096 | kein sofortiges bhSetMotion/bhCalcModel | `enemy_script_evaluate(e, byte 6)` auch im Subplayer-Zweig |
| Common 7/11/2/4 schreiben nicht die Work | event.c:12034/12068/12167/12223 | nur `e.transform` | bei gebundener Work zusätzlich `w.px/pxb…`, `flr_no`, `w.ax/ay/az` |
| bhRandamSet/bhRandTest/bhCmpB 17 | event.c:4942/6407/701: immer `bhEtask[0]` | Ergebnis in aufrufender Task, `bhCmpB 17` las nie geschriebenes `values[17]` → **jede Zufallsverzweigung nahm Fall 0** (142 Stellen, z. B. RM_0000 scd1, RM_1100 scd1, RM_0090 ev33) | Schreiben/Lesen über `tasks_[0]`; `Event_init` lässt `wpnl_no`/`addpx` stehen |
| (vorher, anderer Agent) bhCamSet2 0x2a | event.c:994, cut.c:448 | `gm_flg 0x1000` blieb, `fixcno` fehlte | bereits in e6904d7 |

Nutzerfehler „RM_1102“: gemeint ist **RM_1002** (Raum RM_1000, Zustand 2) — Hotspot 0
an der Palasttür startet Events 0–9 (Wesker-Szene). Skip (SELECT) läuft über
scd1 (`cb_flg 0x10000000` = bhCk 10/3) → Event 10 → `Common 34` (Motion 9,
Bild 630) → `bhMotionPosSetEnePly`. Vorher stand Claire bei (106.8, 0, 1.7) hinter
der Palasttür (veralteter Baum der Double-Work), jetzt am Endpunkt der Szene
(127.9, 0, 94.9) im Hof. Ohne Skip hing die Szene vorher an `bhCamCheck`.
Belege: `build/validation/door1102-claude-20261014/` (Replays `sk.txt`/`ns.txt`,
Zustände, Endbilder). Die Startposition ist eine Diagnoseposition.

Restbeobachtung: beim Ende der Szene (`bhCineSet 2`) rutscht Claire in einem Bild
von z 94.9 nach 69.5 (Wand-/Bodenausgleich). Nicht geklärt, ob das Original
dasselbe tut.

## Offene Lücken (nach Daten gewichtet)

### A. Opcodes ohne nativen Fall (werfen → Task blockiert)
- **Nachtrag 2026-10-14: `0x73/0x74` und `0xa8/0xa9` portiert** (event.c:5454/5516/7067/7119).
  `e_lgt[2][5]` liegt im Task-Kontext (`RoomWorld::Context`), Start interpoliert mit
  `njLinear` von `e_lgt[1]` nach `e_lgt[0]` bei `(1/cnt3)*cnt2`; Licht schreibt
  `lgtp/evlp[v0]` r/g/b/nr/fr (wie 0x78), Nebel `sys->fog_col` (Byte-Reihenfolge wie
  bhFogColorSet), `st_flg 0x100000`, `gm_flg 0x10`. Beide Start-Befehle geben 0 zurück.
  Replay RM_1050: Events 36–41 laufen bis bhEvtEnd, 0 blockierte Befehle.
In Raumdaten benutzt (Vorkommen):
`0x1e bhAdxTimeCk` 239 (RM_2031), `0xaf bhPlyHandChange` 48 (RM_9110),
`0x40/0x41 bhLookGsetPlStart/Stop` 27/1 (RM_3000, RM_6030), `0x73/0x74
bhLightParameterCSet/Start` 20/20 (RM_1050 Events 36–41), `0xc0 bhCyodanSetEx` 16
(RM_0090 ev23), `0xc8 bhEffBakuDrmSet` 11, `0x98 bhEffectSandSet` 9, `0x97
bhEffectKokuenSet` 8, `0xb0 bhHEffectSet2` 7, `0xd3 bhRankingCall` 6, `0xb1
bhObjDposCk` 5, `0xa8/0xa9 bhFogParameterCSet/Start` 4/4, `0x6f
bhObjLinkSetObjPly` 3, `0x96 bhEffectSensyaSet` 3, `0xaa bhEffUVSet2` 1,
`0xb3/0xb4 bhEtcAtariEnePosSet/EvtPosSet` 1/1 (RM_0110 scd0/scd1!).
Unbenutzt: `0x82`, `0xb5`. Längen stehen fest (siehe Tabelle unten).

### B. Unterbefehle ohne nativen Fall (werfen)
- **Nachtrag 2026-10-14: `Player_controll` 150/129/130/142 portiert** (event.c:9275/9290/9353/9385)
  auf den Kopfbesitzer `RECVX_HEAD_STATE` (Flags `EXP1_I(0)`, Nickfolge `EXP1_I(84..112)`,
  `EXP1_UC(120)`; Bindung `RoomWorld::bind_player_head`). 129/130/142 nur für die Spieler-Work;
  auf einer anderen Work bleibt der Befehl gemeldete Lücke. `EXP1_I(92/104/116)` haben keinen
  Leser (player.c:6776-6818) und werden nicht geführt. Replay RM_1050 (Wesker-FMV-Event 0,
  Video per Skip): Event läuft jetzt bis bhEvtEnd 3118, Steuerung kehrt zurück; vorher bei
  0xb3e blockiert. Weiter blockiert in RM_1050: Events 36–38 (`0x73`, `0xa8`).
- `Player_controll` (0x64) **150** (23×, u. a. **RM_1050 ev0 – Wesker-FMV-Event
  blockiert bei 0xb3e**, RM_1040, RM_5650…), **129** (17×), **130** (7×), **142**
  (10×). Im Original zusätzlich 131/132/140/141/143/145 (unbenutzt).
- `Common_controll` (0x69) **53** (RM_0080 ev7/8), **51/52** (RM_0110 ev60),
  **10** (RM_9301 ev15); im Original zusätzlich 0/22/35/43.
- `Obj_controll` 12 (RM_3020 ev6): im Original `default: return` ohne Vorschub —
  das Byte wird danach als eigener Opcode gelesen. Nativ Abbruch.

### C. Ohne Weltbindung (still „pending“, Skript läuft weiter)
`0x6d bhCyodanSet` 664, `0x5a bhCamYureSet` 374, `0xd1 bhEffSSizeSet` 359,
`0x2f bhInitSetKage` 119, `0x80 bhInitPonySet` 77, `0xca bhEffClearEvt` 55,
`0x4a bhYakkyouSet` 54, `0x42 bhItmAspdSet` 36, `0x2e bhMotionPauseSetPly` 32,
`0x88 bhEffUVSet` 27, `0x7e bhKageSwSet` 26, `0x30 bhInitMotionPauseEx` 18,
`0x6c bhObjAlphaSet` 17, `0x2d bhInitMotionPause` 12, `0xce` 10, `0x3a` 7,
`0x6e` 7, `0xa1` 2, `0xcf` 1.

### D. Semantische Abweichungen im Interpreter
1. **Rückgabe 0 innerhalb eines IF.** Original (bhEventScheduler2, event.c:13045-13059):
   jede 0-Rückgabe bei `bhIfelFlg > 0` springt an das IF-Ende und läuft im selben
   Durchlauf weiter — auch bei bhSleep/bhSleeping/bhEvtNext/bhEvtEnd. Nativ geben
   diese 2 zurück und die Task wartet innerhalb des IF. Datenstellen: RM_0090 ev33
   (zwei `sleep 1` im IF → nativ je ein Bild zu viel), RM_6080/RM_9080 ev42/50/51/58
   (`bhEvtEnd` im IF; ohne sichtbare Folge, weil danach sofort ein Sleep kommt).
2. **G_Sp ist kein Stapel.** `bhIfelCk` überschreibt den globalen Zeiger, `bhEndif`
   zählt ihn nur um 4 Bytes zurück. Eine 0-Rückgabe in einem äußeren IF nach einem
   abgeschlossenen inneren IF springt im Original an `innerer Zielpunkt − 4`. Nativ
   echter Stapel. In den Daten nicht systematisch gesucht.
3. **bhSv-Schlüssel ohne Originalwirkung:** 16 schreibt im Original
   `plp->flr_no` (19×), nativ `values[16]`, das zugleich als `flr_idx` (bhCk 10/22,
   Bodenauslöser) dient — Vermischung zweier Felder. 18 (`plp->wpnr_no`,
   `ply_wno`, 17×) ändert die Waffe nativ nicht. 2 (`cam.ncut`) wird von der
   Runtime jedes Bild mit `camera_index` überschrieben. 19 (`ef_slow`) ohne Leser.
4. **bhEvtOn ohne freie Ebene:** Original nimmt dann Ebene 15 und überschreibt sie
   (`Event_exec`); nativ wird nicht gestartet.
5. **Common 4** setzt im Original auch `ax/ay/az` (addax…), nativ nur die Lage;
   Common 8/9 schreiben `objP[model_cno]` des Modells, nativ `node_overrides`.

### E. Daten, die linear nicht dekodierbar sind
RM_2031 ev0/1 @3366 (`69 4b`, Common 75 existiert nicht), RM_3091 ev3 @1756
(`0xe0`), RM_7250 ev8–20 (Offsets zeigen in Datenbereiche). Wahrscheinlich
unerreichbare Pfade; vor einem Fix im laufenden Spiel bestätigen.

## Längen (Opcode → Bytes) für A
1e:6, 40:8, 41:2, 6f:12, 73:22, 74:4, 82:4, 96:2, 97:8, 98:18, a8:10, a9:2,
aa:10, af:4, b0:22, b1:6, b3:8, b4:4, b5:4(+Fall), c0:28, c3:4, c8:8, d3:2.

## Prüfung
- `check_no_original_core.py` vor jedem Build PASS.
- `build/door1102-claude-20261014`: ctest 86/86 grün.
- `event_lifecycle_control_release` war mit der Zufallskorrektur rot: der Vergleich
  „gezeichnete Gegnerdreiecke Skip = natürliches Ende“ zählte Effektsprites mit
  (Standardkategorie 1), deren Satz jetzt von bhRandamSet abhängt. Gemessen: ohne
  Zufallskorrektur grün, mit ihr rot (ein Sprite id 124, 2 Dreiecke). Zähler auf
  Gegnermodelle eingegrenzt (`after_modifiers`), die Prüfung selbst unverändert.
- Keine neuen Einheitentests für die vier Fixes; Nachweis über Replays RM_1002.
- Nicht ins Paket/Starter installiert, kein Commit.

## Nachtrag: verify_event_freeze (RM_0050 Hundefalle) nach der Zufallskorrektur

Gemeldet rot. Gemessen in frischen Builds (Temp-Kopien, nicht `rsync -a`: erhaltene
Zeitstempel hatten einen veralteten Vergleichsbuild erzeugt): ohne alle Event-Fixes grün,
mit ihnen rot; einzeln zurückgenommen ist nur die bhRandamSet/bhCmpB-17-Korrektur
verantwortlich. Die Korrektur ist richtig (event.c:4942 `bhEtask->wpnl_no = v0 % v1`,
:701 liest `bhEtask->wpnl_no`); die Prüfung hing am alten Fehler.

RM_0050 scd0 @708 zieht 0..9: 0–5 löschen, 6–9 setzen `rm_flg 9`. @920–1086 schaltet danach
Bodenfläche 7 (x −142) bzw. 23 (x −194) als Auslöser von bhEne04_AtariCheck frei; der Hund
springt in Falle 22 bzw. 10 und setzt `rm_flg 15` bzw. 14 (en04.c, `1 << (prm3+16)`), scd1
@1766 startet Event 15 bzw. 16. Vorher las bhCmpB 17 immer 0 → nur Falle 15 war je erreichbar.
`tools/verify_event_freeze.py` liest jetzt `rm_flg 9` nach dem Laden und stellt die
Diagnoseposition in die freigeschaltete Fläche; geprüft wird das Event des Originalzweigs.
Beide Zweige gemessen grün (aktueller Stand: Falle 16, ohne Korrektur: Falle 15).
