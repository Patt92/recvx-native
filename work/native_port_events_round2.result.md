# Events Runde 2: Wesker-Boden, Spielerwechsel Steve, RM_3180-Aufzug (2026-10-14)

Agent `events-player`. Arbeitsbaum + `./build.sh --test` (Dev-App aktualisiert), nichts
committet, kein Paket ersetzt. Belege: `build/validation/events_round2-20261014/` (README dort).
Alle Startpositionen, Debug-HP, Teleports und Snapshots sind Diagnoseaufstellungen.

## 1. Claire „im Boden“ nach dem Wesker-Event (BEHOBEN)

**Zuordnung.** Die Spielerlage nach der Szene setzt nur RM_1002 ev0 @930
`7d 00 01 00` (bhMotionPosSetEnePly, Gegner 0, grand[2]). RM_1100 Event 2 ist das
Blitz-Event der Schlosstreppe und positioniert den Spieler nicht. Beide Räume wurden
im Ablauf gespielt.

**Original** `event.c:5844-5888`: px/pz aus `owP->mtx[12/14]`, `py = grand[v1+1]`,
**`exp0[18] = pxb = px` (:5868), `flr_no = bhCheckFloorNum(py)` (:5873),
`exp0[20] = pzb = pz` (:5877)**, ay, bhCalcModel.

**Nativ vorher** (`native/world.cpp` 0x7d): nur Lage und Winkel. EXP_WORK.spx/spz
(der Host führt sie als `collision_safe_`, vgl. Common 7 an derselben Stelle) blieben auf
der Lage vor dem Event `(120, 33.49)`, `flr_no` blieb 1. Sobald die Steuerung zurückkam,
zog der Settle-Schritt (pl_evt.c:132-145) Claire in drei Bildern von `(130.3, 0, 93.1)`
nach `(121.0, 0, 69.5)`, also Richtung Torbereich mit Bodenebene 1 (y 8), während sie auf
y 0 blieb. Im Bild steckt sie dort hinter bzw. in der Bodenkante.

**Änderung** `native/world.cpp` 0x7d: `collision_safe_ = Lage; collision_floor_ = floor_at(py)`
(floor_at entspricht pwksub.c:112 bhCheckFloorNum). Fundstelle steht im Kommentar.

**Prüfung** `rm1002/` (Replay ns.txt, natürlicher Ablauf ohne Skip, ~3180 Bilder):
vorher (Binary 18:26) `3183 pos=130.33,0,93.06 floor=1 safe=120,33.49`, dann 127.2/72.5,
124.1/69.5, 121.0/69.5. Nachher `3183 pos=130.33,0,93.06 floor=0 safe=130.33,93.06`, danach
keine Bewegung mehr. Bilder: before_t3186_dragged.png, after_t3190.png (Füße sichtbar auf
dem Hofboden). Der SELECT-Skip-Pfad (sk.txt) endet unverändert bei (127.86, 0, 94.89), floor 0.

**RM_1100** (`rm1100/`, Diagnoseposition vor etc[1], OK): scd1 @932 startet ev0-3,
die Höhe folgt der Treppe (y 0→64, flr 0→6, a4-Stufenpfad), Blitz-Event ev2 läuft durch, DoorCall
`33 00 80 00 02 00 00 19` lädt RM_2000 bei y 0, flr 0. Kein Einfrieren, kein Höhenfehler.

## 2. Spielerwechsel Steve am Laufzeitpfad (geprüft; zwei Lücken behoben)

**Auslöser gefunden.** RM_3100 scd1 @1168: ev_flg 216 gelöscht und Bodenfeld 17
(flr_idx 0x11, x 40-48, z 51-56) → ev3/0/1/2/4/5/6/7/23/24/25. ev5 @8964 `89 02` ist der
natürliche Wechsel; ev27/28 (`89 02` + DoorCall) sind nur die SELECT-Skip-Zweige. Die Szene
enthält ein GetItem (ev3 @5896-5914: cb_flg 0x10/0x4000, Item 33). Ohne OK-Taste wartet das
Spiel dort. Das ist korrekt und kein Hänger.

