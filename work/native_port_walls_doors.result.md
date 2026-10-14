# walls_doors (2026-10-14): Türen im Lauf, bhCheckL2Wall mit Wandsatz

Subagent `walls-doors`. Belege: `build/validation/walls_doors-20261014/`.
Policy-Prüfer vor Arbeit, vor jedem Build und am Binary: PASS.

## 1. Tür öffnet erst im Stehen

**Ursache (belegt).** `bhCheckExmAtari` (hitchk.c:4588) nimmt für die Typen 0
(Tür) und 3 (Untersuchen) die Sonden `pxn[0]`/`pxn[3]` aus EXP_WORK `fpx/fpz`
und `arn` (hitchk.c:4605-4618: `fpx - sin*(1.5 + (3 - arn))` bzw. `3 + (3 - arn)`),
nur die Typen 1/2/4 nehmen `plp->px` (`0.75`, `4.5`, `2.0`). `fpx` schreibt allein
der Körperteil von bhControlPlayer (player.c:1583-1587) mit der Lage NACH dem
Aktionsschritt und VOR `bhCheckWallEx` (:1622/:1647 verschieben `px`, nicht `fpx`).
`EventVm::interact` rechnete dagegen alle Typen fest von der zurückgeschobenen
Lage `px` (`4.5`/`6.0`/`2.0`). Im Gehen steht `px` wegen `arn = 2` (act_wlk,
player.c:2321) um 2 hinter der Stand-Lage; die Original-Sonde gleicht das über
`fpx` aus, die native nicht. Ergebnis: Türen, deren ATR nur knapp vor der
Wandfläche liegt, griffen erst nach dem Stehenbleiben (arn → 0).
Bedingung/Reihenfolge/mode1 waren nicht die Ursache (Diagnose: Tür-ATR,
Lauf-/Standlage und Sondenrechnung je Tür, `scan_remaining_margin.txt`).

**Änderungen.**
- `native/world.h:371-377,424`, `native/world.cpp:2869`: neuer Besitzer
  `collision_front_point_` = EXP_WORK fpx/fpy/fpz, geschrieben in
  `resolve_player_motion` genau dort, wo die Vorsonde aus `to - sin*arn` entsteht
  (vor den Wandpässen). `place_player` setzt fpx = px (Teleportenden
  player.c:3095-3099). `bind_player_collision` (`world.cpp:2736`) initialisiert
  fpx = Lage für einen OK-Druck vor dem ersten Körperpass des Raumes (Host prüft OK
  am Bildanfang; im Original läuft der Körperteil immer vorher).
- `native/events.h:74-83`, `native/events.cpp:538-551`: `interact(pos,yaw,floor,front,arn)`
  mit der Sondentabelle `pxn[5]/pzn[5]` exakt nach hitchk.c:4601-4618, Auswahl
  per `exp->type`. Die Dreiargumentform ist der stehende Spieler (fpx = px, arn 0).
- `native/runtime.cpp:538`: der Bestätigungsweg reicht `player_front_point()` und
  `player_front_offset()` durch und prüft vorher das Flag-Tor von player.c:1678
  (`EventVm::exm_flags_clear`: `!(stflg & 0x10080)`, `!(flg & 0x4000004)`,
  `!(cb_flg & 0x4017)`) für Stufen- und Tür-/Untersuchungszweig.

**Prüfung (Diagnoseaufstellung, kein Durchspielen).** `scan_doors.py`: jede
aktive Tür (ATR Typ 0) aller 205 Räume, jede erlaubte Anlaufrichtung, Claire
(`--player 10`) 14 vor der Tür, Gehen, OK im Lauf (Bild 45) bzw. Stopp 40/OK 70.
- Vorher (`runtime-before`, Build 18:34): 23 Anläufe öffnen nur im Stehen.
- Nachher: 7 davon öffnen im Lauf (RM_0070/0, RM_0090/14, RM_0160/3, RM_1090/1,
  RM_7060/0, RM_9140/0, RM_9230/1); keine Tür verliert ein Ergebnis
  (`scan_doors_*before/after.tsv`).
- Verbleibend 16 (RM_7120/0 ist ein anderer Fall: dort öffnet nur der Lauf, vorher
  wie nachher). Für die übrigen 15 fehlt laut `scan_remaining_margin.txt` ab
  `px(Wand)+4.5` noch 0,5-1,5; das Original erreicht im Gehen `px_vor_Wand + 4.5`,
  also einen Gehschritt mehr. Der gemessene Gehschritt liegt bei 0,36-0,41
  (`walk_step_trace_rm1000.txt`), für RM_3030/1 öffnet OK in keinem Bild 36-59.
  Das folgt aus der Originalformel (arn 2, ar 3.5), keine Abweichung, die ich mit
  Beleg ändern könnte.
