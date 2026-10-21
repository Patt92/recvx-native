# screen_fade_owner — ein Besitzer für sys->fade_*

Stand 2026-10-20, Arbeitsbaum auf HEAD `055a0ce` (HEAD ist während der Arbeit von `01bf98f` weitergelaufen; Patch gegen den aktuellen Stand geprüft). Kein Commit, kein Build in `build/macos-arm64`, keine App-/Paketänderung. `python3 tools/check_no_original_core.py` vor Arbeit und vor jedem privaten Build: PASS.

## Originalbefund

- **Ein Zustand:** `sys->fade_ct/hkn/ao/an/ap/ro/rn/rp/go/gn/gp/bo/bn/bp/pbk` (types.h:836-850) im prozessweiten `sys`. `bhSetScreenFade` (screen.c:21-47) setzt `cb_flg |= 0x2`, `fade_pbk` = altes Ziel, `fade_ct 0`, `hkn = 1 + ct` (ct > 1, sonst 1), Ausgang = aktueller Wert. `bhControlScreenFade` (screen.c:50-95): erster Pass zählt nur, dann `+= (Ziel-Ausgang)/(hkn-1)`, Sprung aufs Ziel und `cb_flg &= ~0x2` bei `fade_ct >= hkn-1` → für ct = n Ziel in Pass n. `bhDrawScreenFade` (screen.c:97-139): Vollbild-`njDrawPolygon2D` attr 0x60, GS-Alpha `(a+1)>>1` (ps2_NaDraw2D.c:138).
- **Steuerung/Zeichnen je Bild:** game.c:38-41 (Task 7, Anfang `bhMainSequence`, vor allen Steuerpässen) und game.c:109-112; Karte system.c:804-811/864-871; Film sdfunc.c:3352-3356 (ohne Tor). Task 7 läuft während des ganzen Game Over weiter (bhInitGameOver sperrt nur Task 8, ts_flg 0x100), Task 14 `bhSysCallGameover` folgt im selben Bild (main.c:195-201). **gameover.c ruft nie `bhControlScreenFade`**; es setzt nur (:122/262/330/424/485/556/564) und liest `cb_flg 0x2` (:422/514).
- **Zeichenreihenfolge Game Over:** Fade-Quader pri -0.81 → -1 (ps2_NaDraw2D.c:100), `sz` > 65534 geklemmt (ps2_dummy.c:432-442) → OT-Fach 4095 (:1192-1201), zuletzt ausgegeben (:1312). Netz `njDrawTriangle3D` attr 0x80000062 (gameover.c:643) in tieferem Fach → unter dem Fade; Aufforderung (njDrawPolygon2DM pri -0.9, message.c:449/508) hängt Task 14 im selben Fach hinter den Fade (:1203-1215).
- **Spielstart:** `bhFirstGameStart` setzt `ss_flg 0x100` (system.c:422), erreicht über Neues Spiel (system.c:402), Laden (bup_00.c:434 → Titel → Opening) und `bhPopGameData` (room.c:1053). `bhFinishRoom` schreibt dann `fade_an = 255`, rn/gn/bn 0 und `bhSetScreenFade(0, 10)` (room.c:522-534), vor `bhInitEvent` (system.c:1723/1816).

## Alle Originalaufrufer und nativer Stand