**Durchgespielt** (`steve/s2.txt`, `--enemy-ai on`): RM_3100 → (Tick 3653) RM_3180 → (4141)
RM_3160. Danach `player 2`, SYSTEM 0012 (Steve-Modell, Bild steve_rm3160.png), `weapon 8`
(Bank 2: Item 34, 400 Schuss, pip[0]=2), `hp 160`. Claires Bank 0 bleibt
(Messer, Item 33/0, Feuerzeug).

**Retry-Punkt / Rückwechsel** (`h1.txt`, `b3.txt`): Claire auf HP 97 gesetzt (Debug), Wechsel,
Snapshot in RM_3160 (Snapshot = dieselbe capture_snapshot wie der `player_change_push`-Retry):
`ply_id 2, ply_hp (97,160,160,160), ply_wno (0,0,8,0)`. Snapshot laden, Teleport RM_3210
(Diagnose), Position 75/37/10: scd1 @244 startet ev4, `89 00` → RM_3220 als Claire,
`player 0, SYSTEM 0010, hp 97, weapon 0`, Bank 0 unverändert (claire_rm3220.png).

**Lücke A behoben: bhPushGameData/bhPopGameData verloren die gespeicherten anderen
Spieler.** Original `room.c:1033/1043` kopiert den ganzen SYS-Bereich, also auch
`ply_hp/ply_wno/ply_stflg` aller vier Spieler, und überschreibt nur den aktiven Eintrag
(:1022/1030/1031). `native/snapshot.cpp` schrieb nur den aktiven Eintrag und
stellte die anderen als 0 wieder her. Gemessen mit dem gelinkten Prüfling
`change_retry.cpp`: vorher `Retry-Punkt ply_hp={0,0,160,0}` → Rückwechsel
`Claire hp=0 poisoned=0`. Nachher `{97,160,160,160}` → `hp=97 poisoned=1`. Ein Retry
als Steve hätte Claire also mit 0 HP und ohne Vergiftung zurückgebracht. Die Änderung:
capture kopiert die Arrays (inkl. `partner_weapon` = sys->ply_wno), restore stellt auch `ply_wno` wieder her.
Test: `tests/native/snapshot_test.cpp` (2 neue Prüfungen).

**Lücke B behoben: Snapshot-Laden wies jeden Raum mit rom_no ≥ 10 ab.** `runtime.cpp
load_snapshot` verglich den Raumnamen mit `"RM_%X%02X"`. Die Namen sind dezimal
(ps2_dvd_image.c:5-9, `RoomIdentity::name`). Vorher: `Fehler: Snapshot-Raum passt nicht zu
stg_no/rom_no` für RM_3160 (before_snapshot_load_rm3160_rejected.json, betrifft auch RM_1100
usw.). Nachher wird der Präfix aus `RoomIdentity{stg,rom,0}.name()` gebildet und der Snapshot geladen.
Host-Funktion (F9/Debug); der Schreibmaschinen-Ladepfad hat diese Prüfung nicht.

`player_change_push` in runtime.cpp wurde nicht verändert.

## 3. RM_3180-Aufzug (Erstrunden-Fix war unvollständig, jetzt behoben)

Im echten Eventablauf (Tick 3653-4141 aus s2.txt) zeigte die Spur: owP[0] von Steve/Claire
fuhr mit dem Link von y 9.9 auf 0 (Erstrunden-Fix in `calc_enemy_model`), die **gezeichneten
Körper blieben aber konstant bei y −0.17..16.8**. Claires Waffe (obj 12, verlinkt an ihren
Handknoten über owP) lag bei y 17.6..20.0 über ihrem Kopf. Ursache: `entity_geometry`
zeichnete Gegner mit `e.scripted` (Subplayer nach Common 24/34) über den Clip-Pfad an
`e.transform`, ohne Link. Im Original zeichnet bhPutModel immer `mlwP->owP` aus bhCalcModel
(MdlPut.c:248-256, Wurzel aus `lkwkp->owP[lkono]` bei flg 0x80; event.c:3696).

