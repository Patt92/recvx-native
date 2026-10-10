# A01–A04 Spielerkampf nativ portiert — Ergebnis

Stand 2026-10-10. Auftrag: `work/native_port_A01_A04_combat.md`. Eigene Dateien:
`native/player_combat_core.c/.h`, `native/combat.cpp/.h`, `native/combat_geometry.c/.h`,
`native/targeting.cpp`, `tests/native/combat_state_test.cpp`,
`tests/native/replays/combat-handgun.txt`, `tools/verify_combat.py`, diese Datei und
`work/native_port_A01_A04_combat.patch`. Kein Commit, kein Push, kein Paket.
`python3 tools/check_no_original_core.py` vor Arbeit und vor jedem Build: PASS.

Der Zustandsautomat ist eigenständig neu geschrieben. Kein Originalkörper, kein
Generator, kein `.inc` aus den Originalquellen, kein zweiter Arbeitssatz: der
Kern arbeitet auf der einen Kampfsicht `RECVX_COMBAT_PLAYER` des vorhandenen
Spielerbesitzers und auf den `RECVX_ENEMY_WORK`s der gemeinsamen Gegnertabelle
(`native/enemies/enemy_work.h`). `archived/FORBIDDEN_DONTUSE` wurde nicht
gelesen, durchsucht oder verwendet.

---

## 1. Implementiert (neu geschrieben, mit Originalfundstelle)

Alle Fundstellen in `src/ps2/veronica/prog/`, ausschließlich als Semantikreferenz.

### A01 Zielsuche / Zielwechsel / Armwinkel
| Mechanismus | Original | Nativ |
|---|---|---|
| `bhSearchNearEnemy` (Kegel, Sichtkapsel, nächster Gegner, `stflg 0x800`) | `pwksub.c:218` | `player_combat_core.c cb_search_near_enemy` |
| `bhSearchNextEnemy` (sortierte Liste, Besuchtmarke, Rückfall auf `src_no`) | `pwksub.c:506` | `cb_search_next_enemy` |
| `bhCalcLockEneYR` (Nachdrehen mit Schulterversatz `aa-24576`, 3.0) | `pwksub.c:170` | `cb_calc_lock_ene_yr` |
| `bhSearchEnemy` (nach Entfernung sortierte Suchliste in `pp.sch_lst`) | `playpch.c:318` | `cb_search_enemy` |
| `SetLockOnDirection` (`LckTbl`, Zweizielaufteilung, `mode 0x4/0x8/0x10`) | `playpch.c:401` | `cb_set_lockon_direction` |
| `bhCPM2_SearchPch` (Zielwechsel Doppelwaffe, `hed_rate`, Sondermotion 117) | `playpch.c:1174` | `cb_search_pch` |
| `PlyPchMain` (Armwinkel beider Hände, Kopf-/Brustrate, `owP` 4/5/7/11) | `playpch.c:18` | `cb_ply_pch_main` |
| `MixSetToJointRot` (Gelenkkette 1/2/6/7 bzw. 1/2/10/11) | `playpch.c:150` | `cb_mix_set_to_joint_rot` |
| `GetOneObjectMotion` / `SetOneObjectMotion` (Winkelkanal, Hokan-Mischung) | `playpch.c:185/231` | `cb_get_one_object_motion` / `cb_set_one_object_motion` |
| `AngZyxToYzx` / `AngYzxToZyx` | `Motion.c:969/991` | `cb_ang_zyx_to_yzx` / `cb_ang_yzx_to_zyx` |
| Nachdrehen auf `plp->ayp` über `yrct`, Fußklang am Ende | `player.c:1419-1457` | `recvx_combat_tick` |

