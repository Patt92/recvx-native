# Hockaufnahme (bhCPM2_act_cro) und Item-ATR-Zweig von bhCheckExmAtari

Stand 2026-10-21, nicht committet.

## Vorher

`EventVm::interact` startete GetItem/GetFile für jede Item-ATR sofort aus der
Interaktion. Die ID kam aus der Raumtabelle statt aus `itwp[prm0].id`,
`itwp.flg & 1` wurde nicht geprüft, und für attr 0x1 gab es keine Hockaufnahme.
Laut Raumscan haben **212 von 446 Item-ATRs attr 0x1**: Bei ihnen ging Claire im
Original erst in die Hocke, nativ erschien die Frage sofort.

## Original

- hitchk.c:4642-4644: `cb_flg |= 0x100`, `etc_idx = i` für jeden Treffer vor dem
  switch; eine abgelehnte Blickrichtung löscht nur 0x100.
- hitchk.c:4847-4925 (case 4), Item-ATR: attr 0x10 → cb_flg 0x20000; bei
  `itwp[prm0].flg & 1` setzt der Zweig `sb_id = op->id`. Mit attr 0x1 folgen
  flg/stflg 0x10000, st_flg 0x4 und mode 1/0/19/0, sonst st_flg &= ~0x4 und
  cb_flg 0x10. Ohne flg 1 fällt der Zweig in den Kistenzweig.
- player.c:3887 `bhCPM2_act_cro`: Motion 45, am Motionende (flg 0x400000,
  player.c:1504) cb_flg 0x10. Danach öffnet `bhCheckSubTask` (system.c:600) den
  Statustask, und dessen Ende ruft `bhStandPlayerMotion` (sub1.c:3105).

## Nativ jetzt

- `native/player_action.c`: `act_cro` als mode2 19 in
  `recvx_player_action_bas` (wie bhCPM1_act_bas case 19). Dazu kommt ein neuer
  IO-Zeiger `cb_flg`.
- `native/runtime.cpp`: Der Spielerpass führt flg 0x400000 nach jedem
  bhSetMotion (player.c:1504). Neue Bindungen: `item_work` (flg/id der
  Item-Entity) und `set_player_mode` (Weltwerk und Aktionswerk; ersetzt einen
  offenen Wiedereintritt).
- `native/events.cpp` `interact`: case 4 vollständig nach Original, mit
  etc_idx-Reihenfolge. GetItem/GetFile startet nur noch `service_item_request`
  (bhCheckSubTask). Der Kistenzweig setzt jetzt auch `*(int*)&pp->mode0 = 1`.
- `native/world_itmbox.cpp`: Ein fehlender obwp-Platz legt keine Phantom-Entity
  mehr an und wird gemeldet.

## Prüfungen

- Neuer CTest `crouch_pickup` (RM_0010 ATR 3, Item 12, nur Platzierung + OK):
  mode2 19 und Motion 45, kein GetItem vor dem Motionende; dann cb_flg 0x10,
  Statustask und Frage 157; nach Ja liegt Item 12 im Inventar, das Modell ist
  per Skript-bhItmCk inactive (nicht hidden), und der Statusschluss stellt den
  Spieler hin (Motion 42, mode2 0, flg 0x10000/st_flg 0x4 frei).
- Gegenprobe mit der Vorgänger-App (`.previous-eh73sk5a`): Der Test scheitert
  sofort (mode2 0, Motion 42, keine Hocke).
- Angepasste Fixtures nach Original: Messer RM_0000 (Interaktion setzt nur
  cb_flg 0x10/sb_id), Memo RM_6040 (attr 0x11: Hocke + cb_flg 0x20000, GetFile
  über bhCheckSubTask), Kiste (mode0 1).
- Volle CTest-Suite 212/212 grün (3 GPU-Paritätstests übersprungen).
- Dev-App über `./build.sh`, arm64 und signiert; `crouch_pickup` auch gegen das
  App-Binary grün; Titelstart 400 Takte ohne Fehler. RM_1110-Automat (kein
  attr 0x1) nimmt Item 61 weiter korrekt auf. Slot 1 bytegleich.
- Policy-Prüfer vor Arbeit/jedem Build, `tools/port_check.py`, Ledger
  `hitchk.c:bhCheckExmAtari:Typ4` und `player.c:bhCPM2_act_cro`.

Belege: `build/validation/crouch-pickup-20261021/`, `crouch-build*.log`,
`crouch-full-ctest.log`.

## Offene Lücken

- `itwp[prm0]` ab `rom->itm_n` und `obwp` ab `rom->obj_n` sind im Original nicht
  gelöschter Pool eines früheren Raums (room.c:130/141). Nativ wird das als
  Lücke gemeldet, ohne Aktion. Aktive ATRs: RM_0160 ATR 4, RM_2031 ATR 10,
  RM_4001 ATR 10/11, RM_5590 ATR 7.
- Das bhCheckSubTask-Tor `plp->flg & 0x6` hat weiterhin keinen nativen Besitzer.
- RM_6040 Memo (attr 0x11): Im Original öffnet GetFile sofort, während die
  Hocke startet; der Statusschluss stellt den Spieler hin. Nativ ist das genauso
  verdrahtet, aber nur im VM-Fixture geprüft, nicht im Runtime-Durchlauf.
- Kein Bildvergleich der Hockpose; Windows nicht ausgeführt.
