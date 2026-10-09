# Engine-Portierungsstand

> Historischer Abdeckungsstand, keine aktuelle Freigabe. Seit dem Cleanup vom
> 2026-10-09 zählen kompilierte Originalquellen und generierte Funktionskörper
> nicht als native Implementierungen; sie sperren den Build. Zahlen unten sind
> noch nicht nach dieser Regel neu berechnet. Aktuelle Aufgaben:
> [Native Reimplementation Roadmap](NATIVE_REIMPLEMENTATION_ROADMAP.md).

Engine gesamt: **46.6 %** gewichtet, **42.7 %** streng (1098/3168 Funktionen voll belegt, 158946 Originalzeilen ohne Plattformschicht).

Event-Befehle (bhScenarioJmpT): 150/223 gebunden, 44 nur konsumiert, 29 fehlen.

| Bereich | Funktionen | Zeilen | gewichtet | streng | original | opcode | verweis/teilw. | fehlt |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Gegner/KI | 305/1357 | 62910 | 23.4 % | 22.9 % | 14416 | 0 | 616 | 47878 |
| Effekte | 175/361 | 27671 | 52.0 % | 51.7 % | 14301 | 0 | 202 | 13168 |
| Events/Skript | 251/272 | 12358 | 91.7 % | 90.7 % | 11206 | 0 | 258 | 894 |
| Spieler/Kampf | 159/166 | 15830 | 95.2 % | 90.8 % | 14381 | 0 | 1379 | 70 |
| Raum/Kamera/Tür | 64/188 | 9931 | 51.2 % | 39.5 % | 3920 | 0 | 2333 | 3678 |
| Menü/Items/Text | 130/241 | 14026 | 70.4 % | 63.4 % | 8888 | 0 | 1980 | 3158 |
| Modelle/Animation | 11/88 | 4212 | 48.7 % | 16.9 % | 713 | 0 | 2675 | 824 |
| Audio | 0/164 | 3013 | 16.8 % | 0.0 % | 0 | 0 | 1011 | 2002 |
| Ablauf/Speichern | 3/331 | 8995 | 11.9 % | 0.1 % | 12 | 0 | 2120 | 6863 |
| Plattform (Host-ersetzt) | 97/704 | 17856 | 27.8 % | 19.4 % | 3460 | 0 | 3024 | 11372 |

Spalten original…fehlt in Originalzeilen. Plattformschicht zählt nicht zum Engine-Wert.

## Größte offene Funktionen

- `en19.c:982` bhEne19_Br01 (473 Zeilen, fehlt)
- `effsub1b.c:1618` bhEff138 (384 Zeilen, fehlt)
- `en19.c:543` bhEne19_Br00 (368 Zeilen, fehlt)
- `en19.c:1539` bhEne19_Br02 (367 Zeilen, fehlt)
- `en05.c:1079` bhEne05_BR00 (336 Zeilen, fehlt)
- `sub1.c:5225` ItemCombination (660 Zeilen, partial, Regeln über Original Combi_00/01 und combidata; Menüablauf/Animation nativ)
- `system.c:1305` bhSysCallMonitor (656 Zeilen, partial, Raumladen/-wechsel nativ in runtime/world, nicht als mn_md-Zustandsmaschine)
- `effsub3.c:404` bhEff301 (310 Zeilen, fehlt)
- `en03.c:6366` bhEne03_DamageInit (302 Zeilen, fehlt)
- `effsub1b.c:220` bhEff134 (296 Zeilen, fehlt)

Ledger-Einträge ohne passende Funktion: system.c:bhSysCall

Erzeugt mit `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
