# combat_barrels_muzzle — Fassexplosion und Calico-Mündungsfeuer

Thema: combat_barrels_muzzle (2026-10-14). Auftrag: zwei gebündelte Kampffehler.

## 1) Fässer explodieren nicht (kein Effekt, kein Licht, kein Ton, kein Flächenschaden)

**Originalpfad** (Semantikreferenz, nicht kompiliert):
- `eneset.c:391 bhEne00` / `eneset.c:461 bhSetEffect(17,…)`: das Sprengfass. Beim Treffer
  (`flg 0x4`, Waffe ungleich `wpnr_no 2`) ruft es `bhSetExplosion((px,py+10,pz))`, setzt
  `flg |= 2` (sterbend), `mdflg |= 1`, `flg &= ~0x78`, `atr.flg &= ~1`, `mode0 = 2`
  (nativ bereits vorhanden in `native/enemies/en00.c:145-157`).
- `weapon.c:1706 bhSetExplosion`: `CallPlayerActionSe(534,1)` (Bank 2, Liste `534&255`),
  Blitzlicht `rom->lgtp` (flg 3, type 101, aspd 8, lsrc 4, r/g/b 4.5/3.0/1.5, nr 30, fr 200),
  `bhSetEffect(16,&pnt,NULL,16)` und die Feuerbälle 23 + zwei Trümmer 24 (Position um
  `4.0f*cam.vx/vy/vz` zum Betrachter versetzt).
- `weapon.c:1812 bhSetExplosionEffect`: dieselbe Folge ohne Ton/Licht/Zünder (gameover.c:539,
  event.c:8766).
- `effsub1.c bhEff016`: zählt `lkono` (hier 16) Passes und ruft dann
  `bhCheckBombAtari(&px, 20, 150, 100)` = der Flächenschaden. `bhEff023/024` sind bereits
  handportiert (`native/effect_sprite_procedures.cpp:2948/2496`).
- `weapon.c:1321 bhCheckBombAtari`: Spieler- und Gegnerhälfte; nativ als
  `RoomWorld::bomb_damage` (`native/world.cpp`) vorhanden.

**Ursache**: `native/effect_sprites.cpp:1109 set_explosion/…:1110 set_explosion_effect` waren
leere Stubs (`(void)position;(void)view;`). `RoomWorld::explode` (world.cpp:1356) rief sie auf
und erzeugte damit nichts.

**Änderung** (`native/effect_sprites.cpp`): beide Funktionen vollständig implementiert.
`set_explosion` legt Ton 534, das `rom->lgtp`-Blitzlicht, `request_direct(16,&pos,16)` und über
die gemeinsame Originalvorlage `sys->ef` (`effect_detail::request_template`/`submit_template`)
den Feuerball 23 (Skalierung 6, Position `-4*cam.v`) plus die zwei Trümmer 24 an. Die
`rand()`-Reihenfolge je Trümmer ist **sx, ay, ax** (wie im Original), der Startversatz entsteht
aus `njRotateXYZ(ax,ay,0)` auf `(0,0,-2)` (`njUnitMatrix`/`njRotateXYZ`/`njCalcVector`, exakt
die Originalfolge). `set_explosion_effect` ist dieselbe Folge ohne Ton/Licht/Zünder. Das
persistente `sys->ef`-Verhalten (Felder, die das Original nicht neu setzt, bleiben stehen) und
die Position der Trümmer (px/py/pz des Feuerballs) sind erhalten.

**Prüfungen**:
- Fester Test `tests/native/effect_debris_test.cpp` (CTest `effect_collision_debris`): prüft
  die Auftragsfolge `[23,24,24,16,23,24,24]`, `sounds == {534}`, das Blitzlicht (flg/type/aspd/
  lsrc/nr/fr/Farbe) und dass der Zünder `16` nach 16 Passes `bhCheckBombAtari(20,150,100)`
  anfordert. `tests/native/progression_test.cpp` (CTest `engine_progression_resources`) prüft
  dieselbe Anfrage (7 Aufträge, 1 Ton, Licht). Beide ersetzten frühere „explosion gap“-Zusagen.
- Runtime: `build/validation/combat_barrels_muzzle-20261014/` — Diagnose-Replay
  `barrel-replay.txt` in RM_0080 (Sprengfass bei (16.63,0,-16.14)), Handfeuerwaffe per
  Debugmenü. Screenshot `barrel-explosion-175.png`: großer Feuerball + Blitzlicht. Der
  Fasswork wechselt auf `mode [2,0,0,0]` (gezündet). Eine Zusatzspur zeigte die Aufträge
  `spawns=4 ids=[16,23,24,24] sounds=1 light=1` und danach den Bombenauftrag (`bomb_damage`,
  Spielertest `hit=1`; die Sichtlinie zur Diagnoseposition ist mit `wall=0.479<1` verdeckt, wie
  im Original `bhCheckC2WallN(…,0x400)` — daher dort keine HP-Abweichung).

## 2) Mündungsfeuer der Calico (M-100P, Item 142 → `wpnr_no 9`) zeigt senkrecht nach oben

