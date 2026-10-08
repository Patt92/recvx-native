# Engine-Portierungsstand

Engine gesamt: **18.6 %** gewichtet, **11.6 %** streng (353/3168 Funktionen voll belegt, 158946 Originalzeilen ohne Plattformschicht).

Event-Befehle (bhScenarioJmpT): 137/223 gebunden, 53 nur konsumiert, 33 fehlen.

| Bereich | Funktionen | Zeilen | gewichtet | streng | original | opcode | verweis/teilw. | fehlt |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Gegner/KI | 108/1357 | 62910 | 13.2 % | 12.0 % | 7547 | 0 | 1559 | 53804 |
| Effekte | 14/361 | 27671 | 7.2 % | 5.1 % | 1402 | 0 | 1195 | 25074 |
| Events/Skript | 148/272 | 12358 | 57.8 % | 36.2 % | 1536 | 2938 | 5335 | 2549 |
| Spieler/Kampf | 31/166 | 15830 | 22.2 % | 10.7 % | 1689 | 0 | 3650 | 10491 |
| Raum/Kamera/Tür | 18/188 | 9931 | 21.6 % | 13.2 % | 1309 | 0 | 1680 | 6942 |
| Menü/Items/Text | 34/241 | 14026 | 33.8 % | 14.8 % | 2079 | 0 | 5311 | 6636 |
| Modelle/Animation | 0/88 | 4212 | 10.7 % | 0.0 % | 0 | 0 | 899 | 3313 |
| Audio | 0/164 | 3013 | 15.2 % | 0.0 % | 0 | 0 | 914 | 2099 |
| Ablauf/Speichern | 0/331 | 8995 | 8.3 % | 0.0 % | 0 | 0 | 1499 | 7496 |
| Plattform (Host-ersetzt) | 12/704 | 17856 | 7.5 % | 2.0 % | 359 | 0 | 1953 | 15544 |

Spalten original…fehlt in Originalzeilen. Plattformschicht zählt nicht zum Engine-Wert.

## Größte offene Funktionen

- `event.c:9963` Common_controll (2600 Zeilen, verweis)
- `pwksub.c:718` bhCheckL2Wall (868 Zeilen, fehlt)
- `effsub1.c:9001` bhEff130 (794 Zeilen, fehlt)
- `hitchk.c:2178` bhCheckWallRefAngle (607 Zeilen, fehlt)
- `hitchk.c:1276` bhCheckWall2Box (554 Zeilen, fehlt)
- `hitchk.c:2904` bhSetWallRefAngle (548 Zeilen, fehlt)
- `player.c:5262` bhCPM2_act_atk (533 Zeilen, fehlt)
- `light.c:332` bhControlLight (480 Zeilen, fehlt)
- `en19.c:982` bhEne19_Br01 (473 Zeilen, fehlt)
- `weapon.c:473` bhCheckGunAtari (410 Zeilen, fehlt)
- `hitchkl.c:123` bhCollisionCheckLineMain (407 Zeilen, fehlt)
- `effsub1.c:731` bhEff002 (405 Zeilen, fehlt)
- `playpch.c:602` bhCPM2_act_atk_pch (405 Zeilen, fehlt)
- `effsub1b.c:1618` bhEff138 (384 Zeilen, fehlt)
- `en19.c:543` bhEne19_Br00 (368 Zeilen, fehlt)

Ledger-Einträge ohne passende Funktion: system.c:bhSysCall

Erzeugt mit `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