**Änderung** `native/world.cpp entity_geometry`: Subplayer-Works (`subplayer_bound`,
`enemy_native`, `enemy_calculated`) werden aus ihrem owP-Baum gezeichnet, wie die übrigen
nativen Gegner. Ihr Baum wird ohnehin jedes Bild vom nativen Gegnerpass gerechnet
(w.frm_no = e.frame im Lauf geprüft).

**Prüfung** `elevator/`: nachher Körper 9.8..26.8 → −0.1..16.9 synchron mit owP, Waffe in
Handhöhe (Claire 10.1..27.4 / Waffe 17.6..20.0 bei Tick 3653). Bilder: after_t3690/3720
(Draufsicht, beide Figuren auf der Plattform, größer/näher als vorher), after_t3745/3850/4050
(unten angekommen, Füße am Boden). Dieselbe Linkbindung gilt in RM_3100 (Figuren an obj 6,
gleiches Modell 325): ab Tick ~3300 fahren sie dort mit dem Lift abwärts (rm3100_before_after_*.png).
In der Szene vor der Fahrt (t2300) sind Vorher- und Nachher-Bild identisch.
Grenze: mit `--enemy-ai off` (Host-Debug, Gegnerpass angehalten) bleibt der zuletzt berechnete
Baum stehen (Datei enemy_ai_off). Das entspricht einem angehaltenen Pass.

## Prüfungen
- `check_no_original_core.py` PASS vor jedem Build. Build 1 und Build 2 `./build.sh --test`:
  **91/91 grün**.
- Mit Build 2 grün: `verify_events.py`, `verify_event_state.py`, `verify_snapshot.py`,
  `verify_event_freeze.py`.
- Binärstände: „vorher“ RM_1002 = Binary 18:26 (vor meinem 0x7d-Edit). Andere Agenten bauten
  zwischendurch mit meinem Edit. „Vorher“ Aufzug = Binary mit 0x7d/Snapshot-Fixes, aber ohne
  Zeichenpfad-Änderung. Snapshot vorher/nachher = gelinkter Prüfling mit alter/neuer snapshot.cpp.

## Offen
- Kein echter Game-Over-Retry als Steve gespielt. Retry und Snapshot nutzen dieselbe
  capture/restore-Funktion; belegt sind Prüfling und Snapshot-Laufzeitkette.
- `plp->wpnr_no = sys->ply_wno[new]` (system.c:1787) wird nativ aus der pip-Bank abgeleitet. In allen
  gesehenen Fällen stimmt das überein (Steve 8). Abweichende Stände wurden nicht geprüft.
- plp->stflg-Bits 0x38000000 überleben nativ keinen Raumwechsel (flags[14] je EventVm neu),
  das betrifft auch den Wechsel. Nicht untersucht, kein Leser gefunden.
- Nach der Rückkehr lief der Ablauf von RM_3220 in RM_3120 weiter. Nicht weiter geprüft.
- Beobachtung (Host, nicht mein Bereich): headless mit `--debug-config` im selben
  Ordner wie die Replay-/State-Dateien endete beim Öffnen des Debugmenüs mit rc 134. Mit
  debug.ini in eigenem Ordner rc 0. Ursache nicht geklärt.

## Geänderte Dateien
- `native/world.cpp` (0x7d spx/spz/flr_no; entity_geometry Subplayer aus owP)
- `native/snapshot.cpp` (ply_hp/ply_wno/ply_stflg aller Spieler push/pop)
- `native/runtime.cpp` (load_snapshot: dezimaler Raumpräfix)
- `tests/native/snapshot_test.cpp` (2 Prüfungen)

## Ledger-Vorschläge (TAB)
```
native/world.cpp:RoomWorld::bind 0x7d bhMotionPosSetEnePly	done	event.c:5844-5888 inkl. exp0[18]/[20]=spx/spz, flr_no=bhCheckFloorNum (:5868/:5873/:5877)
native/snapshot.cpp:capture_snapshot/restore_inventory	done	room.c:1016-1043 bhPushGameData/bhPopGameData kopieren ply_hp/ply_wno/ply_stflg[4]
native/world.cpp:RoomWorld::entity_geometry subplayer	done	MdlPut.c:248-256 bhCalcModel/bhPutModel zeichnet owP, Link-Wurzel lkwkp (event.c:3696)
```
