# 01 — Belegter EN07-Startablauf: RM_0130

## Gewählter Raum

`data/rooms/RM_0130.RDT` ist der erste verbindliche EN07-Abnahmeraum. Die
Fledermäuse werden dort **nicht** durch ein späteres Raumsonderereignis
erzeugt: Sie liegen als aktive `ETTY_WORK`-Einträge in der RDT und entstehen
beim normalen Raumladen. Das ist der originale Spawnablauf für diesen Raum.

RDT-Kopfwerte (little-endian):

| Feld | Wert |
| --- | --- |
| Dateigröße | `0x284bc4` / 2.640.836 Bytes |
| `RDT_WORK` | `0x80` |
| `rom->enep` | `0x1a2c` |
| `rom->ene_n` | 5 |
| Eventsektion | `0x4d180` |
| Modelltabelle | `0x2440` |
| Motiontabelle | `0x3ed40` |

`ETTY_WORK` ist `0x24` Bytes groß (`types.h:1287`): `flg` +0, `id` +4,
`type` +6, `flr_no` +8, `mdlver` +9, `wrk_no` +0xa, Position +0xc/+0x10/+0x14
und `ay` +0x1c.

## Tatsächliche RDT-Enemy-Works

Alle fünf Einträge haben `flg == 1`, `id == 7`, `type == 0`, `mdlver == 0`
und Floor 3. Die `wrk_no`-Werte entsprechen der Raumreihenfolge.

| RDT-Index / `wrk_no` | RDT-Offset | Position (x, y, z) | `ay` |
| --- | --- | --- | --- |
| 0 | `0x1a2c` | 70.29535, 26.0, 93.17194 | 0 |
| 1 | `0x1a50` | 60.65765, 26.0, 86.96921 | 38236 |
| 2 | `0x1a74` | 54.01295, 26.0, 90.45558 | 0 |
| 3 | `0x1a98` | 37.16267, 26.0, 89.97851 | 32776 |
| 4 | `0x1abc` | 44.76495, 26.0, 93.61502 | 0 |

Der erste Identitäts-7-Motionpointer steht bei `0x3ed40` und verweist auf
`0x3ed68`; wie `room.c`/`RoomWorld` es vorsehen, teilen weitere Einträge
derselben Identität diese Motionbibliothek. Die ersten dokumentierten
Blockgrößen sind 648, 1416, 2828, 1800, 1032, 1676, 3020; die eigentliche
Clipdekodierung gehört zu Aufgabe 03.

## Eventbefund

Am Offset `0x4d180` stehen als Eventkopf `0x00000008` und `0x0000003a`.
Ein 8-Byte-Kopf enthält nur `scd0`/`scd1`; es gibt keine zusätzlichen
Event-init-Offsets. Somit kann kein Event-init-Script die fünf Fledermäuse
spawnen. `scd0` beginnt bei `0x4d188`, `scd1` bei `0x4d1ba`; sie sind für den
Raumablauf relevant, aber nicht die Quelle der EN07-Works.

## Originale Aufrufkette und Zeitpunkt

1. `room.c:28` `bhInitReadRDT()` ruft `bhInitEnemy()` und `bhInitRoom()`.
2. `room.c:47` `bhSetRoom()` übernimmt `RDT_WORK` und durchläuft
   `rom->enep[0..rom->ene_n)`.
3. Für jeden Eintrag mit `flg & 1` ruft `room.c:559`
   `bhSetEneMdl(..., ep, ep->mdlver, i)` auf.
4. `bhSetEneMdl` ruft `bhSetEnemy((EGG_WORK*)ep, eno)` auf.
   `eneset.c:170` kopiert `flg`, `id`, `type`, Floor, Modellversion,
   Position und `ay` aus genau diesem RDT-Eintrag in einen freien gemeinsamen
   `BH_PWORK`.
5. Im ersten normalen Spielpass mit `sys->sp_flg & 2` durchläuft
   `eneset.c:344` `bhControlEnemy()` diese Works und dispatcht
   `bhJumpEnemy[ep->id]`, also für alle fünf `bhEne07`.

Für die spätere Runtime-Abnahme muss RM_0130 daher über den normalen
Raumwechsel/-load gestartet und bis mindestens zum ersten
`bhControlEnemy`-Pass getickt werden. Ein direkter `recvx_en07_init`, ein
künstlich erzeugter Actor oder ein abgetrennter Unit-Test erfüllt diesen
Nachweis nicht.

## Abgrenzung

Der aktuelle Test `tests/native/en07_bat_test.cpp` konstruiert bereits eine
`RoomWorld` und greift danach direkt auf EN07-Work zu. Er ist kein
Spawn-/Eventablaufnachweis und darf Aufgabe 08 nicht ersetzen.
