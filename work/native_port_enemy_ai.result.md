# Gegner-Angriffskette im normalen Spiel — 2026-10-12

Auftrag: Grapple/Biss, Befreiung durch Hebelrütteln, Schadensübertragung auf die
Spieler-HP und natives Game Over als geschlossene Schleife im Standard-Gameplay.
Umfang: `native/world_enemies.cpp`, `native/world.cpp/.h`, `native/enemies/en01_core.c`,
`native/combat.cpp/.h`, `native/runtime.cpp`, `native/debug_menu.cpp/.h`, neu
`tools/verify_enemy_ai.py`. Kein Originalkörper, kein Generator, keine Raumregel.
`tools/check_no_original_core.py` → PASS (Quelle und Binary). Kein Commit/Push.

## Baseline (vor den Änderungen, Build `build/enemy-ai-claude-20261011`)

RM_0080, Spieler per Diagnose direkt vor Zombie 0 (`position -3 0 24 0`),
`--enemy-ai on --enemy-grab on`, 30…900 Ticks: HP bleibt 160 (Startwert),
`player_scripted` nie wahr, Zombie 0 steht in Motion 0 auf seinem Spawn.
Lückenmeldungen: `bhSearchPlayer (pwksub.c:664)`, `bhCheckPlayer: EXP_WORK nlxb`,
`bhCheckRouteID / bhCheckRoute`, `bhGetFrameNum`, `bhCalcFixOffset`,
`bhEne01_DmgCheck: exp0`, `mlwP->owP[joint].mtx`, `bhSetEffect(17)`.
68/68 CTest grün.

## Ursachen und Portierung (Originalfundstellen)

| Mechanismus | Original | Nativ | Wirkung |
|---|---|---|---|
| Spielersuche | `pwksub.c:664 bhSearchPlayer`, `en01b.c:922 bhSearchPlayer2` | `en.search_player` | Zombie erkennt den Spieler im Sichtkegel (Brain/Search/EatCheck). |
| Gegner-Spieler-Kontakt | `hitchk.c:5712 bhCheckPlayer` | `s.check_player` | Bedingung `plp->flg & 8` war invertiert portiert; `nlxb` gilt nur ohne Flag 8 (sterbender Spieler). |
| Bereichs-/Bodeneffektflächen | `zonzon.c:373/402`, `hitchk.c:5503` | `en.enemy_atari(_atr)`, `en.floor_effect` | MV-Modus-Wechsel, Wasser-/Effektflächen. |
| Laufmotionswechsel | `pwksub.c:144 bhGetFrameNum` | `en.frame_num` | Ohne ihn kehrte `bhEne01_ChgWalkMtn` vor dem Motionswechsel zurück: kein Gehen. |
| Fußfixierung | `Motion.c:868 bhCalcFixOffset` | `en.calc_fix_offset` | Kette aus `en01_tree` über `objP`, minus `owP[last]` des letzten Passes. |
| Wegzonen | `rutchk.c:5/47` | `en.route_id/route_next`, `RoomWorld::route_areas_/route_table_` (Header 0x28/0x2C/0xA8) | Vorher an einen leeren Legacy-Setter übergeben. |
| Bewegung aus der Motion | `zonzon.c:209/251`, `pwksub.c:133` | `en.translate_motion`, `en.add_speed` | Wurzelkey-Delta → Weltbewegung bzw. `spd`. |
| Treppenflächen | `zonzon.c:310`, `hitchk.c:4965/5003` | `en.stair_area_atr`, `s.stair_use` | `sys->metcp`-Einträge nur gemeldet (bhObj001-Besitz). |
| Spielerkapsel | `player.c:1758 plp->watr`, `njplus.c:498` | `en.player_capsule`, `s.sphere_capsule` (→ `recvx_cb_check_sc`) | EatCheck Modus 2. |
| Waffenflag / Blickwinkel | `weapon.c WpnTab[].flg`, `plp->way` | `en.weapon_flg`, `en.player_way` (Runtime → `set_player_way` aus `CombatController::aim_yaw`) | DmgCheck-Trefferregeln, Blutwinkel. |
| Biss-Blut | `zonzon.c:466 bhEne_SetBlood2` | `en.set_blood2` | Partikel laufen; Spriteprozedur `bhEff005` fehlt (gemeldet `bhEff5`). |
| DmgCheck-Reihenfolge | `en01.c:3195` | `recvx_en01_dmg_check` | EXP0 erst hinter dem flg-Tor lesen (erster Pass vor Init). |
| Erstpass-`owP` | `bhMlbBinRealize` nullt `owP` | `s.joint_matrix/joint_point` | Vor dem ersten `bhCalcModel` liefert der Dienst die genullten Matrizen statt einer Lücke (Originalzustand). |

