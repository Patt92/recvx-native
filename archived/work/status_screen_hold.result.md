# status_screen_hold — Statusbildschirm-Taskrahmen (Monitor Modus 5, ItemTaskCheck/taskloop)

Stand 2026-10-20. Arbeitsbaum geändert (eigene Dateien), gemeinsame Dateien nur als Patch.
Nicht gebaut in `build/macos-arm64`, keine App/kein Paket ersetzt, kein Commit.

## Originalbefund

Eintritt (alle Statuseintritte, auch Kiste/GetItem/Datei, über `cb_flg 0x64010` bzw. Pad 0x4000):

- `system.c:603-611` bhCheckSubTask (Task 7): `st_flg |= 0x8`, `mn_mode0 = 5`, `sp_flg = 0x30`,
  `bhSetScreenFade(0xFF000000, 5)`; im selben Task 7 folgt bhMainSequence mit
  `bhControlScreenFade` (`game.c:38-40`). `screen.c:21-90`: hkn = 6, erster Aufruf nur
  `fade_ct = 1`, `cb_flg 0x2` fällt im **fünften** Steueraufruf.
- `system.c:1953-2025` bhSysCallMonitor Modus 5 (Task 20, nach Task 7..9 desselben Bildes;
  `main.c:195-201`): mn_md1 0 (Texturen sichern) → 1 (Datei 145 anfordern) → 2 (wartet, bis
  `cb_flg 0x2` frei ist; setzt dann `ts_flg |= 0x80` = Task 7 ruht, und bei fertigem Lesen md1 3)
  → 3 SbsTextureInit → 4 `ItemTaskCheck()` (Einstieg `sub1.c:3029-3077`: `ts_flg &= ~0x200`,
  taskloop 0, `statusflg &= ~0x80000`, SE 3), `mn_md0 = 0`.
- Task 9 (`bhSysCallItemselect`, `system.c:903-929`) läuft erst im Folgebild:
  StatusMain case 0x1 (`sub1.c:3342-3349` ItemBoxInit bei `cb_flg 0x40000/0x80000/0x100000`).

Austritt:

- SpriteH (`sub1.c:2740-2834`): Einfahrt 9 Durchläufe (cnt 0..8), im neunten fällt statusflg 0x40
  → ItemBox liest das Pad im selben Durchlauf (`sub1.c:7224`); StatusCancel (`sub1.c:4408`), Ausfahrt
  9 Durchläufe, dann `statusflg 0x80000`. Danach ruft StatusMain ItemBox nicht mehr (`sub1.c:3463`).
- ItemTaskCheck läuft **vor** StatusMain: mit 0x80000 zählt es taskloop in vier Durchläufen hoch
  (`sub1.c:3227-3237`, StatusMain läuft dabei weiter), erst im fünften mit `taskloop == 4 &&
  mn_md0 == 0` (`sub1.c:3103`): bhStandPlayerMotion, cb_flg 0x10/0x20000/0x1c0000 frei,
  `ts_flg |= 0x200`, `st_flg &= ~0x8`, `ts_flg &= ~0x80`, `sp_flg = -1` bzw. `|= 0xCC`.
  Task 9 ruft dann statt StatusMain `bhSetScreenFade(fade_pbk, 3)` (`system.c:916-928`).

Ereignisse laufen währenddessen jedes Bild (Task 8, `loop_ct` bleibt 1, sp_flg 0x30 behält 0x10).
RM_0090: scd1 @1378/@1472 startet auf Boden 4 Ebene 0 in jedem Durchlauf neu, solange
`st_flg & 0x10040008` frei ist (Event_exec/Event_init `event.c:13001/12956`). Ereignis 19
(scd1 @2110 beim Untersuchen, Ebene 0): bhSleep 30 (`event.c:9100-9129`) = 30 Ruhedurchläufe,
im 31. bhLoadWork/bhCommonCtr (beide Rückgabe 1) mit Common 9 → Klappe obwp[5] absolut 0.