### A02 Ready / Release / Reload / Munition / `at_flg`
| Mechanismus | Original | Nativ |
|---|---|---|
| `bhCPM0_action` (Eintritt, `mode1=1`, `mode2=0x40`, `at_flg=1`, `EXP1_I(0)`) | `player.c:1777` | `cb_cpm0_action` |
| Kampfanteil `bhControlPlayerPad` (Freigabe: `mode1=0`, `stflg 0x400`, `st_flg 0x4`) | `player.c:1816-1830` | `cb_cpm0_action` |
| `bhCPM2_act_suw` (Ziehen, Zielsuche, Munitionsprüfung, `gm_flg 0x40000`) | `player.c:4569` | `cb_act_suw` |
| `bhCPM2_act_wre` (Wegstecken, `mtn_add -131072`, `flg 0x10000` löschen) | `player.c:5198` | `cb_act_wre` |
| `bhCPM2_act_rld` (Nachladen, Magazin/Hülsen, SE 257/258/263/264/265/272/285/286) | `player.c:5953` | `cb_act_rld` |
| `bhCheckBullet` / `bhCountBullet` (vorzeichenbehafteter 16-Bit-Zähler, Endlosbit 0x800) | `weapon.c:421/439` | Dienste, gebunden an `Inventory` |
| `bhSetMagazine` | `pwksub.c:3372` | `cb_set_magazine` |
| `bhSetYakkyou` (Hülse, `yk_ct`, Wechselzähler `hr/hl` für Waffe 8) | `pwksub.c:3242` | `cb_set_yakkyou` |

### A03 Waffenzustandsautomat Pistole / Salve / Doppelwaffe
| Mechanismus | Original | Nativ |
|---|---|---|
| `bhCPM1_act_atk` (Verteilung 64..72/164/169/197, `rtspd`, `flg`-Maske) | `player.c:4481` | `cb_act_atk_dispatch` |
| `bhCPM2_act_wpn` (Halten, Hoch/Tief, Abzug, `at_flg 0x10/0x20/0x40`, Waffe-3-Sonderschuss) | `player.c:4764` | `cb_act_wpn` |
| `bhCPM2_act_atk` Einzelschuss (Motionwahl, Trefferabfrage, Nachladesprung) | `player.c:5269` | `cb_act_atk_single` |
| `bhCPM2_act_atk` Dauerfeuer (`ct0/ct1`, `fend_ct`, mode3 0..4, Leerlaufmotion 102/107/112) | `player.c:5560` | `cb_act_atk_auto` |
| `bhCPM2_act_suw_pch` (Doppelwaffe ziehen, `SetLockOnDirection(32768,8,0,0)`) | `playpch.c:480` | `cb_act_suw_pch` |
| `bhCPM2_act_atk_pch` (beide Hände, `gun_delay`, `mode 0x4`-Sondermotionen) | `playpch.c:602` | `cb_act_atk_pch` |
| `CheckGunHit` (statisches `GA_WORK`, Gelenk 9/13, Armwinkel im Schusswinkel) | `playpch.c:1130` | `cb_check_gun_hit` |
| `bhCPM2_act_wsc_pch` (im Original leer) | `playpch.c:596` | `cb_act_atk_dispatch` case 197 |
| `bhSetGunFire` (Licht, Vibration, `WpnEffTab[0]/[1]`, gemeinsames `bhEff003`, Matrixdrehungen je Waffe) | `pwksub.c:2670` | `cb_set_gun_fire` |
| `bhSetWeapon` (`obwp[0]`, `plp->flg 0x20000`, `owP`-Flags) | `weapon.c:128` | `recvx_combat_set_weapon` |
| `bhObjWpn` (Ruhewinkel aus `WpnAngTab`) | `weapon.c:197` | `cb_obj_wpn` |
| `bhActionWeapon` (Schlitten `0x80000`, Hahn `0x400000`, Trommel `0x800000`, je Spieler) | `weapon.c:217` | `cb_action_weapon` |
| `bhSetMotion` (Framevorschub, `frm_mode`, `mtn_attr`, `hokan_count--`) | `Motion.c:21` | `cb_set_motion` |
| `bhFixPosition` / `bhCalcFixOffset` (Fußfixierung über `PlyLegRoute`) | `Motion.c:861/872` | `cb_fix_position` |
| `bhAddSpeed` (Vorschub bei `flg 0x20000000`) | `pwksub.c:133` | `cb_add_speed` |
| `PlyPchInit` (PP_WORK nullen) | `playpch.c:12` | `recvx_combat_room_reset` |

