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

## Offen (Hostpfad)

- Ein Snapshot speichert das geladene Raumklangpaket nicht; nach dem Laden in
  einer Raumvariante fehlt die Variantenbank (Zombies in RM_2011 stumm), wie
  beim originalen Retry. Ein Feld `room_sound_case` im Snapshot-Host-Teil würde
  das beheben; nicht umgesetzt (Formatänderung).

## Prüfungen

Volle CTest-Suite, Dev-App über `./build.sh` (arm64, signiert), Titelstart,
`rm0150_statue_turn` am App-Binary, Policy-Prüfer, `tools/port_check.py`.
Belege `build/validation/vib-flr-build1.log`, `subtask-gate-build*.log`,
`port-batch-full-ctest.log`.