Zeitachse Original, Untersuchen in Bild P = 230, Abbrechen im frühesten Durchlauf:

| Bild | Vorgang |
|---|---|
| 230 | Task 7 hitchk.c:4905 cb_flg 0x40000; Task 8 Ereignis 19 Durchlauf 1 (cnt 29) |
| 231 | Task 7 bhCheckSubTask (st_flg 0x8, Fade, ct 1); Task 20 md1 0→1 |
| 232–234 | Fade ct 2–4; md1 1→2, dann Warten auf cb_flg 0x2 |
| 235 | Task 7 fünfter Steuerlauf → cb_flg 0x2 frei; Task 20 ts_flg 0x80, md1 3 |
| 236 / 237 | md1 3→4 (SbsTextureInit) / ItemTaskCheck-Einstieg |
| 238 | Task 9 StatusMain case 0x1 ItemBoxInit |
| 239–247 | SpriteH-Einfahrt; 247 nimmt ItemBox pad_ps 0x1000 an (StatusCancel) |
| 248–256 | Ausfahrt; 256 statusflg 0x80000 |
| 257–260 | taskloop 1..4; **260 Task 8: Ereignis 19 Durchlauf 31 → Common 9, Klappe 0** |
| 261 | Task 9 Abbau: st_flg &= ~0x8, bhSetScreenFade(fade_pbk, 3) |
| 262 | Task 8 scd1 @1472 startet Ebene 0 neu (Ereignis 19 ist schon fertig) |

Ergebnis: Das Original hält st_flg 0x8 lang genug (Eventdurchläufe 231..261, benötigt bis 260).
Die DVD-Lesezeit in md1 2 (GetReadFileStatus) kann den Halt nur verlängern; nativ ist das Lesen
synchron. Also wird nicht verdrängt; der native Halt war zu kurz → portiert.

## Messung (Headless, Dev-Runtime nur lesend, eigenes `--user-dir` unter $TMPDIR)

Replay `tests/native/replays/status-hold-rm0090-earliest-cancel.txt` (Abbrechen 239 und 247;
jeweils einer fällt in Ein-/Ausfahrt und bleibt wirkungslos), `--frames n --state` je Bild 229..270:

- **Vorher** (`build/macos-arm64/recvx-runtime`, = Start-App 0.5.77 für dieselbe Folge):
  ItemBoxInit schon in 230, Abbrechen 239 angenommen, Kiste zu nach 248; st_flg 0x8 in den
  Eventdurchläufen 231..248 (18). Ereignis 19 bei Rest 11 in Durchlauf 249 von scd1 @1472
  ersetzt; Bild 420: Klappe offen. (Vorhandenes Replay `rm0090-box-quick-cancel.txt`: 231..249, 19.)
- **Nachher** (private Runtime aus Kopien, HEAD+Änderung und zusätzlich Arbeitsbaum mit
  Fade-Besitzer + Patch, identische Zahlen): cb_flg 0x2 fällt nach 235, ItemBoxInit 238,
  Abbrechen 247, Abbau nach 261; st_flg 0x8 in den Eventdurchläufen 231..261 (31). Ereignis 19
  erreicht Common 9 in Durchlauf 260 unter st_flg 0x8; Bild 420: Klappe geschlossen.
- Allgemeiner Statusschirm (RM_0090, `100 inventory`, `140 inventory`): vorher inaktiv nach 154,
  nachher nach 159 (+5 taskloop); Bild 102 Raum mit laufender Blende ohne Menü, 107 schwarz,
  112 Einfahrt. `--inventory` (RM_0060) zeigt den Statusschirm sofort; Titelstart ohne
  `--in-game` (600 Bilder) läuft.

## Änderungen

Eigene Dateien (Arbeitsbaum):