**Originalpfad**: `pwksub.c:2670 bhSetGunFire(pp,wno,jno,hand,ang)`. Für `WpnEffTab[wno][0]`
(id 3, `flg 0x1000`) wird `op->mtx` **einmal beim Schuss** gesetzt:
`njSetMatrix(op->mtx, &pp->mlwP->owP[jno].mtx); njTranslate(op->mtx, ops); …` mit `ops` =
`{-wp_fps1.x,…}` bei `hand==1`. Der zweite (verlinkte) Satz kommt aus dem `switch(wno)`
`case 3..7,9` und wird beim Zeichnen aus `op->lkwkp->mlwP->owP[lkono].mtx` gebildet. Beide
lesen `pp->mlwP->owP[jno]` — das **owP des zuletzt von `bhCalcModel(plp)` berechneten
Baums** (`jno 9` rechte, `jno 13` linke Hand; `PlyPchMain playpch.c:78` schreibt vorher die
Schulterknoten 7/11). Der linke Lauf feuert über `pp->gun_delay` zwei Ticks nach dem rechten
(`playpch.c:1105-1124`).

**Befund**: `CombatController::publish_pose` (`native/combat.cpp`) hat `view.owp_mtx` bei jedem
Kampftick **neu aus `(motion,frame)` gesampelt**. Das liest `cb_set_gun_fire` (Kern) für die
gesetzte Matrix/Position. Am linken-Lauf-Tick (Feuerbewegung `101`, Bild 2) ist das der
Rückstoßrahmen mit hochgerissenen Armen, während der zuletzt berechnete (committete) Baum
(`player_joint_matrices_`, `native/runtime.cpp` `head_hair_step`) einen flacheren linken
Handrahmen hat. Messung (Diagnosespur): linkes Mündungsfeuer vorher `pos=(-31.27,**17.71**,48.45)`
gegenüber rechtem `14.02` — die linke Handachse zeigte dort nach oben, das Feuer stand über dem
Kopf. Screenshot `muzzle-before-left-305.png`.

**Änderung**: `publish_pose` liest `view.owp_mtx` jetzt aus dem **zulettzt berechneten
Spielerbaum** `RoomWorld::player_committed_joints()` (neuer öffentlicher Zugriff in
`native/world.h`; Rückfall auf das Sampling nur, solange noch kein Baum committet ist). Damit
liest `bhSetGunFire` genau das `owP` des vorigen `bhCalcModel(plp)` wie das Original und
sampelt keine neue Pose aus dem laufenden Motionzustand. Der lokale `objP`-Satz (`view.obj`)
für `GetOneObjectMotion`/`MixSetToJointRot` bleibt unverändert.

**Prüfungen**:
- Runtime, Calico per Debugmenü in RM_0080 (`calico-replay.txt`), Screenshots:
  linkes Mündungsfeuer vorher über dem Kopf (`muzzle-before-left-305.png`), nachher am linken
  Lauf (`muzzle-after-left-305.png`, heller Pixelblock von `y[129..143]` nach `y[166..176]`),
  rechtes unverändert am rechten Lauf (`muzzle-after-right-302.png`).
- `tools/verify_combat.py`: Tabellen, Komponententest und beide Runtime-Stufen bestanden
  (M93R-Zielablauf, Munition 12→10, Schussklang; RM_0080 Aufschaltung, 4 Treffer, Gegner-HP
  378 statt 450 ohne Schuss). Keine Regression durch die geänderte Posepublikation.
- Volle CTest-Suite: **100 % bestanden (91 Tests)**, u. a. `combat_weapons_ammunition`,
  `native_weapon_flash`, `effect_collision_debris`, `engine_progression_resources`.

## Geänderte Dateien
- `native/effect_sprites.cpp` — `set_explosion`/`set_explosion_effect` implementiert.
- `native/combat.cpp`, `native/combat.h` — `publish_pose` liest den committeten Spielerbaum
  (`RoomWorld* world` als Parameter).
- `native/world.h` — öffentlicher Zugriff `player_committed_joints()`.
- `tests/native/effect_debris_test.cpp`, `tests/native/progression_test.cpp` — echte
  Explosionszusagen statt „gap“.

## Offene Punkte / Grenzen
- `bhSetExplosionEffectEx` (`weapon.c:1874`, Skalierungsvariante, effsub1.c:10072) bleibt
  unportiert — nicht Teil des Fass-/`bhSetExplosion`-Pfads.
- Der Spieler-Flächenschaden an der Diagnoseposition wird durch die Sichtlinie
  (`bhCheckC2WallN(…,0x400)`) verdeckt; ein Ort ohne Wandverdeckung wurde noch nicht
  beobachtet. Die Gegnerhälfte von `bomb_damage` war bereits vorhanden und ist unverändert.
- Bug 2 betrifft die *gesamte* Schusspose des Kampfkerns (jede Waffe), nicht nur die Calico;
  korrigiert wurde der gemeinsame Mechanismus in `publish_pose`.

## Ledger-Vorschläge (TAB-Format, Hauptagent führt zusammen)
```
native/effect_sprites.cpp:set_explosion	done	weapon.c:1706 bhSetExplosion (Ton 534, rom->lgtp, bhSetEffect 16, 23/24); test effect_collision_debris + Runtime RM_0080
native/effect_sprites.cpp:set_explosion_effect	done	weapon.c:1812 bhSetExplosionEffect (23/24 ohne Ton/Licht/16); test effect_collision_debris
native/combat.cpp:CombatController::publish_pose	done	pwksub.c:2670 bhSetGunFire liest owP des letzten bhCalcModel(plp); publish_pose nutzt RoomWorld::player_committed_joints()
```
