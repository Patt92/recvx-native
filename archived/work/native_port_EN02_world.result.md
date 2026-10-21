# EN02 Gulp Worm — nativer Port (2026-10-14)

Build: `build/en53-claude-20261014` (Release, Ninja, arm64).
Semantikreferenz: `src/ps2/veronica/prog/en02.c` (3384 Zeilen), `en02sub.c` (271),
`effsub2.c:1061/1237` (Sandsprites). Kein Originalkörper kompiliert, kein Generator,
keine zweite Workwelt.

## Umfang

| Datei | Inhalt |
|---|---|
| `native/enemies/en02.h` | Dienstvertrag `RECVX_EN02_SERVICES` (15 Dienste), Zustand `RECVX_EN02_STATE` (exp0, 144 Byte benannt), Einstiege |
| `native/enemies/en02.c` | bhEne02 komplett: Init, Brain/BR00, Move, MV00/02/03/04/05, Damage/DG01, DamageInit, Die/DD00/DD01, Sandeffekte, PlayerControl, CheckWall, CameraControl, WarpCheck, FallingRock, CallSE, HitMark; bhEne02sub (Slot 31): Init, MV01–MV04 |
| `native/enemies/en02_tables.c/.h` | Reine Datentabellen, **per Skript aus der Referenz erzeugt** (SD_01 38, SD_03 54, SD_11 113, SD_14 8, SD_16 47, SD_17 37, P0_42 3, P0_43 2; DmgReact 21, CombWepTbl 21, CombJointTbl 24, BloodTbl 24, CapColTab 13 — Zeilenzahlen gegen die Originalarrays geprüft) |
| `native/effect_sprite_procedures.cpp` | `step_e02_sand_dust` (253), `step_e02_sand_particle` (254) |
| `native/effect_sprites.cpp` | Atlas 82/83 in `ported_effect_atlas` |
| `native/world_enemies.cpp` | EN02-Dienste; `RECVX_ENEMY_EXT.effect_se/effect_se_playing` (Stimme 127) |
| `native/enemies/enemy_dispatch.c` | `jump[2] = recvx_en02` |
| `tests/native/en02_world_test.cpp` | RM_5830 (CTest `enemy_en02_world`) |

## Belegte Entscheidungen

- **Kinder** (en02.c:777-803): Maul `mlwP = &mdl[5]`, `obj_a/obj_b = mdl[5]/mdl[6].objP`
  → `create_child(parent, tmpl, base 5)` + `morph_sources(child, 0, 1)` (relativ zur Reihe
  des Kindes); Ring `base 4`; 16 Felsen `base 1`, in FallingRock per `child_model` auf
  `mdl[(int)(3r)+1]` umgehängt (:3160). Vorlage `child = {0x8001, 31, 0}` (ETTY_WORK).
- **Mobiler ATR** (`&sys->mwalp[sys->mwal_n++]`, :803-815): `mobile_wall_alloc/get/set`,
  type 3, w 10; DD00 setzt ihn ab Bild 158 auf owP[10] (flg 1) und ab Bild 350 wieder flg 0.
- **`sys->ef`-Maskensemantik**: SetSandSpr schreibt id/type/flg/pos/scale/mdlver,
  SetSandParticle nur id/flg/type/pos — `sx/sy/sz/mdlver` bleiben vom vorigen Aufruf
  (:3007-3016). Der Dienst `effect_tb(mask, …)` schreibt nur maskierte Felder der
  persistenten Vorlage `effect_detail::request_template()`.
- **`bhCheckFloorEnemy`-Identität** (:3108 vergleicht den ATR_WORK-Zeiger): Dienst liefert
  die Listenposition über `rom->flrp + sys->mflrp`; `-1` = NULL.
- **Stapel-Dummy `bhCheckWall(&dmy_ene)`** (:1946-1962): `wall_probe(flg 0x19, stflg, pos,
  ar 20, ah 1)`. `dmy_ene.flr_no` ist im Original uninitialisiert und wird von `bhCheckWall`
  nur für Wände mit `attr & 1` gelesen (hitchk.c:87). Die Bindung prüft, ob der Raum eine
  solche Wand hat: nein → Probe exakt; ja → benannte Lücke, kein Raten.
- **Zufallsreihenfolge** je Ausdruck links→rechts; `bhEne_SetFireEffect(.., 1+r, (int)(40r)+20)`
  in der Reihenfolge, die EN23/EN05 per Disassembly belegt haben (size vor len).
