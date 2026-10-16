# Gegner-Lücken: Bestandsaufnahme und drei Schließungen (2026-10-14)

Build: `build/en53-claude-20261014` (Release, Ninja).
Messung: `build/validation/enemy_scan_20261014/scan_enemy_gaps.py` über alle 205
Räume, je 600 Frames, `--enemy-ai on --enemy-grab on`, ohne Eingabe.

## Messung statt Annahme

Die alte Ledger-Zeile zu EN04 („Verfolgung/Angriffe/gesamte MV-Zustandsmaschine
fehlen") war **veraltet**: `native/enemies/en04.c` enthält mv00..mv12, ng00/01,
dg00/01, dd00 und den Brain-Dispatch. Der Hundelauf RM_1000 meldete keine
Gegner-Lücke. Deshalb wurde die Lückenliste gemessen statt aus dem Ledger
übernommen (`scan_enemy_gaps.py` sammelt die `Enemy gaps:`-Zeilen jedes Raums).

Bestand vor den Fixes: **205 Räume, 51 mit Lücken.**

## Geschlossene Lücken

### 1. `mlwP->owP[0].flg |= 3` (en01.c:11910) — 7 Räume
`native/enemies/en01_core.c` `recvx_en01_flying_cap` brach die Originalzeile mit
`en01_gap` ab; der Kommentar behauptete, der Vertrag habe dafür kein Feld. Der
Dienst `RECVX_EN01_SERVICES::joint_flags_or` (en01_work.h:551) ist aber längst
gebunden (`world_enemies.cpp`), und en01_work.h:549-552 nennt genau diesen
Aufruf als Zweck. `sepw->mlwP` ist der geteilte Elternbaum (`epp->mlwP = epw->mdl`,
en01.c:2828), deshalb schreibt der Dienst auf dem Baum von `sepw`.

### 2. `bhCheckWallRefAngle` (hitchk.c:2178) — 7 Räume
Der Dienst `RECVX_EN01_SERVICES::wall_ref_angle` war im Vertrag definiert, aber
nirgends gebunden. Die Bausteine liegen vor: `reflect_effect_wall` entspricht
`bhSetWallRefAngle` (Antwort), die Suche fehlte mit ihren **zwei** Toren.

Die beiden Suchvarianten unterscheiden sich nur in den Toren
(nachgelesen in hitchk.c):
| type | `(!(type & 1)) \|\| (!(flg & 0x400))` | `(!(attr & 0x20)) \|\| (!(flg & 0x200))` |
|---|---|---|
| 0..5 | ja | ja |
| 6 | nein (:2594-2595) | nein |
| 7 | nein | ja |

Das vorhandene `wall_contact` bildet nur das erste Tor ab und ist als
`bhCheckWallType` dokumentiert; die Effekte (`effsub1.c:518/5404`) dürfen das
zweite nicht bekommen. Deshalb ist das Tor ein eigener Parameter
(`effect_wall_ref_angle` neben `effect_wall`), kein Sonderfall für `flg == 0`.
Beide Aufrufer (en01sub.c:937 `flg 0`, :1118 `flg 0x400`) machen das zweite Tor
zwar wirkungslos; es wurde trotzdem vollständig portiert.

### 3. `eff[sdw].mdlver` (en07.c:216) — 4 Räume
`RECVX_ENEMY_SERVICES::effect_set_mdlver` war deklariert, aber nicht gebunden.
Gebunden auf `world->sprites_[slot].sprite.mdlver` wie `set_shadow`; ein Slot
außerhalb des Pools meldet die Lücke, statt wie das Original mit `eff[-1]` vor
den Pool zu schreiben.

## Ergebnis

| Stand | Räume mit Lücken |
|---|---|
| vorher | 51 |
| FlyingCap | 44 |
| + wall_ref_angle | (s. u.) |
| + effect_set_mdlver + en08 collision | 38 |
| + EN02 Gulp Worm (Port, 6 Räume) + Stimme 127 | 35 |
| + bhEff_Sub355 (3 Räume, schließt `eff[-1]` mit) | 32 |
| + bhEff219 (4 Räume, Fehllücke entfernt) | 32 Räume / 48 Lücken-Einträge |
| + EN11-Dienstbindung je Pass (RM_9100) | 31 Räume / 45 Einträge |
| + EN19 Tyrant T-078 (Subagent, geprüft) | **31** Räume / **52** Einträge (s. u.) |
| + Effekt 256/257 (bhEff_E03_Acid/bhEff_E00_Acid) + EN14-Säuredienste | **31** Räume / **50** Einträge |
| + Effekt 267/269 + alle EN14-Dienste (LookPlayer, Strahlsuche) | **31** Räume / **46** Einträge |
| + Effekt 250 (Mince) + `model_texp_index` | **31** Räume / **44** Einträge |
| + Morph-Zeichenpfad (actor.cpp) | **31** Räume / **44** Einträge (run13; Fix außerhalb des Scan-Endbilds) |

Prüfläufe: `run1` (vorher), `run2`…`run13` (nachher) in diesem Verzeichnis,
jeweils `gaps.json` mit allen Räumen je Lücke.

Tests: 31/31 Gegner-/Effekt-/Morph-Tests grün (`enemy*`, `en01*`, `en04`,
`en07`, `effect*`, `lighter`, `beam`, `actor`, `morph`, `shadow`).

## Verbleibende Lücken (Top, nach run6)

| Räume | Lücke | Art |
|---|---|---|
| 4 | `bhEne19` | Gegnerfamilie (en19.c, 4825 Z.) |
| 4 | `bhEff108` | Wasserhöhenkarte `sys->wt_*` fehlt komplett (Subsystem) |
| 4 | `bhEff219` cam vor `bhControlCamera` | Reihenfolge/Kamera |
| 3 | `EN53 CoreInit` | `child_ambient`: ein Modellstrom je `mbp[k]` (Subsystem) |
| 3 | `en02.c:2452 ang` | belegte Original-UB (Gulp Worm, Fall 8) |
| 3 | `bhEne03`, `bhEne29` | Gegnerfamilien |
| 2 je | `bhCheckRoute`, `bhSetMotion frm_no`/`Motionkanal` | belegte Original-UB |
| 2 je | `bhEne15`, `bhEff267`, `bhEne14:*` | Familien/Effekte |

## Grenzen dieser Messung

- Gezählt wird je **Lückenmeldung**, nicht je Gegner oder Frame: eine Lücke in
  Frame 0 und in Frame 599 ergeben einen Eintrag. Die Meldungen sind benannte
  Originalstellen; ein Eintrag heißt nicht zwangsläufig sichtbarer Fehler.
- Kein Spielablauf, keine Fensterabnahme, kein Durchspiel. 600 Frames ohne
  Eingabe erreichen nicht jeden Gegnerzweig (z. B. Angriff nur bei Spielernähe).
- Kein Originalkern, keine Quarantäne berührt.

## Nachtrag (2026-10-14 abends, zweite Runde)

### 4. `bhEne03_Collision` in EN08 (en08.c:41) — 3 Räume
`native/enemies/en08.c:114` meldete den gemeinsamen Wanddurchlauf **unbedingt**, mit
dem Kommentar „gehoert zur Identitaet 3 und ist im gemeinsamen Vertrag nicht
enthalten". Das war falsch: `collision_works` steht in `RECVX_ENEMY_SERVICES`
(enemy_work.h:390) und ist in `world_enemies.cpp` gebunden; `en07`/`en14`/`en24`/`en30`
nutzen ihn bereits mit Fallback. Jetzt `(void)sv->collision_works(w)` wie im
Original (en08.c:41 verwirft den Rückgabewert).

Ergebnis: **38 Räume** mit Lücken (von 51).

### Vorarbeit EN02 (Gulp Worm) — eigener Auftrag
Struktur vollständig bestimmt, Umsetzung noch offen:
- **EXP0** (144 Byte): `0x24` float, `0x28` Kindwork `mlwP = &mdl[5]` (obj_a mdl[5],
  obj_b mdl[6]), `0x2C` Kindwork `mlwP = &mdl[4]`, `0x30..0x6F` **16** Kindworks
  `mlwP = &mdl[1]`, `0x70`/`0x74` int, `0x78` float, `0x7C` **mobiler ATR-Eintrag**
  (`sys->mwalp[mwal_n++]`, type 3, w 10) — dafür gibt es `mobile_wall_alloc/_get/_set`.
- `bhEne_SetCallFunc(bhEne02sub, 31)`; Tabellen `Mode0[6]`, `MoveMode2[6]`, `DamageMode2[2]`,
  `DeadMode2[2]` (Index 1 zeigt auf MV00), `CapColTab[13]`.
- Alle Subroutinen liegen in `native/enemies/en03_lib.h` bereit
  (`recvx_t3_arc_tan2`, `_dir_check`, `_attack_hit_check`, `_add_null_trans_dir`, …).
  `bhEne_DGDirCheck(plp)` ist `recvx_t3_dir_check(plp->ay, dv)` — `recvx_t3_dg_dir_check`
  nimmt nur einen Work.
- Umfang: 3384 Zeilen Original (MV02 allein 300, DD00 340, Sandeffekte 400,
  PlayerControl 280). Die Sandeffekt-Routinen schreiben in `sys->wt_*`.

### Befund `bhEff108` (4 Räume): fehlendes Subsystem
`effsub1.c:6435 bhEff108` schreibt in die Wasserhöhenkarte `sys->wt_wvp` mit
`wt_px/wt_pz/wt_maxx/wt_maxz/wt_zp`. Diese Felder existieren nativ **nirgends**
(12 Nutzungen im Original). Das ist kein Einzelfix, sondern das fehlende
Wasserwellen-Subsystem.

### KORREKTUR `eff[-1]` (3 Räume): Fehldiagnose, mit `bhEff355` behoben

**Der frühere Eintrag hier war falsch.** Er behauptete, in RM_5550/5630/8000 laufe der
eff[512]-Pool tatsächlich voll und der native Code melde deshalb korrekt. Gemessen:

- `RoomWorld::spawn_effect` (world.cpp:1336) gibt **-1 zurück, wenn die
  Effektprozedur nicht portiert ist** — ohne einen Slot zu belegen.
- `bhEne01Scope_Effect` (en01sub.c:1981-1983) prüft die Originalbedingung
  `(eno != -1) || (eno < 512)`, die immer wahr ist, und meldet bei `eno == -1` die
  Lücke „eff[-1]".

Die Meldung `eff[-1]` war also die **Folge** des fehlenden Effekts 355, nicht eines
vollen Pools. Beide Meldungen („bhEff355" und „eff[-1]") haben dieselbe Ursache und
zählten in der Lückenliste doppelt. Nach dem Port von 355 sind **beide** in allen drei
Räumen verschwunden, ohne dass am Pool etwas geändert wurde.

Merksatz für die Triage: eine `bhEff<NNN>`-Meldung, die über `bhSetEffectTb` läuft,
erzeugt regelmäßig zusätzlich eine `eff[-1]`-Meldung. Das ist eine Diagnose-Dopplung,
kein zweiter Fehler.

### 5. `bhEff_Sub355` (effsub4.c:1117) — 3 Räume, schließt beide Meldungen
Zielfernrohr-Marke des EN01-Kindes `bhEne01Scope` (en01sub.c:1948): zwei Sätze an
`scope_ofs` über der Zombiematrix, `exp0` = Elternwork, `ct3` = 0. Texturgruppe **4**
Bild 2 — eine **globale** Effekttextur aus `bhInitEffect` (effect.c:68; room.c:295-303
nullt nur 30..99/400..449), nativ aus SYSTEM/0001.bin in `effect_textures_`. Blend 8/10,
Rückseitentor 0x20000. Alpha startet bei 0xF0 (`tv[0].col` 0x00FF0000 + 0xF0000000) und
sinkt je Pass um 0x10 bis unter 0x10 — 16 Passes; der Satz stirbt mit `stflg 0x1000000`
des Elternwerks. `flg 0x4000000` und `0x80000` werden wie im Original gesetzt, aber
`effect.c` liest beide **nirgends** (allein 0x20000, :1241).

Nachweis: RM_5550 headless → 34 Sätze id 355, 30 sichtbar, Textur 4 / Atlas 2, keine
Lücke mehr. Bekannte Abweichung: das native Spawn-Template führt kein separates `sxb`,
deshalb ist `sx = sxb` im Init nativ ein Nulleffekt; im Original liest die Zeile einen
Altwert aus dem persistenten `sys->ef`.

### Vorarbeit EN02 (Gulp Worm) — eigener Auftrag
Struktur vollständig bestimmt, Umsetzung noch offen:
- **EXP0** (144 Byte): `0x24` float, `0x28` Kindwork `mlwP = &mdl[5]` (obj_a mdl[5],
  obj_b mdl[6]), `0x2C` Kindwork `mlwP = &mdl[4]`, `0x30..0x6F` **16** Kindworks
  `mlwP = &mdl[1]`, `0x70`/`0x74` int, `0x78` float, `0x7C` **mobiler ATR-Eintrag**
  (`sys->mwalp[mwal_n++]`, type 3, w 10) — dafür gibt es `mobile_wall_alloc/_get/_set`.
- `bhEne_SetCallFunc(bhEne02sub, 31)`; Tabellen `Mode0[6]`, `MoveMode2[6]`, `DamageMode2[2]`,
  `DeadMode2[2]` (Index 1 zeigt auf MV00), `CapColTab[13]`.
- Alle Subroutinen liegen in `native/enemies/en03_lib.h` bereit
  (`recvx_t3_arc_tan2`, `_dir_check`, `_attack_hit_check`, `_add_null_trans_dir`, …).
  `bhEne_DGDirCheck(plp)` ist `recvx_t3_dir_check(plp->ay, dv)` — `recvx_t3_dg_dir_check`
  nimmt nur einen Work.
- Umfang: 3384 Zeilen Original (MV02 allein 300, DD00 340, Sandeffekte 400,
  PlayerControl 280). Die Sandeffekt-Routinen schreiben in `sys->wt_*`.

### Befund `bhEff108` (4 Räume): fehlendes Subsystem
`effsub1.c:6435 bhEff108` schreibt in die Wasserhöhenkarte `sys->wt_wvp` mit
`wt_px/wt_pz/wt_maxx/wt_maxz/wt_zp`. Diese Felder existieren nativ **nirgends**
(12 Nutzungen im Original). Das ist kein Einzelfix, sondern das fehlende
Wasserwellen-Subsystem.

### Befund `eff[-1]` (3 Räume): korrekt gemeldete Original-UB
`en01sub.c:1983` prüft `(eno != -1) || (eno < 512)` — immer wahr. Bei vollem
eff[512]-Pool schreibt das Original `eff[-1]` vor den Pool. Der native Code
(`native/enemies/en01_sub.c:2293`) meldet genau dann und arbeitet sonst normal;
die Meldung erscheint, weil der Pool in RM_5550/5630/8000 tatsächlich voll läuft.
Kein Fix ohne Klärung, ob der Pool nativ zu früh voll ist.

### 5. `bhEne02` Gulp Worm — 6 Räume (Port)
Vollständiger Port nach [work/native_port_EN02_world.result.md](native_port_EN02_world.result.md),
dazu die Sandsprites 253/254 und die Effektstimme 127. Verbleibende EN02-Meldung ist die
belegte Original-UB `ang` (Fall 8) in RM_5830/7230/7231. Ergebnis: **35 Räume**.

### 6. `bhEff219` — 4 Räume: eine *überflüssige* Lücke, keine fehlende Funktion

Zählweise: die Zahl der **Räume mit Lücken** sinkt hier nicht (RM_5800/5840/6100/9000
haben weitere Lücken — EN53/EN15 bzw. EN19 — und zählen weiter). Was sinkt, sind die
**Lücken-Einträge**: 58 (run5) → 52 (run6, Effekt 355) → 48 (run7, Effekt 219).

`step_eff219` wartete mit dem Anlegen der Schneebox, solange `EffectContext::camera_ready`
false war, und meldete „cam.wpx/cam.vx vor dem ersten bhControlCamera". Das war eine
**Fehllücke**: der Originalzustand ist dort nicht undefiniert, sondern 0.

- `room.c:33` ruft **bei jedem Raumladen** `bhInitCamera()`, das laut `camera.c:8`
  `npSetMemory(&cam, 0x4AC, 0)` das ganze cam-Objekt nullt (danach nur flg/ln/pers/
  ncut/ocut/ocflg/attr und zwei Einheitsmatrizen). `cam.wpx/wpy/wpz` und `cam.vx/vy/vz`
  sind im ersten Effektpass eines Raums also **0**.
- `game.c:50` `bhControlEffect()` läuft **vor** `:58` `bhControlCamera()`, der Effektpass
  sieht die Kamera des Vorticks — im ersten Tick eines Raums die genullte.

`EffectContext::camera_position/camera_view` sind genauso mit 0 initialisiert. Die
Wächterzeile war damit nicht nur überflüssig, sie ließ den Effekt den Spawn **überspringen**
— eine echte Verhaltensabweichung. Entfernt, die Box wird wie im Original am Ursprung
angelegt und folgt der Kamera, sobald sie läuft. Nachweis: RM_5800/5840/6100/9000 ohne
`bhEff219`; die Sätze (Typ 1/2, `flg 0x1000000`, `queue_custom`) werden über
`draw_snow_rect` gezeichnet, deshalb ist `visible:false` im Zustandsdump korrekt.

Die beiden anderen `camera_ready`-Verwendungen bleiben: `camera_rotation` (E12) und
`njUnitMatrix`-Pfad lassen die Matrix bei fehlender Kamera auf der Einheitsmatrix stehen —
das ist genau der genullte `cam.mtx` aus `bhInitCamera`.

### 7. EN11/EN16: Dienste nur einmal gebunden — RM_9100 lückenlos

`RM_9100` meldete **drei** EN11-Lücken (`bhEne11: linked bhEff261 work`,
`bhEne11_BR00: effect/system flags`, `bhEne11_MV09: matrices/O_WORK flags/motion`).
Alle drei hatten **eine** Ursache: `en11_services` (`extra`) war zur Laufzeit **NULL**.

Nachweis mit einer Diagnose in `effect_read` (danach wieder entfernt): `extra=0 get=0`
in jedem Pass ab dem zweiten.

Ursache: die Anbieter von EN11 und EN16 wurden in `RoomWorld::init_enemy_works()`
(also einmal beim Aufbau) gebunden, aber `RoomWorld::control_enemy_works()` löst am
**Ende jedes Gegnerpasses** alle prozessweiten Dienste wieder (`recvx_en11_bind(nullptr)`
usw.). EN21/EN09/EN05/… binden sich dagegen *innerhalb* von `control_enemy_works` und
sind damit korrekt pro Pass gültig — EN11/EN16 waren es nicht.

Behoben: beide Blöcke liegen jetzt im Pass-Scope von `control_enemy_works`, wie die
übrigen. Da freie Funktionen keinen Zugriff auf die privaten `RoomWorld`-Member haben
(`sprites_`, `collision_events_`, `unbound_enemy_`, `effect_tb`), musste der Block
*wandern*, nicht in einen Datei-Scope-Anbieter gehoben werden.

Ergebnis: RM_9100 ist im Scan **lückenlos** (3 → 0). Test `enemy_en11_world`
(RM_9100, 40 Passes): beide EN11-Works halten ihren Effektslot mit id 261.

**Nebenbefund geklärt (2026-10-15):** die im Test sichtbare Meldung
`bhEff_E11_CheckCollisionPlayer(plp): owP vor bhCalcModel` (und `plp->mlwP->owP[4].mtx
(en05s)`) war **kein Engine-Fehler, sondern ein Fehler meiner Test-Fixture**: sie rief nie
`RoomWorld::commit_player_joints`, also blieb `player_joint_matrices_` leer. Die Runtime
schreibt den Spielerbaum vor `events->initialize()` (runtime.cpp:450 = player.c:840
bhSetPlayer) und je Bild in `head_hair_step` (runtime.cpp:493 = player.c:1688
bhCalcModel) fest — deshalb zeigte der headless-Scan die Meldung in keinem Raum.
`en05_world_test.cpp:27/31` macht es seit jeher richtig. Fixture korrigiert, Ausnahmen
im Test entfernt, `enemy_en11_world` grün ohne Ausnahmen. Neuer Test `player_owp_order`
belegt die Ursache in beide Richtungen (mit Commit: keine Lücke; ohne: genau die beiden
Meldungen). Details und die Verbrauchertabelle aller plp-owP-Leser:
[work/native_port_player_owp_effect_pass.result.md](native_port_player_owp_effect_pass.result.md).

**Daraus eine echte, benannte Reihenfolge-Lücke (offen, nicht umgesetzt):** die Runtime
rechnet `bhCalcModel(plp)` *nach* `world->tick()` (runtime.cpp:1343 → :493); der Effektpass
(game.c:50) liest darum den Spielerbaum des **vorigen** Bildes, das Original den des
**selben** Bildes (game.c:49 bhControlPlayer vor :50 bhControlEffect). Der Gegnerpass liest
korrekt den vorigen (game.c:48 vor :49). Betroffen: Treffertests 261/270, Spielerlinks
lkflg 1, Nachbild 136, E12-Feuer am Spielergelenk — ein Bild Latenz. Ein Fix braucht einen
Spielerpass-Haken in `RoomWorld::tick` zwischen `control_enemy_works()` und
`update_player_ground_work()` (world.cpp:1777-1778) samt Verlegung von
`locomote()`/`head_hair_step()` aus runtime.cpp; das ist kein chirurgischer Edit in den
gerade fremd bearbeiteten Dateien und bleibt als Auftrag offen.

### EN19 (Tyrant T-078) — Aufnahme, eigener Auftrag

`bhEne19` steht in 4 Räumen (RM_1122/3230/5010/5840) und ist mit 4825 Zeilen der
größte offene Posten. Die Aufnahme zeigt eine **eigene Architektur**, die EN02/EN23
nicht teilen:

- `FW_WORK` (0x204 Byte, ~60 benannte Felder) mit **zwei parallelen Actionspuren**
  (`a_*` und `b_*`: status/act_flg/prgP/act_frm/act_now/mtn_rte/chg_rte/act_cnt) plus
  Zielverfolgung, Schadensstufen, Klauenlagen (`ClwPos[4]`), Waffenreichweiten und
  Verweisen auf `PMA_WORK`/`R0_WK`/`R0A_WORK`/`R49_WORK`.
- `EA_WORK En19ActTbl[50]` (Label → Mv-Routine), `ACT_TBL_WORK En19PlyActTbl[17]`,
  `COMBWEP_WORK[21]`, `CPCL[28]`, `FlpTbl[26]`, `En19HpTbl[4][3]`, `En19AtkRng[3][4]`,
  `PMA_WORK Eff30aPrm1/2[3]`.
- Drei Brains `Br00/Br01/Br02` (je ~400–500 Zeilen), **21** Mv-Routinen (`Mv00..Mv20`,
  Signatur `(ewP, fwP, count)`), Action-Framework (`ActionChange`/`ActionMain`/
  `TargetAnalyze`/`PositonFix`/`HeadTurn`), `SetDmgEffect`, `CollisionCircle2Oval`,
  `TyBloodSet`, `ClawReset`, `SoundSet`, `MtnAttrbuteGet`, `DmgCheck`, und für den
  Spieler `PlySetDamage`/`PlyMoveMain`/`PlyDmg042..053/117/118/Fal`.

Geschätzt 2.000+ Zeilen nativer Code plus neue Zustandsstruktur — ein Mehrfaches von
EN02. Nicht begonnen, damit kein halbfertiger Tyrant im Dispatch steht (die
EN09-Erfahrung: halbe Zustandsmaschine = Stillstand statt Lückenmeldung).

### EN14 (Rest) — hängt an Effekten
Die drei EN14-Meldungen brauchen Effektprozeduren: `bhEne14: eff[512] erste
Eff130/type4-Suche` → `bhEff130` (effsub1.c:9001), `bhEne14_Acid: Eff256/SetEffectTb`
→ `bhEff_E03_Acid` (256). `bhEne14_LookPlayaer: owP.flg` braucht den `joint_flags`-Dienst.

### 8. EN19 Tyrant T-078 — Portiert und geprüft; Rest sind Effektprozeduren (Subagent, 2026-10-15)

Ein Subagent hat EN19 portiert (`native/enemies/en19.c` 2353 Z., `en19.h`, `en19_tables.c`),
in `jump[19]` und CMake eingetragen, Test `enemy_en19_world` angelegt. Der Agent starb danach
am API-Kontingent; ich habe seinen Stand gegen die Originale geprüft:

- **Baut fehlerfrei**, Test grün, keine TODO/Stub-Marker in `en19.c`.
- **Tabellengrößen exakt wie im Original**: En19ActTbl[50], En19CombWepTbl[21],
  En19PlyActTbl[17], Ene19CapColTbl[28], FlpTbl[26], En19CombJointTbl[26], En19HpTbl[4],
  En19AtkRng[3], En19OvlTbl[3], Eff30aPrm1/2[3]. Alle **21** Mv-Routinen vorhanden (mv00..mv20
  inkl. mv04a/mv04b).
- **Verhalten in RM_5840** (dort läuft er frei): mode0 1, Motion 14 auf Bild 28, hp 900
  (= En19HpTbl[0][2] für Typ 2), **Spieler-HP 160 → 120** — die Angriffskette greift.
  RM_1122/RM_3230/RM_5010 sind skriptgehalten (`stflg 0x1004000` bzw. mode0 5), dort steht er
  zu Recht; der Test nutzt RM_5840.
- **Spot-Checks** gegen die Originale bestanden: `head_turn` stimmt Zeile für Zeile
  (objP[5].ang[1], `dlt/8`, Klemme ±7281 als short, objP[6].ang[1] = 0, owP[4]/[5].flg 0x2,
  else-Zweig löscht); die eine Zeile in `native/world.cpp` (Spielerpass-Gate um mode0 5
  erweitert) ist belegt: `bhCtrPly_mode0[9]` = {…, nage(4), **enedam(5)**, enedie(6), …}.
- Ledger: 26 Zeilen mit ehrlichen `missing`/`partial`-Markern (z. B. `SetLeftClaw`/`SetClawPlane`
  `missing`, Effekte 303/304/305/306/309/310/311/349 als Lücke benannt).

**Zählweise, ehrlich:** die Lücken-**Einträge stiegen** von 45 auf 52, obwohl die Substanz
besser wurde. Vorher meldeten die 4 EN19-Räume je *eine* Meldung „bhEne19" (Gegner fehlt
ganz); jetzt läuft der Gegner und meldet 10 benannte **Effekt**lücken. Die Zahl der Räume
bleibt 31. Wer die Einträge als Fortschrittsmaß liest, sieht hier eine Verschlechterung; richtig
ist: eine Lücke „Gegner fehlt" wurde durch benannte Einzellücken ersetzt.

