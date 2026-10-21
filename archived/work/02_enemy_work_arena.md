# 02 — Gemeinsame native Enemy-Work-Arena

## Auftrag

Portiere die Semantik von `bhEne_CallocWork` aus
`src/ps2/veronica/prog/eneset.c:906` als gemeinsame native Arena für die
produktiven Enemy-Works. Sie muss bei Raumwechsel/Neuinitialisierung am
originalen Lebenszyklus zurückgesetzt werden und EN07s `exp0` (0x44) sowie
der gruppengeteilten `exp1`-Allokation (0x8) exakt bedienen.

## Nicht zulässig

- pro Gegner fest eingebaute EXP-Blöcke;
- separate Arena nur für EN07;
- Pointertrunkierung oder PS2-Emulation;
- stilles Ersetzen eines Arena-Fehlschlags durch beliebige Speicherbelegung.

## Semantik, die erhalten bleiben muss

`bhEne_CallocWork` prüft die Obergrenze, richtet den Cursor auf 64 Byte aus,
liefert den ausgerichteten Start, erhöht ihn um `size` und nullt genau diese
Bytes. Der zweite Parameter ist im Original unbenutzt. Die Host-Repräsentation
darf sichere native Zeiger verwenden, muss aber denselben beobachtbaren
Allokations-, Nullungs- und Resetablauf haben.

## Ergebnisartefakt und Abnahme

Lege `archived/work/02_enemy_work_arena.result.md` mit Quelldateien, Reset-Einstieg und
Testbeleg an. Der Test muss Ausrichtung, Reihenfolge, Nullung, volle Arena und
Raumreset prüfen. Er darf nicht nur eine lokale Hilfsfunktion testen: mindestens
eine produktive Work-Initialisierung muss denselben Arena-Owner verwenden.
