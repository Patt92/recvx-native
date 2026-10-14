# Reihenfolge-Befund: `bhEff_E11_CheckCollisionPlayer(plp): owP vor bhCalcModel`

Stand 2026-10-15, eigener Build `build/plpowp-claude-20261014` (Release, Ninja).
Kein Commit/Push, keine Paketersetzung, keine Aenderung an `native/`.

## Ergebnis in einem Satz

Die Meldung (und die verwandte `plp->mlwP->owP[4].mtx (en05s)`) ist **kein Engine-Fehler**,
sondern ein Fehler der Test-Fixture `tests/native/en11_world_test.cpp`: sie ruft nie
`RoomWorld::commit_player_joints`, waehrend die Runtime den Spielerbaum vor `Event_init`
(player.c:840) und in jedem Bild (player.c:1688) festschreibt. Die Runtime zeigt die
Meldung darum in keinem der 205 Raeume. Unabhaengig davon bleibt eine **benannte
Reihenfolge-Luecke** offen: der native Effektpass liest den Baum des *vorigen* Bildes,
das Original den des *selben* Bildes (game.c:49 vor :50). Sie ist nicht geaendert, weil
sie einen Umbau von `RoomWorld::tick` und des Runtime-Bildablaufs braucht (Vorschlag unten).

## Original (Semantikreferenz)

| Stelle | Inhalt |
|---|---|
| game.c:48-58 | je Pass: `bhControlEnemy` -> `bhControlPlayer` -> `bhControlEffect` -> `bhControlObjItm` -> (`bhSetPlEyeCamera`) -> `bhControlCamera` -> `bhControlLight` |
| player.c:1349-1365 | `bhControlPlayer` laeuft nur bei `sp_flg & 1` und `!(plp->stflg & 0x1000000)`; :1377-1387 liest `plp->mlwP->owP->mtx[12/14]` des **vorigen** `bhCalcModel` (nlxb/nlzb) |
| player.c:1685-1688 | `bhControlPlayerHead()`, dann `bhCalcModel(plp)` am Ende des Spielerpasses |
| player.c:840 | `bhSetPlayer`: `bhSetMotion` + `bhCalcModel(plp)` beim Raumaufbau, vor `Event_init` (event.c:352 bhControlEnemy) |
| effect.c:750/776/800-812 | `bhControlEffect` (Tor `sp_flg & 8 || ef_flg & 1`): bei `flg & 0x80` `owp = &pp->mlwP->owP[op->lkono]`, `njCalcPoint(&owp->mtx, &op->lox, &op->px)` — liest den im selben Pass von `bhControlPlayer` geschriebenen Baum |
| effsub2.c:2296-2340 | `bhEff_E11_CheckCollisionPlayer`: `plp->mdl[3].objP` unter `plp->mlwP->owP` (obj_num Eintraege), danach Figuren 41..43/91..93 mit `mdl[1]` |
| effsub2.c:2162-2171 / :2423 | Aufrufer: bhEff_E11_SearchLightDraw (261, nur bei `sp_flg & 8`) und bhEff_E16_LaserSightDraw (270) |
| en05.c:1021 | `ene05_child->mtx = &plp->mlwP->owP[4].mtx` (Kopfsonde von bhEne05s, Gegnerpass) |

## Native Reihenfolge (gemessen am Code)

Runtime-Bild (`native/runtime.cpp:1343` headless, `:1443` Fenster), je Bild:

1. `world->tick()` (`native/world.cpp:1750-1958`):
   Kopf = player.c:1366-1387 (pxb, nlxb aus `player_joint_matrices_[0]`, **voriger** Baum) ->
   `bhObj001`-Kiste (`push_->control`) -> EN04-Spawn -> `tick_en01()` -> `control_enemy_works()`
   (= bhControlEnemy) -> `update_player_ground_work()` (player.c:1743-1762) ->
   `control_player_effects()` -> **Effektschleife** (effect.c:750, Zeile 1834-1899) ->
   `process_effect_output()` -> Posen-Commit der Entities -> `control_enemy_player_motion()`
   (player.c:1502 fuer den gegnergefuehrten Spieler) -> `sync_player_grab()` ->
   `control_player_grab_collision()` (player.c:1600-1664) -> `calc_enemy_draw_models()`.
2. `follow()` / `world->tick_camera()` (bhControlCamera).
3. `movie.tick()`, `audio.mix()`.
4. `locomote()` (Bewegungsanteil von bhControlPlayer) und **`head_hair_step()`**
   (`runtime.cpp:462-510`): bhControlPlayerHead, dann `player->pose(...)` und
   `world->commit_player_joints(player->script_nodes(nodes))` = **bhCalcModel(plp)**,
   dazu `commit_afterimage_model(0,0,0,...)` fuer bhEff136. Fruehausstieg ohne Commit bei
   `world->player_hidden`, `pose.size()<6` oder Ausnahme in `player_local_pose()`.
