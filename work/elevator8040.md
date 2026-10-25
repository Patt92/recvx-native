# RM_7130 -> RM_8040: Aufzug, Chris danach nicht steuerbar (Arbeitsbaum behoben)

Repro: Nutzersnapshot 1 (2026-10-25 19:50, RM_7130), Kopie in
`build/validation/elevator8040/s/`; `e0.txt` (Position 21/0/20, OK am Aufzug).

Ursache: RM_8040 Event 3 loescht ev_flg 36 und ruft DoorCall `08 04 01 21`.
Das Original setzt sys->stg_no/rom_no/pos_no schon in bhStartDoorDemo Fall 0
(room.c:987-989) auf das Ziel; scd1 laeuft jeden Takt (bhControlEvent) und sieht
danach pos_no 1. Nativ wurde values[0]/[1]/[15] erst beim Raumladen gesetzt:
scd1 sah weiter pos_no 3, startete die Fahrt (Event 0/1/3) neu und setzte
ev_flg 36 wieder. Am Ziel hielt scd0 0x24e Chris deshalb per bhLoadWork +
Common 25 (mode0 7) fest, ohne dass ein Skript ihn freigibt.

Fix: `native/runtime.cpp` door_demo_start setzt values[0]/[1]/[15] aus der
Tueranforderung wie room.c:987-989.
Belege (App-Binary, --enemy-ai on): RM_7130 -> RM_8040 -> RM_8040, danach
player_scripted false, mode [1,0,0,1], keine Tasks; `keys forward` bewegt
Chris (25.5 -> 48.5). Hunter-Pfad RM_7110/7160 (ohne Skip, Skip 280/500/Kette)
unveraendert RM_7110 frei; Titelstart ohne --in-game ok. CTest 246/247,
rot nur development_app_packaging (build_native_test.py erwartet `--opengl off`
statt `--d3d12 on`; Build-Skript, unabhaengig von dieser Aenderung).
Fenster: Nutzer bestaetigt 2026-10-25 (Fahrt, danach steuerbar). Nicht geprueft: Windows, uebrige Raeume mit scd1-Vergleichen auf
stg/rom/pos waehrend der Tuerausblende. Kein Commit.