### A04 Messer und Trefferzuweisung
| Mechanismus | Original | Nativ |
|---|---|---|
| Messerabfrage nach `bhCalcModel` (`mode2 0x45`, `mode3 1`, Fenster `KnfAtrTab..ef_yct`, SE 276) | `player.c:1689-1724` | `recvx_combat_after_model` |
| `st_flg 0x4` bei `plp->flg 0x4`, `flg2 &= ~0x200` | `player.c:1726-1735` | `recvx_combat_after_model` |
| `bhCPM2_act_knf` (Wandrückstoß, Motion 103/108/113) | `player.c:6154` | `cb_act_knf` |
| `bhCheckKnifeAtari` (Wand zuerst, dann alle Gegner, `dam[]`, `comb_flg 0x10`, SE 275/277) | `weapon.c:1002` | `cb_check_knife_atari` |
| `bhCheckGunAtari` (Wandstrecke +10, Streuradius `rn/rmax`, Rückwärtsfilter, Kapseltreffer, nächstes Gelenk, Entfernungsstufen 15/40, Durchschlagsbit `0x80000000`, Gelenkkoordinaten `0x20000000`, Wandfunken) | `weapon.c:473` | `cb_check_gun_atari` |
| `bhCheckWallAttrB89` (Sichtbarkeitsregel der Wandfunken über `cam.ncut`) | `hitchk.c:5881` | `cb_wall_attr_b89` |
| Kollisionsprimitive `njDistanceP2L`, `npCollisionCheckCC/CCEx/SC`, `npDistanceP2C` | `ps2_NaColi.c`, `njplus.c` | `combat_geometry.c` (vorhanden, jetzt im Link) |

### Reine Originaldatentabellen (zulässig, mit Wertregression)
`WpnAngTab[21][3]` (`weapon.c:118`) und `PlyLegRoute[2][7]` (`player.c:214`) fehlen in
`native/combat_tables_data.inc` (fremdes Paket) und liegen deshalb als
`RECVX_CB_WPN_ANG_TAB` / `RECVX_CB_PLY_LEG_ROUTE` in `native/player_combat_core.c`.
`tools/verify_combat.py --tables` vergleicht sie wertgleich mit der Quelle.

### Vertragsänderungen in `native/player_combat_core.h` (eigene Datei)
* `RECVX_CB_MOTION`: `md2P`-Kanalzeiger ersetzt durch `has_data` plus den Dienst
  `motion_angle` (GetOneObjectMotion, `playpch.c:185`). Ein eigener Zeiger auf
  `NJS_MKEY_A`-Speicher wäre eine zweite Motionquelle gewesen.
* `obj[]` trägt je Pass die Pose, die das Motionsampling des Besitzers geschrieben
  hat (im Original ist genau das `objP`); `obj_dirty` markiert die Knoten, die der
  Kampf selbst geschrieben hat. Nur diese gehen als Überschreibung zurück.
* `wax..waz` auf `int16_t` (BH_PWORK-Breite), `ct3` ergänzt.
* `RECVX_COMBAT_SYS.cam_pe_ax/cam_pe_pers/cam_written` für die Kameraschreibzugriffe
  des Originals aus dem Kampfpfad.
* `RECVX_COMBAT_RESULT` um Zustands- und Zählfelder erweitert (`mode1/2/3`, `at_flg`,
  `flg`, `stflg`, `mtn_no`, `frm_no`, `src_no`, `locked`, `shells`, `magazines`,
  `reloads`, `gun_atari`, `knife_wall`).

---

## 2. Produktiv angeschlossen (`native/combat.cpp`)

Gebunden an die vorhandenen Besitzer, kein zweiter Arbeitssatz:

* Tabellen `WpnTab`/`WpnEffTab`/`PlMtnWpn`/`KnfAtrTab`/`EneDamNear|Mid|Far` als Sicht
  auf genau die vorhandenen Tabellen (`static_assert` auf 0x68/0x28 Byte), `WpnAngTab`
  aus `player_combat_core.c`.
* Munition: `bhCheckBullet` aus `swork.pip` (Endlosbit 0x800, `at_flg 0x40000000`),
  `bhCountBullet`/`bhSearchBullet` über `Inventory::count_bullet()`/`reload()`.
  `gm_flg 0x40000` hat genau einen Platz: `Inventory::chamber_empty` wird vor dem Pass
  in `events->flags[11][0]` gespiegelt und danach zurückgelesen.
* Ereignisflags: `bhCkFlg/bhStFlg(sys->ev_flg, n)` auf Flagmatrix 1 (`snapshot.cpp:73`);
  `sys->gm_flg/st_flg/sp_flg` auf `events->flags[11/5/6][0]` wie `computer.cpp:41`.