| Fundstelle | Aufruf | Nativ |
|---|---|---|
| game.c:40/111 | Steuerung/Zeichnen Task 7 | Runtime `fade_control` im Weltpass, Zeichnen runtime.cpp; **neu:** in Game-Over-Phasen ≠ 0 vor Task 14 |
| system.c:579 / bup_00.c:466 | Schreibmaschine (FF000000,5)/(pbk,3) | gebunden (runtime typewriter_subtask/release) |
| system.c:611/921 | Status (FF000000,5)/(pbk,3) | gebunden (map_task) |
| system.c:628/806/844/866, map.c:334/540 | Karte | gebunden (MapServices) |
| room.c:950/1000 | Tür | gebunden (transition_tick) |
| effsub1b.c:516/575/1762/2047/2098 | bhEff134/bhEff138 | gebunden (RoomWorld-Hooks) |
| player.c:6362/6469 | direkte Nullung bhCPM0_die | gebunden (`clear_now`) |
| gameover.c:122/262/330/424/485/556/564 | Game Over | **neu auf dem einen Zustand** (vorher eigener Fade in gameover.cpp) |
| event.c:3215 bhFadeSet (Opcode 0x36) | Skript | **neu gebunden** (vorher zweiter linearer Fade `RoomWorld::fade_`/`overlay`, ohne cb_flg, a/255) |
| room.c:522-534 | Spielstart nach bhFirstGameStart | **neu gebunden** (Neu, Laden, Retry) |
| system.c:596 / sp_evt.c:764 | Terminal ein/aus | Lücke: ComputerTerminal startet ohne den Fade-Wartezustand (mn_mode 7 wartet auf cb_flg 0x2, system.c:2119-2128); siehe Ledger system.c:2123 |
| system.c:645/1278 | Option | Lücke: Adv_GameOptionScreen nicht portiert (Host-Einstellungen) |
| objitm.c:1971/1992/2039 | bhObj009 | Lücke: Objekt 9 ohne nativen Besitzer |
| pad.c:98 | Demo-Abbruch | Lücke: Attract-Demo nicht portiert |
| sdfunc.c:3338/3352/3356 | Filmabbruch-Fade | Lücke: MovieInfo.MovieFadeMode/-CancelFlag ohne Besitzer; Host-Videoskip bleibt getrennter Mechanismus |
| event.c:9065 | bhRankingCall | Lücke: Ranking ohne Besitzer |
| door.c:369, system.c:1003/1041/1106/2008/2143, bup_00.c:513 | Fade nach Ps2ClearOT neu zeichnen | nicht einzeln nachgebildet (Tür-/Monitor-Leerbilder, siehe Ledger room.c:bhStartDoorDemo) |

Titel: adv.c ruft `bhSetScreenFade` nicht (eigene Adv-Blende) — nichts zu binden.

## Änderungen

Eigene Dateien (direkt):
- `native/screen_fade.{h,cpp}`: `system_screen_fade()` (prozessweiter `sys->fade_*`, Muster wie `back_color`), `set_screen_fade(cb_flg, argb, ct)` (screen.c:21-23), `control_screen_fade(cb_flg)` (game.c:38-41 Tor + screen.c:67/89), `set_current()` für die direkten Schreiber (room.c:524-528, bhCPM0_die), Lesezugriffe `rn/gn/bn/ao/hkn/ct`. API kompatibel (bestehende Methoden unverändert).
- `native/gameover.{h,cpp}`: eigener Fade (`set_fade/control_fade/fade_color/blend_over` mit a/255) entfernt. Die sieben gameover.c-Aufrufe gehen über `set_screen_fade(events.flags[10][0], …)`; md1 1/3 lesen `cb_flg 0x2`; keine Steuerung in `tick`. `draw`: Netz → gemeinsamer `bhDrawScreenFade` (Verhältnis `((a+1)>>1)/128`) → Aufforderung.
- `tests/native/screen_fade_owner_test.cpp` (neu).

Gemeinsame Dateien → `archived/work/screen_fade_owner.integration.patch` (`git apply --check` bestanden, 16 Hunks):
- `native/runtime.cpp`: `screen_fade` ist Referenz auf `system_screen_fade()`; `fade_control()` in `game_over_tick` vor dem md1-10-Weltpass und vor Task 14 (game.c:38-41); `first_game_start` (= ss_flg 0x100) gesetzt bei Neues Spiel, Laden (typewriter_apply, im Fehlerfall zurückgenommen) und Retry, verbraucht in `load()` vor `Event_init` (room.c:522-534); Fade-Hooks der Welt schon beim Raumladen (bhFadeSet in Event_init); `world->overlay(image)` entfernt.
- `native/world.{h,cpp}`: Opcode 0x36 → `screen_fade_set_(v0<<24|v1<<16|v2<<8|v3, v4)` bzw. benannte Lücke `bhFadeSet: bhSetScreenFade ohne Besitzer`; `fade_/fade_target_/fade_step_/fade_color_`, deren Fortschreibung in `tick` und `overlay()` entfernt (zweite Fade-Welt).
- `CMakeLists.txt`: Ziel `recvx-screen-fade-owner-tests`, CTest `screen_fade_owner`.
- `tests/native/gameover_test.cpp`: nur das Gerüst — `Session` startet mit frischem `sys->fade_*`, `step()` ruft `control_screen_fade` vor `over.tick` (Task 7 vor Task 14). Erwartungen unverändert. Ohne diese Gerüständerung wird der Test mit dem neuen gameover.cpp rot (Retry wartet ewig auf cb_flg 0x2) — belegt.