**Diese Effekte sind an einem fehlenden SUBSYSTEM blockiert.** Die EN19-Lücken sind
`effsub3.c`-Prozeduren (303/304/305/306/307/308/309/310 (=bhEff30a)/311/312 (=bhEff30c)/349),
nativ ist davon **keine** registriert. Der Erzeuger des Schattens, `rySetShadow` (effsub3.c:1640),
setzt `oP->mlwP = &sys->efm[1]` — die **globalen Effektmodelle** aus `bhInitEffect`
(effect.c:60-77, `bhMlbBinRealize` für ef_info-Einträge mit flg & 1). `sys->efm` kommt nativ
**nirgends** vor (0 Treffer in native/). Vor der Prozedurportierung ist also dieses Modellsubsystem
zu klären. `oP->ct0/ct1/ct2` sind im Original **Zeiger** in den Objektbaum des Gegners
(`&owP[obj].mtx[12]`) — nativ ist das wie bei EN23 (`RECVX_EN03S_LINK` owner+node) als
Besitzer+Knoten aufzulösen, nicht als roher Zeiger.

### 9. Effekt 256/257 (Säure) portiert; EN14-Säuredienste gebunden (2026-10-15)

`bhEff_E03_Acid` (256, effsub2.c:1389) und sein Kind `bhEff_E00_Acid` (257, :533) sind in
`native/effect_sprite_procedures.cpp` von Hand portiert (`step_e03_acid`, `step_e00_acid`).
Dafür fehlten drei Dinge am Besitzer, nicht nur die Prozedur:

- **`pxb/pyb/pzb` am `EffectSprite`** — effect.c:794 hält die Vorbildlage vor jedem Prozeduraufruf
  fest; nativ gab es das Feld nicht. `RoomWorld` setzt es jetzt direkt vor `step_effect_sprite`.
- **Haken `player_watr`** (`EffectWorldHooks`): `plp->watr` = Kapsel owP[4]→owP[0], r 2.5
  (player.c:1758), aus dem von der Runtime festgeschriebenen Spielerbaum. Dieselbe Quelle wie
  `en.player_capsule` der Gegnerdienste.
- **`effect_detail::collision_line`**: `bhCollisionCheckLine2(p1, p2, 0x4400, -1)` samt
  `bhGetHitCollisionNormal` auf dem Wandsatz des Raumbesitzers. Das Original **kürzt p2 auf den
  Treffer** (hitchkl.c:135ff schreibt `p2->y = hp->py + h` usw.), der Tropfen landet also auf der
  Fläche; der native `line_query` liefert dasselbe `end`.

EN14 (`bhEne14_Acid`, en14.c:1311) hatte seine Dienste `acid_alloc`/`acid_setup` schon als Vertrag,
aber ungebunden; sie liegen jetzt in `control_enemy_works` (sys->ef id 256/flg 1/type 4 bzw. 6,
danach stflg 0x20, `txp[0] = mdl[2].texP` als Texturliste, tex_id 0, Richtung, Lage, mdlver = SE).
EN23 (`bhEne23_Acid`) spawnte 256 schon über `effect_tb`/`effect_patch` — jetzt läuft die Prozedur.

**Geprüft:** `bhEff_SetAlign(2)` ≡ `e12::align2` (tv y −2/−2/0/0, x ±1); Kinder 257/269 bekommen
die UV-skalierte `op->sx` (1.5·0.15625/0.125 = 1.875), nicht die Spawn-Skalierung — mein erster
Testwert war falsch; SE nur, wenn `bhEne_CheckPlayEffectSE` 0 liefert (type 6 am Boden: nur bei
mdlver 0, ohne Vorprüfung); `*(int*)&plp->mode0 = 2` nullt mode1..3; Gift mit `rand() < poison`
**nach** dem HP-Abzug (Zufallsreihenfolge wie Original). Test `effect_e03_acid`: Flug, Bodenaufprall
(Lage auf y 10 gekürzt, 8 Passes Aufpralltabelle), Spielertreffer (HP 100→80, flg 0x10004,
stflg 0x10000, Schub = Richtung), `plp->flg 4` sperrt, type 5 ohne Wandprüfung, type 6 → Kind 269
(1.5/2/1), 257-Alphaabbau. 36/36 Gegner-/Effekt-Tests grün.