Originaldefekt nachgebildet: `en01.c:10297` multipliziert den schon geklammerten
`bhGetFrameNum`-Wert nochmals mit 65536 (32-Bit-Umlauf, vorzeichenlos
gerechnet); bei Klammerung auf `frm_num` wird der Startframe `frm_num`, einen
Key hinter der Motion. Dieser Pass behält nativ die Vorpose und meldet
`bhSetMotion:frm_no ausserhalb von frm_num`; der Vorschub wickelt wie
`Motion.c:106`.

Standard: `enemy_grab` ist in einer frisch erzeugten `debug.ini` nicht mehr
gesetzt; interaktiv gelten damit die Originalangriffe (`--headless/--replay`
aus, `enemy_grab = off`/Debugmenü/`--enemy-grab off` bleiben Overrides). Die
vorhandene Benutzer-`debug.ini` (nur `hotkey`) ist unverändert.

Diagnose: `--state` führt je gebundenem Gegner `work` (flg, stflg, mode0-3,
mtn_no, ct0, hp, pos, ay).

## Nachweise

- 68/68 CTest seriell (`build/enemy-ai-claude-20261011`), inkl. `debug_menu_hotkey_registry`.
- `tools/verify_enemy_ai.py --runtime … --data data` → PASS
  (`build/validation/enemy-ai-20261012/verify`):
  - natural (Spawn 0, keine Eingabe, 1500 Ticks): HP 160 → 10, Spieler am Ende gegriffen,
    Zombies in Verfolgung/Angriff; pending world bindings 0x8e vorbestehend, 0 blockiert.
  - front (Diagnose vor Zombie 0): Griff ≤ 90 Ticks, HP 140 / 140 / 40 (90/200/1000),
    Freigabe zwischen zwei Griffen, kein vorzeitiges Game Over.
  - lever (Richtungswechsel jeden Tick): HP 60 statt 40 nach 1000 Ticks.
  - death (1500 Ticks): HP −10, `gameover_active`, Phase 2 (bhGameOverSet-Anzeige).
- `tools/verify_combat.py --runtime-only` und `tools/verify_progression.py` bestanden.
- Gegen das installierte App-Binary (`build/validation/enemy-ai-20261012/<suite>`):
  detector, doors, gameover, frontend, lighter, stairs, continuity, motion_collision,
  push grün. `verify_events.py` scheitert bei `cemetery-walk` (`event_camera`/
  `event_tasks` nach 3600 Ticks) — identisch mit dem gesicherten Vorgänger-Binary
  (`events-baseline.log`), also vorbestehend aus paralleler Arbeit, nicht aus dieser
  Änderung.
- Verbleibende Lückenmeldungen in RM_0080: `StartVibrationEx` (kein Rumble-Besitzer),
  `bhEff5` (Blutsprite), `bhSetEffect(17)` (bhEne00-Fass), der Frame-Umlauf oben.

## Auslieferung

