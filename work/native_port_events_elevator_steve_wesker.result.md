# Event-/Spielerfehler: RM_3180 Aufzug, Charakterwechsel, Wesker-Boden (2026-10-14)

Agent `events-player`. Arbeitsbaum, nicht installiert, kein Commit. Vorarbeit:
`work/native_port_event_parser_audit.result.md` als Stand gelesen.

## 1. RM_3180 Aufzug — Figuren an das Aufzugsobjekt gebunden (BEHOBEN)

**Original.** `event.c:5312 bhObjLinkSetObjEne` (Opcode `0x51`) setzt am Ziel-
Gegnerwork `e_ep->lkwkp = &sys->obwp[v0]`, `e_ep->lkono`, `e_ep->lox/loy/loz`
und `flg |= 0x80`. `MdlPut.c:248-256 bhCalcModel` baut die Wurzelmatrix dann
**nur** aus `lkwkp->mlwP->owP[lkono].mtx` (nicht aus `px/py/pz`).

In RM_3180 sind Steve und Claire zwei Subplayer-Works (`identity 91/93`); scd1
startet bei `spawn==1` die Events 0–9, und `ev1@1022 / ev2@1216` binden die
beiden Figuren mit `0x51` an das Objekt `obwp[4]` (Knoten 0). Das Objekt wird
von `ev5` (`65 02 04 00` bhLoadWork → `obwp[4]`) über die Splines
`Common 26/30` (`event.c:11876` bhGrdPosMove / Interpolation) gefahren; die
Figuren reiten über den Link mit.

**Nativ vorher.** Die Linkfamilie in `native/world.cpp:571` setzte für `0x51`
nur die `WorldEntity`-Felder `e.linked/parent_kind/index/node/link_offset`.
`calc_enemy_model` (`native/world_enemies.cpp:1502`) las aber ausschließlich
`w.flg & 0x80` + `w.lkwkp` (nur Gegner-Zeiger); für einen Objekt-Elternteil ist
`w.lkwkp` nie gesetzt. Ergebnis: `e.linked` eines Gegners war ein toter Wert,
die Figuren standen eingefroren auf ihrer Bindepose.

**Änderung.** `native/world_enemies.cpp calc_enemy_model`: neuer Zweig ganz
oben — bei `e.linked` wird `root` aus
`link_matrix(e.parent_kind,e.parent_index,e.parent_node)` +
`link_offset` + `njRotateXYZ(ax,ay,az)` gebildet (spiegelt `bhCalcModel` für
den Link; der Objekt-Elternteil hat keine eigene gespeicherte Gegner-`owP`).

**Prüfung (Diagnoseoverrides gekennzeichnet).**
`build/validation/events_elevator_steve_wesker-20261014/`. RM_3180, `--spawn 1`,
`RODRIGO_DBG=1 --validate-poses` (Diagnose), 200 Frames:
- `elevator_before_trace.log` (Vorfix-Binary): enemy0 `root.y ∈ {-0.0489,-0.0937}`
  — eingefroren nahe Bindepose.
- `elevator_after200_trace.log` (Nachfix): enemy0 `root.y ∈ {-0.0937,1.4711}`,
  enemy1 `∈ {0.0652,1.5714}` — die Figuren folgen jetzt dem verknüpften Objekt.
- `elevator_after.png` (aus `elevator_after.bmp`): beide Figuren sichtbar auf
  dem Boden der Aufzugsregion.

**Weiches Mitfahren.** Mit `--enemy-ai on` (= Produktionspfad) rechnet
`recvx_enemy_control` den Subplayer jeden Frame: 400 Frames → 810
`calc_enemy_model`-Aufrufe, 102 verschiedene Elternpositionen `y 10.0 → 0.0`
(`elevator_after_ai_trace.log`, `elevator_ride_ai.png`). Der steppige Eindruck
im headless-Standard entsteht nur, weil dort die Gegner-KI aus ist und der
Subplayer dann selten gerechnet wird; das ist kein Restfehler und braucht
keinen zusätzlichen Host-Tick.

## 2. Charakterwechsel Steve (BEHOBEN; Runtime-Trigger fortschrittsgebunden)

**Original.** `event.c:6264 bhPlayerChangeSet` (Opcode `0x89`) setzt
`sys->cng_pid = v0` und `sys->cb_flg |= 0x80`. Verbraucher ist der
Raumlader `system.c:1729-1776 bhFinishRoom`: bei `cb_flg & 0x80` den alten Work
unter `ply_id` retten (`ply_stflg/ply_wno/ply_hp`), `sys->ply_id = sys->cng_pid`,
SYSTEM-Eintrag `ply_id + 10 + costume*4` lesen, dann
`player.c:993-996 bhResetPlayer` (hp/stflg/wno des neuen Spielers).
Rückkehr zu Claire läuft symmetrisch (z. B. `RM_3210 ev18` `89 00`).

`RM_3100 ev27/28`: `89 02` (Steve, cng_pid 2) gefolgt von einem direkten
DoorCall `33 00 00 00 03 12 01 ff` nach RM_3180.