**Messung (`run10`):** 31 Räume / **50** Einträge (vorher 52), 0 Fehlschläge. Einzige Änderung
gegenüber `run9`: `bhEne14_Acid: Eff256/SetEffectTb` ist aus RM_5810 und RM_9302 verschwunden;
dort laufen 8 bzw. 24 Säuresätze, der Spieler verliert HP. RM_9050 (EN23) kommt in 600 Bildern
nicht bis zum Spucken — dort nur der Unit-Test als Beleg, kein Laufzeitnachweis.

**Offen:** 269 `bhEff_E14_Fire` (effsub2.c:3963, 251 Z.), das Feuerkind von type 6 (EN14 Typ ≠ 0) —
als `bhEff269` gemeldet, Ledger `missing`. Die übrigen EN14-Lücken (`bhEff267` Fluid,
`eff[512] erste Eff130/type4-Suche`, `LookPlayaer: owP.flg`) sind unverändert.

### 10. Effekt 267/269 portiert, EN14-Dienste vollständig gebunden (2026-10-15)

`bhEff_E13_Fluid` (267, effsub2.c:3698) und `bhEff_E14_Fire` (269, :3963) sind von Hand portiert
(`step_e13_fluid`, `step_e14_fire`). Befunde vor dem Port, die die Richtung bestimmt haben:

- **267 erzeugt nicht EN14**, sondern `bhEne30_SetFluidEffect` (en30.c:984, nativ schon vorhanden,
  vier Aufrufstellen) — die Lücke in den EN14-Räumen kam von EN30-Tropfen. Texturgruppe **81**
  musste in `ported_effect_atlas` frei (sonst wird der Raumatlas nicht dekodiert).
- **269** entsteht nur aus meinem 256-Port (type 6, EN14 Typ ≠ 0). Seine UV-Tabellen sind
  byteidentisch mit denen von E12_Fire (geprüft, wiederverwendet). Der Lichtblock `rom->lgtp[1]`
  weicht von E12 ab (kein +5 in y, Reichweite fest 20, Farbe 3/1.8/0.7) — der Haken `fire_light`
  ist deshalb um Farbe und Reichweitenteiler verallgemeinert; E12 übergibt 5+py, 1.3/0.3, Teiler 10.
- **Strahlsuche** (`bhEne14: eff[512] erste Eff130/type4-Suche`): `find_beam` sucht jetzt im echten
  Pool (en14.c:213-221), `beam_line` liest O_WRK +0x10 `px` und +0x64 `lox` — belegt über die
  gleiche Kopfsicht `RECVX_EN16_EFF` (en16.h, Laser 270) und `bhEff130` (effsub1.c:9073, `px += lox`
  je Pass). Im Baum gibt es **keine `types.h`**; die Offsets stammen aus diesen Fundstellen.
  **Ehrlich:** `bhEff130` (Spielerwaffenstrahl, 1120 Z., `bhCheckFlyAtari`) ist nicht portiert und
  `spawn_effect` legt ihn nicht an — die Suche findet nativ nie einen Strahl, EN14 weicht dem
  Spieler-Strahl nicht aus. Das bleibt als `bhEff130` gemeldet, sobald die Waffe feuert.
- **LookPlayer**: `joint_flags` (owP[6].flg Bit 2 als mask/value), `node_angles`, `motion_angles`
  (`md2P[6].p[1][frm]`, wie en12), `joint_matrix_set`, `node_local`, dazu `floor_enemy`
  (bhCheckFloorEnemy wie en12) und `game_mode` — alle in `control_enemy_works` gebunden.

**Geprüft:** Test `effect_e14_fire` (269: Start, einmaliger Spielertreffer −20 HP / mode0 4 / vier
`bhEne_SetFireEffect(plp, 3, …)`, Licht ohne +5, Phasen bis Ende bzw. Dauerbrand type 0; 267:
Tabellenwahl 2,3,0,1, Landung 0.5 über der Fläche, Wandhaften, Ausblenden). **37/37** Tests grün.
Raum RM_9302 headless: **21 sichtbare 256-Sätze** (Bild 240, Texturliste aus `mdl[2].texP`),
**19–21 sichtbare 257-Spritzer** (Bild 250–260, globale Gruppe 2), RM_5810: **12 sichtbare
267-Tropfen** (Gruppe 81). Spieler-HP sinkt in beiden Räumen unter 0 (Boss, KI an, Spieler steht).

**Messung (`run11`):** 31 Räume / **46** Einträge (vorher 50), 0 Fehlschläge. Weg: `bhEff267`,
`Eff130/type4-Suche`, `LookPlayaer: owP.flg` (je RM_5810/RM_9302). **Neu:** `bhEff250` und
`bhEff301` jetzt auch in RM_9302 — der Kampf kommt dort erst durch den Port so weit, dass EN14
Blut-/Fleischeffekte anfordert; aufgedeckt, nicht verursacht.

**Offen (EN14-Räume):** `bhEff250` (bhEff_E00_Mince), `bhEff301` (Blutfleck, effsub3.c, 703 Z.),
`bhEff130`. `./build.sh` gelaufen (Start.command-App aktualisiert). Fremde Änderungen im Baum
(`native/events.cpp`, `tools/verify_clock_puzzle.py`, `work/rm1090_clock/`) nicht berührt.

### 11. Effekt 250 (Mince) portiert; Morph-Zeichenpfad-Abbruch gefunden und behoben (2026-10-15)

`bhEff_E00_Mince` (250, effsub2.c:23) ist portiert (`step_e00_mince`): strukturell ein Zwilling von
267 (dieselben vier UV-Tabellen, hier in natürlicher Reihenfolge über `type & 3`), Startrichtung aus
`ay`, Fall 0.06, Blutkinder 296 in den Passes mit ct0 1..9, `type & 0x100` unterdrückt sie. Erzeuger
sind `bhEne_SetMinceEffect/2` (zonzon1.c:52/92) und `SetMince` (en15.c) — nativ `t3_set_mince`.

**Zweite Schicht:** nach dem Port meldeten RM_5810/RM_9302 `zonzon1.c:84 mdl[BloodType[id].mdlno].texP`
— der Erzeuger holt die Blut-Texturliste über `RECVX_ENEMY_EXT.model_texp_index`, und genau der war
nie gebunden (nur `model_texp` ohne Index). Jetzt gebunden (gleiche Regel wie en23 `model_texture`).
Beide Räume melden danach nur noch `bhEff301`.

**Morph-Abbruch (neu, durch den EN14-Fortschritt erreichbar):** beim Abtasten mehrerer Endbilder brach
RM_5810 bei **Endbild 505–535** mit `Native runtime error: morph object has no vertex stream` ab —
Endbild 500, 550, 600 und 900 liefen durch. Ursache: `actor.cpp` rief `morph_a.vertex()` *vor* der
beidseitigen Prüfung in `morph_blend`; EN14 **MV02** (en14.c:456, Morph mdl[0]/mdl[1]) hat **Objekt 1
mit evalflags 0xE** (Bit 0x8 = HIDE, Modell vorhanden) — genau den Fall, den `npCalcMorphing`
(njplus.c:677) überspringt und unverblendet lässt. Fix: Quellzugriff nur nach erfolgreichem
`morph_blend`; `RECVX_MORPH_TRACE=1` nennt übersprungene Objekte. **Vorherstand gemessen:** HEAD
(7d1f366, Worktree-Build in `$TMPDIR`) rendert Endbild 510/520/900 fehlerfrei — nicht, weil der Pfad
dort robust wäre, sondern weil EN14 ohne die LookPlayer-/Strahl-Dienste MV02 in diesem Fenster nicht
erreichte. Der Abbruch war also durch meine Dienstbindung neu erreichbar; der Fehler lag im nativen
Zeichenpfad. Die Scans sehen so etwas nicht: sie exportieren nur das Endbild 600.

**Belege:** Test `effect_e14_fire` Abschnitt 5 (250), 37/37 grün; RM_5810 Endbild 505/515: **5 sichtbare
250-Brocken** (Textur 2 aus `BloodType`) und 296-Blut sichtbar; Sweep 505–545 ohne Abbruch.

**Messung (`run12`, vor dem Morph-Fix):** 31 Räume / **44** Einträge (vorher 46), 0 Fehlschläge; einzige
Änderung: `bhEff250` aus RM_5810/RM_9302 weg.
**Messung (`run13`, mit Morph-Fix):** identisch 31 Räume / **44** Einträge, 0 Fehlschläge — der Fix
liegt außerhalb des Scan-Endbilds 600 und ist nur im Endbild-Sweep 505–545 sichtbar.

**Letzte Lücke der EN14-Räume:** `bhEff301` (Blutfleck, effsub3.c:404, 703 Z.) zeichnet über
`sys->ef_trs2d` (bhDrawTrsEffect2D, effect.c:912) — eine **2D-Effektliste, die der Host nicht hat**
(0 native Treffer); dazu `sys->bl_ct` (nativ nur als Partikelzähler) und die globale Gruppe
`ef_tn[4]`. Wie `sys->efm` und `sys->wt_*` ein Subsystem, kein Einzelport.

### 12. Modifier-Schatten 304 und EN29 Tentakel (2026-10-15)

Zwei gemeldete Gegnerluecken geschlossen; beide waren Bindungs- bzw. Portierungsluecken,
keine fehlenden Subsysteme.

**`rySetShadow` / `bhEff304` (4 Räume, EN19).** Der Dienst war deklariert, aber nie gebunden.
`effsub3.c:1640` legt einen `eff[]`-Platz an (`flg 0x8240001`, `id 304`, `mlwP = &sys->efm[1]`)
und haelt in `ct0..ct2` Zeiger auf `owP[obj].mtx[12]` **dreier** Gelenke. Nativ stehen dort die
Gelenknummern; gelesen werden im Effektpass dieselben `owP`-Matrizen. `bhEff304` (:1683) waehlt
zwei der drei Punkte (der weiteste Abstand gewinnt), setzt `ay` aus `atan2(vct.z, vct.x)`,
`sx/sz = (alt + Abstand + xn/zn)/2`, `py` aus `bhGetGroundPosition`, das Sichtgate aus
`stflg 0x8/0x1000000` und `mdflg 0x1` und traegt den Satz in `sys->ef_mdf` ein. Der Zeichenweg
existiert bereits (Modifier-Liste mit `mlwP = &sys->efm[1]`, ids 1/258); 304 ist dort nur
mitgelaufen. Messung: Meldung aus RM_1122/3230/5010/5840 verschwunden.

