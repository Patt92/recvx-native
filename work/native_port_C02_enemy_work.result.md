# C02 — Gemeinsamer Gegnerwork am vorhandenen Besitzer (Claude, 2026-10-10)

Fortsetzung von T1 Stufe A/B (`native/enemies/enemy_work.*`, `enemy_dispatch.*`).
Bis heute lagen Vertrag, Dispatchkette und zehn Gegnerroutinen im Baum, waren
aber weder gebaut noch an `WorldEntity`/`RoomWorld` angeschlossen; die World
lief weiter über den inerten `RECVX_EN01`-Speicher. Dieses Paket schließt den
Anschluss. Kein Originalkörper, kein Generator, keine zweite Workwelt;
Pflichtprüfer vor jedem Build PASS.

## Was jetzt gilt (implementiert und produktiv angeschlossen)

| Original | Nativer Besitzer / Stelle |
|---|---|
| `room.c:114-126` bhSetEneMdl → `eneset.c:170` bhSetEnemy in Raumreihenfolge | `RoomWorld::init_enemy_works` (`native/world_enemies.cpp`); `RECVX_ENEMY_WORK enemy` ist Member der `WorldEntity` (`native/world.h`), die Tabelle hält nur Zeiger. Geprüft: `wrk_no` == vergebener Slot für alle 759 aktiven ETTY der 205 Räume. |
| `eneset.c:344` bhControlEnemy | `RoomWorld::control_enemy_works` → `recvx_enemy_control` im `tick()` direkt nach dem Legacy-en01-Pass; `ExecEnemySeManager` danach einmal für beide. |
| `eneset.c:51` bhJumpEnemy | `recvx_enemy_dispatch_init`: 0, 7, 8, 10, 16, 20, 24, 25, 56, 71 nativ. 54/55 nicht linkbar (rufen `recvx_en17_blade_*`, `en17.c` fehlt) → Lücke „bhEne54/55“. |
| `Motion.c:21` bhSetMotion + SetMtnNormal/Fast/Slow und alle Hokan-Varianten, AngZyxToYzx/AngYzxToZyx | `native/motion_set.cpp` auf dem nativen objP-Baum der Entity. Gegen Disassembly SLUS_201.84 0x12F510..0x130D88 abgeglichen: nur SetMtnNormalHokan verbraucht für owP-0x4-Knoten einen Kanal; Euler-Flip = ang0+0x8000, 0x8000−ang1, ang2+0x8000; statisches `mka_ang` (SlowHokan ohne Winkelschlüssel schreibt nur [0]). |
| `MdlPut.c:240/262` bhCalcModel/bhCalcTree | `RoomWorld::calc_enemy_model` + `PlayerAsset::tree_matrices` (flg 0x80 an lkwkp->owP[lkono], 0x1000 behält root_mtx, owP-flg 0x1 behält Matrix). Danach einmalige Veröffentlichung Work → Entity. |
| `eneset.c:797/866` bhDrawEnemy/bhDrawEneObject | `RoomWorld::append`: Gate `recvx_enemy_draw_enabled(st_flg)` bzw. en_obj-Bedingung, mdflg 0x1; gezeichnet aus objP + owP-Matrizen (`PlayerAsset::sample_work`). |
| Skriptschreibrichtung Entity → Work | `adopt_enemy_script`: geänderte Felder (Lage, Motion, HP, mdflg 0x200 = bhEneRenderSet, stflg 0x1000000 = bhEneSetCk, flg 0x8000 = bhEneLookFlgSet) über `recvx_enemy_adopt` vor dem Pass. |
| `eneset.c:906` bhEne_CallocWork | Raumlebensdauer, 64-Byte-Cursor, Grenze 0xC40000 über den unausgerichteten Cursor. |
| `sys->mwalp` der Gegner (eneset.c:427 …) | `enemy_mobile_walls_`, in `collision_geometry()` und Bodenabfrage zwischen rom->walp und den bhObj001-Flächen (game.c:48-51: Gegnerpass vor ObjItm). |
| `en03.c:5220` bhEne03_Collision (Dienst `collision_works`) | `recvx_en03_collision` mit T3-Ergänzung `RECVX_ENEMY_EXT` (wal_count/wal_at in rom→mwalp-Reihenfolge, rom->h, set_call_func, gap), nur während des Passes gebunden. |
| Weitere Dienste | RNG (`core_random`), bhCheckWallType, bhCollisionCheckLine 1/2/3 inkl. ATR-Satz und Flächenidentität, bhGetGroundPosition, bhCheckFloorNum, rom->grand, Enemy-SE/Ex/Playing, CallPlayerVoice, CallSystemSe Bit 31, MN_WORK frm_num/obj_num/Wurzelschlüssel, objP[0].pos, evalflags 0x8, owP-Matrizen/Punkte, ene-Liste/enow, cb/rm/st/sp_flg (EventVm-Bänke 10/4/5/6), bhSetExplosion, bhEne_Event (Subplayer-Apparat), bhEne_SetWeponAtr. |