- Einzel-Replay `door_walk_rm0070.txt`: vorher `door_active false`, nachher
  `door_active true, door_ticks 35` (JSONs; Bilder `door_walk_rm0070_before_f62.png`
  Raum, `..._after_f62.png` Türsequenz, angesehen).
- Fester Test `tests/native/game_test.cpp:357-366`: Tür nur über fpx/arn erreichbar,
  Reichweitengrenze, und jedes Bit des Tors player.c:1678.

## 2. bhCheckL2Wall liefert den echten Wandsatz

**Befund.** Die Bindungen `combat.cpp engine.l2_wall` und `world.cpp gun_fire
s.l2_wall` nutzten `wall_ray_distance` (allgemeiner Strahltest, Typ 6/7 übersprungen)
und gaben `hit->atr = {}` zurück. Damit lief `bhCheckWallAttrB89` immer auf
attr 0 → 1 (Funken auch an Wänden mit attr 0x100/0x200, die je Kameraschnitt
unterdrückt werden), `prm0 == 1` (Wasser) war nie erreichbar, Bodentreffer (Typ 7)
gab es nicht. Zusätzlich war `sys.cam_ncut` nie gesetzt (0) und `gun_fire` hatte
kein `sys->st_flg`.

**Änderungen.**
- `native/wall_capsule.cpp:112` `is_paralell_l2pl` (ps2_NaColi.c:35) und
  `:168-286` `check_l2_wall`: eigenständiger Port von pwksub.c:718-1890 über die
  vorhandenen Ninja-Ports (`check_bc`, `distance_l2pl`, `inner_triangle`,
  `recvx_cb_check_cc`, `recvx_cb_distance_l2l`): 0,01-Kapsel als Vorauswahl mit
  verkürztem c2 je Treffer, Flächenebenen je Typ 0/1, 2/3 (Deckel nur bei nicht
  paralleler Linie, Mantel auf den Radius gezogen), 4/5 (Box je id, Dreiecks-
  prüfung), 6 (Rampe je id), 7 (h < 0, `ps.y <= 0 && ps.y >= h`), Filter
  `type & 1 && flg & 0x400`, `*len = nr`, ohne Treffer `apos = p + v`, `ahtp = NULL`.
  Typ 4-6 mit id > 3 lassen im Original die Box des Vorgängers stehen; keine
  Raumtabelle enthält das (Datenscan), dort wird der Satz übersprungen.
- `native/world.h` / `world.cpp:2656` `RoomWorld::l2_wall` über `collision_geometry()`
  (rom->walp + sys->mwalp), liefert apos, ATR-Satz und Index.
- `native/combat.cpp:174-185` und `world.cpp:1333-1345` (gun_fire): `hit->apos`,
  vollständiger `RECVX_ENEMY_ATR` inkl. prm0..3/id und `hit->id`.
  `combat.cpp:221` und `world.cpp:1391`: `cam_ncut` = bhCmpB-Schlüssel 2
  (`events->values[2]`, wie world_enemies.cpp:1370), gun_fire bekommt `st_flg`.
- `world.cpp:1386-1388`: gun_fire bindet `gun_spark` (pwksub.c:3188) an denselben
  Effektbesitzer; ohne das meldete der jetzt erreichte Wandtreffer der Steve-
  Doppelwaffe in RM_0030 die Lücke `bhSetEffGunSpark`.
- `world.cpp:2239`: der Spurstrahl von bhDrawWeaponEffect Typ 3 (effsub1.c:1715)
  nutzt denselben Port statt `combat_wall_distance`.

**Prüfung.**
- `tests/native/collision_test.cpp:35` `l2_wall()`: nächster Treffer gewinnt und
  benennt den Satz (Reihenfolge fern/nah), Typ 1 mit/ohne 0x400, Ergebnis ohne
  Treffer, Typ 7 Wasserboden von oben und darüber, Zylindermantel, Prisma innen/
  außen; Erwartungen von Hand aus den Originalebenen.
- `l2wall_room_check.cpp/.txt` (eigenes Prüfprogramm, unter der Bausperre gelinkt):
  RM_0050 abwärts → Satz 0, Typ 7, prm0 1, apos y -0,1; Pistolenlinie → Satz 3,
  attr 0x300, prm 0/5/255/255. Doppelwaffenzweig wno 21 mit derselben Bindung wie
  gun_fire: auf den Wasserboden 4 bhEff011-Tropfen + 1 Funke (pwksub.c:3196-3237),
  an die Wand (prm0 0) 0 Tropfen + 1 Funke. Vorher war prm0 immer 0.
- Runtime `replay_shoot_rm0050_wall3.txt` (Diagnose: Debug-Ausrüstung, Position,
  `--camera` als Testaufstellung für cam.ncut): vorher Funke (bhEff308) bei Schnitt 0
  und 5; nachher Funke bei Schnitt 0, keiner bei Schnitt 5 (prm1 = 5,
  hitchk.c:5893-5903). `wall_attr_b89_runtime.txt`.
