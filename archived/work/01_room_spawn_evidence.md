# 01 — Fledermausraum und Original-Spawnablauf belegen

## Auftrag

Bestimme aus den echten RDT-Daten den ersten reproduzierbaren EN07-Ablauf. Die
bisherige Bestandsaufnahme nennt `RM_0130`, `RM_1030`, `RM_2010` und `RM_4010`
mit ID 7; sie ist nur ein Startpunkt. Belege für einen gewählten Raum die
genaue Gegnerliste, `wrk_no`, Typ, Starttransform, die zuständigen Eventdaten
und den Zeitpunkt, zu dem die normale Raum-/Eventkette ihn aktiviert.

## Originalreferenzen

- `src/ps2/veronica/prog/room.c` — RDT-Übernahme und Enemy-Work-Erzeugung.
- `src/ps2/veronica/prog/eneset.c` — `bhSetEnemy` und `bhControlEnemy`.
- `src/ps2/veronica/prog/event.c` — gegebenenfalls aktivierende Opcodes.
- Die gewählte `data/rooms/<ROOM>.RDT`.

## Ergebnisartefakt

Lege `archived/work/01_room_spawn_evidence.result.md` an: Byte-/Tabellenoffsets,
dekodierte Werte, Originalfunktion/Aufrufreihenfolge, erwartete Frames und
welche Eventflags den Ablauf beeinflussen. Ein kleines lesendes Diagnosewerkzeug
ist erlaubt, aber keine Raumdaten oder Runtime-Sonderregel.

## Abnahme

Ein anderer Agent kann allein mit dem Bericht denselben Raum starten und
belegen, dass EN07 aus der normalen Raumdaten-/Eventkette entsteht. Keine
synthetische Gegnerinstanz und kein direkter EN07-Dispatch als Beweis.