`Start.command` startet `build/start/macos-arm64/RECVX Native.app`. Vorheriger
Stand gesichert nach `archived/releases/enemy-ai-before-<zeit>/`, Runtime ersetzt,
ad hoc signiert, `check_no_original_core.py --binary` PASS, Smoke über
`./Start.command --headless` (RM_0080, Griff/Biss). Windows nicht gebaut.

## Offen

Hunde-KI (en04) jenseits der Aktivierung, Zombie-Treppen (`bhEne01_Kaidan*` nutzt
jetzt die Flächen, Motion-Zweige ungeprüft), `bhEff005`, Vibration, `nlxb`-Zweig,
`sys->metcp`-Treppenflag, Blutsprite-Typ 11 `op->mode1`. Keine Vollport- oder
Durchspielbehauptung.

## Nachtrag 2026-10-12 abends: Hunde, Spielermotion im Griff, Blutsprite, Vibration

Build `build/enemy-ai-claude-20261011` (Release, arm64), Policy PASS (Quelle, Mac-Binary,
Windows-PE mit Builddir). Kein Commit/Push. Geänderte Dateien dieses Nachtrags:
`native/world_enemies.cpp`, `native/world.cpp/.h`, `native/runtime.cpp`, `native/motion_set.cpp`,
`native/enemies/en04.c`, `native/enemies/en01_core.c`, `native/enemies/en01_work.h`,
`native/enemies/enemy_work.h`, `native/effect_sprites.cpp`, `native/effect_sprite_tables_data.inc`,
`tests/native/effect_debris_test.cpp`, `tools/verify_enemy_ai.py`. `native/vibration.*` stammt aus
der parallelen Sitzung und ist hier nur angebunden.

### Ursachen und Portierung

| Mechanismus | Original | Nativ | Befund |
|---|---|---|---|
| Spielermotion im Griff | `player.c:1502` `bhSetMotion(plp, mtn_add, mtn_md, mtn_tp)` jeden Pass (nach `bhControlEnemy`, game.c:48/49) | `RoomWorld::control_enemy_player_motion` → `set_motion` (Motion.c:79-115) auf `frm_no`/`hokan_count` des Spielerworks, Bibliothek `plp->mnwP` bzw. `mnwPb` | Vorher lief nur ein Vorschub ohne `frm_num`-Umlauf und ohne Hokan-Abzug: `bhEne04_PlyDG00` blieb in Zweig 1/2 stehen, der Hund wartete in MV04 auf `plp->flg & 4` – Griff endete nie. Pose weiterhin gesampelt (kein Hokan-Blend der Spielerpose, F01). |
| Griffbibliothek | `plp->mnwP = epw->mnwP` (en01.c:7584, en04.c:3405) | `RECVX_ENEMY_PLAYER::mtn_enemy_work`, `RoomWorld::player_enemy_work_`, `player_enemy_clip()` | Vorher nur die Gegner-ID: in RM_0031 fiel die Wahl auf einen anderen id-1-Work (Raum-Motionsform) → „actor motion unavailable“/„player enemy motion without clip“. |
| Spielerpose während des Griffs | en01.c:2829, en07.c:2190 | `model_pose_physical` kind 0, `link_matrix` kind 0 | Beide Pfade sampeln jetzt die Gegnerbibliothek statt der Spielerliste (Absturz RM_1000 ab Tick ~270 behoben). |
| `bhEff001` (Schattenbesitzer) | effsub1.c:599 `op->lkwkp` | `tick_shadow`/`bhDrawMdfEffect`: Besitzer über `RECVX_ENEMY_WORK::owner` (auch `enemy_children_`) | Kindworks lagen nicht in `entities_`; Lücke „linked native owner missing“ verschwunden. F05 zeichnet nur mit geladenem SYSTEM-Modifierbündel (wie `effect_model0_`). |
| `bhEff005` Blutspritzer | effsub1.c:2099, UV-Tabellen S/M/L | `step_eff005`, `eff005_uv_*` | Vorher gemeldete Lücke `bhEff5`; Testliste der unportierten Prozeduren entsprechend bereinigt. |
| Vibration | sdfunc.c:3382 → vibman.c:58 → ps2_sg_pdvib.c:46 | `VibrationUnit` (parallele Sitzung), Runtime: Tür-Shake, Treppen, Gegner-Anforderungen, `set_event_mode`, Pad-Rumble je Tick | Headless: 20 Anforderungen, 0 Starts (kein Pad → −2), `--state` `vibration_requests/starts`. |
| `bhCheckRoute` ohne Zone | rutchk.c:5 schreibt `pos` nur bei gültigem Zonenpaar | en04-Brain meldet und läuft weiter (vorher Abbruch des Brain-Passes) | RM_1000: Zonen mit Höhe 0, Hund 1 auf y=8 → auch im Original `0xFF` und uninitialisiertes `pos`. |
| Motion mit Überschusskanälen | Motion.c:131 schreibt `objP[n]` hinter das Modell | `Pass::node` Sentinel `surplus`, Kanal wird verbraucht | RM_0020/RM_0050 (id 1, mtn 0); Lücke gemeldet statt Abbruch. |
| `objP[0].pos[1] = 0` | en01.c:6311 | `en.node_local_set` | Vorher Lücke ohne Schreibweg (RM_0060). |
| Vererbte Treppen-/Wegezonen-Daten | – | `--state`: je Gegner `type`, Spieler `player_work` (mode0-3, flg, mtn_no, frm_no, hokan_count, mtn_enemy[_owner]) | Diagnose für die Nachweise. |

