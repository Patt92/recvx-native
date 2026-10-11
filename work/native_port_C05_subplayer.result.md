# C05 Subplayer — Validierung des angewendeten Anschlusspatches (2026-10-11)

Umfang: nur Prüfung des bereits im Arbeitsbaum liegenden Stands
(`work/native_port_C05_subplayer.result.patch` ist angewendet: `e.subplayer` →
`e.enemy`, Dispatch aller Subplayer-IDs auf `recvx_subplayer_enemy`, zweite
Tickwelt in `RoomWorld::tick` entfernt). Kein Code geändert, nichts committet.

## Prüfung

- `python3 tools/check_no_original_core.py`: PASS (vor dem Build).
- Eigenes Build-Verzeichnis `build/events-claude-20261011` (arm64, Release,
  SDL2 aus dem System; der FetchContent-Download ist im Sandbox-Netz gesperrt).
- CTest gezielt, 5/5 bestanden: `enemy_work_contract`, `original_map_screen`,
  `subplayer_apparatus`, `native-gameplay-bindings`, `script_motion`.
- 205-Raum-Scan (600 Ticks, Spawn 0, ohne Eingaben, `tools/scan_event_rooms.py`),
  Ausgabe `build/validation/event-gaps-20261011/scan-c05-baseline.json/`.

## Ergebnis des Scans gegen `build/validation/room-effects-20261011/scan.json`

- 204/205 Räume erreichen den Report, 0 blockierte Befehle, 130 Räume mit
  pending_commands (unverändert).
- Zwei Zeilen weniger pending: `0x29` (RM_9321) und `0x67` (RM_9301) —
  Herkunft nicht untersucht; sie sind kein Beleg einer Wirkung.
- **1 Raum fällt aus: RM_2010** (Vormittags-Scan: Exit 0). Fehler
  `enemy library motion unavailable` ab Tick 265 (Bisektion 264 ok / 265 Fehler).

## Ursache RM_2010 (nicht C05)

RM_2010 enthält fünf Fledermäuse (Identität 7), keinen Subplayer. Die neue, noch
uncommittete EN07-Arbeit (`native/enemies/en07.c`, `en07_player_control`) setzt
beim Griff `pl.mtn_enemy = 1` mit `mtn_enemy_owner = sv->enemy_now()`. Der Host
übernimmt `mtn_enemy` als `player_enemy_motion_`
(`native/world_enemies.cpp:79`) und löst die Spieler-Griffmotion dann in
`RoomWorld::sample_player` und `player_joints_now` (`native/world.cpp:844,974`)
über `enemy_library_clip(1, motion_)` auf. Die Bibliothek ist dort **fest die
des Zombies (Identität 1)** — diese Festlegung steht schon im HEAD
(`git show HEAD:native/world.cpp`, Zeilen 774/870). Ein Raum ohne Zombie hat
`enemy_motions_[1]` nicht (Diagnose: `id=1 index=32 entries=0`) → Ausnahme.
Mit dem Vormittags-Binary läuft derselbe Raum 300 Ticks ohne Fehler.

Original: `plp->mnwP = epw->mnwP` — die Bibliothek gehört dem greifenden Gegner.
Der Host kennt dafür `player_enemy_owner_`, aber die Besitzer schreiben es
**uneinheitlich**: `en04.c:771` den Identitätswert (`w->id`), `en07.c:2457`
`sv->enemy_now()`, `en12.c:769`/`en14.c:764` `w->idx_ct`. Ein Fix in `world.cpp`
(Bibliothek des Besitzers statt fester Identität 1) setzt daher zuerst eine
einheitliche Bedeutung von `mtn_enemy_owner` voraus. Das ist eine
Vertragsfrage der Gegnerpakete (C04/C07/…), kein Eingriff in dieser Arbeit; es
wurde bewusst nichts geraten.

## Grenzen

- Kein Beleg, dass die C05-Semantikabweichungen aus dem T5-Bericht (P7) behoben
  sind — sie sind es nicht; sie werden in Schritt 2 des Event-Plans geschlossen
  (Release-Bits, ct2-Lowbyte, 0x93 ohne ct2-Reset, Hokan-Zweig).
- Eingabefreier Scan ist eine Untergrenze, kein Durchspielnachweis.
- Windows nicht gebaut/ausgeführt.
