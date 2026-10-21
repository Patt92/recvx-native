# EN05, EN13/13B, EN23 und bhStandPlayerMotion — Stand 2026-10-14

Auftrag: „EN05, EN23, EN13/13B und bhStandPlayerMotion weiter“. Eigenständige
native Umsetzung auf den bestehenden Besitzern (WorldEntity-Work, Spieler-/
Inventar-/Effektbesitzer). Originalquellen und Disassembly nur als
Semantikreferenz; kein Originalkern, kein Generator, keine zweite Workwelt.
`tools/check_no_original_core.py` vor jeder Build: PASS.

## Umgesetzt

| Bereich | Datei | Stand |
|---|---|---|
| bhStandPlayerMotion | `native/player_action.c`, `native/runtime.cpp` | done (Kiste, Karte case 2, Status, Computer, Player_controll 52, bhEff134) |
| EN13 Alexia II / EN13B | `native/enemies/en13.c` | partial (Lücken siehe unten) |
| EN23 Riesenspinne, bhEne03s, EN24-Kinder | `native/enemies/en23.c`, `en03_spider.c` | partial |
| EN05 Hunter/Sweeper + bhEne05s | `native/enemies/en05.c`, `en05_tables.c` | partial |
| Raumgegnermodelle mdl[0..n] mit Texturbündeln | `native/world.cpp` | partial (bhSetEneMdl) |

EN05 im Einzelnen: Rahmen, Init (gegen Disassembly 0x1ACA10), SearchPlayer,
BR00 (Treppe/Leiter/Sprung über Ereignisflächen, Kopfabriss, Hieb, Sprung),
MV00–17, InitDamage, Damage mit Dmg-/DeadSwitchTbl, DG00–13, ChainDamage,
DD00–08, Prüfungen (LeaningWall/Jump/BackAttack/Hikkaki/PlyRoute),
FixedLegPos, CheckWall-Kapseln, FloorCollision, SetWeponAtr, HitMark,
CallSE, PlayerControl. WaterEffect liest die `WE_*`-Tabellen wie das Original
zusammenhängend (ELF 0x311BD0..0x311DAF): fünf `num`-Werte laufen über ihr
Feld hinaus, `WE_23` liest die ersten 32 Byte von `WaterEffectTbl` (Zeigerbits
als Denormale, PS2-FPU rechnet sie als 0). Originalfehler erhalten:
`hokan_count = 45875` in DG08/09/DD02/03, DD01 bleibt in case 3.

Neue Dienstbindungen (`native/world_enemies.cpp`): EN05-Dienste auf
vorhandenen Anbietern (Route, bhSearchPlayer, bhAddSpeed, bhCalcFixOffset,
bhCheckWater/`water_top`, bhSetWaterSplash2 mit gemeinsamer Vorlage `sys->ef`,
`rom->etcp[].attr`, objP-Winkel, owP-Flags, `rom->h`). Gift
(`plp->stflg |= 0x80000`) geht als Anforderung an das Inventar
(`take_player_poison`), `PEXP0_I(0x14)` dmlvl kommt aus dem Aktionsbesitzer.

## Reihenfolgekorrektur Spielerbaum

`player.c:840` bhSetPlayer rechnet `bhCalcModel(plp)` vor `Event_init`, dessen
`bhControlEnemy` (event.c:352) den ersten Gegnerpass fährt. Die Runtime hat
die Spielergelenke bisher erst nach dem ersten Bild übergeben; der erste Pass
meldete deshalb `plp->mlwP->owP[n]` (en05s, en04.c:1061, en01.c:2840). Jetzt
wird die Pose vor `events->initialize()` übergeben.

## Messungen (`build/validation/en05-20261014/`)

- `./build.sh --test`: 89/89 CTest grün (`build-test.log`), darunter neu
  `enemy_en05_world` (Init, Partner, Kind, natürliche Jagd mit Treffer, DD00).
- Headless 600 Bilder ohne Eingabe, KI an: RM_5590/5600/8020 Hunter jagen,
  Hiebe (MV15/17), Spieler-HP sinkt unter 0, Fressen MV11; keine Gegnerlücken.
  RM_7081/7130: Hunter nach dem ersten Pass angehalten (Raumereignis hält den
  Gegnerpass; nicht weiter untersucht).
- `verify_head`: identisch mit `stand-20261014/head.log` (vorbestehender
  Fehler „frame 20: stays on PlMtnAct motion 42“).
- `verify_enemy_entry`: grün.
- `verify_enemy_ai` rot, mit echtem Vorherstand verglichen: ohne die
  Reihenfolgekorrektur (`dogs3000-without-initcommit.*`, gleicher Quellstand
  sonst) endet RM_1000 mit HP 0 ohne Game Over (= Fehler in padgate/en13-Logs).
  Mit Korrektur töten die Hunde, Game Over Phase 2, aber `plp->mode0` ist 2
  statt 6 (Todesbiss NG01) — die nächste Prüfung schlägt fehl. Ursache in EN04
  nicht untersucht.
- `tools/port_check.py`: exit 0.

## Offen (ehrlich)

- EN05: `plp->txp[2]` (Leichentextur) ohne Zeichenbesitzer (benannte Lücke);
  Sprung, Treppe/Leiter, Wand-Lehnen, Wasser und Kopfabriss nicht natürlich
  durchgespielt. Undefiniertes Original als Lücke: MV01 `rate` bei dist 0,
  CheckJump `dist` bei dmlvl > 2, MV07 ohne ATR.
- bhEne05s MV01/MV02/FloorCollision: im Original ohne Aufrufer, gemeldet.
- EN13: bhEff267, EN14 Eff130/type4-Suche, bhEne14_Acid Eff256,
  bhEne14_LookPlayaer owP.flg. RM_5810 meldet zusätzlich bhEff250/bhEff301
  (Effektprozeduren); ohne Vorherlauf nicht zugeordnet.
- EN23: bhEff238, Effekt 256 (bhEff_E03_Acid).
- EN12-MV04 bleibt offen (Nutzerentscheidung).
- Keine Fensterabnahme; headless ist keine Bildabnahme. Windows nicht gebaut.
  Nur das Entwicklungs-Start.command wurde durch `./build.sh` aktualisiert,
  kein Paket ersetzt, kein Commit.
