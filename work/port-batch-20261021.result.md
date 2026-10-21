# Portierungsrunde nach dem EventVm-Audit (2026-10-21, Arbeitsbaum)

Nicht committet. Gewählt wurden die Ledgerlücken mit Spielwirkung und klarem
Originalbeleg; der Rest der partial-Zeilen sind Effekt-/Gegnerdetails.

## Portiert

1. **StartVibrationEx(0, 8) beim Schieben und Drehen** (player.c:3833/3844 für
   bhCPM2_act_psh, :4430/:4441 für bhCPM2_act_rpsh). Vorher nur als
   „StartVibrationEx(0, 8)“ gemeldet (psh, rpsh Fall 4), bei rpsh Fall 5 ganz
   fehlend. Jetzt über `RoomWorld::vibration_requests_` an die Runtime-
   Vibrationseinheit (vibman.c-Port), wie bhPuruPuruStart und die Gegner.
   Test: `rm0150_statue_turn` verlangt `vibration_requests > 0`.
2. **flr_no bei Common_controll 7 und Player_controll 7** (event.c:12053/:9679,
   `bhCheckFloorNum(py)`): der Spieler setzt jetzt `collision_floor_` an beiden
   Stellen; damit ist die im Eintrag `event.c:bhCommonCtr` offene Nachführung
   geschlossen.
3. **bhCheckSubTask-Tor plp->flg 0x6** (system.c:565): ohne Cinematic startet
   kein Untertask; die Statustaste verfällt, Skriptanforderungen bleiben
   stehen. Nativ in `GameplayInput::subtask_locked` (Open-Taste,
   `status_requested`, `inventory_pending_`); die Schreibmaschine hatte das Tor
   schon. Test in `native-gameplay-bindings`.

## Geprüft und als erledigt eingetragen

- **objitm.c bhObj001 Kistenklettern:** mwal/metc/mflr werden je Pass
  veröffentlicht; Stufeneinstieg (`begin_stairs` mit metcp), dnu/dnd in
  `player_stairs.c`, Bodenabfragen (`motion_floors`, bhCheckWater,
  bhCheckFloorEffect) und Gegnerböden lesen sie. Tests `player_push`,
  `box_jump_landing`, `motion_collision`.
- **player.c bhControlPlayer :1540-1572:** Fußfixierung (`player_stairs.c`) und
  der Rückstoßabbau schwerer Waffen (`player_combat_core.c:3189`) sind gebunden.
- **Audio „SE bank 3 program N outside the TSNDDRV table“** (RM_2011, RM_7071,
  RM_9302 im Scan): kein Portfehler. Die Variantenpakete (2011, 7071, 9301)
  wählt allein `bhRoomSoundCase` (0xB6) im scd1 des Quellraums vor der Tür; ein
  Direktstart lädt Fall 0 wie der originale Retry/Load. Ledgerzeile
  `sdfunc.c:RequestRoomSoundBank`.

## Nachtrag: Raumklang-Variante im Snapshot (Hostpfad, umgesetzt)

- Snapshot-Format 2: `SnapshotHost::room_sound_case` (ein Byte vor der CRC)
  trägt die Fallnummer, mit der `load_room_sounds` das aktuelle Paket gewählt
  hat (`AudioMixer::loaded_room_sound_case`). Beim Laden setzt die Runtime sie
  vor dem Raumaufbau, sodass dasselbe Paket geladen wird. Format-1-Dateien
  (dein Slot 1) laden unverändert mit Fall 0. Retry-Punkte und das
  Schreibmaschinen-SAVEFILE bleiben wie im Original bei Fall 0.
- Tests: `snapshot_savefile_roundtrip` (Feld, Format-1-Lesbarkeit) und neu
  `snapshot_room_sound` (RM_2011: Snapshot aus dem Direktstart, Fall-Byte auf 1
  gesetzt, danach 52 Zombie-SE aus Paket 2011 und keine Bank-3-Lücke; Gegenprobe
  Fall 0 mit 81 fehlenden). Auch am installierten App-Binary grün; Slot 1
  bytegleich (SHA 76a14a1f…).
- Sieben Prüfskripte (rm0110_event_zombie, verify_event_freeze/clock_puzzle/
  ammunition/enemy_ai/pickups/self_destruct) prüften das Formatwort hart auf 1
  und patchen das SAVEFILE mit neuer CRC; sie akzeptieren jetzt 1 und 2 (der
  Hostschwanz bleibt beim Patchen erhalten). Volle Suite 213/213 grün.
- Lauf der nicht in CTest eingebundenen Skripte: `verify_event_freeze`,
  `verify_clock_puzzle`, `verify_pickups` grün; `verify_self_destruct` (EN19
  frm_no 71 statt zwischen 80 und 463) und `verify_ammunition` (leere Waffe:
  Inventar/ARMS-Klang) rot. Beide fallen **mit der Vorgänger-App (Format 1,
  `.previous-1qzdtrnf`) an denselben Prüfungen**, sind also Vorherstand und
  nicht Folge dieser Änderung. Belege `build/validation/snapshot-sound-verify/`.

## Prüfungen

Volle CTest-Suite, Dev-App über `./build.sh` (arm64, signiert), Titelstart,
`rm0150_statue_turn` am App-Binary, Policy-Prüfer, `tools/port_check.py`.
Belege `build/validation/vib-flr-build1.log`, `subtask-gate-build*.log`,
`port-batch-full-ctest.log`.