5. `third_person_tick()`, `compose()`/`draw()`.

Raumaufbau (`runtime.cpp:450`): `if(player&&player->has_motion(motion)) world->commit_player_joints(...)`
**vor** `events->initialize()` — das ist player.c:840. Bedingung `has_motion(motion)`:
ist die Startmotion keine Spielerbibliotheksmotion, bleibt der Baum bis zum ersten
`head_hair_step` leer (im Scan nicht aufgetreten, siehe Zahlen).

Folge: Gegnerpass (Schritt 1, vor dem Commit) sieht den Baum des vorigen Bildes — wie
das Original (game.c:48 vor :49). Effektpass (Schritt 1, ebenfalls vor dem Commit) sieht
**auch** den vorigen Baum — das Original sieht den aktuellen (game.c:49 vor :50).
Ebenso laeuft die Kamera (Schritt 2) vor dem Spielerpass, im Original danach (game.c:58).

## Tabelle: native Verbraucher von plp->mlwP->owP (`player_joint_matrices_`)

Schreiber: nur `RoomWorld::commit_player_joints` (`native/world.h:238`), aufgerufen in
`runtime.cpp:450` (Raumaufbau), `runtime.cpp:493` (`head_hair_step`) und in Tests
(`en05_world_test.cpp:27/31`, `lighter_test.cpp:74/142/147`, `effect_room_sprites_test.cpp:1864`,
neu `player_owp_order_test.cpp`).