## Tests (privat: HEAD + eigene Dateien + Patch unter `$TMPDIR`, Bibliotheken aus build/macos-arm64 kopiert, geänderte/abhängige TUs neu übersetzt)

`screen_fade_owner` prüft:
1. 32 Originalaufrufer-Parametersätze (Startfarbe, pbk, ao, hkn, Werte je Pass, Sprung in Pass n, cb_flg) gegen die geschlossene Form aus screen.c, dazu Tor ohne cb_flg 0x2.
2. bhFadeSet über `RoomWorld::bind` (echtes RM_0020): Byte-Reihenfolge v0..v4, hkn 13, Folge, Lücke ohne Hook.
3. Game Over auf dem gemeinsamen Zustand: Ausgang = laufender Spielfade (pbk FF000000, ao 255), kein Steuern im Setzpass, gameover.c:122 Pass 1..60, :330 am Ende md1 0, :424 genau bei gov_ct 121 und Folge, Neusetzen bei gov_ct 151, Ende Pass 172 (Kampfmodus); :485 → bhPopGameData genau 10 Bilder danach, Schirm schwarz; Todesszene :262, :556 in Pass 97 (gov_etb[4]), :564 in Pass 151.
4. Zeichnen: GameOver zeichnet exakt `ScreenFade::draw`, Verhältnis (a+1)>>1 (a=1 → 1/128), nichts bei fade_an 0, Fade deckt das Netz.

Vorher/Nachher (vorher = vorhandene Binaries in build/macos-arm64, nur gelesen; nachher = private Builds):
- gameover_original, original_map_render, back_color, presentation_gates, oil_puzzle_world: vorher alle exit 0, nachher alle exit 0.
- tools/verify_gameover.py, tests/native/gameover_retry_death_check.py, tools/verify_map.py, tools/verify_doors.py (Kopien nur mit `--user-dir` in $TMPDIR): vorher und nachher bestanden.
- Laufzeitbeobachtung (Diagnoseaufstellung von verify_gameover, RM_0020 + bhGameOverSet, confirm in Bild 175): nachher Ausblende FF000000/10 ab Bild 176, Retry in Bild 186, Raumstart an 255 → 229.5 (188) → 178.5 (190) → 0 in Bild 196/197, cb_flg frei; vorher blieb `sys->fade_*` im ganzen Game Over unberührt (eigener Fade) und es gab keinen Spielstart-Fade.

## Gegenproben (Zeile sabotiert → rot, zurück → grün)

1. gameover.cpp: `control_screen_fade` wieder am Ende von `tick` → rot „gameover.c:122 Pass 1: 255/7.47/…“; zurück grün.
2. screen_fade.cpp: Tor `if(!(cb_flg&0x2))return;` entfernt → rot „bhControlScreenFade lief ohne cb_flg 0x2 (game.c:38)“; zurück grün. (Erste Fassung des Tests hätte das nicht erkannt — Testfall ergänzt.)
3. world.cpp (Patch): Alpha/Rot im bhFadeSet vertauscht → rot „Byte-Reihenfolge …“; zurück grün.
4. gameover.cpp: Fade-Zeichnen entfernt → rot „GameOver zeichnet nicht den gemeinsamen bhDrawScreenFade“; zurück grün.
5. runtime.cpp (Patch): `fade_control()` in `game_over_tick` entfernt → Laufzeit: Retry kommt nie (Bild 186/200/260: Phase 3, an 255, running); zurück: Retry Bild 186.
6. runtime.cpp (Patch): Spielstart-Fade entfernt → nach Retry bleibt der Schirm schwarz (an 255 in Bild 186–230); zurück: Einblende bis 0.