* Pose: `obj[]` aus `PlayerAsset::local_pose` (Skriptknoten über `model_index`),
  `owp_mtx[]` aus `script_nodes(pose(...))`, `motion_angle` aus `motion_clip`.
  `mnw` (MN_WORK-Sicht) aus `motion_frames`/`motion_clip`.
* Kollision: `bhCheckL2Wall` und `bhCheckC2WallN` gegen `RoomWorld::combat_wall_distance`
  (Flags 0x400 bzw. 0x40000400).
* Effekte: `bhSetEffectTb` id 3/type 0 auf den vorhandenen `WeaponFlash`-Port,
  id 0/type 10 auf `RoomWorld::spawn_player_effect` samt `yk_ct`.
* Ton: `CallPlayerWeaponSeEx` auf `AudioMixer::play_weapon_sound`
  (`snd_wpno` = Bank, `seno0` = Programm); Fußklang auf `AudioMixer::footstep`.
* `AimSelection` aus `pp.lck_tbl` (Hostindex über `RECVX_ENEMY_WORK.owner_index`) und
  `pp.obj_r_ang/obj_l_ang`.
* Die alte Host-Näherung `CombatController::hit` (geratener Streuradius, eigene
  Trefferliste) ist entfallen; sie hatte keinen Aufrufer. `native/targeting.cpp` nennt
  jetzt die Fundstellen im Kern und hält ausdrücklich keine zweite Zielliste.

---

## 3. Komponentengeprüft

`tests/native/combat_state_test.cpp` treibt `native/player_combat_core.c` direkt und
prüft gegen echte `RECVX_ENEMY_WORK`s (zwei Works mit `watr`-Kapsel, `sv->obj_num`
und `sv->joint_matrix`). Keine abgeschwächte Erwartung.

Befehl und Ergebnis:

```
python3 tools/verify_combat.py --tables   # PASS
python3 tools/verify_combat.py --unit     # PASS
```

Abgedeckt:
1. **Tabellen**: `WpnAngTab` Zeile 10/13/14/19/20 und der Nullkopf, `PlyLegRoute`
   beide Routen, `PlMtnWpn`, `KnfAtrTab`, `EneDamNear` unverändert.
2. **Eintritt**: ohne Pad 0x10 gehört der Pass nicht dem Kampf; mit 0x10 setzt
   `bhCPM0_action` `mode1/mode2/mode3 = 1/0x40/0`, `at_flg 0x1`, `flg 0x10000`,
   `stflg 0x10400`, Motion `PlMtnWpn[0]`, `hokan_count` 4 → 3 (`bhSetMotion`).
3. **Zielaufschaltung**: nächster Gegner wird `src_no`, Gegner-`stflg 0x800` gesetzt,
   `flg 0x100000`; Wandverdeckung (`bhCheckC2WallN 0x40000400`) verhindert jede
   Aufschaltung; ohne Gegner wird kein Ziel erfunden; `bhCalcLockEneYR` dreht auf die
   Seite des Gegners (Vorzeichen geprüft) und liefert bei Gegner geradeaus nur die
   Schulterkorrektur.
4. **Zielwechsel**: Pad 0x80 schaltet auf den zweiten Gegner, markiert ihn und setzt
   `yrct` 4 (im selben Pass auf 3 dekrementiert, `player.c:1419`).
5. **Ziehen → Halten**: Ablauf bis `mode2 65` mit `at_flg 0x2`.
6. **Schuss**: `mode2 69`, genau ein `bhSetGunFire`, `at_flg 0x40`, Mündungslicht,
   Schussklang `seno0`; `WpnEffTab[3][0]` ist selbst `bhEff003`, deshalb zwei
   `bhEff003`-Arbeiten und drei `bhSetEffectTb`-Anforderungen (`pwksub.c:2742/2872/2956`).
   Munition sinkt erst im folgenden `bhCPM2_act_atk` (`bhCountBullet`).
7. **Treffer**: Gegner-`flg 0x4`, `dam[djnt_no] += EneDamNear[id][wno]`, `comb_flg 0x10`,
   `wpnr_no`, `djnt_no > 0`; der entfernte Gegner bleibt unversehrt.
