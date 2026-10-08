# Engine-Portierungsstand

Engine gesamt: **47.3 %** gewichtet, **43.9 %** streng (1135/3168 Funktionen voll belegt, 158946 Originalzeilen ohne Plattformschicht).

Event-Befehle (bhScenarioJmpT): 147/223 gebunden, 46 nur konsumiert, 30 fehlen.

| Bereich | Funktionen | Zeilen | gewichtet | streng | original | opcode | verweis/teilw. | fehlt |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Gegner/KI | 342/1357 | 62910 | 26.2 % | 25.9 % | 16319 | 0 | 351 | 46240 |
| Effekte | 175/361 | 27671 | 52.0 % | 51.7 % | 14301 | 0 | 202 | 13168 |
| Events/Skript | 251/272 | 12358 | 91.7 % | 90.7 % | 11206 | 0 | 258 | 894 |
| Spieler/Kampf | 159/166 | 15830 | 95.2 % | 90.8 % | 14381 | 0 | 1379 | 70 |
| Raum/Kamera/Tür | 64/188 | 9931 | 50.3 % | 39.5 % | 3920 | 0 | 2151 | 3860 |
| Menü/Items/Text | 130/241 | 14026 | 69.2 % | 63.4 % | 8888 | 0 | 1627 | 3511 |
| Modelle/Animation | 11/88 | 4212 | 40.7 % | 16.9 % | 713 | 0 | 2002 | 1497 |
| Audio | 0/164 | 3013 | 16.4 % | 0.0 % | 0 | 0 | 987 | 2026 |
| Ablauf/Speichern | 3/331 | 8995 | 11.5 % | 0.1 % | 12 | 0 | 2052 | 6931 |
| Plattform (Host-ersetzt) | 97/704 | 17856 | 25.6 % | 19.4 % | 3460 | 0 | 2238 | 12158 |

Spalten original…fehlt in Originalzeilen. Plattformschicht zählt nicht zum Engine-Wert.

## Größte offene Funktionen

- `en19.c:982` bhEne19_Br01 (473 Zeilen, fehlt)
- `effsub1b.c:1618` bhEff138 (384 Zeilen, fehlt)
- `en19.c:543` bhEne19_Br00 (368 Zeilen, fehlt)
- `en19.c:1539` bhEne19_Br02 (367 Zeilen, fehlt)
- `en05.c:1079` bhEne05_BR00 (336 Zeilen, fehlt)
- `adv.c:2048` Adv_BioCvTitle (330 Zeilen, fehlt)
- `sub1.c:5225` ItemCombination (660 Zeilen, partial, Regeln über Original Combi_00/01 und combidata; Menüablauf/Animation nativ)
- `system.c:1305` bhSysCallMonitor (656 Zeilen, partial, Raumladen/-wechsel nativ in runtime/world, nicht als mn_md-Zustandsmaschine)
- `effsub3.c:404` bhEff301 (310 Zeilen, fehlt)
- `en03.c:6366` bhEne03_DamageInit (302 Zeilen, fehlt)
- `effsub1b.c:220` bhEff134 (296 Zeilen, fehlt)
- `effsub1b.c:1258` bhDraw138 (273 Zeilen, fehlt)
- `effsub4.c:2722` bhEff_Sub398 (265 Zeilen, fehlt)
- `effsub0.c:1953` bhEff182 (264 Zeilen, fehlt)
- `effsub2.c:2595` bhEff_E12_Fire (260 Zeilen, fehlt)

Ledger-Einträge ohne passende Funktion: system.c:bhSysCall

Erzeugt mit `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
