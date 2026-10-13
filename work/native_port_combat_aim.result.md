# Nutzermeldungen 2026-10-12: Inventarpause und Treffer/Aufschaltung

Auftrag des Nutzers (zwei Meldungen, zusammen bearbeitet):

1. „wenn das inventar offen ist, laufen gegner einfach weiter, normal freezt der
   rest des games“
2. „schießen trifft keine gegner, auch kein autoaim“

Bearbeitet in `native/runtime.cpp`, `native/combat.cpp/.h`,
`native/world.h`, `native/player_combat_core.c/.h`; Nachweise in
`tools/verify_combat.py` und `tests/native/replays/combat-aim-enemy.txt`.
Kein Originalkörper, kein Generator, kein zweiter Workbesitzer. Kein
Raum-Sonderfall, keine Ersatzlogik.

## 1. Inventarpause: der Statusbildschirm stoppt Task 7

Originalbelege:

* `main.c:69` – Task 7 ist `bhSysCallGame`; `njUserMain` ruft eine Task nur,
  wenn `(sys->tk_flg & (1<<i)) && !(sys->ts_flg & (1<<i))`.
* `system.c:539` `bhSysCallGame` ruft `bhCheckSubTask()` und danach
  `bhMainSequence()`.
* `game.c:20` `bhMainSequence` enthält `bhControlEnemy`, `bhControlPlayer`,
  `bhControlEffect`, `bhControlObjItm`, `bhControlCamera`, `bhControlLight`
  und die Drawphase; `game.c:312` darin `bhControlEvent` nur bei `loop_ct > 1`.
* Statusbildschirm: `system.c:600-612` (`bhCheckSubTask`, Zweig
  `cb_flg & 0x64010`) setzt `AllStopEnemySe()`, `st_flg |= 0x8`, `mn_mode0 = 5`,
  `sp_flg = 0x30`; sub1.c:3038 setzt `sys->ts_flg |= 0x80` – genau das Bit, das
  Task 7 stoppt. `bhSysCallItemselect` (Task 9, system.c:905) führt
  `ItemTaskCheck`/`StatusMain` aus.
* Task 8 (`bhSysCallEvent`, system.c:659/734 `bhControlEvent`) und der
  Soundmonitor laufen weiter.

Host vorher: `world->tick()` (Gegner-, Effekt-, ObjItm-, Spielerpass) lief auch
bei offenem Inventar, ebenso `follow()`, `world->tick_camera()`, `locomote()`,
`head_hair_step()` und `third_person_tick()`.

Host jetzt: alle diese Schritte hängen an `!menu.active` (Task-7-Ersatz).
`events->tick()`, `box_menu.synchronize()`, `gameplay_input.synchronize()`,
`audio.mix()`, `movie.tick()` und `vibration.tick()` laufen unverändert weiter
(Task 8 bzw. Soundmonitor).

## 2. Treffer und Aufschaltung: die Gegnertabelle war nicht gebunden

Originalbelege (alle drei lesen `sys->ene`/`sys->ewk_n`):

* `pwksub.c:218` `bhSearchNearEnemy` (`cb_search_near_enemy`) – Aufschaltung des
  normalen Waffenpasses (`player.c:4569` `bhCPM2_act_suw` Fall 0).
* `playpch.c:318` `bhSearchEnemy` / `:401` `SetLockOnDirection`.
* `weapon.c:473` `bhCheckGunAtari` – Trefferzuweisung.

Der Dienst `RECVX_COMBAT_SERVICES::enemies` war in
`native/combat.cpp` (CombatOriginal) **nie belegt**. `cb_enemies()` meldete
deshalb `sys->ene / sys->ewk_n (Gegnertabelle)` und lieferte NULL; damit fanden
weder die Zielsuche noch die Trefferzuweisung einen Gegner.

Neu: `RoomWorld::enemy_table()` (native/world.h) gibt genau die eine Tabelle des
Raumbesitzers heraus (nur Zeiger auf die vorhandenen Entities), und
`CombatController` bindet sie als Dienst. Kein zweiter Satz, keine Kopie.

Messung (RM_0080, M93R aus dem Steve-Ereignis, Diagnose-Aufstellung 5 Einheiten
vor Zombie 3, Blickrichtung 8192 = 45° neben dem Ziel; Kontrolllauf = derselbe
Replay ohne `attack`):

| | ohne Bindung | mit Bindung |
|---|---|---|
| Aufschaltung | nie (`bhSearchNearEnemy` liest die Tabelle nicht) | `near_found >= 1` |
| Spielerdrehung | 8192 (kein Dreh) | 8192 → 1968 (dreht aufs Ziel) |
| Schüsse / Treffer | 3 / 0 | 6 / 4 |
| Gegner-HP (Summe) | 570 (unverändert) | 378 |

`tools/verify_combat.py --runtime … --data data` prüft das jetzt (Tabellen,
Komponententest und beide Runtime-Stufen):
`M93R -> RM_0080 -> Aufschaltung (yaw 8192 -> 1968) -> 6 Schuesse, 4 Treffer,
Gegner-HP 378 statt 570 ohne Schuss.`