8. **Entfernungsstufen**: 15/40 → `EneDamNear/Mid/Far` mit `comb_flg 0x10/0x20/0x40`.
9. **Blockierter Schuss**: Wand zwischen Trefferpunkt und Mündung (`bhCheckC2WallN 0x400`)
   verwirft den Treffer, verbraucht aber die Patrone; derselbe Schuss trifft ohne Wand.
10. **Rückstoß → Nachladen**: leeres Magazin setzt `gm_flg 0x40000`, Abzug führt nach
    `mode2 70`, Motion 116, `bhSetMagazine` läuft, Rückkehr in den Haltezustand; ohne
    Reserve nur SE 260 und kein Schuss.
11. **Hoch-/Tiefzielen**: `mode2 66/67`, `at_flg 0x4/0x8`, `waxp ±8192`, Rückkehr auf 65.
12. **Wegstecken**: Zieltaste los → `mode2 68`, danach Freigabe (`mode1 0`),
    `flg 0x10000` und `stflg 0x10400` gelöscht.
13. **Messer**: Hieb im Originalfenster (`KnfAtrTab[0]=6` .. `ef_yct=13`), `dam[]`
    genau einmal (`flg 0x4`-Filter), `comb_flg 0x10`, SE 275; Fehlschlag außer
    Reichweite bucht nichts; Wandtreffer → `mode2 72`, SE 277, kein Schaden,
    Rückstoß läuft aus.
14. **Salvenzweig**: Waffe 12 (`flg 0x40`) läuft über `bhCPM2_act_atk` Dauerfeuer und
    ruft `bhSetGunFire` wiederholt.
15. **Doppelwaffe**: Waffe 7 → `mode2 164`, `LckTbl` gefüllt mit dem Gegner-Work,
    `flg 0x100000`; Abzug → `mode2 169`, rechte Hand sofort `CheckGunHit`, linke Hand
    nach `pp.gun_delay`; `PlyPchMain` beansprucht `owP[7]/[11]`.
16. **Lücken**: `SetMtn*Hokan` und `bhControlPlayerPad` werden namentlich gemeldet;
    die gebundene Gegnertabelle wird nicht als Lücke gemeldet.

---

## 4. Runtime-geprüft

Build nur im eigenen Verzeichnis:

```
cmake -S . -B build/a01-combat -DCMAKE_BUILD_TYPE=Release -DRECVX_FETCH_SDL2=OFF \
      -DBUILD_TESTING=ON -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build/a01-combat --parallel 8
```

`ctest -C Release` (ohne die drei unten genannten fremden Ziele): **49/51 bestanden**.
Die zwei Fehlschläge sind keine Kampfregression, siehe Abschnitt 6.

Replay `tests/native/replays/combat-handgun.txt` (originaler Steve-Ablauf, kein
Positionssetzen, keine Flagbearbeitung, kein Debug-Inventar):

```
python3 tools/verify_combat.py --runtime-only --runtime <runtime> --data data
```

* **Ohne** den Anschluss aus `work/native_port_A01_A04_combat.patch` (Abschnitt 1):
  gemessen `combat_shots = 0`, `combat_attacks = 0`. Ursache gemessen: `native/runtime.cpp`
  nimmt `events->flags[5][0] & 4` (`sys->st_flg 0x4`) in seine `blocked`-Bedingung auf,
  und genau dieses Bit setzt `bhCPM1_act_atk` (`player.c:4483`) in jedem Kampfpass selbst.
  Der Kampf tritt ein, sperrt sich im nächsten Tick selbst aus und setzt `inhibited_`.
  Das Skript meldet das namentlich.
* **Mit** dem Anschluss (Runtime aus demselben Buildstand, nur `runtime.cpp` mit der
  einen Zeile, gebaut und gelinkt in `build/a01-combat/own/`): bestanden —
  Skriptgeschenk M93R, automatische Ausrüstung (`weapon = 4`), Zielen, **zwei Schüsse**
  (`combat_shots = 2`, `combat_attacks = 2`), **Munition 12 → 10**, Schussklang
  `ARMS/0001/0005` zweimal, Wegstecken zurück auf Leerlaufmotion 42, keine blockierten
  Eventbefehle.