Nachgebildete Originaldefekte (gemeldet, kein Ersatz): `bhEne04_MV02 rot undefiniert (en04.c:1825)`
(Zweig 0x1000 lässt `rot` uninitialisiert, `bhEne04_RunMotion` erhält Stackwert);
`en01:Nachbar ohne exp0 (en01.c:11440)` (bhEne01_GetWalkMotion liest `EPP_EXP0_I(0xA0)` eines
skript-inaktiven Vorgängerworks mit `exp0 == NULL` – im Original ein Lesezugriff auf EE-Adresse
0xA0, nativ Meldung ohne Ersatzwert); `bhSetMotion:frm_no ausserhalb von frm_num (id 1, mtn 117)`
(bhGetFrameNum/ChgWalkMtn, siehe oben, jetzt mit id/mtn in der Meldung).

### Nachweise (alle headless, Originalressourcen, `--enemy-ai on --enemy-grab on`)

`tools/verify_enemy_ai.py` → PASS (`build/validation/enemy-ai-20261012/verify-final/summary.json`):
- RM_0080 natural/front/lever/death wie zuvor (HP 10 / 140-140-40 / 60 statt 40 / −10 + Game Over Phase 2).
- RM_0060 natürlich (vier Zombies InitType11): HP 44. RM_0031 natürlich (Zombie type 9, InitType02 liegend):
  HP 40, Griff aktiv. RM_3070 natürlich (type 6, InitType06 mode2 4 = Kriechvariante; frühester Raum mit
  dieser Art in den Raumdaten – in RM_0020…RM_0160 gibt es keinen): HP 0.
- RM_1000 natürlich (drei Hunde): Tick 350 Griff (`plp->mode0 4`, Hundebibliothek, HP 138), Tick 500
  freigegeben (`mtn_enemy 0`, Hund 1 wieder mode0 1), Tick 3000 HP −1, `plp->mode0 6` (NG01),
  Game Over Phase 2.
- RM_0050 Falle (Diagnose-Snapshot ev_flg 117 + Vorhängeschloss wie `verify_event_freeze.py`,
  Platzierungen): Ereignisse 15/17, beide Hunde in MV02, HP 106 nach 1100 Ticks. Ohne Platzierung in
  den Wegezonen (Spieler bei −142/0/0 außerhalb `rom->rutp`) bleibt es bei der Originalmeldung.