## Grenzen / offen

- Benannte Lücken siehe Tabelle (Terminal-Ein-/Austritt inkl. Wartezustand, Option, bhObj009, Demo, Filmabbruch-Fade, Ranking, Ps2ClearOT-Neuzeichnungen).
- bhFadeSet nur im Komponententest mit echter RM_0020-Welt belegt; kein Laufzeitbeleg in einem echten Raumskript mit Opcode 0x36. Skripte, die auf cb_flg 0x2 warten, warten jetzt wie im Original — mögliche sichtbare Zeitänderungen in Ereignisszenen sind nicht einzeln geprüft.
- Spielstart-Fade zur Laufzeit nur über Retry beobachtet; Neues Spiel und Laden (Schreibmaschine vom Titel) nur über dieselbe Bindung, nicht am App-Binary/Start.command geprüft (Titelstart nicht gefahren).
- Native Bildgrenze bleibt: im Weltpass steht `fade_control` nach dem Ereignistick (bestehende Verschiebung Task 8 → Task 7 der nativen Bildschleife), nicht geändert.
- Nichts über Start.command ausgeliefert; Build-/App-Stand unverändert.

## Ledger-Vorschläge (Text)

- `screen.c:bhSetScreenFade	partial	Ein prozessweiter sys->fade_* (native/screen_fade.{h,cpp} system_screen_fade/set_screen_fade mit cb_flg 0x2, screen.c:21-47). Gebunden: Status/Karte/Tuer/Schreibmaschine/Effekte (bisher), Game Over gameover.c:122-564 (eigener Fade entfernt), event.c:3215 bhFadeSet (zweiter RoomWorld-Fade entfernt), room.c:522-534 Spielstart (Neu/Laden/Retry). tests/native/screen_fade_owner_test.cpp (32 Aufrufer gegen screen.c, 6 Gegenproben). Offen ohne nativen Besitzer: system.c:596/sp_evt.c:764 Terminal (samt Wartezustand system.c:2119-2128), system.c:645/1278 Option, objitm.c:1971/1992/2039 bhObj009, pad.c:98 Demo, sdfunc.c:3338 Filmabbruch.`
- `screen.c:bhControlScreenFade	partial	control_screen_fade (game.c:38-41 Tor). Task 7 steuert auch im Game Over (runtime game_over_tick vor Task 14 und vor dem md1-10-Weltpass); GameOver steuert nicht selbst. Belege screen_fade_owner, gameover_original, verify_gameover (Retry 10 Bilder nach gameover.c:485). Offen: sdfunc.c:3352 Filmabbruch; native Weltpass-Steuerung nach dem Ereignistick (Bildgrenze der Runtime).`
- neu `screen.c:bhDrawScreenFade	partial	ScreenFade::draw ((a+1)>>1)/128; Game Over: Netz, Fade, Aufforderung (OT-Fach 4095, ps2_dummy.c:432-442/1192-1215). Offen: Neuzeichnen nach Ps2ClearOT door.c:369, system.c:1003/1041/1106/2008/2143, bup_00.c:513, event.c:9065.`
- neu `event.c:bhFadeSet	done	Opcode 0x36 (6 Byte) -> bhSetScreenFade(v0<<24|v1<<16|v2<<8|v3, v4) auf den einen sys->fade_* (RoomWorld::bind, Hook schon beim Raumladen); ohne Hook benannte Luecke. screen_fade_owner (Byte-Reihenfolge-Gegenprobe rot).`
- neu `room.c:bhFinishRoom:ss_flg 0x100 Fade	partial	room.c:522-534: fade_an 255, rn/gn/bn 0, bhSetScreenFade(0,10) vor bhInitEvent nach bhFirstGameStart (Neu system.c:402/422, Laden bup_00.c:434, Retry room.c:1053). Laufzeit ueber Retry belegt (an 255 -> 0 in 10 Bildern); Neues Spiel/Laden nicht am App-Binary geprueft.`