* Integrationslauf mit echtem `PlayerAsset` und echter Raumgeometrie
  (`tests/native/gameplay_bindings_test.cpp`, RM_0050-Hundefixture, 240 Ticks
  Zielen + Abzug + Tiefzielen): gemessen `mode2 = 67`, `mode3 = 1`, `shots = 1`,
  `attacks = 1`, Munition 12 → 11. Genau ein Schuss ist originalgetreu: `at_flg 0x40`
  bleibt verriegelt, solange Pad 0x100 gehalten wird (`player.c:5511`). Mit der im
  Patch enthaltenen korrigierten Erwartung läuft dieser Test durch (gemessen).
* Keine Regression in Nachbarsuiten, je einmal mit und ohne Anschluss gemessen:
  `tools/verify_events.py`, `tools/verify_progression.py`, `tools/verify_stairs.py`
  bestanden.

Nicht in der Runtime geprüft: Aufschaltung, Zielwechsel, Messerschaden und
Gegnerschaden — dafür fehlt die Gegnertabelle (Abschnitt 5, Punkt 1). Der bisherige
Replay `tests/native/replays/knife-combat.txt` endet mit `weapon = 1` (Feuerzeug statt
Messer); das ist eine Drift der Inventar-/Menüticks (fremdes Paket) und war schon vor
dieser Arbeit so. Kein Durchspielnachweis, kein Paketbau, kein Windows-Lauf.

---

## 5. Offen (benannt, nicht verdeckt)

Jede nicht gebundene Originalroutine wird zur Laufzeit über den `gap`-Dienst mit ihrem
Originalnamen gemeldet (`CombatController::unbound_routines()`), nie durch einen
plausiblen Ersatzwert.

1. **Gegnertabelle (C02)** — `RECVX_COMBAT_SERVICES.enemies` bleibt in `combat.cpp`
   NULL, weil `RoomWorld` die `RECVX_ENEMY_TABLE` noch nicht exponiert. Folge: in der
   Runtime gibt es keine Aufschaltung, keinen Zielwechsel und keinen Schaden. Der Kern
   ist fertig; eine Zeile in `combat.cpp` schließt es (Patch Abschnitt 4). Gemeldet als
   `sys->ene / sys->ewk_n (Gegnertabelle)`.
2. **`bhCheckCapCol2Capsule`** (`weapon.c:1496`) — die CPCL-Gelenkkapseln gehören dem
   Gegnervertrag. Waffen mit `WpnTab.flg & 0x8` (Waffe 13) treffen deshalb nicht;
   gemeldet statt genähert.
3. **`bhCPM2_act_scp`** (`playpch2.c:13`) — Zielfernrohr (`mode2 71`, Waffen mit
   `flg 0x20`, u. a. Waffe 13/18). Braucht `exp0->scp_ct`, `cam.ppers` und
   `sys->pad_ay/pad_dy`; gemeldet, nicht genähert.
4. **`SetMtn*Hokan` Knoteninterpolation** (`Motion.c:232/472/747`) — der Framevorschub
   und `hokan_count` sind portiert, die Blendrate der Knotenwinkel gehört dem
   vorhandenen Posenbesitzer, der ohne Blendung sampelt. Gemeldet.
5. **`bhFixPositionXYZ`** (`flg 0x8000000`-Zweig, `player.c:1554`) — gemeldet.
6. **`bhControlPlayerPad`** Laufsteuerung (`player.c:1836-1960`) und
   **`bhCPM1_act_bas`** (`player.c:1964`) — Bewegungsbesitzer (B00). Nur der
   Kampfanteil der Freigabe ist portiert; der Rest gemeldet.
7. **Wurf-/Streueffekte** in `bhCPM2_act_wpn` (`player.c:4993-5120`, Waffen 10/14-20),
   **`bhSetGunFire`** Waffen 11/20/21 (`pwksub.c:2978/3127/3156`), **Waffe-12-Rauch**
   (`pwksub.c:3350`) und der **Trommeleffekt** in `bhActionWeapon` (`weapon.c:360`) —
   Effektgeometrie der E-Pakete; die davon unabhängigen Originalwirkungen
   (`bhStFlg(ev_flg,75)`, `bhCountBullet`) sind portiert.
8. **Wassereinschlag** in `bhCheckGunAtari`/`bhCheckKnifeAtari` und `bhSetWaterSplash*` —
   Dienste der E-Pakete, gemeldet.