**Nativ vorher.** `native/events.cpp case 0x89` setzte nur
`change_player` + `flags[10][0] |= 0x80` — Kommentar „consumer pending“. Der
Raumlader wechselte nie; Spieler-ID `args.player`, `Inventory.player` und das
SYSTEM-Modell blieben die ganze Zeit Claire.

**Änderung.**
- `native/inventory.{h,cpp}`: neue Besitzer `ply_hp[4]` / `ply_stflg[4]`
  (AllItemInit-Defaults `system.c:483-489`) und
  `Inventory::switch_player(target)` (`system.c:1729-1776`): alten Zustand
  unter `player` retten, `player=target`, hp/stflg des Ziels setzen. Der
  Pip-Block und die abgeleitete Waffe folgen aus `slots()` (je `player*16`).
- `native/runtime.cpp load()`: vor dem Laden, bei `events->flags[10][0] & 0x80`,
  `inventory.switch_player(change_player)`, `args.player = 10 + target`,
  neues `PlayerAsset` (inkl. `mods/players/`-Override), `combat` neu,
  `action_player_set=false` (bhResetPlayer leitet `wpn_tp` neu ab).
- `native/snapshot.cpp restore_inventory`: `ply_hp/ply_stflg` mit dem Spielstand
  in Schritt halten (bhPopGameData).

**Prüfung.** `check_no_original_core.py` PASS; `./build.sh --test` **90/90 grün**.
Nachbarläufe grün: `tools/verify_events.py` (RC 0), `tools/verify_event_state.py`
(RC 0), `tools/verify_event_freeze.py` (RC 0, `--output`).
**Kein frischer Runtime-Trigger möglich:** alle `0x89`-Ereignisse sind
fortschritts-/türgebunden (RM_3100 ev27/28, RM_3210 ev18, RM_6100 scd1,
RM_9280 ev16, RM_5500 ev24 laufen mit spawn 0..3 nicht, geprüft). Der
Mechanismus ist über Original + Build + Tests belegt, nicht über einen
Durchspiellauf; das als Grenze benannt.

## 3. RM_1102 / RM_1002 Wesker-Boden (Stand geprüft, keine neue Ursache reproduziert)

Der vorhandene Fix (`bhMotionPosSetEnePly 0x7d`, Bericht
`work/native_port_event_parser_audit.result.md`) ist im Baum. Mit dem
Berichts-Replay (`build/validation/door1102-claude-20261014/sk.txt`,
`--spawn 0/1/2`) endet Claire in RM_1002 bei `(127.857, 0, 94.895)` (y nur
`{0,8}`), `POSITION_FRAME` ohne Versenkung. Der Nutzerort „RM_1102“ ist RM_1002
(stage 1 raum 00 variante 2, Palasttür-Hotspot 0); RM_1100 hat keine
RM_1102-RDT und seine Events 0–3 laufen mit spawn 0–3 nicht.
**Offen:** „Claire steckt kurz vor der Tür im Boden“ ließ sich in einem
erreichbaren Pfad nicht reproduzieren; ohne einen frisch reproduzierbaren
Zustand wird keine Logik erfunden. Zu prüfen wäre der RM_1100-Wesker-Pfad, der
nur fortschrittsgebunden startet.

## Geänderte Dateien
- `native/world_enemies.cpp` (calc_enemy_model: e.linked-Zweig)
- `native/runtime.cpp` (load(): cb_flg-0x80-Verbraucher)
- `native/inventory.h`, `native/inventory.cpp` (ply_hp/ply_stflg, switch_player)
- `native/snapshot.cpp` (restore_inventory: ply-Arrays)

## Ledger-Vorschläge (TAB)
```
native/world_enemies.cpp:RoomWorld::calc_enemy_model	done	event.c:5312 bhObjLinkSetObjEne; MdlPut.c:248-256 (e.linked Zweig)
native/runtime.cpp:load	done	system.c:1729-1776 bhFinishRoom cb_flg 0x80; event.c:6264 bhPlayerChangeSet
native/inventory.cpp:Inventory::switch_player	done	system.c:1729-1776; player.c:993-996 bhResetPlayer
```

## Roter Nachbar (nicht mein Bereich, ehrlich benannt)
- Erster Build: `effect_collision_debris`, `engine_progression_resources`,
  `enemy_en05_world` rot — fremde, unfertige Tests anderer Agenten
  (`effect_debris_test.cpp`, `progression_test.cpp`, `en05_world_test.cpp`).
- Der Build nach dem Entfernen des zeitweiligen Diagnose-Prints brach in
  `tests/native/effect_room_sprites_test.cpp:1755` ab:
  `use of undeclared identifier 'muzzle_weapon229'` — fremder, halbfertiger
  Effekt-Test. `recvx_game` und `recvx-runtime` wurden davor gebaut
  (`Built target recvx_game`), der Fix ist im Binary aktiv (smooth ride,
  s.o.); nur ctest laeuft dadurch nicht durch. Kein Bezug zu meinen Dateien.
- Der vorige Build (nur meine Aenderungen) war **90/90 grün**.