**EN29 Tentakel (3 Räume).** `native/enemies/en29.c` (+ `en29.h`) entstanden aus
`en29.c:164-1818` und `en29.h`: Mode0-Tabelle (Index 4 NULL), BrainType[4] (`Br00` Typ 0/1,
`Br01` Typ 2/3 mit `sys->rm_flg`-Wartezustand), Aktionstabelle `En29ActTbl[40]` im
`EA_WORK`-Layout (Binärsuche, `-1`-Ersatzschluessel), fuenf Bewegungsroutinen mit
`RequestEnemySe` 74498/74499 und `ChechPlayEnemySe`, `TargetAnalyze` an `bas_obj`/`atk_obj0/1`,
`DmgCheck` mit der Wundtabelle `DmgDat[21]`, `AttackHitCheck` als Kugeln an sechs Gelenken
gegen `plp->watr` (`njCollisionCheckSC`), `PlySetDamage`/`PlyMoveMain`/`PlyAction*` mit
derselben `plp->mnwP`-Umschaltung wie EN19, `SetDmgEffect` (Blut 309/311, Splitter,
Gelenk-Z-Achse, skalierter Satz), und der Schadenskapsel-Mechanismus `SetDmgEne`/
`CalcDmgEne`/`CheckDmgEne`: fuenf Kindwerke (Typ 8) als Ring in `DmgWrk[8]`, die ihr Modell
mit dem Koerper teilen (`enP->mlwP = ewP->mlwP`). Der `wt_*`-Wasserzaehler aus `bhEff108`
wird hier nicht gebraucht.

**Geprueft:** Test `effect_ry_shadow` (Feldbelegung, erstes Pass gegen dieselben `owP`-Matrizen
gerechnet, Sichtbarkeit in `scene.shadows`) und Test `enemy_en29_world` in RM_9000
(Init: ar/ah/car/cah, flg 0x958, clp_jno, TypPrm[0], fuenf Kapselkinder, Ringreihenfolge,
Schatten 304 mit mdlver 1/type 1/sy 6; Angriffskette `Mv01` -> `PlySetDamage` -> `PlyMoveMain`
mit hp −5, `mode0 2`/`mode2 2`; Trefferuebernahme einer Kapsel an den Koerper). CTest 102/103.

**Messung (`run15`):** 26 Räume / **37** Eintraege (vorher 31/44), 0 Fehlschläge.

**Naechster Posten:** EN17 (Monster Steve) — Vertrag `native/enemies/en17.h` und die
Kettenmodule `en54.c`/`en55.c` liegen fertig, es fehlt die Implementierung; sie schliesst
`bhEne17`/`bhEne54`/`bhEne55` in RM_9280. Danach das `sys->wt_*`-Wassertiefenfeld
(`bhObj005`, objitm.c:1213-1560) fuer `bhEff108` in vier Räumen.

### 13. EN17 Monster Steve und die gepanzerten Ritter EN54/EN55 (2026-10-15)

`bhEne17`, `bhEne54` und `bhEne55` waren die drei letzten Meldungen in RM_9280. EN54/EN55
lagen schon fertig als Module vor (`native/enemies/en54.c`/`en55.c`), waren aber nie gebaut:
sie lesen die Klingenspitze des linken Arms von EN17 (`recvx_en17_blade_tip`/`_hit`), und der
Besitzer dieser Kettenwerke fehlte. Mit EN17 ist der Besitzer da; beide Ritter sind jetzt in
Build und Dispatch (`jump[54]`/`jump[55]`).

**EN17** (`native/enemies/en17.c`, Vertrag `en17.h` ergaenzt um `create_child`/`set_call_func`/
`player_library`): nach `en17.c:13-1880` und `en17sub.c:23-203`.
- Init (`:789`): ar/ah/car/cah, hp 250, `flg 0x78|0x800`, Schatten `SdwTab {2,5,15,20,9,13}` ueber
  `bhSetShadow`, zwei **Kettenwerke** (Arme) ueber `bhEne17_SetLinkWork` (`flg 0x80A1`, id 0x1F/0x20,
  `mlwP = &epw->mdl[4/3]`, gemeinsame Motionquelle, per `set_call_func` auf `LArm`/`RArm` umgehaengt);
  Morphpaar mdl[0]/mdl[1]; `CalcEnemy` im Init (cah = ah = owP[5].y − py).
- Pass (`:347`): DmgChk (`CalcDamage`, `DamageAdd` mit Blut/Feuer/San, `ChgDmgMode`), `Mode0`,
  `SetMtn` (sieben Nachbilder in MTN 15/16, Kamerawackeln, Rauch, Zielgelenk-Staub,
  `MtnTblPlay` mit Wurzelversatz ueber `bhCalcFixOffset` und Stimmenzeitplan), Spielergriff
  (`PlyDG00`/`PlyDG01` mit den vier Wurzelversatztabellen), `EneToPlyDist`, `CollCheck`, `CalcEnemy`,
  `CameraControl`, Klingentreffer an den Spieler (vier `bhEff_SetPtcl2`, Licht, Stimme).
- Brain (`:927`) mit `bhCheckRoute`, `Brain00`, MV00..MV06, DG00, `PlayerDGCheck` (fuenf Kapseln
  Klinge gegen `plp->watr`, `npCollisionCheckCC`) und die Armwerke (`AxsCheck`: sechs Historienpunkte,
  `njBezierSpline` in 19 Schritten, je Klingenspitze und Wurzel).
- `bhEne17_DamageAdd` zieht **keine** hp ab (das tut der Waffenpfad; der Test belegt es).

**Nicht gebunden, benannt statt geraten:** `bhEff_SetPtcl`/`SetPtcl2`/`SetPtcl2V` (effsub4.c:19-276),
`bhEne17_AfterimageAxEffect` (Effekt 364), `rom->lgtp[2]` (`bhEne17_SetLight`) und das Spielerblut
`bhEne_SetBlood2(plp)`. Sie liegen ausserhalb des Gegners; die Routine ueberspringt sie und meldet den
Originalnamen (`bhEff_SetPtcl`, `bhEff365`/`366`, `bhEne17_AfterimageAxEffect`, `rom->lgtp[2]`).

**Geprueft:** Test `enemy_en17_world` in RM_9280: Init (hp/ar/car, flg, exp0.flags, clp_jno), zwei
Kettenwerke 0x1F/0x20 an Gelenk 13/9 mit flg 0x80A1, `CalcEnemy` im Init gegen die owP-Matrix, Brain00 →
MV03/MV04 → Angriffsmotion (15/16/11/2) mit Spielertreffer, DmgChk/DamageAdd ohne hp-Abzug, Wechsel
`damage_toggle`/`flags 0x200`, `ChgDmgMode`. CTest 103/104 (47 fremd). Alle 205 Raeume mit
`--validate-poses`, 0 Fehlschlaege.

**Messung (`run16`):** 25 Raeume / **34** Eintraege (vorher 26/37). Weg: `bhEne17`, `bhEne54`, `bhEne55`.
Steve bleibt in 600 Bildern ohne Skript inaktiv (`stflg 0x1000000`), wie im Original; die Ritter laufen.

### 14. Wasserflaeche `bhObj005`, Wasserringe `bhEff108` und das Wassergatter `st_flg 0x40` (2026-10-16)

`bhEff108` (4 Raeume) war die letzte Meldung aus Abschnitt "Befund bhEff108": die Prozedur
schreibt in die Wasserhoehenkarte `sys->wt_*`, die nativ nirgends existierte. Ihr Besitzer ist
`objitm.c:1213 bhObj005` (`bhJumpObject[5]`, Objekttyp 5 = Wasserflaeche), kein Effekt.

**Referenzlage.** Die C-Fassung von `bhObj005` enthaelt zwei handrekonstruierte Inline-Asm-Bloecke
mit VU0-Aufrufen. Beide wurden gegen das ELF geprueft (`tools/disasm_ps2.py --elf data/files/SLUS_201.84
bhObj005`, 0x285C14-0x285D88): wortgleich, inklusive einer Eigenheit, die die Rekonstruktion nur
versteckt zeigt - der Rest der Abklingschleife hinter der letzten vollen 16er-Gruppe wird gelesen,
aber nicht zurueckgeschrieben (`.L00285D6C` ohne `sqc2`). Die Mikroprogramme `VU0_WAVE_INIT`/
`VU0_WAVE_CALC` stehen in `archived/ps2/vsm/ps2_vu0.vsm:482-505` (die Referenz aus
`docs/PS2_HARDWARE.md`, nicht die Quarantaene): INIT setzt vf04/vf05 auf (0,-1,0,0); CALC bildet aus
den vier Nachbarzellen (h[c-1], h[c+zp], h[c+1], h[c-zp]) den Vektor (h[c+zp]-h[c-zp], -2,
h[c+1]-h[c-1]), teilt durch seine L1-Norm und negiert x/z. Das ist die Normale eines Eckpunkts; sein
y ist Original-y plus Zellhoehe.

**Port** (`native/world_water.cpp`, `native/scene.cpp`, `native/effect_sprite_procedures.cpp`):
- `bhObj005` case 0: Karte `(max x - px + 2) x (max z - pz + 2)` aus `objP[0].model->vlist`
  (Chunk 51, 3135..4643 Eckpunkte je Raum), Zelle je Eckpunkt `(int)z + (int)x * wt_zp`, Kopie der
  Eckpunkte/Normalen (`exp2`), `sys->st_flg |= 0x40`. case 1: `npSetOffsetUV2` je `ct3` (16 Schritte
  zu 256/1024 = Kachel eines 4x4-Atlas, Summe nach 16 Passes 0), dann der EE-Block mit der
  `gfrm_ct`-Paritaet (gerade Passes: y der geraden Eckpunkte, Normalen der ungeraden; ungerade
  umgekehrt; Eckpunkt 0 bekommt in geraden Passes nur sein y, der letzte nie eine Normale - so die
  Schleife), dann Abklingen `max(h - 0.2, 0)` in 16er-Gruppen.
- Zeichnen: `ModelGeometryOptions.local_vertices/local_normals` ersetzen den Chunk-Inhalt von
  `objP[0]` vor der Knotenmatrix, `uv_object/uv_chunk/uv_u/uv_v` verschieben die u/v des getroffenen
  Chunks 66 (short-Summe). Der plist-Lauf von `npSetOffsetUV2` (je Kopf ein short, je Nicht-66-Chunk
  ein weiteres) wird nachvollzogen und gegen die regulaere Chunk-Kette geprueft; in allen Wasserraeumen
  liegt Chunk 66 bei Offset 16 mit `usr_num` 0.
- `bhEff108`: Tor `st_flg 0x40`/`gm_flg 1`, Lage in Kartenzellen, `sy = 0.1*lkono`, Bresenham-Ring in
  acht Oktanten mit `(unsigned short)`-Randpruefung, Maximum je Zelle, Radiuswachstum, Abklingen, frei
  unter 0.
