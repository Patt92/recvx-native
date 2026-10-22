# 3rd-Person-Mod: Zielen und Schiessen behalten die Schulterkamera

Host-Kameramod (kein Originalverhalten, AGENTS.md "Bewusste Host-Abweichungen").
Nutzerbefund: beim Zielen springt die Sicht auf die fixierte Originalkamera.

## Ursache (belegt)

`native/runtime.cpp` bildete `ThirdPersonState.free` u. a. aus
`!(events->flags[5][0] & 4)` = `sys->st_flg 0x4`. Dieses Bit setzt der Kampfzweig
des Spielerpasses selbst:

- `player.c:4483` `bhCPM1_act_atk`: `sys->st_flg |= 0x4; plp->stflg |= 0x10000;`
  in jedem Durchlauf mit `plp->mode1 == 1` (Ziehen 64, Zielen 65..67,
  Wegstecken 68, Schuss 69/169, Nachladen 70, Messer 72).
- `player.c:1777` `bhCPM0_action`: Zieltaste (`pad_on 0x10`, `flg 0x20000`)
  setzt `flg 0x10000`, `stflg 0x400`, `mode1 = 1`, `mode2 = 64`.
- `player.c:1823-1838` `bhControlPlayerPad`: erst nach Ende des Kampfzweigs
  `mode1 = 0`, `stflg &= ~0x400`, `sys->st_flg &= ~0x4`.
- Nativ identisch: `native/player_combat_core.c:2913` (`cb_st_or(c, 0x4)` im
  Kopf von `cb_act_atk_dispatch`), `:2984` (Loeschen in bhControlPlayerPad),
  `:3241`. `combat.aiming()` = `result.active` = `p->mode1 != 0`
  (`native/combat.cpp:181`, `player_combat_core.c:3244`).

Dieselbe Flagge setzen auch Stufen/Leiter/Schieben/Hocke/Greifdrehen
(`player.c` 2884/3146/3353/3515/3733/3889/3941/4136/4328, mode2 14..22),
Schaden/Tod (6218/6352/6604/6643/6682) und Skripte (`bhCineSet` Modus 0/3/5,
Tueranforderung, Itembox; `native/events.cpp:279-283/624/820/830`). Als
Mod-Sperre mischte sie also die gewollten Sperren mit dem Zielen.

`flg 0x10000` taugt ebenfalls nicht als Sperrkriterium: `player.c:2146`
(Leerlaufhaltung mode2 13) und `:2396` (Laufen mode2 6) setzen es auch.

## Aenderung

- `native/runtime.cpp`: neues Lambda `third_person_free` vor `third_person_tick`.
  Frei = kein Preview/Titel/Tuerwechsel/Film/Itembox, kein Ereignis-/Festkamera,
  Spieler weder skriptgefuehrt noch versteckt, kein Cinematic (`cb_flg 0x4`,
  `events->flags[10][0] & 4`), und im Spielerwerk `mode0 == 1`
  (`bhCPM0_action`, `player.c:1336` `bhCtrPly_mode0`) ausserhalb von
  `mode2 14..22` (`player.c:1957` `bhCPM1_act_bas`). Die nativen Besitzer der
  Sonderaktionen werden dort gelesen, wo sie liegen: `stairs.active`
  (14..17/20/21), `world->pushing()` (18), `world->player_mode()` (19/22),
  `world->turning()`. `st_flg 0x4` wird nicht mehr gelesen. `combat.aiming()`
  bleibt wie bisher der Blend auf `aim_distance`/`aim_fov`.
- `--state`-JSON: neue Diagnosefelder `third_person_active`, `combat_aiming`.
- `native/third_person.h`: Kommentar der beiden Zustandsfelder.
- `tests/native/third_person_aim_check.py` + `add_test(third_person_aim_camera)`
  in `CMakeLists.txt` (nur dieser Block).
- Doku: `docs/CONTROLS.md` Abschnitt "3rd-person camera mod",
  `docs/VALIDATION.md` Testtabelle. `docs/NATIVE_GAMEPLAY.md` hat keinen
  Mod-Abschnitt (Suche "third"/"3rd" leer), deshalb dort nichts angehaengt.
- Originalbesitzer unveraendert, kein Ledger-Eintrag.

## Messwerte (headless, RM_0030, `combat-handgun.txt`, `--third-person`, `RECVX_CAM_TRACE=1`)

| Stand | third_active je Takt | Takte 3401..3519 (Zielen ab 3400, Schuss 3500) |
|---|---|---|
| vorher (Binary 2026-10-22 00:01) | 1..38 = 1, 39..3219 = 0 (Steve-Szene), 3220..3400 = 1, **3401..3519 = 0** | 0 / 119 aktiv |
| nachher | 1..38 = 1, 39..3219 = 0, **3220..3519 = 1** | 119 / 119 aktiv |

Bei Takt 3520 vorher wie nachher: `st_flg 0x5016` (Bit 0x4 gesetzt),
`cb_flg 0x2000` (kein Cinematic), `event_camera -1`, `player_scripted false`,
`combat_shots 1`, `combat_attacks 1`, `motion 104`, `yaw 16384`.
Nachher gegen Lauf ohne Mod: `camera_pose` (originale CAM_WORK-Lage) und
`player_work` identisch, `combat_shots` 1 = 1.

Titelstart (`--third-person --headless --frames 300`, ohne `--in-game`):
rc 0, `third_person_active false`, `blocked_commands 0`.

## Tests

`./build.sh --platform macos --test-regex ...` (mit Build-Sperre): 7/7 gruen —
`third_person_camera_mod`, `third_person_aim_camera` (neu, 25 s),
`combat_weapons_ammunition`, `player_action`, `core_continuity`,
`event_lifecycle_control_release`, `main_menu_settings_video_skip`.
Neuer Test prueft: Steve-Szene (Ereigniskamera/Cinematic) -> Mod aus; Zielen
Takt 3450 (`st_flg 0x4` gesetzt, 0 Schuesse) -> Mod an; nach dem ersten Schuss
Takt 3520 -> Mod an, Kampfwerte/Spielerwerk/Originalkamera gleich dem Lauf ohne
Mod; RM_1120 Skriptstart `bhCPM2_act_kdd` (mode2 15) -> Mod aus.
Vorherstand des Zielfalls: 0/119 aktiv (oben), der neue Test waere dort rot.

## Offene Grenzen

- Schaden/Tod/Wurf/Gegnergriff (`mode0` 2..6) und Itembox schalten wie bisher auf
  die Originalkamera; nicht neu bewertet, nur das bisherige Verhalten erhalten.
- Nachladen (mode2 70) und Messer (72) laufen im selben Kampfzweig und bleiben
  damit in der Schulterkamera; nur der Handfeuerwaffen-Schuss wurde gemessen.
- Kein Bildvergleich in der App; Nachweis headless ueber Trace und State.
- `third_person.ini` des Nutzers (`enabled = 1`) unveraendert; Start.command-
  Dev-App wurde durch `build.sh` aktualisiert, Paket `build/packages` nicht.
