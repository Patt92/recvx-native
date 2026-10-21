# Event-Lücken — Prioritätsstufe (Common_controll 8 / 41) — 2026-10-11

Umfang dieser Sitzung: zwei bestätigte, noch ungebundene Common_controll-Zweige
geschlossen (`native/world.cpp`, `native/world.h`). Kein Originalkern, keine
Raumsonderregel, kein Ersatzlogik-Zweck, nichts committet.

## Ausgangslage (Baum hat sich unter der Sitzung bewegt)

Der Plan `polymorphic-scribbling-corbato.md` war beim Einstieg teilweise bereits
anderweitig gelandet: Commit `6da8fd2` („native enemies, live effect pool, audio
sample loops and event map“) enthält **0xa0 bhMapSystemOn (MP_SET_EVENT)**,
**0x49 bhSetGunFire** und die C05-/C07-Gegnerarbeit. 62 CTests grün zum Stand des
Commits. Damit sind Schritt 4 (0xa0) und der 0x49-Teil von Schritt 6 erledigt.

Vier Sitzungen arbeiten gleichzeitig an diesem Baum (`Audio: Waffen-SE-Vertrag`,
`Gegner port`, `bhFaceRep Morph-Pfad via 0x9a`,
`Greifer-Motionbibliothek: einheitlicher Besitzer`). Die letzten beiden beanspruchen
genau die Verträge, die Schritt 3 (0x9a, Zombie-Bit) und den cmd-41-Verbraucher
(einheitlicher Gegner-Besitzer) öffnen. Diese Sitzung hat deshalb **nur** die
sauberen, niemandem zugeordneten pending-Zeilen in `world.cpp`/`world.h`
geschlossen und die übrigen Schritte als offene Verträge stehen lassen.

## Geändert

### Common_controll 8 (0x69/8) — `event.c:12099`
Original schreibt `work->mdl->objP[model_cno].pos` mit Vorzeichen aus
`bhCetask->bpx/bpy/bpz`. Gebunden wie die Nachbarn 36/38 (Position auf
`node_overrides[physical]`, danach `actor_nodes_.clear()`). Unterschied zu 38:
Knoten ist der per `bhLoadWork` gefangene Task-Knoten (`ctx.node`, Host: Befehl
0x65), nicht ein Operand; Vorzeichen kommen aus `ctx.pos_sign` (Host: Befehl 12),
nicht aus einem Byte. Parser (`events.cpp:287`, Länge 8) unverändert.

### Common_controll 41 (0x69/41) — `event.c:10346`
Bindung gab vorher ausdrücklich `false` zurück. Setzt jetzt das neue Feld
`WorldEntity::model_link` (analog `combat_enabled` für Bit 0x20 / Befehl 46
direkt darüber) nach dem u16-Operanden: ≠0 → gesetzt, 0 → gelöscht.

**Verbraucher — belegt, aber familien-/spielerspezifisch, kein generischer:**
Original-Leser von `work->flg & 0x40000` sind `player.c:1539`
(Fussfixierung über `PlyLegRoute`; nativ portiert in `player_stairs.c:918`, das
`state->flg & 0x40000` aus `flags[13][0]` liest) und die Gegnerfamilien
`en01.c:10498`, `en05.c:4877`, `en09.c:7574`, `en12.c:1601`, `en17.c:681`,
`en26.c:1270`. In `MdlPut.c`/`objitm.c` existiert **kein** generischer
Modelldraw-/Link-Leser (per grep belegt). Der Plan erlaubt genau diesen Fall:
„ohne belegten Leser nur Bit speichern und als Lücke melden (kein Raten)“. Der
gemeinsame Work-/Model-Link-Vertrag bleibt offen und ist die Domäne der
Greifer-Besitzer-Sitzung.

## Prüfung

- `python3 tools/check_no_original_core.py`: PASS (vor dem Build).
- Eigenes Build-Verzeichnis `build/eventgaps-claude-20261011` (arm64, Release,
  SDL2 aus dem System; FetchContent-Download in der Sandbox gesperrt). Build grün.
- 205-Raum-Scan (`tools/scan_event_rooms.py`, 600 Ticks, Spawn 0, ohne Eingaben):
  - Baseline vor der Änderung: `build/validation/event-gaps-20261011/scan-baseline-postcommit/`
    — 204/205 Räume erreichen den Report, 0 blockierte Befehle, **1160 pending-Zeilen**.
  - Nach der Änderung: `build/validation/event-gaps-20261011/scan-cmd8-41/`
    — 204/205, 0 Blocker, **1009 pending-Zeilen (−151)**.
  - Aufschlüsselung: `(0x69,8)` 13 → 0, `(0x69,41)` 138 → 0. **Keine neue
    pending-Zeile, kein neuer Raumfehler.**
- CTest: 61/62 grün. Ein rot: `native-gameplay-bindings`
  (`enemy gap changed the original HP`).

## Roter Test — nicht diese Änderung

`native-gameplay-bindings` schlägt in der Hunde-HP-Prüfung fehl
(`gameplay_bindings_test.cpp:76`). Die drei uncommitteten Dateien
(`en04.c`, `world_enemies.cpp`, `gameplay_bindings_test.cpp`) gehören der
Greifer-/Gegner-Sitzung: ihr Diff fügt direkt vor die Assertion ein zusätzliches
`world.tick()` ein und protokolliert HP-Übergänge 90 → 60 → 90 über einen Tick.
Das ist reiner en04-Gegnerkern; mein cmd-8/41-Zweig ändert weder Gegner-HP noch
`damage`/`state`. Der Test war vor dieser Sitzung bereits rot (Commit `6da8fd2`
meldet 62/62, die uncommitteten Änderungen danach nicht).

## RM_2010 — vorbestehend, anderer Besitzer

Der eine Raum ohne Report ist in **beiden** Scans identisch RM_2010
(Fledermäuse, kein Subplayer). Ursache ist die uncommittete EN07-Griffarbeit
(`enemy library motion unavailable`), dokumentiert in
`archived/work/native_port_C05_subplayer.result.md`; Besitzer ist die
Greifer-Motionbibliothek-Sitzung. Nicht diese Änderung.

## Grenzen / offen (nicht geraten)

- **Common_controll 42 (0x69/42):** in keinem Scan eine Raum-Skript-Nutzung
  (weder Baseline noch gespeicherte Scans). Plan: nur dokumentieren, kein Zweig.
- **0x8e bhZombieUpDieCk** (15 Räume), **0x4f bhCyoutenHenkeiSet** (14 Räume):
  geparst (`events.cpp:192`/`:98`), aber kein Bindungszweig. 0x8e braucht den
  Zombie-Zustandsbit `exp1 & 2` (Domäne Gegnerkern), 0x4f den Morph-Feldvertrag
  (Domäne bhFaceRep/0x9a). Bewusst nicht in dieser Sitzung angefasst.
- **0x89 bhPlayerChangeSet:** VM setzt `change_player` + `cb_flg 0x80`, nativer
  Leser steht aus (G02, Player-Lifecycle).
- **Player_controll 0x83** (RM_7010): geparst, ungebunden.
- Eingabefreier Scan ist eine Untergrenze, kein Durchspielnachweis.
- cmd 41 speichert das Bit exakt; der Familien-Verbraucher ist offen, das Bit
  wirkt daher noch nicht auf Darstellung/Motion.
- Windows nicht gebaut/ausgeführt.