- Schalter am nativen Besitzer: `--enemy-ai off` → Gegnerpass läuft nicht (mode 0/0/0/0, HP 160);
  `--enemy-grab off` → Zombies laufen, kein Biss (HP 160).
- 76/77 CTest seriell; einzige rote Suite `shadow_modifier_pixels` (untracked Schattenarbeit der
  parallelen Sitzung, bereits vor diesem Nachtrag rot – Baseline-Lauf identisch). `script_motion` und
  `effect_collision_debris` waren nach dem Besitzerfix kurz rot (Welt ohne SYSTEM-Bündel bzw.
  `bhEff005` nun gebunden) und sind nach den oben beschriebenen Korrekturen grün.
- Windows x64 cross-gebaut (`build/windows-enemy-ai-claude-20261011/recvx-runtime.exe`,
  SHA256 64db1186…0c06), Policy mit Builddir PASS; nicht ausgeführt, keine Windows-Spielaussage.

### Auslieferung

`build/start/macos-arm64/RECVX Native.app` (von `Start.command` gestartet) ersetzt, ad hoc signiert,
`codesign --verify --deep --strict` OK, arm64, SHA256 44bb6154…1743. Backup
`archived/releases/enemy-ai-dogs-before-20261012-204915` (d3865a12…e4c8). Über `./Start.command
--headless …` gegen das installierte Binary: RM_0080 Griff/Biss (HP 140), RM_1000 Hund (HP 138);
normaler Starterlauf ohne Replay/Headless (SDL-Dummy, 500 Ticks, RM_1000): Hund greift mit den
aktiven Standardwerten (HP 154, `plp->mode0 4`). `debug.ini`/`controls.ini` unverändert
(Zeitstempel 5. Okt.). Keine andere Sitzung lief während der Installation.

### Offen

Hokan-Blend der Spielerpose im Griff (Pose gesampelt, Takt original), Zombie-Treppenzweige,
`sys->metcp`-Treppenflag, `nlxb`-Zweig, Blutsprite-Typ 11 `op->mode1`, EN04-Treppen (`stair_rec`
nativ hinter dem Originalblock), der Stackwert von `rot` in MV02, Bosse/übrige Gegnerfamilien.
Keine Vollport- oder Durchspielbehauptung.


## Nachtrag 2, 2026-10-12 spaet: BH_SET_EFFECT, MV02-Zweig, MTN-Indexraum; Originaldefekte belegt

Auftrag des Nutzers: die im Bericht offen gelisteten Punkte beheben. Umgesetzt
sind die drei, die auf der Hostseite lagen; die uebrigen sind jetzt mit
Disassembly bzw. Originaldaten belegt und als Originaldefekte eingeordnet.
Kein Commit/Push.

### Behoben