- `tools/verify_combat.py --tables --unit` vorher/nachher bestanden;
  `--runtime-only` mit dem Endstand (`runtime-after4`) bestanden
  (`verify_combat*.txt`). Die Aufschaltlage im Diagnoseraum ist 3521 statt 3710
  (Build 2 mit derselben L2-Änderung ohne gun_spark-Bindung: 3710); vermutlich
  verschiebt der jetzt erzeugte Steve-Funke den gemeinsamen Zufallsstrom bzw.
  fremde Änderungen dazwischen, nicht isoliert.
- `ctest` vollständig: 91/91 (Build 4, `ctest_full_after.log`). Rote Suiten der
  Zwischenbuilds: 14/19/63/64 Inventar standen schon im Build `enemies` (18:51) vor
  meinem Test rot; Build 3 brach mehrere Suiten ohne Ausgabe ab und das Binary
  endete mit SIGABRT schon nach 2 Bildern in jedem Raum, Build 2 (meine Runtime-
  Änderungen) und Build 4 nicht → fremder Zwischenstand, mit Wiederholung belegt.

## 3. Weitere Wandbefunde (nur berichtet)

- `bhCheckC2WallN` (pwksub.c:2172) ist nativ weiterhin `combat_wall_distance` /
  `wall_ray_distance` (Strahl statt Kapsel, Typ 6/7 übersprungen): combat.cpp
  `engine.c2_wall_n`, `search_near_enemy_b` (world.cpp), Kopf-Sichtprüfung
  (runtime.cpp:295), Bombenverdeckung (world.cpp). Offene Annäherung.
- `bhCheckWallEx` (geometry_motion.cpp) habe ich nicht vollständig auditiert;
  beim Gehen gegen Wände stimmt die gemessene Lage (Vorsonde 3,5 vor der Wand,
  Körper arn dahinter) mit player.c:1600-1655 überein.
- Der Tür-/Untersuchungszweig läuft weiter am Bildanfang beim Bestätigen
  (vor bhCheckFloorP dieses Bildes), nicht in bhControlPlayer nach bhCheckFloorP
  (player.c:1664/1678); `mode0 == 1` ist nicht geprüft (kein durchgehender
  mode0-Besitzer), und `EventVm::interact` hat zusätzlich das Host-Tor
  `st_flg & 4`/Dialog/Menü. Typen > 4 behalten die alte Hostsonde (pxn[type]
  läge außerhalb; keine Raumtabelle enthält sie). Benannte Lücken.
- Die Explosionswand `attr 0x1000000` (weapon.c:554) erreicht jetzt den Kern,
  `explosion`/`object_mdflg_or`/`wall_disable` sind weiter nicht gebunden; kein
  Raumsatz trägt das Bit.

## Geänderte Dateien

native/world.h, native/world.cpp, native/events.h, native/events.cpp,
native/runtime.cpp, native/combat.cpp, native/wall_capsule.h, native/wall_capsule.cpp,
tests/native/collision_test.cpp, tests/native/game_test.cpp.
Ausgeliefert nur als Build `build/macos-arm64` + Start.command-Dev-App (Build 4);
kein Paket.

## Ledger-Vorschläge

```
pwksub.c:bhCheckL2Wall	done	native/wall_capsule.cpp check_l2_wall + RoomWorld::l2_wall (rom->walp+sys->mwalp), Typen 0-7, nr/apos/ahtp; Bindung combat.cpp/gun_fire/bhDrawWeaponEffect; tests/native/collision_test.cpp l2_wall, build/validation/walls_doors-20261014/l2wall_room_check.txt
ps2_NaColi.c:njIsParalellL2PL	done	native/wall_capsule.cpp is_paralell_l2pl (0.025-Schwelle), genutzt von check_l2_wall Typ 2/3
hitchk.c:bhCheckWallAttrB89	done	player_combat_core.c cb_wall_attr_b89 bekommt jetzt den echten ahtp-Satz und cam.ncut (events values[2]) aus combat.cpp und gun_fire; Runtime RM_0050 Wand 3: Funke bei Schnitt 0, keiner bei Schnitt 5 (wall_attr_b89_runtime.txt)
hitchk.c:bhCheckExmAtari	partial	Sonden pxn[0..4] exakt (fpx/arn fuer Typ 0/3, hitchk.c:4605-4618), Tor player.c:1678 Flags; offen: Lauf am Bildanfang statt nach bhCheckFloorP, mode0==1, Host-Tor st_flg 4; walls_doors-20261014/scan_doors_*.tsv
player.c:bhControlPlayer	partial	EXP_WORK fpx/fpy/fpz (player.c:1583-1587) als RoomWorld::player_front_point aus dem Koerperpass vor bhCheckWallEx
```
