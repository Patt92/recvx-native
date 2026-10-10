# 06 — Ergebnis: EN07-SFX, Effekte und Spielerinteraktion

## Portierter gemeinsamer Core

`native/core_enemy_interaction.c` und `.h` enthalten die nativen
Originaltranskriptionen für die im EN07-Pfad erreichten Interaktionen:

| Originalroutine | Quelle | Native Funktion |
| --- | --- | --- |
| `bhEne_CallSE` / `_EX` | `zonzon1.c:828/837` | `core_bh_ene_call_se` / `_ex` |
| `bhEne_CallEffectSE`, `bhEne_SetSEPan`, `bhEne_CheckPlayEffectSE` | `zonzon1.c:848-860` | entsprechende `core_bh_ene_*`-Funktionen |
| `bhEne_CallPlayerVoice` | `zonzon1.c:521` | `core_bh_ene_call_player_voice` |
| `bhEne_SetWeponAtr` / `bhEne_AttackHitCheck` | `zonzon1.c:591/608` | entsprechende `core_bh_ene_*`-Funktionen |
| `bhEne_SetBloodEffect4` | `zonzon1.c:175` | `core_bh_ene_set_blood_effect4` |
| `bhEne07_PlayerBloodEffect` / `PlayerControl` | `en07.c:2157/2172` | `core_bh_ene07_*` |

Die nativen Plattformblätter sind ausdrücklich Bindungen für
`RequestEnemySeBasic`, `ChechPlayEnemySe`, `CallPlayerVoice` und
`bhSetEffParticleMk2`; sie behalten die Originalargumente, Flag-Gates und
Reihenfolge bei. Die gemeinsame Runtime besitzt bereits entsprechende
AudioMixer-/Partikel-/Effektbesitzer, aber die produktive EN07-Work ist noch
nicht dieselbe Workquelle wie dieser Core.

## Geprüft

`recvx-core-enemy-interaction-tests` und CTest `core_enemy_interaction`
bestehen. Der Test prüft unter anderem die Soundnummern `0x2302`,
`sys->enow`, Fade/Flag, Voice-OR `0x400`, die Kapselgelenke, Blut-Farbtabelle,
den Effektwork und alle drei EN07-PlayerControl-Zweige einschließlich des
gepackten Mode-Write.

## Nicht als abgeschlossen ausgeben

`native/en07_bat.c` enthält im aktuellen gemeinsamen Arbeitsstand noch eigene
SFX-/Effekt-/Kollisions-Stubs und eine getrennte Workdarstellung. Diese Datei
ist **nicht** berechtigt, die oben portierten Core-Funktionen durch eigene
leere Funktionen zu ersetzen. Erst Aufgabe 02/03/07 darf die gemeinsamen
Works und diese Bindungen in den produktiven EN07-Lauf zusammenführen; Aufgabe
08 muss danach echte Runtime-SFX und Playerinteraktion belegen.