9. **`sys->pt_flg`** — `RoomWorld` hat nur einen Schreibweg (Patch Abschnitt 6).
10. **`cam.pe_ax` / `cam.pe_pers`** — Kamerabesitzer (Patch Abschnitt 7).
11. **`bhSetWeapon` `exp0[0]`** — das erste `int` von `EXP_WORK` (`weapon.c:160`);
    Semantik aus dem Decomp nicht belegbar, gemeldet.
12. **`WpnTab.act_ct[ply_id == 3]`** (`player.c:5412`) — das Original liest dort über
    `act_ct2` hinaus in das Feld `r`. Dieser Quirk ist nicht nachgebildet, sondern
    gemeldet.
13. **`owP[7]/[11].flg |= 8`** aus `bhInitPlayer`/`bhResetPlayer` (`player.c:850/944`) —
    die `owP`-Flagbank gehört dem Posenbesitzer; gemeldet.
14. **Waffenobjektteile** — `RECVX_CB_WEAPON_OBJECT.part_pos0/part_ang1/part2_hidden_cleared`
    (Schlitten, Trommel, verstecktes Teil) werden originalgetreu berechnet, aber vom
    `WeaponAsset`-Zeichenpfad noch nicht gelesen.
15. **A05 weitere Waffen** — nicht Teil der Abnahme; die Zweige für Waffen 10/11/14-22
    sind im Automaten vorhanden, ihre Effekte nach Punkt 7 offen.

---

## 6. Fremde Baustellen (gemessen, nicht angefasst)

* `recvx-subplayer-tests` und `recvx-enemy-spawn-tests` bauen nicht: die Verlegung nach
  `native/enemies/` ist im Gange, diese beiden Ziele haben `native/enemies` nicht im
  Includepfad (`native/subplayer.c:11`, `tests/native/enemy_spawn_test.cpp:1`).
* `recvx-progression-tests` bricht wie im Auftrag angekündigt bei HEAD
  (`tests/native/progression_test.cpp:125` nutzt `context.random`, `EnemySpawnContext`
  hat `rand`).
* `effect_pool_lifetime` (`tests/native/effect_pool_test.cpp:30`) sichert die
  Partikellücke zu; ein Peer hat `native/effect_particles.*` implementiert. Der Pfad
  berührt den Kampf nicht.
* `native-gameplay-bindings` (`tests/native/gameplay_bindings_test.cpp:62`) sicherte zu,
  dass der Spielerkampf inaktiv bleibt. Diese Zusicherung ist jetzt falsch, weil die
  Lücke geschlossen ist. Die Erwartung wird im Patch nicht abgeschwächt, sondern auf die
  gemessenen Originalwerte gehoben.

---

## 7. Integrator-Patch

`work/native_port_A01_A04_combat.patch` (`git apply --check`: sauber). Inhalt:

| # | Datei | Art | Zweck |
|---|---|---|---|
| 1 | `native/runtime.cpp` | Diff (Pflicht) | `sys->st_flg 0x4` aus `blocked` nehmen, solange der Kampfpass läuft — sonst sperrt sich der Kampf selbst aus |
| 2 | `tests/native/gameplay_bindings_test.cpp` | Diff (Pflicht) | stale Lückenzusicherung auf die gemessenen Originalwerte heben |
| 3 | `CMakeLists.txt` | Diff (Pflicht) | `native/combat_geometry.c` als eigene Quelle, neues Testziel `combat_player_state` |
| 4 | `native/world.*` | Vertrag | `RECVX_ENEMY_TABLE` exponieren (C02) — eine Zeile in `combat.cpp` schließt A01/A04 produktiv |
| 5 | `native/combat_tables_data.inc` | Vertrag | `WpnAngTab`/`PlyLegRoute` dort zusammenlegen, Wertregression mitnehmen |
| 6 | `RoomWorld` | Vertrag | Leseweg für `sys->pt_flg` |
| 7 | Kamerabesitzer | Vertrag | `cam.pe_ax`/`cam.pe_pers` übernehmen |
| 8 | `native/targeting.*` | Vertrag | Hostklasse `Targeting` entfällt mit `tests/native/combat_test.cpp` |

Bis Punkt 3 im Build ist, übersetzt `native/player_combat_core.c` die
Kollisionseinheit per `#include "combat_geometry.c"` mit (dort kommentiert), damit der
produktive Link vollständig ist.