- `sys->gfrm_ct` hat keinen nativen Besitzer; die Paritaet kommt aus einem Passzaehler des
  Objektdurchlaufs. Das Original setzt `gfrm_ct` alle 60 VSyncs auf 0 (gerade), bei 60 Bildern je
  Sekunde bleibt die Paritaet erhalten.

**Befund: das Wassergatter war nativ immer zu.** `sys->st_flg 0x40` setzt im Original nur
`bhObj005` (und loescht es nie: `room.c:908` nimmt `0x400200`, nicht `0x40`). Nativ hat es nie jemand
gesetzt. Damit waren in allen 13 Raeumen mit Wasserflaeche saemtliche wassergebundenen Pfade stumm:
der Zombie-Wassercheck (`en01.c:4071`, dessen Dienst `bhCheckWater` fuer EN01 zudem nie gebunden war -
jetzt `RECVX_EN01_SERVICES.check_water = water_top`), die Spritzer 15 der fallenden Effekte
(`effect_sprites.cpp:323`), die Wasserschritte des Spielers (`player_stairs.c:518/593`) und die
Zweige `effsub1.c:486/2361/3566/9335`, `en22.c:775`, `weapon.c:876/1033` soweit portiert. Sie laufen
jetzt wie im Original. `bhCheckL2Water` (`water_line`/`l2_water`) bleibt eine benannte Luecke. **Nachtrag 2026-10-16:** portiert (`RoomWorld::l2_water`) und an EN01/T3 `water_line`, Kampfkern und bhEff130 gebunden; die EN01-Meldungen `bhCheckL2Water (hitchk.c:5561)` entfallen.

**Belegte Original-UB (neu in der Liste, 12 Raeume):** in RM_0140, 1130, 3210, 4000, 4001, 5710, 7060,
7100, 8000, 8001, 9110 und 9130 liegen Eckpunkte der Wasserflaeche bei x = 0 oder z = 0. Ihre
Nachbarzellen `c-1` bzw. `c-zp` liegen **vor** `wt_wvp`; das Original liest dort den vorherigen
`bhGetFreeMemory`-Block (bzw. dessen 64-Byte-Ausrichtungsrest), dessen Inhalt vom Raum abhaengt.
Nativ wird die Hoehe dort als 0 gelesen und die Stelle einmal je Werk gemeldet
(`bhObj005: Nachbarzelle ausserhalb wt_wvp`). Betroffen sind nur die Normalen der Randeckpunkte.
Nur RM_3060 (x 2..58, z 2..56) ist frei davon. Ein zweites Typ-5-Objekt in einem Raum (Original:
`npSetMemoryL` und danach NULL-Zugriff in mode 1) kommt in den Daten nicht vor und wuerde gemeldet.

**Geprueft:** Test `object_water_wave` (`tests/native/water_wave_test.cpp`): Einheitstest des Rings
(r=1 um die Zelle, 131 Passes, Rand, Tor) und die vier Raeume RM_3060/3210/7060/9130 mit echten
Diensten: ein Typ-5-Objekt, Kartenmasse, Zelle je Eckpunkt, `st_flg 0x40`, UV-Schritt 256 und Summe
0 nach 16 Passes, Ringe der 109-Raumerzeuger heben Zellen (Spitze 1.1/1.8), Eckpunkt-y der im Pass
geschriebenen Paritaet liegt 0..0.2 ueber der Zelle (Hoehe vor dem Abklingen), Normalen L1-normiert
mit y < 0, Abklingen. CTest 104/105 (47 fremd: `bhEff219 without the camera`, unveraendert). Alle 205
Raeume mit `--validate-poses`, 0 Fehlschlaege. Headless-Bild RM_9130 (Brunnenbecken) zeichnet.

**Messung (`run17`):** 32 Raeume / **42** Eintraege (vorher 25/34). Weg: `bhEff108` (4). Neu: die
eine belegte UB `bhObj005: Nachbarzelle ausserhalb wt_wvp` in 12 Raeumen. Ohne die belegten UB-
Eintraege (Nachbarzelle 12, `bhCheckRoute` 2, `bhSetMotion frm_no` 2, `Motionkanal` 2, `en02 ang` 3)
bleiben 21 Eintraege mit fehlender Funktion; die groessten: `bhEne03` (3), `EN53 CoreInit` (3),
`bhEne15` (2), `bhSetRapEff 310` (2), `bhEff301` (2).

### 15. EN03 Schwarze Witwe (`bhEne03`) — Zustandsmaschine, Kinder und der leere Modellsatz (2026-10-16)

**Auftrag:** der letzte verbliebene Eintrag der drei Raeume RM_6090, RM_7080 und RM_7250 war
`bhEne03`. `native/enemies/en03_lib.c` (geteilte T3-Bibliothek) und `native/enemies/en03_collision.c`
(der gemeinsame Wanddurchlauf, Dienst `collision_works`) lagen bereits; `en03_spider.c` trug die
gemeinsamen Spinnenbausteine (`MakeMatrix`, `HideParts`, `GetWall`, `SetModelFlg`, `AddNullTrans`,
`DGDirCheck`, `DirTarget`, `GoAHead`, `GetPartsPos`, `bhEne03s`). Die Identitaet selbst fehlte.

**Portiert:** `native/enemies/en03.c` (rund 4100 Zeilen) mit `recvx_en03` als `bhJumpEnemy[3]`:

- `bhEne03` (:478) — zwei Zweige: `flg 0x4000000` fuehrt ausschliesslich `PlayerControl` (das ist der
  versteckte Anfuehrerwork), sonst `Mode0[mode0]`, `CallSE`, `bhSetMotion`, `aox/aoy/aoz`,
  `car/cah` nach `EXP0_C(105)`, `bhCheckPlayer`/`CheckEnemiesBall` bei `flg 0x8`, die Lagekorrektur
  je Flaechenart, `CollisionLine`/`CollisionWalls`, die Wurzelmatrix `T(p) * exp0` und
  `bhEne_SetWeponAtr(35,1|6,5.0f)`.
- `Init` (:625) — `exp0` (240/8) mit acht `bhEne03s`-Beinkindern auf den owP-Knoten
  `BrokenParts[10,16,22,28,7,13,19,25]`, fuenfzehn EN24-Kindern auf `mdl[1]`, dem versteckten
  Anfuehrerwork (`ene03_leader`, flg 0x4008001), `exp1` (12/8) mit Timer, Schatten 258,
  HITPOINT-Zeile nach `sys->gm_mode`, `clp_jno`, `CapColTabA/B`, `SdwTab`, `flip_tree`.
- Brain `BR00`/`BR01`/`BR02` (:886-1142), Move `MV00`-`MV19` (:1145-2698), Nage `NG00`/`NG01`
  (:2701-3211), Damage `DG00`-`DG12` (:3216-4224), Die `DD00`-`DD05` (:4229-4822), `CollisionWalls`
  (:5064), `CollisionBoxEdge2` (:6074), `CollisionLine`/`2` (:6199/:6216), `DiveSpace` (:6278),
  `GetWallDir` (:6335), `DamageInit` (:6366), `CheckJumpSpace` (:6715), `PlayerControl` (:6798),
  `CallSE` (:7117), `Acid` (:7275), `HitMark` (:7353).
- **`MV00`-Tabelle:** `action_table[2][6] = {1,1,2,2,0,0} / {1,17,17,2,0,0}` wird mit `rand() % 4`
  indiziert, also nur die ersten vier Spalten. `MV12`/`MV13`/`DG00`/`DG05` sind leere Originale.
- **`CheckClimbWall`/`CheckClimbDownWall`/`AvoidWall`** (en03.c:260/266/272) haben im Original leere
  Parameterlisten und geben unbedingt 0 zurueck. Damit sind die Zweige `mode2 = 6/7/8` aus
  `MV01`/`MV03`/`MV17` **unerreichbar**; `MV05`-`MV08` laufen in den Daten nie. Das ist
  Originalverhalten, kein Portierungsrest.
- **Zwei trg_pos[4][2]** in `NG00`/`NG01` liegen zusammenhaengend: ein zweiter Index > 1 liest die
  naechste Zeile (bei `plp->mode2 & 1` unkritisch, bei `plp->mode2` bzw. `mode2 - 2` bis 3 nicht).
  Nativ als flache Tabelle mit Randpruefung; hinter Index 8 meldet der Work die belegte Original-UB.

**Befund 1 — der leere Modellsatz ist keine Luecke.** `room.c bhSetEneMdl` nullt ML_WORK-Sitze
hinter dem geladenen Satz; `bhCalcModel`/`bhCalcTree` laufen dann ueber `obj_num == 0` und tun
nichts. Derselbe Zustand entsteht **innerhalb** eines Satzes, wenn ein MDL-Record absichtlich leer
ist (`u16(6) == 0`, `actor.cpp:493`). Die native Kinderanlage (`create_enemy_child`) fasste beide
Faelle unterschiedlich auf und meldete den zweiten als Luecke; RM_6090 traegt genau so einen Satz
(EN03-Reihe: Record 0 = Spinne, Record 1 = leerer 192-Byte-Record), wodurch `bhEne03_Init` beim
Anlegen der fuenfzehn EN24-Kinder zwei falsche Meldungen erzeugte. `load_models` unterscheidet jetzt
"absichtlich leer" von "Leser hat eine Ausnahme geworfen" (`model_failed`), und `create_enemy_child`
meldet nur noch den echten Lesefehler. Die Messung bestaetigt, dass dadurch **kein** bestehender
Eintrag verschwindet: der Vergleich run17 gegen run18 zeigt nur `bhEne03` weg.

**Befund 2 — `plp->dvx/dvy/dvz` hat keinen verbrauchenden Spielerbesitzer.** `NG00`/`NG01` setzen die
Rueckstossgeschwindigkeit des Spielerworks (en03.c:2856/2916/3109/3190). `RECVX_ENEMY_PLAYER` fuehrt
diese drei Felder nicht; der vorhandene `EffectWorldHooks.player_dv` schreibt sie in
`player_hit_dv_`, das **niemand liest**. Der Port schreibt sie jetzt ueber einen eigenen Dienst
(`RECVX_EN03_ID_SERVICES.player_dv_set`, gebunden an dasselbe Feld) und meldet die fehlende Bindung
sonst namentlich. Der Zustand ist damit sichtbar, aber der Griffrueckstoss bleibt offen.

