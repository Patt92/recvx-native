# Schiebekisten: bhObj001, bewegliche Einträge und bhCPM2_act_psh (alle Räume)

## Nutzermeldung und Vorzustand

Kisten waren im Host nicht anfassbar: kein Schieben, und `verify_push`
(RM_2070) sowie `verify_crate_edge` (RM_0100) schlugen fehl — im Treppenbericht
als „Dabei gemessen, nicht von dieser Änderung“ dokumentiert. Ursache war die
Rückbau-Lücke vom 2026-10-09 in `native/player_push_core.c`: kein `bhObj001`
mit beweglichen Wande-/Stufen-/Bodeneinträgen, kein `bhCPM2_act_psh`, und
`collision_push_count_` (der `psh_ct` aus `bhCheckWallEx`) wurde geführt, aber
von niemandem verbraucht — der Originalübergang `psh_ct > 5 → mode2 18` fehlte
komplett.

## Originalbelege der portierten Mechanik

- `bhObj001` (objitm.c:835-1176, `bhJumpObject[1]`): case 0 erzeugt pro Kiste
  die sieben aspd-Varianten mit ihren Maßen (aspd 0/6: 4.5/7.99/4.5),
  `mwal` Typ 0 (attr 0x10002, prm3 = Objektnummer), bei `pn` zusätzlich
  `mwal` Typ 7 als Deckel (`h -1`, Etage aus `bhCheckFloorNum(py+9)`) und zwei
  `metc` Typ 2 (unten prm0 0, oben prm0 1), dazu `mflr` Typ 1 mit ±4
  Rand und prm0 = param. case 1 prüft Nachbarn über `bhCheckWallType2`-Streifen
  0.2 Einheiten außerhalb (Bits 0x40000/0x80000/0x100000/0x200000), wählt bei
  `attr 0x20000` das Schiebe-SE nach aspd (0:527, 6:528, 1/3:529, 2:531,
  4/5:532), verschiebt bei `stflg 0x80 && mode3 5` um `plp->spd` entlang `ay`,
  löst die Blockseite über `bhCheckWall2Box` + ay-Quadrant (`mode3 6`) und
  schreibt alle mobilen Einträge nach.
- `bhCheckWallType2` (hitchk.c:2046), `bhCheckWall2Box` (hitchk.c:1276) mit
  `njCollisionCheckBS/SC` (ps2_NaColi.c), `bhCheckInnerTriangle2`/
  `bhCheckInnerP4`/`bhCheckBox2Box` (hitchk.c) — alle Typzweige 0–7.
- `bhCPM2_act_psh` (player.c:3728-3884) in allen acht Phasen: `stflg |=
  0x10080`, `flg &= ~0x28C0000`, `flg2 |= 1`, Drehung auf die Kardinalrichtung
  (7.2·rtspd bis < 1310, `ayp = (ay+8192) & ~0x3FFF`), MTN49, Warten auf
  `flg 0x400000`, dann MTN50 mit `CallPlayerActionSe(psh_snd)` +
  `StartVibrationEx(0,8)`, Schiebephase mit `spd = bhGetTransZ(PlyTrsZ[4])`
  (Kurve 9) und `bhAddSpeed`, Loslassen über MTN49 rückwärts
  (`mtn_add -32768`, `mtn_md 8` → SetMtnSlow) bis `frm <= 0`.
- Einstieg `bhCPM2_act_wlk` (player.c:2403): `psh_ct > 5 && !(stflg & 0x80)`
  → `st_flg |= 4`, `stflg |= 0x10080`, `mode2 18`. Abbruchpfad
  `bhControlPlayerPad` (player.c:1820): kein Vorwärts + `stflg 0x80` →
  `flg |= 0x200000`, `mode3 5 → 6`.
- Reihenfolge wie `game.c`: `bhControlObjItm` (Kisten) läuft **nach**
  `bhControlPlayer` und liest `plp->psh_idx/stflg/mode3/spd`.

## Umsetzung

- `native/player_push_core.c/.h`: neu geschriebener C-Kern (kein übernommener
  Originalkörper, kein Generator außer der unveränderten Tabelle `PlMtnAct`,
  player.c:216). Besitzer der Kisten-O_WRKs und ihrer `sys->mwalp/metcp/mflrp`
  — es gibt keinen zweiten. Raumwände (`rom->walp`) und vorher vergebene
  Gegnereinträge reichen als Präfix herein; der Kern ändert sie nicht.
  Spielerfelder kommen als Sicht `RECVX_PUSH_STATE` herein und gehen hinaus.
  Ungebundene Dienste melden in `unsupported` (Bit 0x1 = HOKAN, 0x2 Motion-
  grenzen, 0x4 Trans-Kurve, 0x8 SE, 0x10 Vibration).
- `native/player_push.h/.cpp`: dünner C++-Besitzer; Motionbilder und
  Übersetzungskurve 9 aus dem echten Spielerasset, `pose()` liefert die
  interpolierten MTN49-Rückwärtsbilder (SetMtnSlow, `mtn_md & 8`).
- `native/world.cpp`: `ensure_push/update_push` je Tick (Kistenpositionen
  folgen immer, der Zeichenzustand stoppt `bhControlObjItm` nicht),
  `control()` nach dem Spielertick mit `cb_flg`/`psh_idx`/`stflg`,
  `store_walls()` nach dem Wandpass (übernimmt das `attr 0x20000`-Ergebnis von
  `bhCheckWallEx`), `begin()` in `move_player` bei `psh_ct > 5`, `tick_push()`
  mit Lage-/Motionrückgabe und `collision_push_count_`-Verbrauch.