| Punkt | Original | Nativ | Nachweis |
|---|---|---|---|
| `bhSetEffect(17)` fehlte (Fass/EN00) | `bhEff017` (effsub1.c:4218): `hp = &sys->mwalp[lkwkp->ct0]; hp->flg &= ~0x1`, sobald der verknuepfte Work `stflg & 0x1000000` traegt. Erzeuger: eneset.c:461 `bhSetEffect(17, &pnt, ep, 0)`. | `s.set_effect` (world_enemies.cpp, effect.c:331: freier `eff[512]`-Platz, flg 1/0x80, Nullpunkt, Skala 1) + `step_eff017` + `set_effect_mobile_wall_clear` (world.cpp: `enemy_mobile_walls_[i].flags &= ~1`, danach `update_ground_geometry`). Der Effektplatz bleibt wie im Original dauerhaft belegt (kein Terminator). | RM_0080: Luecke `bhSetEffect(17)` verschwunden; vorher in natural/front/death (3 Laeufe). |
| MV02-Zweig `EXP0_I(0x10) & 0x1000` | en04.c:1824-1826 setzt `mode3 = 2` und laeuft ohne `rot`-Zuweisung in die gemeinsame Schlusszeile (en04.c:1962-1966) `bhEne04_RunMotion(epw, rot)`. | `rot` liegt in `$s0`. `bhEne04_MV02` beschreibt `$s0` vor dieser Stelle nicht; der Aufrufer `bhEne04_MVType00` setzt `$s0 = $sp + 0x58` (SLUS_201.84 0x1A6138) und laesst es bis zum `jalr` 0x1A63A8 nach `bhEne04_MoveMode2[]` unveraendert. Ein EE-Stackzeiger ist positiv; `bhEne04_RunMotion` (en04.c:3657) wertet nur `rot != 0` und `rot > 0` aus, also `idx 0` = `mtn_tbl[0]`/`mtn_tbl2[0]`. Nativ `rot = 1`. | RM_0050 (Falle): Luecke `bhEne04_MV02 rot undefiniert` verschwunden. |
| Skript-Treppenanforderung `0xa4` auf einem `sys->metcp`-Eintrag | `rom->etcp[0..etc_n-1]` und danach `sys->metcp`; `bhObj001` fuellt ihn (objitm.c:953/974 `op->ct1 = sys->metc_n++`). | `drain_script_stairs` (runtime.cpp) bildet denselben Indexraum aus `events->interactions` + `world->push_steps()`; ein Satz aus `sys->metcp` traegt keine Raumtabellenmarke. | Baum baut, Suite gruen; ein Skriptfall mit Index >= `etc_n` ist in den geprueften Raeumen nicht aufgetreten. |
| `frm_no >= frm_num` beim Motionswechsel (`frm_no 44 / frm_num 44`, `72 / 70`) | `Motion.c:131` liest den Key bei `frm_no >> 16` ohne Grenze gegen `frm_num`. Erreichbar ueber `bhGetFrameNum` (klemmt auf die reine Bildzahl, `*65536` in en01.c:10297) und `chg_mtn_upper` (en01.c:10314, Frame der laengeren Unterkoerpermotion an eine kuerzere Oberkoerpermotion). | `decode_motion_asset` haengt die Nachbarkeys aus demselben MTN-Bereich als zusaetzliche Keyelemente an jeden keyfuehrenden Kanal (`tail_keys`, gemeinsames Minimum, Grenze 8, `ee_f32` wegen moeglicher Rohbytes); `Pass::frame` laesst `frm_no>>16 < frm_num + tail_keys` zu. Liegt der Nachbarb lock nicht im eigenen MTN-Bereich, bleibt es bei der Meldung. `join_motion_segments` nimmt das Minimum beider Teile. | RM_0080/RM_0060/RM_0031/RM_3070: die Luecken `frm_no 44/44` (7 Laeufe) und `72/70` (2) verschwunden; CTest 77/78 unveraendert. |

### Am Original belegt, nicht Hostfehler

- Belegprobe zum Nachbarb lock (RM_0080, Motion 2, 59 Bilder): die Positionstafel
  von Kanal 0 endet auf Nullpolster, der gelesene Nachbarb lock ist `(0,0,0)`;
  genau das steht jetzt im Posezustand dieses Bildes.
- `en01:Nachbar ohne exp0 (en01.c:11440)`. Der Filter von `bhControlEnemy`
  (eneset.c:364) ueberspringt Works mit `stflg & 0x41000000`; `bhEne01_GetWalkMotion`
  (en01.c:11420-11432) prueft nur `id == 1 && flg & 1 && !(flg & 0x80)`. Ein Work
  mit `stflg 0x1000000` (bhEneSetCk) hat deshalb noch kein `exp0`, und
  `EPP_EXP0_I(0xA0)` liest im Original die EE-Adresse 0xA0.