**Befund 3 — belegte Original-UB, nicht nachgebildet:** `EXP0_INT(pno, 980) = EXP0_C(105)`
(en03.c:4592/6465) schreibt 768 Byte hinter den 212-Byte-Block des Beinkindes
(`bhEne_CallocWork(0xD4, 8)`) und damit in den Block eines spaeteren Kindes. Kein Leser im Original.
Die drei Byte-Schreibvorgaenge `EXP0_BYTE(pno,12/14/15)` liegen dagegen im Block und treffen
`EXP0_F(0xC)`; sie werden als `recvx_en03s_leg_mark` erhalten.

**Geprueft:** Test `enemy_en03_world` (`tests/native/en03_world_test.cpp`) in RM_6090 mit echten
Diensten: Init (Modus [0,0,0,0] -> [1,1,0,0], exp0/exp1, hp/hp2, dist 999, ct 60..149, clp_jno,
flg 0x1078|0x800 und 16-Bit-Beschnitt, acht Beinkinder mit lkwkp/lkono/owP-Verweis, fuenfzehn
EN24-Kinder, Anfuehrer flg 0x4000000 mit geteiltem exp1), `MV00` waehlt nach 96 Passen den Zweig 1
(erwartet: `rand() % 63 + 40` Passe, dann `{1,1,2,2}`), nach 200 weiteren Passen weicht die
Gelenkmatrix von der Bind-Pose ab, `bhEne03_GoAHead` hat den Work bewegt, `hokan_rate 32768`.
Gegenprobe mit `set_enemy_ai(false)`: derselbe Work bleibt in [0,0,0,0], legt kein exp0 an und keine
Kinder; nach dem Zuschalten laeuft Init. Keine Luecke in Init/Move/Zustandswechsel.
Headless-Bild RM_6090 zeigt den Raum (Spinne nicht im Bildfeld). CTest 105/106, rot nur die fremde
Suite `effect_room_sprites` (47, `bhEff219`) wie zuvor.

**Messung (`run18`):** 30 Raeume / **40** Eintraege (vorher 32/42). Weg: `bhEne03` (3). Neu:
`bhSetMotion(plp): mnwPb[mtn_no] fehlt` in RM_7250 (1). Ursache: nach dem Ende des Griffs stellt
`PlayerControl` (en03.c:6983) `plp->mnwP = plp->mnwPb` wieder her, laesst aber `plp->mtn_no` auf dem
Wert der Gegnerbibliothek (62..71) stehen; der Spielerbesitzer sucht diesen Clip einmalig in der
eigenen Bibliothek und meldet ihn. Das Original liest dort einen Satz mit fremder Knotenzahl;
nativ wird der Zustand gemeldet statt geraten. Nach Klassen: **22 Eintraege in 20 Raeumen** sind
belegte Original-UB oder hostseitig nicht nachbildbare Originalzustaende (`Nachbarzelle` 12,
`en02 ang` 3, `bhCheckRoute` 2, `bhSetMotion frm_no` 2, `Motionkanal` 2, `bhSetMotion(plp)` 1);
es bleiben **18 Eintraege in 11 Raeumen** mit tatsaechlich fehlender Funktion, die groessten
`EN53 CoreInit` (3), `bhEne15` (2), `bhSetRapEff 310` (2), `bhEff301` (2).

### 16. EN15 Nosferatu (`bhEne15`) — Griff, Wurf, Sturz und das Gift 397 (2026-10-16)

**Auftrag:** `bhEne15` in RM_5800 und RM_6100 (der Kern EN53 dort ist ein getrennter Posten).

**Portiert:** `native/enemies/en15.c` als `jump[15]` — der ganze Gegner nach en15.c:265-3347:
Bewegungswahl (Stand/CloseTurn/KeepFar/Chase/`__attack`), die drei Hiebe (Fluegelquader gegen
`plp->watr` mit `njCollisionCheckBC2`, mittlerer Hieb als Kapsel mit `njCollisionCheckCC`), der
Spritzer `SpecialAttack`, der Wurf (Griff bei Bild 22, Hieb 55, Wurf 78), Schaden mit Kerntreffer
(x20 Handfeuerwaffe nahe owP[3], x5 Granate am Kern), Gift, Feuer, Tod mit dem `gm_flg 0x40`-
Abschluss, die Motion-Staffeln `mtn_relay`/`mtn_relay_relay` und `LockLeg`. Die acht
Spielerfuehrungen (`Ply_func`) laufen wie im Original im Gegnerpass, solange das Haltebit
`EXP0_S(0x5A)` steht; die Spieler-Modi 4/5/6 sind im Original passiv (player.c:6596-6700).
`bhEff_E15_Poison` (397) liegt als Effektprozedur beim Effektbesitzer, `njCollisionCheckBC2` in
`native/wall_capsule.cpp`.

**Vier Befunde am vorhandenen Besitzer (alle mechanismusweit, nicht EN15-eigen):**
1. `bhSetMotion(plp, ...)` aus einem Gegnerpass gab es nativ nicht (EN01 meldet genau das,
   en01.c:7943). Die Spieler-Motionuhr von `control_enemy_player_motion` ist jetzt
   `player_set_motion_now(add)` und wird von beiden Aufrufstellen benutzt.
2. `plp->spd` lief nur von der Aktion in die Welt. Ein Gegner, der den Spieler ueber mehrere Bilder
   rutschen laesst (`spd *= 0.94`), haette jedes Bild den Wert der Aktion zurueckbekommen.
   `take_player_speed` reicht einen im Gegnerpass geschriebenen Wert an `action.spd` zurueck.
3. Die Giftbits `plp->stflg 0x80000/0x200000` gehoeren dem Inventar. Ein Gegner- oder Effektpass,
   der sie ueber die Spielersicht setzt, erreichte den Besitzer nicht. `enemy_player_set` reicht
   neu gesetzte Bits jetzt weiter. **Folge ausserhalb EN15:** die Saeure 256 (`bhEff_E03_Acid`,
   effsub2.c:1452, EN23/EN14) vergiftet seither wie im Original; vorher blieb das Bit stumm.
4. Effektsaetze tragen jetzt `O_WRK.exp0` als erzeugendes Gegnerwerk (`owner_work`) und koennen
   `txp[0]` des Elternsatzes erben; das Gift liest/schreibt darueber Haltebit und Giftzaehler.

**Belegte Original-UB, gemeldet statt geraten:** die `_mtnno`-Schalter in DrivePlayer/SlidePlayer
(en15.c:2953/3066/3134/3168) haben kein `default`; ein anderer `plp->mtn_no` liest einen
Stapelwert. `plp->dam[kno]` wird geschrieben, aber weder player.c noch pwksub.c lesen es.

**Geprueft:** Test `enemy_en15_world` (RM_5800, echte Dienste): Init (exp0, hp 600, Kapseln,
flg 0x178|0x800, watr), Wurfkette Stand -> Throw -> Hold (plp->mode0 4) -> Flying -> Drive, danach
Drive -> Slide -> Falling -> FallDie: der Spieler rutscht ueber den Plattformrand (`rom->posp`
x 0..120, z 0.31..120.55; Lage z 120.566) und stirbt mit `plp->mode0 6`, `hp -1`, `flg 2`,
`ts_flg 0x4000` — der Originalsturz vom Turm. Ein Treffer erzeugt drei 397-Saetze (Typ 1 und
zwei Typ 0) mit `exp0 = epw`; die Prozedur laeuft 20 Passes ohne Luecke. Gegenprobe ohne
Gegnerpass: kein Init. CTest 106/107, rot nur die fremde Suite `effect_room_sprites` (47,
`bhEff219`) wie zuvor. RM_5800 headless mit `--validate-poses` ohne Fehler.

**Messung (`run19`):** 30 Raeume / **38** Eintraege (vorher 30/40). Weg: `bhEne15` (2), neu: nichts.
Die Raumzahl bleibt, weil RM_5800/6100 weiter `EN53 CoreInit` (child_ambient) melden.

### 17. EN53 Nosferatu-Kern — Materialbindung und Ambient-Grün (2026-10-16)

Die fehlende `child_ambient`-Bindung ist geschlossen. Der vorhandene WorldEntity hält
beschreibbare Ströme je `mbp[k]`; der Dienst folgt dem echten ersten Kind und patcht dessen
Polygonliste. Der Zeichenpfad liest den Live-Strom des gewählten Morphmodells.

**Semantikbefund:** `ChangeAmbient` (`en15.c:3350`) schreibt +5/+9 der Chunks 18/19/22/23:
das ist nach `njCnkCmA/Da/As/Das` **Ambient-Grün**, nicht Alpha. Der native Decoder erhält
Materialschreibvorgänge auch ohne Dreiecke. Der reguläre Zeichenpfad reicht sie geordnet weiter;
Multi-Licht multipliziert Ambient mit dem Material vor Lichtaddition/clamp, Einzellicht bleibt
unabhängig. Keine pauschale RGB-/Alpha-Ausblendung.

**Prüfung:** neuer `enemy_en53_world` über alle drei Originalräume: Init, Pulsmodellfolge,
bytegenaue Patchgrenzen, diagnostischer Tod mit Zwischenwerten und Debris, Rückkehr mit frischen
Materialwerten. Produktiver append-/Lichtpfad numerisch geprüft. Neun gezielte Tests grün;
`effect_room_sprites` bricht im gemessenen alten und neu gebauten Binary identisch an der
`bhEff219`-Kameraerwartung ab. Dessen spätere Prüfungen sind nicht als bestanden belegt.

**Gezielte Messung:** Vorher bei 60 Bildern in RM_5800/6040/6100 jeweils
`EN53 CoreInit: model/material services`; über die neu gebaute, signierte arm64-Dev-App von
Start.command bei 1/8/30/60/600 Bildern in allen drei Räumen keine Enemy gaps, keine blockierten
Befehle und kein Posenabbruch. Kein neuer Gesamtscan, daher keine neue globale Raumzahl.

RM_6040 bleibt regulär in Ereignissteuerung, RM_6100 nach Init deaktiviert; ihre Puls-/Todtests
setzen ausdrücklich Diagnosevorbedingungen. Keine natürliche Kampf-/Fensterabnahme und kein
Windows-Lauf. Allgemeine Renderergrenzen (Specular/Konstantfarbe, alternative Zeichenpässe,
Materialzustand über RoomWorld-Austausch) bleiben offen.

Details: [EN053.result.md](enemies/EN053.result.md), Prüfausgaben
`build/validation/en53_20261016/`. Kein Commit/Push.

### 18. EN19 Tyrant — die Effektanlage `bhSetRapEff` 305..312 / 349 (2026-10-16)