Legacy en01/en04 (`RECVX_EN01`-Store) bleibt für 1/4 bis C03/C04; ihre Works
stehen in der Tabelle (InitDamage, enemy_at), die Entity bleibt dort Autorität.
`damage()` liefert für native Works keinen erfundenen HP-Abzug mehr, sondern die
Lücke „A04: Trefferzuweisung an RECVX_ENEMY_WORK“; `combat_targets()` filtert
native Works nach pwksub.c:445.

## Nachweise

macOS arm64, `build/c02-claude` (Release, Unix Makefiles):
- Neuer CTest `enemy_work_contract` (`tests/native/enemy_work_test.cpp`):
  bhSetEnemy-Felder/Quirk/Lücken/Vollbelegung, Passreihenfolge inkl.
  sp_flg-Gate und Offset-Löschung davor, enow je Slot, InitDamage nach der
  Routine, en_obj, Zeichen-/Zielfilter; bhSetMotion Vorschub/Wrap/Pingpong/
  Attribut/Fast/owP-0x4/Slow/Hokan/datP NULL; RoomWorld mit Originaldaten:
  RM_0130 fünf ID-7-Works gebunden, bhEne07_Init-Zustand, Zeichnen nach dem
  Pass, Work→Entity-Veröffentlichung, Uhr läuft, bhEneSetCk übernommen und
  Zeichengate wirkt; RM_0080 Sprengfass-Fläche in der Kollision. PASS.
- Gesamt-CTest 54/56. Die zwei Ausfälle sind vorbestehend und gemessen:
  `effect_pool_lifetime` scheitert identisch mit Binaries von 22:29
  (`build/macos-arm64`) und 23:21 (`build/a01-combat`), vor dieser Arbeit;
  `engine_progression_resources` kompiliert nicht, weil
  `tests/native/progression_test.cpp:125` noch `EnemySpawnContext::random`
  benutzt (C01-Umbau von `enemy_spawn.h`).
- `tools/verify_rooms.py`, 600 Ticks: 205/205 laden, 0 blockierte
  Eventbefehle, kein Laufzeitfehler (ein zwischenzeitlicher RM_9170-Abbruch
  im Legacy-Refresh für native Works ist behoben).
- Runtime RM_0130 (diagnostisch, `--in-game --headless`, kein Storyablauf):
  vorher (a01-combat 23:21) Fledermäuse y=26, Motion 0, kein Ton; jetzt per
  Linienprüfung an die Decke angedockt (y=27), Motion 3 mit zufälligem
  Startframe, MV00-Leerlauf mit Zwitschern 0x2303 → `MULTSPQ1/0017/3/0003`,
  sichtbar in Kameras 3/4.
- Policy: Quelle/Build/Binary PASS für `build/c02-claude/recvx-runtime`.
- `tools/check_build_coverage.py`: nie kompiliert 23 → 9 Quellen; die
  Gegnerroutinen stehen erwartungsgemäß als „nur_referenziert“ (Aufruf über
  die Sprungtabelle).

Windows x64 (`build/windows-c02-claude`, zig/Ninja): `recvx-runtime.exe` und
`recvx-enemy-work-tests.exe` kompiliert und gelinkt, Policy inkl.
Vergleichslink PASS. **Nicht ausgeführt** (Wine im Sandbox nicht startbar).

## Offen / blockiert (nicht verdeckt, alle als Lücke gemeldet)

1. **Spielerwork-Besitzer fehlt** (`player_get/player_set`): en07/en10/en16/
   en24/en25 lesen u. a. plp->mode0..3, stflg, flg, wpnr_no, Lage, hp,
   mlwP/mnwP. Es gibt keinen nativen Besitzer mit originalen mode0..3; die
   Werte liegen verteilt (EventVm-Bänke 13/14/15, SceneSpawn, Inventory,
   Kampfkern). Damit bleiben BR00-Suche, Angriff, Griff und Spielerschaden
   aller nativen Gegner offen. Kein Teilbild mit erfundenen Modi.
2. bhCheckPlayer / bhCheckEnemies (hitchk/weapon, A04), bhSetShadow,
   bhSetEffect(17) und Effekt-Nachfelder (E03/E04), bhGetHitCollisionNormal,
   Get3DSoundParameter (Lage des Gegnertons im AudioMixer), plp->ofx/ofz.
3. Sub_controll 0x80/0x8f/0x90/0x92/0x93 für native Works schreibt noch nicht
   mode0..3/ct2/stflg/flg/mnwP sowie die Taskfelder mtn_md/mdflg/hokan_rate
   (event.c:9758 ff., Fallthrough 144→143 erst gegen Disassembly klären).
4. Kinder zur Laufzeit (`spawn_enemy`), bhControlMask für id>90, Morph
   (mdflg 0x2), en17 (blockiert en54/en55), en01_core/en04 (C03/C04).
5. Das Budget von bhEne_CallocWork zählt nur Gegnerblöcke, nicht die
   übrigen sys->memp-Verbraucher (Motion-/Raumtabellen).

Kein Paket, kein Commit/Push. Der gemeinsame Arbeitsbaum wurde während der
Arbeit von einem Peer-Build mitgenutzt (`build/stairs-claude-20261010`,
23:41 mit einem Zwischenstand gebaut); der Endstand baut sauber.