- `bhCheckRoute: keine Wegezone`. `pos` liegt in `bhEne04_Brain` auf `$sp+0x20`
  (SLUS_201.84 0x1A6D64 `addiu $a2,$sp,0x20`); der Rueckgabewert 0xFF wird nicht
  geprueft, gelesen wird 0x1A6D70/0x1A6D7C. In RM_1000 haben die Wegezonen
  `h = 0`, ein Hund auf y=8 faellt damit aus jeder Zone. Nativ bleibt das
  vorherige Ziel stehen und die Luecke wird gemeldet.

### Bereits gebunden, im Bericht noch als offen gefuehrt

- `nlxb`-Zweig (player.c:1379-1387): `RoomWorld::player_nl_` fuehrt die gesetzte
  Wurzellage im Raumtakt nach (world.cpp:991-997) und geht ueber
  `s.check_player` in `bhCheckPlayer` ein; Ledger-Zeile auf `done` gesetzt.
- Blutsprite-Typ 11 mit `op->mode1 = 1` (zonzon.c:619-627): nativ ueber
  `RECVX_T3_EFF_MODE1` -> `EffectParticles::set_mode1` vorhanden. Kein Aufrufer
  der Originalquellen uebergibt Typ 11 (`SetBlood2`-Argumente: 0/1/2/4/6/9),
  und `bhEff302` (effsub3.c:1107-1385) liest `mode1` nirgends. Der Schreibzugriff
  ist damit wirksam, aber unerreichbar; kein Handlungsbedarf.

### Weiterhin offen

Hokan-Blend der Spielerpose (F01: der Host sampelt `plp->mlwP->objP` weiterhin
statt es fortzuschreiben; ein vollstaendiger Umbau muss die Pose fuer Zeichnung
und Gelenkmatrizen aus derselben Quelle ziehen), Zombie-/EN04-Treppenzweige im
echten Ablauf, Bosse und uebrige Gegnerfamilien. Beim Nachbarb lock bleibt eine
Grenze: der Decoder sieht je Motion nur ihren eigenen MTN-Bereich; endet eine
Keytafel am Bereichsende, liest das Original in den *naechsten* Motionblock
weiter, die Portierung meldet dort. Keine Vollport- oder Durchspielbehauptung.

### Nachweise dieses Nachtrags

- `tools/verify_enemy_ai.py` PASS, zuletzt gegen das installierte Paket
  (`build/validation/enemy-ai-20261012/verify-gaps3`: RM_0080 mit HP 75 nach
  1200 Ticks, 0 blockierte Befehle).
- CTest seriell 77/78; einzige rote Suite `shadow_modifier_pixels` (parallele
  Schattenarbeit, Baseline-rot).
- Lueckenliste ueber alle neun Laeufe: vorher `bhSetEffect(17)` (3), `bhEne04_MV02
  rot undefiniert` (1), `frm_no 44/44` (7), `frm_no 72/70` (2), `bhCheckRoute` (4),
  `en01:Nachbar ohne exp0` (1); nachher nur noch `bhCheckRoute` (4) und
  `en01:Nachbar ohne exp0` (1) — die beiden, deren Originalwert nicht
  reproduzierbar ist.
- `tools/port_check.py` rc=0; `tools/check_no_original_core.py` PASS fuer Quelle
  und Mac-Binary (ueber `Start.command`).
- Auslieferung: `build/start/macos-arm64/RECVX Native.app` ersetzt, ad hoc signiert,
  `codesign --verify --deep --strict` OK, arm64, SHA256 d4946b19…;
  Backup der vorher installierten Runtime
  `archived/releases/enemy-gaps-before-20261012-215111`. Normaler Starterlauf
  (SDL-Dummy) RM_1000: 400 Present, 200 Rendering, 199 Ticks, HP 154, 0 blockierte
  Befehle; RM_0080 mit 600 Ticks ebenso. Gegenprobe mit der zuvor installierten
  Runtime (23d6c7ad…): RM_0080 zeigt dort noch die Luecke `frm_no 44/44`, HP 75,
  dieselben 6 offenen Weltbindungen — die Zahl stammt also nicht aus dieser
  Aenderung. Benutzer-INIs unveraendert.