Abschnitt 17 hielt fest, dass die EN19-Restlücken unportierte `effsub3.c`-Effekte sind.
Diese Runde schließt davon neun Sätze. Sie liegen als eigenständige Handfassungen auf
demselben `eff[512]`-Satz des Raumbesitzers: 310 `bhEff30a`, 349 Rückruf und 308
`bhSetEffSpark` in `native/effect_rap_camera.cpp`; 307 Klauenfläche und 312 gespiegelte
Klaue in `native/effect_rap_claw.cpp`; 309/311 Blutstrahlen in
`native/effect_rap_blood.cpp`; 305 Staub und 306 Rauch in `native/effect_eff30x.cpp`.
Kein zweiter Effektpool, kein `sys->efm`-Nachbau: 305/306 nehmen ihre Texturgruppen über
den vorhandenen Sprite-Pfad, 307/312 arbeiten mit `EffectMatrixRef` auf echten
Besitzermatrizen statt mit Rohzeigern.

**Semantikbefunde, die den Port getragen haben:** `effect.c:742` leert die Zeichenlisten
je Pass — ohne eigenes Leeren sammelten 309/311 alte Billboards (vom Komponententest
aufgedeckt). `effsub3.c:2270` hält einen 307-Punkt sichtbar, solange `alpha >= 0`, nicht
`> 0`. `effsub3.c:2082` lässt den Nachlauf von 306 im Pass des Übergangs nach `mode0 == 4`
aus. `bhEff30a` Typ 1 nullt die Kameraversätze jeden Pass und löscht `flg` nie; Typ 2 endet
bei `x_rang + y_rang < 0.01` (85 lebende Pässe) und addiert `off_pos` im Endpass noch.
`bhSetEffSpark` benutzt eine **eigene** Tabelle `Eff308Prm[2]` und `bhGetGroundPosition`,
nicht die GunSpark-Werte. `Tex5Buv[12]` (309) liest im Original hinter die Tabelle: die
ELF-Bytes bei `0x359370` sind Sprungtabellenworte, als `float` Denormale um `3.4e-39`,
nativ als `{0,0,0,0}` geführt und am ELF belegt (ebenso `Eff309Prm @0x359190`,
`Eff30bPrm @0x329FB0`, `Tex5Buv @0x3592B0`). Bereichsüberschreitungen bleiben benannte
Lücken statt Ersatzwerte.

**Gezielte Messung** (`build/validation/rapeff310_20261016/`, je 600 Bilder, `rc=0`):
RM_5010 trägt zusätzlich die Sätze **310 und 349**, RM_5840 zusätzlich **305 (3×), 307
(2×), 309, 310 und 312 (3×)**. Kamerapose und Tyrant-Zustand sind vorher/nachher gleich,
beide Läufe melden `0 pending world bindings, 0 blocked commands` und keine Effektlücke.
`enemy_en19_world` belegt `310=1 349=0 312=3` in RM_5840 — Typ 2 (`status 0xE0000000`) hat
nach `en19.c:248` kein 349. Vier neue Komponententests grün. **Kein neuer Gesamtscan**,
daher keine neue globale Raum- oder Eintragszahl.

**Nebenbefund zu Abschnitt 17:** die dort gemessene `effect_room_sprites`-Abbruchstelle ist
jetzt geklärt. `step_eff219` meldet die Kameralücke seit `7d1f366` bewusst nicht mehr, weil
`room.c:33` `bhInitCamera` das ganze `cam`-Objekt beim Raumladen nullt und `game.c:50`
`bhControlEffect` vor `:58` `bhControlCamera` läuft — die Box liegt im ersten Pass am
Ursprung wie im Original. Die drei veralteten Prüfungen sind nachgezogen. Der danach
verbleibende Fehler derselben Suite („afterimages must draw with material alpha 200..40")
ist mit HEAD-Worktree plus ausschließlich der 219-Testkorrektur gemessen und damit
**vorbestehend**. Er ist in derselben Runde geklärt und behoben.

Eingrenzung durch Bauen und Messen, nicht durch Lesen: bei `7b81215` ist die ganze Suite
grün; bei `7d1f366` mit ausschließlich der 219-Testkorrektur ebenfalls — die Nachbildprüfung
bricht erst ab `f7f6c67`. Gemessene Ursache (RM_7070, `producing=1 shots=6 zanzo=5 fnc=0`):
die fünf Nachbildalphas sind korrekt da, je **2704** Dreiecke in **Materialphase 6**
(`40/80/120/160/200`); zusätzlich kommen **228 mit 156** und **255 mit 132** Dreiecken aus
**Phase 3** durch den alten Testfilter
(`light_category==1 && !after_modifiers && gouraud && blend==Ninja && diffuse_material`).
Phase 3 ist ein Objektlistenwerk: `f7f6c67` hat `light_category = 1` für
`model.kind==2 && object_list>=2` eingeführt.

**Das ist richtig, nicht die Regression:** `objitm.c:518-529` zeichnet `ob_hlg` und `ob_spc`
nach `bhSetHalfLight()` mit `amb_chr` — genau halbes Licht und derselbe Umgebungsslot wie
der Spieler. Zu grob war der Test, der „halbes Licht + Ninja + gouraud" als Nachbildmerkmal
nahm. Korrigiert ist deshalb der Test, nicht der Zeichenpfad: die Alphaprüfung läuft jetzt
auf `material_phase == 6` (die `ef_fnc`/`ef_Zanzo`-Phase von `bhEff136`), dazu neu die
Forderung gleicher Dreieckszahl je Alphastufe (dasselbe Modell in fünf Pässen) und eine
ausdrückliche Prüfung, dass Phase 3/4 auf halbem Licht liegt — der Auslöser ist damit eine
belegte, geprüfte Eigenschaft statt eines Zufalls. Gegenprobe: mit abgeschaltetem
Alphaschreiben in `append_afterimage` schlägt genau diese Prüfung fehl.
Danach **112/112** CTest grün.

**Offen:** `bhEff298` und `bhEff303` sind unportiert (`SetDmgEffect` Fall 2 und
`TyBloodSet` melden ihre Originalnamen). `bhEne19` Mode0-Index 4 ist im Original NULL.
Keine natürliche Kampf-/Fensterabnahme: der Spieler stirbt im nativen Lauf, die Endbilder
zeigen „YOU DIED / Retry?". Kein Windows-Lauf, kein Commit/Push.

### 19. EN19 Tyrant — `bhEff298` und `bhEff303`, der letzte Rest der Kette (2026-10-16)

Abschnitt 18 ließ `bhEff_E00_AcidGenerator` (298, `effsub2.c:623`) und `bhEff303`
(`effsub3.c:1507`) offen. Beide sind jetzt portiert: 298 in
`native/effect_sprite_procedures.cpp` (`step_e00_acid_generator`), 303 in
`native/effect_rap_blood.cpp` (`step_eff303`, dort wegen derselben ryRap-Hilfen und
Texturgruppe 5), beide in `hand_effect_procedures[]`. Damit trägt `enemy_en19_world`
**keine** Effekt-Ausnahmeliste mehr; jede gemeldete Lücke ist dort ein Fehler.

**Semantikbefunde:** 298 hält bei `stflg 0x1000000` des verlinkten Erzeugers das ganze
Werk an, auch den Wartezähler (`:630`) — `lkwkp` ist hier ein Gegnerwerk, dessen `stflg`
wie bei `O_WRK` auf `0x88` liegt (`types.h:121/:282`), die Zeigerumdeutung des Originals
trifft dasselbe Feld. 303 zählt `sys->bl_ct` **erst** und verwendet es **dann**
(`:1532/:1534`) — umgekehrt zu `bhClrEff_RY` (`:176/:178`) und `bhEff301`
(`:529/:531`); `flg = 0x200001` ist eine Zuweisung, `sy` wächst gegen `szb` statt `syb`
(`:1577`), und `oP->tvp = DSP_WRK.VtxBuf` ist nativ `EffectSprite::tv`.

**Zwei Besitzerfragen, die der Port geklärt hat.** `sys->bl_ct` lag als Feld in
`EffectParticles::Impl`; weil `bhEff303` denselben Zähler braucht, liegt er jetzt bei
`effect_blood_counter()` (`native/effect_sprites.h`) und der Partikelpfad liest ihn dort
— keine zweite Zählung, der Schnappschuss sichert ihn weiter. Und `bhEff349` führt im
Effektpass Gegnercode aus (`en19.c:250` trägt `bhEne19_TyBloodSet` ein, das
`bhGetGroundPosition`, `mlwP->objP[18]` und `bhSetEffectTb` ruft): im Original derselbe
Satz wie im Gegnerpass, nativ waren Weltzeiger und EN19-Dienste nur während
`bhControlEnemy` gebunden. `RoomWorld::EnemyCallbackFrame` stellt beide für den
Effektpass her; **ohne den Rahmen stürzt `enemy_en19_world` ab (rc 139)**, das ist die
Gegenprobe.

**Gezielte Messung** (`build/validation/eff298_303_20261016/`): RM_5010, RM_5840 und
RM_3230 headless je 600 Bilder, `rc=0`. Satz-IDs, Kamerapose, HP und Motion sind
vorher/nachher **identisch** — wie erwartet, denn kein natürlicher Lauf erreicht die
beiden Sätze. Voller CTest **112/112** grün; vier Gegenproben greifen (`bl_ct`-Reihenfolge,
`sy`-Ziel, `stflg`-Tor, Zufallsreihenfolge). Kein neuer Gesamtscan.

**Offen und gemessen, nicht vermutet:** RM_5010 und RM_3230 halten den Tyrant in
`mode0 5`, wo `TyBloodSet` wie im Original sofort zurückkehrt (`en19.c:3614`); RM_5840
ist Typ 2 und hat nach `en19.c:248` kein 349; RM_1122 ist nicht im Kampf. Der
303-Nachweis läuft deshalb über das gekennzeichnete `status`-Bit `0x10000000` im
Weltest. Der Erzeugerzweig von 298 (`SetDmgEffect` Zeile 5) ist im Raum nicht belegt:
Zeile 5 wird nur bei einem Treffer mit **Waffe 15** gewählt, und `w->comb_wep` entsteht
in `recvx_t3_calc_damage` (`en03_lib.c:1314`) aus `w->wpnr_no`, das der Kampfkern beim
Schuss schreibt (`player_combat_core.c:969/980/1088`). Diese Kette ist hier nicht
aufgebaut; die Prozedur selbst ist auf dem produktiven `step_effect_sprite`-Pfad belegt.
Kein Windows-Lauf, kein Commit/Push.
