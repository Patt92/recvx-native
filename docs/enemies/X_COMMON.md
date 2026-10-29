# Gemeinsame Voraussetzungen aller Gegner (Querschnitt)

## Stand 2026-10-22 (am Code nachgeprueft)

- **X01 erledigt:** 0x200 setzen Aktions-, Ereignis- und Treppenpfad (player_action.c footstep,
  player_event.c water_step, player_stairs.c), 0x100 der Kampfkern (player_combat_core.c, wie
  player.c:4985/5690, playpch*.c); das fehlende `plp->stflg &= ~0x300` im Passtor (player.c:1390)
  steht jetzt in runtime.cpp. Test `player_stflg_pulse` (Gegenprobe rot). mode2 6..8 = bhCPM2_act
  wlk/run/bak-Familie, Leser en07.c:301.
- **X02 erledigt:** `--state` fuehrt `enemy_gaps` (RoomWorld::unbound_enemy_routines).
- **X03 erledigt:** Framezahlen je Identitaet ueber `enemy_mn_work` fuer alle gebundenen Gegner.
- **X04:** bhControlMask ist gebunden (enemy_dispatch.c:170, world_enemies.cpp:1418); Audit je
  Identitaet siehe X04_SUBPL.md.
- **X05/X06 erledigt:** MTN_TBL-Abspieler in en09/en17, gemeinsame zonzon/hitchk-Dienste
  (recvx_t3_*, en03_lib) nativ vorhanden und von en09/en22 benutzt.
- **X07/X08:** Hinweise zur Abnahme bzw. Patchvorschlaege, kein offener Port.

## X01 – Spieler-Statusbits für Gegner-KI (blockiert u. a. EN07-Wecken)

- player.c:1390 `plp->stflg &= ~0x300` je Bild in bhControlPlayer – im Host ohne Entsprechung.
- `stflg |= 0x200` bei Fußkontakt: player.c:1435 (bhControlPlayer), :2195 (bhCPM2_act_srt) u. a. – normaler Host-Lauf `RoomWorld::player_footstep` (native/world.cpp:348) setzt es nicht.
- `stflg |= 0x100` beim Schuss (player.c:4985/5690) – im Kampfkern vorhanden, Weg nach `flags[14]` nachweisen.
- Spieler-`mode2` 6..8 (en07 BR00) Bedeutung im Original klären.
- Leser im Original: en07.c (3)

## X02 – Lücken sichtbar machen

Fehlende `bhEneNN` und `sv->gap` erscheinen nicht in `--state` (RM_1060: `blocked_commands 0`). Ausgabe der Gegnerlücken je Work ergänzen.

## X03 – Motionbibliothek je Identität

`motion_frames`/MN_WORK-Framezahlen je Identität (T1-Notiz: nur 1/4 im RoomWorld). Ohne Motion zeichnet der Renderer die Bind-Pose (T-Pose).

## X04 – bhSubpl, siehe [X04_SUBPL.md](X04_SUBPL.md)

## X05 – Motion-Tabellenabspieler (MTN_TBL_FMTN_WORK / MTN_TBL_ATB_WORK)

en09 (`SetMtn`/`MtnTblPlay`) und weitere Gegner spielen Motionfolgen aus Tabellen. Die Typen einmal gemeinsam aus types.h abbilden; jeder Gegner bringt nur seine Tabellenwerte.

## X06 – Gemeinsame zonzon/hitchk-Dienste

Wiederkehrend: `bhEne_ChgMtn`, `ikou`/`ikou3`, `bhEne_CalcPartsPos`, `NitenDir_ck`, `bhEne_CollisionCheckWall2`, `bhEne_CheckDirWall2`, `bhEne_CheckSideWall`, `bhCheckWallType`, `bhCollisionCheckLine`, `bhGetHitCollisionNormal`, `bhEne_SetBlood/2/4`, `bhEne_SetDFireEffect`, `bhEne_SetSanEffect`, `bhSetEffParticle`, `bhSetBloodPoolLnk`, `bhEne_HitCheckParts`, `bhEne_EnemyAtariCheck`, `bhEne_CalcDamage`, `bhDGCdirCheck`, `npSetAllMatColor`. Jeder Dienst hat **einen** Besitzer im gemeinsamen Dienstsatz; der erste Gegner liefert den Patchvorschlag, alle weiteren nutzen ihn. Kein Gegner implementiert einen Engine-Dienst privat.

## X07 – Raumscan-Grenzen

EN018, EN027, EN028 haben im Ein-Bild-Scan keine Instanz (Kindwork/Event-Spawn); Abnahmeraum aus Elternwerk/Skript bestimmen. EN053 ohne Originaldatei: zuerst bhJumpEnemy[53] lokalisieren. EN054/055 haengen an EN017.

## X08 – Griff-/Spielerkette

en01, en04, en07, en09 und Bosse teilen `PlayerLink`/`PlyDG*`/`mtn_enemy_work`. Erweiterungen nur als Patchvorschlag.
