# Gemeinsame Voraussetzungen aller Gegner (Querschnitt)

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