- `native/ui.h`, `native/ui.cpp`: `StatusTask` (Phase None/Monitor/Main/Done): `frame()`,
  `enter()` (bhCheckSubTask), `monitor(fade_pending)` (Modus 5 md1 0..4), `item_task_check(closed)`
  (ItemTaskCheck vor StatusMain, taskloop, Abbau), `resume()` (Karte case 6). `StatusMenu::task`,
  `screen()`; StatusMenu::tick/input/sprite_on_off und `GameUi::draw` laufen bzw. zeichnen nur, wenn
  Task 9 läuft; Ausfahrt setzt statusflg 0x80000 (`task_closed_`) statt sofort zu schließen.
  `StatusMenu::reset` behält die Rahmenbindung.
- `native/box_menu.{h,cpp}`: ItemBoxInit erst nach dem Einstieg (`claim_box`), ItemTaskCheck vor
  SpriteH, `closed_` = 0x80000 (ItemBox/Pad ruht), Abbau `leave()` erst im fünften Durchlauf.
- Ohne `frame()` (StatusMain-Prüfstände der übrigen Tests) bleibt der bisherige Ablauf.

Integrationspatch `archived/work/status_screen_hold.integration.patch` (gegen den aktuellen Arbeitsbaum mit
Fade-Besitzer, `git apply --check` besteht):

- `native/runtime.cpp`: `menu.task.frame()` (`--inventory` diagnostisch `resume()`); map_task:
  Eintritt mit `recvx::set_screen_fade(cb_flg, 0xFF000000, 5)` + `enter()` + erstem
  `recvx::control_screen_fade` (gemeinsamer Besitzer), sonst je Bild `monitor()` und, solange Task 7
  läuft, `control_screen_fade`; Karte case 6 → `resume()`; Fadezeichnung über `!menu.screen()`.
- `native/input.cpp`: `status_busy` = `!menu_.screen() || pad_locked` (GetItem erst aus StatusMain).
- `CMakeLists.txt`: Test `status_screen_hold`.
- `tests/native/replays/rm1140-lighter-run.txt`: Menüdrücke +8 Bilder (feste Drücke lagen im
  alten, zu frühen StatusMain); Erwartungen von `effect_after_player_check.py` unverändert.

## Tests

`tests/native/status_screen_hold_test.cpp` (`_Make_SinTable()`):
1. Rahmen gegen echten ScreenFade: Task 9 ruht im Monitor; md1 2 bleibt bei cb_flg 0x2; ts_flg 0x80
   im ersten Monitorlauf nach dem fadelöschenden Task 7; Einstieg zwei Läufe später; taskloop 1..4,
   Abbau im fünften; case 6 ohne Monitor.
2. StatusMenu: kein Bild/keine Eingabe/kein SpriteH vor dem Einstieg; Statustaste im Monitor wirkungslos;
   taskloop und Abbau nach StatusCancel.
3. RM_0090 mit EventVm/RoomWorld/BoxMenu in Runtime-Reihenfolge: Untersuchen, Abbrechen im frühesten
   ItemBox-Durchlauf → Ereignis 19 endet unter st_flg 0x8, Klappe 0 (Halt 31 Durchläufe); Gegenstand
   ohne Rahmen: Klappe offen (18). Diagnoseaufstellung am Runtime-Standort 44.5/9.5.

Gegenproben (Zeile sabotiert → rot, zurück → grün):
- `monitor` md1 2 ohne `fade_pending`-Warten → „Modus 5 verlässt mn_md1 2 mit gesetztem cb_flg 0x2“.
- Abbau bei taskloop 0 → „taskloop 1“.
- BoxMenu ohne Suspended-Tor beim Init → „ItemBoxInit während bhSysCallMonitor Modus 5“.
- StatusMenu::tick ohne Suspended-Rückkehr → „StatusMain (SpriteH) lief vor dem Monitorlauf“.
- BoxMenu `leave` sofort beim SpriteH-Ende (nur Raumteil) → „Ereignis 19 endet nicht unter st_flg 0x8“.