| Verbraucher (nativ) | Fundstelle | Pass | Original erwartet | Native Lage |
|---|---|---|---|---|
| `effect_player_hit` (bhEff_E11_CheckCollisionPlayer, plp-Teil) <- `beam_player` <- bhEff261 `step_e11_searchlight` / bhEff270 `step_e16_laser` | world.cpp:1529-1546; effect_sprites.cpp:675, :736, :782 | Effekt | Baum **dieses** Bildes (game.c:49->50) | voriges Bild; im Runtime nie leer; in der en11-Fixture leer -> Meldung |
| `link_matrix(0,…)` fuer Effektlinks lkflg 1 (effect.c:803-812): `link_point` / `set_effect_link_matrix` (bhEff000) und `s.link_matrix` aller Prozeduren (z. B. bhEff145/146 Augenlichter) | world.cpp:1861-1864, :1872; :2365-2384 | Effekt | dieses Bild | voriges Bild; leer -> `plp->mlwP->owP vor bhCalcModel(plp)` |
| `set_effect_link_lookup` -> `find_link_node_matrix`/`link_node_matrix` (kind 0) | world.cpp:1783; effect_sprites.cpp:135-137 | Effekt | dieses Bild | voriges Bild |
| bhEff136 Nachbild: `svc.capture(0,…)` -> `capture_afterimage` liest das in `head_hair_step` festgeschriebene `commit_afterimage_model(0,0,0/4,…)` | world.cpp:1824, :2016; runtime.cpp:497-510 | Effekt | objP/owP dieses Bildes | voriges Bild (derselbe Commit-Zeitpunkt) |
| `h.player_fire` -> `effect_player_fire` -> `recvx_t3_set_fire_effect_link` -> `t3_player_joint_pos` (bhEff_E12_Fire am Spielergelenk) | world.cpp:1790; world_enemies.cpp:1236; en03_lib.c:198-212 | Effekt | dieses Bild | voriges Bild |
| `RECVX_ENEMY_SERVICES.player_joint_matrix` / `player_joint_point` -> `enemy_player_joint` | world_enemies.cpp:380-383, :281-283 | Gegner | voriges Bild (game.c:48 vor :49) | voriges Bild — korrekt |
| davon: en01_core `en01_player_root` :912, `mv03` :3976, `eat_check` :6383 (owP[3]), `rot_neck` :7944, `ply_dg01` :10764; en01_sub `arm_dg01` :788/:827 (owP[11]); en04 `ply_joint` :84, `mv_type_distance` :673 (owP[1]); en05 `player_control` :2439 (owP[3]), **bhEne05s Kopfsonde :2783 (owP[4])**; en07 `ng01` :1366 (owP[5]); en12 `look` :630 / `control_player` :828 (owP[5]); en16 `mv02` :350/:529 (owP[3]); en21 `set_spark_eff` :1021; en24 `dead_check` :868 | native/enemies/* | Gegner | voriges Bild | korrekt; leer nur in Fixtures ohne Commit -> `(en05s)`-Meldung |
| `x.player_obj_num` = `plp->mlwP->obj_num` | world_enemies.cpp:1079 | Gegner | obj_num | 0 solange nie committed |
| `dog.player_collision_parts` (en04.c:914-933, owP[previous_joint] des letzten Passes) | world_enemies.cpp:1173-1185 | Gegner | ausdruecklich voriges Bild | korrekt |
| `en.player_link` -> `link_matrix(0,0,0)` | world_enemies.cpp:810-815 | Gegner | voriges Bild | korrekt |
| Kopf von `RoomWorld::tick` (player.c:1366-1387 nlxb/nlzb aus owP[0]) | world.cpp:1754-1763 | Spieler (Kopf) | voriges bhCalcModel | korrekt |
| `bhEasySESet`-Lage ueber `link_matrix(0,…)` | world.cpp:812 | Skript | zuletzt festgeschriebener Baum | korrekt |
| `head_hair_step`: Haar liest `nodes[5]` desselben Commits (bhObjClpn nach bhCalcModel) | runtime.cpp:500-510 | Spieler/ObjItm | dieses Bild | korrekt |

## Warum Test-Fixture ja, Runtime nein

- `tests/native/en11_world_test.cpp` baut `RoomWorld` + `EventVm`, ruft `events.initialize()`
  und je Pass nur `world.set_simulation_flags(...); world.tick();`. **Kein** `commit_player_joints`.
  `player_joint_matrices_` bleibt leer, `effect_player_hit` meldet bei `sp_flg & 8`
  (RM_9100 hat das Bit, der EN11-Scheinwerfer laeuft) „owP vor bhCalcModel“, und die
  EN05-Kind-Kopfsonde meldet `(en05s)`.
- `tests/native/en05_world_test.cpp:27/31` macht es richtig (Commit vor `events.initialize()`
  und je Pass) und dokumentiert genau diesen Runtime-Schritt.
- Die Runtime committed am Raumaufbau (`runtime.cpp:450`) und je Bild (`:493`), daher ist
  der Baum im Effektpass nie leer. Das `sp_flg 8`-Tor und `presentation_flags` sind in
  Fixture und Runtime gleich (beide aus `events->flags[6][0]`); sie sind nicht die Ursache.

## Zahlen

- Raumscan (`scan_enemy_gaps.py`, 205 Raeume, 600 Bilder, `--enemy-ai on --enemy-grab on`),
  Ausgabe `build/validation/enemy_scan_20261014/plpowp-agent/`:
  `rooms=205 with_gaps=31 failures=0`, 45 Eintraege — **identisch zu run8**
  (Differenzvergleich je Meldung: keine Abweichung). Keine Meldung mit `owP`/`bhCalcModel`/`E11`;
  einzige owP-nahe Zeile ist die bekannte `bhEne14_LookPlayaer: owP.flg` (RM_5810, RM_9302).
- RM_9100 einzeln (600 Bilder): keine `Enemy gaps:`-Ausgabe.
- Betroffene Raeume/Effekte des Fixture-Fehlers: nur Fixtures ohne Commit. Im Runtime sind
  EN11 (261, RM_9100), EN16-Laser (270), bhEff135/136, EN05-Kinder nicht betroffen.
- Betroffene der **Reihenfolge-Luecke** (voriger statt aktueller Baum im Effektpass): alle
  Effektverbraucher der Tabelle (261/270-Treffertest, lkflg-1-Spielerlinks, 136-Nachbild,
  E12-Feuer am Spieler) — ein Bild Latenz, sichtbar nur bei Spielerbewegung.

## Aenderungen

1. **Neu** `tests/native/player_owp_order_test.cpp` (CTest `player_owp_order`,
   CMake-Ziel `recvx-player-owp-order-tests` nach Muster `recvx-en02-tests`):
   RM_9100, 40 Passes; Fixture A committed wie die Runtime (vor `events.initialize()`, nach
   jedem `world.tick()`) und darf keine owP-Luecke melden; Fixture B laesst beides weg und
   muss genau die beiden Meldungen zeigen (Ursachenbeleg).
2. `CMakeLists.txt`: Testziel angehaengt (kleiner Hunk).
3. `docs/port_ledger.tsv`: Zeile `player.c:bhCalcModel(plp)` (partial) angehaengt.
4. **Nicht geaendert** (Besitz Hauptagent): `tests/native/en11_world_test.cpp`. Empfohlener Fix
   dort, wie en05_world_test: `world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));`
   vor `events.initialize()` und am Ende von `tick()`; dann die Ausnahmemenge `known` entfernen.
   Probebau einer so geaenderten Kopie (in `$TMPDIR`, ohne Ausnahmen, gleiche Compile-/Link-Zeile
   wie `recvx-en11-tests`) laeuft gruen:
   ```
   EN11: 2 Works, Effektslot(s) 261 leben nach 40 Passes
   EN11 world PASS
   rc=0
   ```

## Offen: Reihenfolge-Luecke Effektpass (Vorschlag, nicht umgesetzt)

Original: `bhCalcModel(plp)` (player.c:1688) liegt **zwischen** bhControlEnemy und
bhControlEffect. Nativ liegt es nach `world->tick()`. Ein Fix, der der Originalreihenfolge
entspricht, ohne eine Pose aus dem naechsten Motionzustand zu erzeugen (AGENTS.md:
Werkzustand != Objektbaum), braucht:

- `native/world.cpp` `RoomWorld::tick` (Zeilen 1777-1778): zwischen `control_enemy_works()` und
  `update_player_ground_work()` einen Spielerpass-Haken (`std::function<void()> player_pass_`)
  oder eine Teilung in `tick_enemies()` / `tick_effects()`. Die Spielerteile am Ende von
  `tick` (`control_enemy_player_motion()` = player.c:1502, `sync_player_grab()`,
  `control_player_grab_collision()` = player.c:1600-1664, Zeilen 1943-1948) gehoeren im
  Original ebenfalls **vor** bhControlEffect und muessten mit vor den Haken.
- `native/runtime.cpp` (1343/1443): `locomote()` + `head_hair_step()` in diesen Haken
  verlegen (mit den dortigen `!menu.active && !map_active`-Toren); `tick_camera` bleibt danach
  (game.c:58). Zu pruefen: die nach `tick()` geleerten Warteschlangen (`take_player_exp0_writes`,
  `take_blast_damage`, `take_player_poison`), die `player_hidden`-/`has_motion`-Fruehausstiege
  (dann bliebe der Baum wie im Original vom vorigen Bild stehen) und die Testfixtures, die
  den Commit heute vor oder nach `tick()` setzen.
- Nicht umgesetzt, weil `native/world.cpp` parallel von Hand bearbeitet wird und
  `native/runtime.cpp` fremde uncommittete Aenderungen traegt; der Umbau ist nicht chirurgisch.

Weiterer Nebenbefund (nicht geaendert): `runtime.cpp:450` committed nur bei
`player->has_motion(motion)`; mit einer Raum-/Gegnermotion als Startmotion bliebe der Baum bis
zum ersten `head_hair_step` leer (dread.c:161 liest im Original dann uninitialisierten Speicher —
die Meldung `plp->mlwP->owP vor bhCalcModel(plp)` ist dafuer korrekt). Im Scan nicht aufgetreten.

## Pruefprotokoll (woertlich)

`python3 tools/check_no_original_core.py` vor Arbeit und jedem Build:
`PASS: audited source/build/binary scope has no forbidden core; not a gameplay acceptance.`

`./build/plpowp-claude-20261014/recvx-player-owp-order-tests data`:
```
RM_9100 mit bhCalcModel(plp): keine owP-Luecke nach 40 Passes (22 Knoten)
RM_9100 ohne bhCalcModel(plp): beide owP-Luecken gemeldet (Ursachenbeleg)
player owP order PASS
rc=0
```

`./build/plpowp-claude-20261014/recvx-en11-tests data` (unveraenderte Fixture):
```
bekannte Fremdluecke: bhEff_E11_CheckCollisionPlayer(plp): owP vor bhCalcModel
bekannte Fremdluecke: plp->mlwP->owP[4].mtx (en05s)
EN11: 2 Works, Effektslot(s) 261 leben nach 40 Passes
EN11 world PASS
rc=0
```

`ctest -R "enemy|effect|lighter|actor|event_lifecycle" -E "effect_room_sprites"` im eigenen
Build-Ordner (`effect_room_sprites` ausgenommen: Ziel `recvx-effect-room-tests` baut wegen
des fremden fehlenden BinaryView-Includes nicht; nicht repariert, fremde Baustelle):
```
100% tests passed out of 24
Total Test time (real) =  32.59 sec
```
(event_lifecycle_control_release, enemy_work_contract, effect_collision_debris, effect_pool_lifetime,
original_effect_particles, native_lighter_effect, enemy_damage_original_en01, enemy_spawn_activation,
enemy_c07_group, enemy_en07_component, enemy_en04_component, enemy_en11_world, enemy_en02_world,
enemy_en53_component, enemy_en09_world, enemy_en13_world, enemy_en23_world, enemy_en05_world,
enemy_beam_effects, enemy_en21_component, enemy_en01_sub_component, shadow_effect_state,
actor_morph_blend, enemy_morph_selection)

Scan: `rooms=205 with_gaps=31 failures=0` (Log `build/validation/enemy_scan_20261014/plpowp-agent-scan.log`).