- `native/runtime.cpp`: `push_steps()` füttern die Original-Type-2-Flächen für
  `bhCheckExmAtari` (Stufenkanten auf Kisten); die Kistenwände/-stufen/-böden
  hängen im gewöhnlichen Wandpass mit drin.

## Nachweise (macOS arm64)

Gemessener Vorzustand (Treppenbericht, Binary 10:04): Schieben startet nie,
Kisten nicht anfassbar; `stflg 0x20000` wird jetzt über den echten Wandpass
gesetzt.

- Komponententest `player_push` (CTest, echter Spielerasset): Einträge für
  aspd 0 (2 mwal / 2 metc / 1 mflr, Maße und prm3), Nachbarbit 0x80000 aus
  Ostwand, Einstiegsschwelle 5 vs. 6, Drehung auf Kardinalyaw, MTN49 → MTN50
  mit SE + Vibration, Kistenweg gleich der Summe der `spd` (Kurve 9), mobile
  Einträge folgen der Kiste, Loslassen mit MTN49 rückwärts und `mtn_md 8`
  (Pose nicht leer), Phase-7-Freigabe, Blockseite (Ostwand) hält die Kiste
  ≤ 0.1 und zwingt Phase 6, fremder `psh_idx` bricht ab und löscht 0x20000.
  Bestanden.
- `tools/verify_push.py` (RM_2070, Original-Kistenplatz 70/0.1/33 aus dem
  Init-Skript, nur diagnostische Platzierung, keine Skriptflags gesetzt):
  22 MTN49 Phase 4, 30–80 MTN50 Phase 5, Kiste z 33 → 28.38, 105 MTN49
  rückwärts Phase 7, 130 freigegeben, Kiste bleibt 26.90; SE
  MULTSPQ1/0045/2/0015 (= 527) gespielt. Bestanden gegen das
  Startpaket-Binary.
- `tools/verify_crate_edge.py` (RM_0100, echte Kanten auf Kiste 7): alle vier
  Geh-/Rennkanten bleiben bei Höhe 13, die Abstiegsaktion kehrt auf 4 zurück.
  Bestanden gegen das Startpaket-Binary.
- Natürlicher Lauf RM_2070 ab Spawn ohne Positionsdiagnose
  (`build/validation/push-claude-20261011/natural/rm2070-natural.txt`):
  rechts, vor, links, vor — ab ~140 Kontakt (`contact 6`), 140 MTN49 Phase 4,
  ab 150 MTN50 Phase 5 mit SE 527, Kiste z 33 → 11.0, Spieler bei z 19.0 von
  der Ostwand gestoppt, bis 500 Phase 7 und freigegeben, Kiste bleibt 11.0.
- RM_0100 Kiste (aspd 6) diagnostisch angelaufen: Schieben ab 60, SE
  MULTSPQ1/0014/2/0016 (= 528, aspd 6 wie vom Original gewählt), Kiste
  z −105.6 → −99.76, bei 150 freigegeben (MTN42).
- CTest 52/54 in `build/stairs-claude-20261010`
  (`effect_pool_lifetime` vorbestehend fehlerhaft;
  `engine_progression_resources` Not Run, weil `recvx-progression-tests`
  vorbestehend nicht übersetzen). `native-gameplay-bindings`,
  `subplayer_apparatus`, `enemy_spawn_activation` bestanden.
- `verify_stairs`, `verify_continuity`, `verify_events`, `verify_progression`,
  `verify_room_spawn`, `verify_gameover`, `verify_motion_collision` bestanden;
  `verify_rooms` 205/205 Räume, 0 blockierte Befehle.
- Policy `tools/check_no_original_core.py`: PASS (Quellen und Binary).
- Auslieferung: das Startpaket-Binary vom 11.10. 00:13 (Commit `82f3c58`) ist
  der Stand dieser Arbeit; `player_push`, `verify_push` und
  `verify_crate_edge` sind gegen **dieses** Binary bestanden.

## Offen (nicht behauptet)

- HOKAN-Mischung der Motionwechsler (Phase 3/6-Übergänge) fehlt wie bei den
  Treppen: Kern meldet dauerhaft Bit 0x1 `RECVX_PUSH_UNBOUND_HOKAN`; die
  Prüfskripte lassen genau dieses Bit zu.
- `StartVibrationEx` hat keinen Treiber; jeder Aufruf landet als
  „StartVibrationEx(t, p)“ in der Audio-Unverfügbarkeitsliste, kein Ersatz.
- Die `psh_ct`-Zählung in `bhCheckWall2Box` gilt im Original nur für
  `plp == bhPlyGetWork()`; Kisten durchlaufen sie nicht — portiert wie im
  Original, kein Verhalten erfunden.
- Kein Windows-Lauf, kein Durchspielnachweis, kein physischer
  Controllertest.

## Fremde Befunde (unverändert, nicht von dieser Änderung)

- `effect_pool_lifetime` schlägt vorbestehend fehl;
  `recvx-progression-tests` (und damit `engine_progression_resources`)
  übersetzen nicht (`EnemySpawnContext::random`).
- Arbeitsbaum enthält eine fremde ungetrackte Datei
  `native/effect_sprite_tables_data.inc`.