Nachbarsuiten: 22 CTest-Einträge rund um Status/Kiste/Eingabe (input_test-Modi, inventory, item_box,
rm0090_box, status_pulse, game, gameplay_bindings, event_lifecycle, …) vorher/nachher auf HEAD-Basis
grün. Vollständige private CTest-Läufe (194) seriell: Arbeitsbaum ohne meine Änderung 2 rot,
mit Patch dieselben 2 rot: `event_lifecycle_control_release` (RM_9370 Ereignis 5) und
`enemy_en53_world`, beide „bhFadeSet: bhSetScreenFade ohne Besitzer“ – kommt aus der
Fade-Besitzer-Integration, nicht aus diesem Auftrag. `effect_after_player` war mit Patch ohne
angepasstes Replay rot (Drücke im alten Takt), mit Replay grün. `tools/verify_snapshot.py`,
`tools/verify_typewriter_save.py` grün.

## Grenzen / offen

- Task 7 läuft im Original in den fünf Fadebildern weiter (sp_flg 0x30: Spieler/Gegner/Effekte/ObjItm
  per sp_flg 1/2/8/4 gesperrt, player.c:1365, eneset.c:352, effect.c:750, objitm.c:289; Kamera, Licht,
  Cinesco, Raumzeichnung laufen). Nativ ruht der Weltpass ab Anforderung; gezeichnet wird das letzte
  Raumbild mit Blende.
- sp_flg 0x30 beim Eintritt und `-1`/`|= 0xCC` beim Abbau, ts_flg 0x80/0x200 in work_flags[12]
  (0x200 fällt nativ weiter bei der Anforderung, input.cpp), SE 3 und `begin_status`/BackColor
  weiterhin beim Öffnen statt in md1 4 bzw. StatusMain case 0x1.
- Rückblende `bhSetScreenFade(fade_pbk, 3)` weiter im map_task beim Abfall von `menu.active`: im
  Kistenpfad ein Bild nach Task 9; im allgemeinen Pfad liegt StatusMain nativ am Iterationsanfang
  (GameplayInput::apply), sein Abbau ist erst nach dem Weltslot dieses Bildes sichtbar (Spiel läuft
  ein Bild später weiter). Beides war schon vorher so.
- GetReadFileStatus/GetInsideFileSize und das Waffenlesen (mn_md0) synchron; flgchk 0x2-Neuaufbau
  (sub1.c:3080-3101) nicht portiert; Abbau und Neueintritt ohne Zwischenbild überspringen die
  Abfall-Aktionen des map_task.
- Replays mit festen Menüdrücken verschieben sich um 8 Bilder; nur `rm1140-lighter-run.txt` hat einen
  Testverbraucher und ist angepasst. `rm0090-box-quick-cancel.txt` (Abbrechen 240) fällt jetzt in die
  Einfahrt; sein Kommentar beschreibt den alten Stand.

## Ledger-Vorschlag

```
sub1.c:ItemTaskCheck	partial	StatusTask (native/ui.{h,cpp}): Einstieg erst aus bhSysCallMonitor Modus 5 (system.c:1953-2025, md1 0..4, wartet auf cb_flg 0x2, ts_flg 0x80), StatusMain im Folgebild; nach statusflg 0x80000 vier taskloop-Laeufe, Abbau (st_flg 0x8, ts_flg 0x200, bhStandPlayerMotion) im fuenften (sub1.c:3103/3227). Gebunden in StatusMenu::tick und BoxMenu::synchronize, Treiber runtime.cpp map_task. Offen: flgchk 0x2, sp_flg 0x30/-1, ts_flg-Bits, mn_md0 bei Waffenlesen, SE 3 beim Einstieg. Test status_screen_hold (5 Gegenproben rot), Replay status-hold-rm0090-earliest-cancel.txt: RM_0090 Ereignis 19 schliesst die Klappe.
system.c:bhSysCallItemselect	partial	ItemTaskCheck vor StatusMain je Bild (StatusTask::item_task_check); else-Zweig bhSetScreenFade(fade_pbk, 3) weiter im map_task beim Abfall (Kiste ein Bild spaeter), bhDeleteYakkyou/bcl_ct/gm_flg 0x8000 nicht nachgeprueft.
```
