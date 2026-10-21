# enemy_rows_audit — Ergebnis (2026-10-20)

Prüfung der partial-Zeilen gegen Code und Original. `tools/check_no_original_core.py` vor
Beginn und am Ende PASS. Kein Build in `build/macos-arm64`; private Prüfläufe unter `$TMPDIR`
(Kopien der `librecvx_*.a`, `ar d`/`ar r`, Runtime aus vorhandenem `runtime.cpp.o` gelinkt).

Hinweis: Der Hauptagent hat meine Arbeitsbaumstände von `native/combat.{cpp,h}`,
`native/player_combat_core.{c,h}`, `tests/native/{combat_state_test.cpp,en29_world_test.cpp,
rm0110_event_zombie_check.py,rm5840_tyrant_gun_check.py}` in Commit 9a7a501 übernommen.
Offen für die Integration: `archived/work/enemy_rows_audit.integration.patch` (CMake + world.cpp +
world_enemies.cpp, `git apply --check` grün) und `archived/work/enemy_rows_audit.ledger.tsv`
(10 Ersatzzeilen, eine neue: `weapon.c:bhSetGunSplash`).

## Ergebnis je Zeile

| Zeile | Ergebnis | Beleg |
|---|---|---|
| en26.c:bhEne26 (362/370) | veraltet | RM_0110-Tötungsreplay Bild 1084: hp -1, 4× eff 362, 8× eff 370, keine Lücke (= en26.c:864-872, 3350-3384). In `rm0110_event_zombie` aufgenommen; Gegenprobe ohne `nikuhen_effect` rot (0 statt 4). Bleibt partial wegen Original-UB (Brain00/PlayerLink) und fehlendem Durchspielen. |
| en29.c:bhEne29 (309/311) | veraltet | SetDmgEffect ruft `blood`/`blood2` (gebunden). `enemy_en29_world` lässt keine 309/311-Lücke mehr zu und prüft genau einen Blutsatz aus DmgDat/E29EffTbl mit Link an owP[10] des Körpers; Gegenprobe (blood/blood2 vertauscht) rot. |
| en19.c:bhEne19_Init (rySetShadow) | veraltet → done | Dienst `shadow` gebunden, `effect_ry_shadow` grün; RM_1122/3230/5010/5840 headless ohne Gegnerlücke. |
| en19.c:bhEne19_DmgCheck (A04) | veraltet → done | bhCheckEnemies schreibt direkt ins native Work (`RoomWorld::damage` mit A04-Text hat keinen Aufrufer). Neuer Test `rm5840_tyrant_gun`: hp 900→840→780→700 = EneDamFar/Mid[19][6] 60/60/80; Gegenprobe ohne `flg |= 0x4` rot. CTest-Eintrag im Patch. |
| en19.c:bhEne19 (Mode0[4]) | bleibt offen | Original-NULL (en19.c:26), en19.c schreibt mode0 4 nie; benannte Lücke. Nur Text geschärft. |
| en19.c:bhEne19_PlyDmg052_053 (310) | veraltet → done | :4264 benutzt denselben gebundenen Dienst `cam_shake` wie :243. Typ-1-Wandabprall im Raum nicht ausgelöst. |
| zonzon1.c:bhEne_SetBloodEffectBurst | veraltet → done; Lücke war bhSetGunSplash → **portiert** | Burst vollständig. Die Notiz meinte weapon.c:1632 bhSetGunSplash: der Kerndienst bekam gap->wpn_no/gap->ay nicht und war im Spielerkampf nicht gebunden. Portiert als `recvx::gun_splash` (combat.cpp) auf der sys->ef-Vorlage, Kerndienst um wpn_no/ay erweitert, gebunden im Spielerkampf; Effektpass (bhEff013) im Patch. Tests `gun_splash` (neu, alle Zweige, Vorlagenpersistenz; Gegenprobe rot) und `test_gun_wall_splash` in `combat_player_state` (Gegenprobe ax statt ay rot). RM_0110-Wandschuss: vorher Lücke „bhSetGunSplash (weapon.c:1632)“, nachher eff 11 type 5 mit lox-Verfall 0,7^7. |
| eneset.c:bhDrawEnemy (EN01-Kinder) | **portiert** (Patch) | Kopf/Arm/Bein/Eltern: mdflg 0x1, im Original nie gezeichnet. Kappe/Wurm/Bom/Scope: mdl[0] = epw->mdl[2/3], gezeichnet bei freiem mdflg 0x1 — nativ fehlte model_bank, sie wurden still übersprungen (30 Räume mit Kappen/Bom/Scope, z. B. RM_3160/5550/6030). Fix in `child_model` (world_enemies.cpp). Neuer Test `enemy_en01_child_draw` (RM_3160, 45 Dreiecke; Gegenprobe ohne Patch bzw. mit mdflg 0x1 rot); Headless-Bild zeigt die Mütze. Zeile bleibt partial (bhDrawEneObject/st_flg-0x20-Pass). |
| en12.c:bhEne12 | Teil veraltet | Effekte 256/257/266/269 sind registriert. Offen bleibt MV04-Original-UB. |
| en05.c:bhEne05 | bleibt offen (konkret) | `plp->txp[2]` (Leichentextur) hat keinen Zeichenbesitzer (`player_texture` gibt 0 zurück). Nicht portiert. |
| en13/en13sub, en23, en02, en09 | kein konkreter fehlender Mechanismus | Nur Original-UB-Lücken (en02.c:2452, en09 PlyDG00 pd) und fehlendes Durchspielen/Fensterabnahme. |

## Beobachtungen außerhalb des Auftrags

- `zombie_blood_pool` ist mit `build/macos-arm64/recvx-runtime` von 22:36 rot. Mit denselben
  Bibliotheken und dem `runtime.cpp.o` von 22:31 ist er grün. Die Ursache liegt also in der
  laufenden, nicht committeten `native/runtime.cpp`-Änderung, nicht in diesem Auftrag.
- RM_9321 headless `--in-game` Spawn 0: Die Akteurslage wird zwischen Bild 500 und 700 der
  Eröffnungsszene NaN. Das war schon im älteren Stand so (Diagnosestart, nicht untersucht).
- Veralteter Kommentar `native/world_enemies.cpp:1879` („bhEff130 ist nicht portiert“): bhEff130
  ist registriert.
