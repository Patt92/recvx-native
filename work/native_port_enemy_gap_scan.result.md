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
| + EN11-Dienstbindung je Pass (RM_9100) | **31** Räume / 45 Einträge |

Prüfläufe: `run1` (vorher), `run2`…`run8` (nachher) in diesem Verzeichnis,
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