`--state` führt unter `combat_aim` die Zähler der Zielsuche
(`near_calls/near_seen/near_filter/near_cone/near_wall/near_found`,
`gun_seen/gun_filter`) sowie die Geometrie der bis zu drei zuletzt von
`bhCheckC2WallN` verworfenen Sonden. Sie werden über die Lebensdauer der
Kampfpässe aufsummiert, damit sie auch nach dem Wegstecken lesbar sind.

## Was der Diagnoseweg ausgeschlossen hat

Zwei Zwischenmessungen waren unbrauchbar und dürfen nicht als Ergebnis gelesen
werden; beide sind mit Ursache festgehalten:

* Eine Aufstellung 10 Einheiten vor Zombie 3 (`position … 40.89`) setzte den
  Spieler **in** ein ATR-Volumen (`type 0`, `pos (-40.5,0,39)`,
  `ext (31,25,2)`): der Sichtstrahl begann darin und galt deshalb zu Recht als
  verdeckt (`t = 0`). Auf derselben Seite der Wand bei `z = 36` ist die Sicht
  frei.
* Am Spawn (z ≈ 49,5) liegt dieselbe Wand zwischen Spieler und Gegner; dort ist
  „keine Aufschaltung“ ebenfalls korrekt.

Damit ist auch belegt: `bhCheckC2WallN` (Host: Strahl durch den
Kollisionssatz mit den Originalfiltern) verwirft nur, wenn wirklich eine Wand
dazwischen liegt.

## Offen

* Keine Aussage über Waffen ohne `WpnTab[w].flg & 0x800`: der `_pch`-Zweig
  (`bhCPM0_action` → `bhSearchEnemy`/`SetLockOnDirection`, `pp.lck_tbl`) läuft
  erst mit diesem Flag; für den M93R (flg `0x10004086`) bleibt
  `combat_target_right` deshalb −1, die Aufschaltung läuft über `plp->src_no`.
* Reihenfolge im Hostframe: `combat.tick()` (Spielerpass) läuft weiterhin vor
  `world->tick()` (Gegnerpass); das Original läuft in `bhMainSequence` erst
  `bhControlEnemy`, dann `bhControlPlayer`. Der Kampf liest die Gegnertabelle
  damit um einen Frame versetzt. Nicht angefasst (Umbau des Eingabepfads).
* Ungebunden bleiben die übrigen Dienste des Kerns (Wassereinschlag,
  Wandzerstörung, Funken/Spritzer, `sys->pt_flg`, Kamerafeld, Vibration ohne
  Treiber); sie melden sich weiter namentlich.
* **Die Pause ist noch nicht messbar nachgewiesen.** Gemessener Zwischenstand:
  in den geprueften Szenarien (Direktstart `--room RM_0080.RDX` und
  Steve-Ereignis -> Raumwechsel RM_0080, Inventar ab Bild 60 bzw. 3850) steht
  der Gegnerpass **schon vor dem Oeffnen** still – die Zaehler `ct0` der fuenf
  Zombies bleiben zwischen Bild 3862 und 4250 in *beiden* Staenden konstant,
  ebenso die Positionen. Damit ist der Gegnerpass in diesen Raeumen nicht das
  richtige Thermometer fuer die Pause; gebraucht wird ein Lauf, in dem die KI
  nachweislich laeuft (z. B. der `natural`-Lauf aus `tools/verify_enemy_ai.py`,
  in dem ein Zombie von selbst greift) plus Inventar-Taste.
* Verdacht, nicht belegt: `events->flags[6][0]` (= `sys->sp_flg`) ist im Host
  eine **dauerhafte** Flagbank. `message.c`/`event.c` loeschen dort bei einem
  Dialog `& ~7` (Host: `events.cpp:158`), im Original rechnet `game.c:196-264`
  die Maske jeden Frame neu. Bleibt Bit 0x2 danach aus, ruht `bhControlEnemy`
  fuer den Rest der Sitzung. Das wuerde erklaeren, warum in den obigen
  Szenarien auch ohne Inventar keine Gegner laufen. Nicht angefasst, nicht
  belegt – gehoert als eigene Aufgabe geprueft.

## Prüfstand

`build/combat-aim-claude-20261012` (Release, eigener Ordner; die parallele
Sitzung baut in anderen Ordnern):

* `tools/check_no_original_core.py`: PASS („audited source/build/binary scope has
  no forbidden core; not a gameplay acceptance“).
* `tools/verify_combat.py --runtime … --data data`: Tabellen, Komponententest und
  beide Runtime-Stufen bestanden.
* CTest 78/79. Rot: `script_motion` („drag leaves the original motion's position
  range“) – **gemessener Vorherstand**: grün im Build `build/c02-claude`
  (22:35), rot im Build `build/enemy-ai-claude-20261011` (23:02). Der Ausfall
  stammt damit aus den Änderungen der parallelen Fog-/Licht-Sitzung an
  `motion_set.cpp`/`actor.cpp` (22:59), nicht aus dieser Arbeit (meine Änderungen
  sind ab 23:10).
* Logs/Rohdaten: `build/validation/inv-freeze-combat-20261012`
  (u. a. `c-after.json`, `c-nobind.json`, `mv62/mv400-*.json`, `measure.sh`).

Kein Paket-/Starterlauf, kein Commit, kein Push.
