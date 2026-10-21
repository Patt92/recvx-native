# 06 — EN07-SFX, Effekte und Spielerinteraktion

## Auftrag

Portiere die im tatsächlich erreichten EN07-Ablauf verwendeten Audio-, Effekt-
und Spielerreaktionen mit Originalsemantik. Dazu gehören mindestens
`bhEne_CallSE`, `bhEne_CallPlayerVoice`, `bhEne_SetWeponAtr`,
`bhEne07_PlayerBloodEffect`, `bhEne07_PlayerControl`, Schatten und die über
die ausgeführten Modi erreichten Effektfunktionen.

## Referenzen

- `src/ps2/veronica/prog/en07.c:818-1127`, `2157-2257`.
- `zonzon*.c`, `effect.c`, `sdfunc.c`, Spielerzustandsroutinen.

## Abnahme

Der echte EN07-Runtimeablauf löst mindestens die zugehörigen Original-SFX über
die native Audiomischung aus; ein Angriff/Interaktionszweig überträgt die
Originalzustandsänderung an den gemeinsamen Spieler-Work. Effekt-/Audiogaps
dürfen nicht als Erfolg geloggt und mit Nullrückgabe fortgesetzt werden.