- **`mnwP != mnwPb`** (CallSE :3175) → `w->mtn_room != 0` wie in EN23.
- **`RECVX_ENEMY_PLAYER`-Schreibungen** gebündelt pro Zweig (`put_player`), Trefferfelder
  `plp->dvx..` über `recvx_t3_player_hit_set`, `bhEne_DGDirCheck(plp)` =
  `recvx_t3_dir_check(plp->ay, dv)`.

## Darstellungsanpassung (zu prüfen)

`plp->xn / plp->zn` (en02.c:1134-1135 schreiben, :1306-1307 lesen) sind das Schluckziel
des Spielers. `grep plp->xn / plp->zn` über das gesamte Original trifft **nur en02.c** —
kein anderer Leser oder Schreiber. Nativ liegen die beiden Werte in `RECVX_EN02_STATE`
(`swallow_x/z`), weil `RECVX_ENEMY_PLAYER` sie nicht führt und `world.cpp`
(Spieler-get/set) gerade fremd bearbeitet wird. Gleiche Werte, gleiche Lebensdauer; wer
sie später auf den Spielerbesitzer legen will, hat hier die Fundstellen.

## Belegte Originallücke: `ang` in `bhEne02_SetSandEffectMain` Fall 8

en02.c:2452/2462-2463 lesen `ang`, ohne es in diesem Fall zu setzen (Originalkommentar:
„devs didn't really initialize this variable on case 8"). Disassembly
(`tools/disasm_ps2.py --elf data/files/SLUS_201.84 bhEne02_SetSandEffectMain`, Block
0x193AE8–0x193D5C): `ang` wird als `$t0 = $s3` an `bhEne02_SetSandSpr` übergeben, `$s3`
wird im Block nie geschrieben → Wert = `$s3` beim Funktionseintritt = `$s3` des Aufrufers.
Die Kette `bhEne02_SetSandEffect` (7 Zeilen) ← `bhEne02sub_MV04` (99) ← `bhEne02sub_Move`
(10) ← `bhEne02sub` (10) ← `bhControlEnemy` (118) berührt `$s3` **in keiner** Funktion;
der Wert stammt also vom Aufrufer der Gegnerschleife. `ang` bestimmt nur `xn/zn`
(−sin/−cos · 0.5) der drei Sprites dieses Falls. Nativ: Lücke gemeldet, die drei Sprites
werden nicht angelegt (kein geratener Winkel), die Partikel (ohne `ang`) laufen.
Betroffen: RM_5830, RM_7230, RM_7231 (Ring-MV04).

## Prüfung

- `recvx-en02-tests data` → „EN02 world PASS": Init (Flags, hp, 18 Kinder mit Typ/Link/
  Modellreihe, Morphquellen 0/1, `bhEne02sub_Init`), MV00 (Bewegung, `rm_flg` 0x1
  gelöscht), MV02 (Motion 14, `rm_flg` 0x1, Maul öffnet bei Bild 3, Morph `shp_ct`),
  DD00 (Motion 11, Kadaverzustand, `flg 0x100000` nach Bild 350). Allowed-Gaps nur die
  `ang`-UB.
- RM_5830 headless 900 Frames, `--enemy-ai on`: Angriffskette MV00 → MV02/MV03 → MV04,
  Spieler-HP 160 → 0 (Frame 300: 140, 1800: 40); einzige Lücke die `ang`-UB.
- CTest Gegner/Effekte: **32/32** grün (inkl. `enemy_en02_world`, alle `effect_*`).
- Raumscan `build/validation/enemy_scan_20261014/run5`: **35 Räume** mit Lücken (vorher 38);
  `bhEne02` (6 Räume) ist weg.
- `tools/check_no_original_core.py`: PASS.

## Offen

- Skriptbefehle **0x98 `bhEffectSandSet`** (event.c:4214, `bhEne02_SetSandEffect(ene[v0], pos, v1)`)
  und **0xC3 `bhEffectSandSetMatsumoto`** (event.c:8595, `bhEne02_SetSandEffectEV`) sind in
  der Event-VM nicht gebunden (`native/world.cpp` wird parallel fremd bearbeitet). Die
  Funktionen `recvx_en02_set_sand_effect` / `_ev` sind exportiert.
- **255 `bhEff_E02_SandParticle2`** (Raumspawner, nicht von EN02 gerufen): braucht
  `sxb/syb/szb` als Spawn-Nachfelder; nicht portiert.
- RM_3091 / RM_7220: Wurm skriptgehalten (`mode0 5`), Freigabe nicht durchgespielt.
- Keine Fensterabnahme (Sandsprites 253/254 nur headless), Windows nicht gebaut.
- Vollbuild bricht weiter an der fremden `tests/native/effect_room_sprites_test.cpp:193`
  (`BinaryView`), nicht an EN02; mit `-k 0` bauen alle übrigen Ziele.
