# Gegner-Auditbefunde angegangen (2026-10-14)

Auftrag: „geh alle davon an“ — Befunde aus
[native_port_EN09_world.result.md](native_port_EN09_world.result.md).
Eigene arm64-Build `build/en09-claude-20261014`, Pflichtprüfer vor jedem Build PASS.
Keine Starter-/Paketersetzung, kein Commit.

## Erledigt

| Befund | Ergebnis | Beleg |
|---|---|---|
| `bhCheckWall` ungebunden (EN10/12/21) | `RECVX_ENEMY_SERVICES.check_wall` → vorhandene direkte ATR-Auflösung `resolve_motion_wall_direct` mit dem Work als Akteur, Stufe `pp->dan_ap` je Entität | Lücke weg in RM_9170/5820/3120; en09/en21/enemy_work grün |
| Zombie `natural` beißt nicht | **Originalverhalten**, kein Fehler: RM_0080-Skript `06 0f 00 00` (pos_no == 0) → Common 46, `Sub_controll 0x8f` Typ 8 für Zombie 0..2. Typ 8 überspringt Brain (en01.c:4873) und erwacht erst durch Schaden (:3218). Auch die Originaltür RM_0050→RM_0080 liefert pos_no 0. Seit 1c2934d erreicht Sub_controll den nativen Work; die alte Erwartung beruhte auf der Fehlbindung. Ebenso RM_0031 (0x93 Typ 10) und RM_3070 (Zombie 1 Typ 8) | Verifier korrigiert; front/lever/death auf Spawn 2 (Typ 0): Biss, Freigabe, Tod/Game Over grün |
| op 0x8e `bhZombieUpDieCk` | gebunden (ed_flg-Bit bei totem Oberkörper) | — |
| `bhControlMask` | Masken-/Lippenbesitzer als `RoomWorld::control_mask` aus der Weltschleife gelöst; Raumgegner mit id > 90 laufen im Gegnerpass (eneset.c:372) | Lücke weg in RM_0020/1020/5820 |
| EN12 `SetWeponAtr`: owP nicht berechnet | room.c:438-444 rechnet den Baum beim Raumaufbau (`bhKeepObjWork` + `bhCalcModel`); nativ jetzt ebenso | Lücke weg; `enemy_work_test` prüft `enemy_calculated` vor dem ersten Pass |
| „kein Modell“ EN24/25/56, EN01-Kinder | Kein Ladefehler: kein MDL mit Objekten (mdl_n 0) bzw. `mdl[slot]` hinter mdl_n → genulltes ML_WORK. `model_absent`: bhCalcModel schreibt nur `ewP->mtx`, bhSetMotion ohne datP kehrt zurück | Meldungen weg; verbleibend nur „Motion ohne Modell (SetMtn schreibt nach objP NULL)“ (Original-UB, gemeldet) |
| EN11-Init-Dienste | rm_flg-Clear, `bhSetEffectTb(261)`, eff-Sicht, objP-Winkel, owP-Flags gebunden | RM_7081/8030 ohne EN11-Lücke |
| Effekt 261 / 270 | `bhEff_E11_SearchLight(Draw)` und `bhEff_E16_LaserSight(Draw)` eigenständig; `bhCollisionCheckL2MDL` (`native/hit_model.cpp`) gegen plp `mdl[3]`; Renderer: Gouraud-Alpha je Eckpunkt, Linien mit eigenem GS-Register; EN16-Effekt-/Bodendienste | neuer CTest `enemy_beam_effects` grün; RM_1020 ohne Lücken |
| EN04 erster Pass | eigene Meldung `plp->mlwP->owP[1] (en04.c:1061)` analog en01.c:2840 (Spieler-owP vor erstem bhCalcModel, Original uninitialisiert) | erlaubt im Verifier |

## Prüfung

- CTest gesamt 85/86. Rot: `script_motion` — Vorbestand, gleiche Meldung mit dem
  fremden Binary `build/box0090-claude-20261014` (12:02) und in STATUS bereits offen.
- Runtime grün: verify_event_freeze, verify_doors, verify_gameover, verify_continuity,
  verify_combat (runtime-only).
- `verify_enemy_ai`: grün bis `dogs-3000`. Rot, **identisch mit Vorher-Binary**:
  Tod bei Bild ~3150 durch Schaden statt NG01 (`plp->mode0` 2 statt 6) und
  `rm0050-trap` Moduscheck bei Bild 1100 (Hunde beißen, HP 84). Zeitlage gegenüber
  2026-10-12 verschoben; nicht durch diese Arbeit verursacht, nicht abgeschwächt.
- `tools/port_check.py` exit 0.
- Keine Fensterabnahme der neuen Effektzeichnung; Windows nicht gebaut.

## Offen (nicht in diesem Lauf)

- **EN05** (6186 Zeilen, RM_5590/5600/7081/8020…), **EN23** (4028, RM_9050),
  **EN13 + bhEne13B** (1389 + 462, RM_5810/9302; Modellskalierung/Store-/Restore
  der Vertexdaten braucht einen beschreibbaren Modellbesitzer) — reine Neuportierungen.
- **EN12**: Feuereffekte `bhEff_E12_*` (262 ff.) ungebunden.
- **EN56**: `sys->ufo_*` gehört `bhEff134` (effsub1b.c:220), nicht portiert.
- **EN16** nach dem Laser: `bhEne_SetBloodEffectBurst(plp)`, `bhCheckWallAttrB89`,
  `bhSetEffGunSpark`/`bhSetGunSplash`.
- Spotter/Alfred laufen in allen Originalräumen skriptgesteuert; ein natürlicher
  Alarm-/Schusslauf ist erst mit Eventfortschritt prüfbar.
- EN09: PlyDG00-`pd` und ChkArmLen-f20 bleiben belegte Original-UB (gemeldet).
- ID-Karte RM_1020: Variante B (originales Pad-Gating) weiter offen.
