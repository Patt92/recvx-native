# A01–A04 — Spielerkampf nativ portieren (Zielen, Schießen, Nachladen, Messer, Treffer)

Stand: 2026-10-10. Eigenständiger Arbeitsauftrag, disjunkt zu den laufenden
Paketen (Gegner C01–C08/T1–T5, D10 Karte, Codex Kamera/Clip, B01/B04/B06).

## Warum

Der gesamte Spielerkampf ist stillgelegt (`native/player_combat_core.c:3–37`,
`native/targeting.cpp`, Roadmap A01–A05): Der Spieler kann nicht zielen,
schießen oder zuschlagen, keine Munition, keine Zielaufschaltung, kein
Mündungsfeuer, keine Waffentöne. Ohne das sind alle Gegnerports im Spiel
nicht bekämpfbar. Es ist der wichtigste fehlende Gameplay-Mechanismus.

## Pflichtlektüre und Regeln

`CLAUDE.md`, `AGENTS.md` vollständig, `QUALITY.md`, Regelteil von
`docs/ORCHESTRATOR_HANDOVER.md`, Roadmap-Zeilen A01–A05
(`docs/NATIVE_REIMPLEMENTATION_ROADMAP.md:193–197`), `native/enemy_work.h`
(gemeinsamer Gegnervertrag, nur lesen). Vor Arbeit und jedem Build
`python3 tools/check_no_original_core.py`.

- `archived/FORBIDDEN_DONTUSE` niemals lesen, durchsuchen oder nutzen; auch
  nicht den dort liegenden alten Generator `generate_player_combat.py`.
- `player.c`, `playpch.c`, `playpch2.c`, `pwksub.c`, `weapon.c`, `Motion.c`,
  `hitchk.c` u. a. unter `src/ps2/veronica/prog/` und das ELF nur als
  Semantikreferenz. Keine Funktionskörper kopieren/generieren/kompilieren/
  linken. Datentabellen (Waffen-/Motion-/Gate-/Winkeltabellen) dürfen als
  native Daten übernommen werden, mit Regressionstest gegen die Quelle;
  vorhandenes `native/combat_tables_data.inc` und `weapon_tables.h` prüfen und
  wiederverwenden.
- Kein BH_PWORK/COMBAT_SYS/Effektpool als eigener Arbeitssatz: Spielerzustand
  bleibt beim vorhandenen Player-/CombatController-Besitzer, Gegner bei
  WorldEntity/`RECVX_ENEMY_WORK`, Inventar bei `Inventory`, Effekte beim
  vorhandenen Effektbesitzer, Ton beim `AudioMixer`.
- Keine Heuristik, geratene Konstante, Sonderregel, stiller No-op oder
  abgeschwächte Testerwartung. Ungeklärtes als benannte Lücke mit Datei:Zeile.
  Fehlende Dienste (z. B. Kapsel-/Wandtreffer, Effekte) als Anschlussvertrag
  melden statt nachbauen, wenn sie einem anderen Paket gehören.
- Gemeinsamer Arbeitsbaum mit aktiven Peers: nichts zurücksetzen, stashen,
  löschen. Kein Commit, Push oder Paket.

## Dateibesitz

Exklusiv: `native/player_combat_core.c/.h`, `native/targeting.cpp/.h`,
`native/combat.cpp/.h`, neue `native/combat_*.c/.h`, neue Tests
`tests/native/combat_*_test.cpp`, `tools/verify_combat.py`,
`work/native_port_A01_A04_combat.result.md`.

NICHT ändern: `CMakeLists.txt`, `native/world.*`, `native/runtime.cpp`,
`native/events.cpp`, `native/enemy_*`, `native/en*`, `native/subplayer.*`,
`native/ninja_math.*`, `native/ninja_matrix.*`, `native/renderer.*`,
`native/scene.h`, `native/effect_*`, `native/inventory*`, `native/ui.*`,
`native/input.*`, `native/map_*`. Benötigte Anschlüsse als
`work/native_port_A01_A04_combat.patch` mit Schnittstellenvertrag übergeben.

## Umfang in Originalreihenfolge

- A01 Zielsuche/-wechsel (bhCPM2_SearchPch, SetLockOnDirection), Wand-
  Sichtprüfung, Armwinkel beider Hände, Höhen-/Blickwinkel (PlyPchMain).
- A02 Ready/Release/Reload, Munitionszähler, at_flg-Verriegelung,
  Inventarübergänge (Abzug über die vorhandene Inventory-API; fehlt eine
  Mutation, Vertrag melden).
- A03 Waffenzustandsautomat Pistole/Burst/Dual (CombatEntry, bhCPM1_act_atk,
  CombatYaw), Motion-/Latch-/Posefolge, bhSetGunFire-Aufrufkette, Waffenton,
  Vibration; vorhandenen WeaponFlash-Port behalten.
- A04 Messer (CombatKnife) und Trefferzuweisung an echte Gegner-Works über
  `RECVX_ENEMY_WORK` (dam[], djnt_no, cpcl_no, comb_*, Kontaktflags) in der
  Originalreihenfolge; keine zweite Ziel-/Schadensliste.
- A05 (weitere Waffen) nur belegt vorbereiten, nicht Teil der Abnahme.

## Abnahme

- Komponententests: Tabellen gegen Quelle, Zustandsfolgen Ziehen → Zielen →
  Schuss → Rückstoß → Nachladen → Ablegen mit Flags/Munition/Frames,
  Zielwahl/-wechsel inkl. Nichttreffer und Wandverdeckung, Messerhieb mit
  Treffer/Fehlschlag, Schadenseintrag im Enemy-Work.
- Runtime (nach Integrator-Patch, sonst als offen berichten): Pistole in einem
  Originalraum mit Zombies ziehen, zielen, schießen, nachladen; Munition sinkt,
  Mündungsfeuer/Ton, Gegner erhält Schaden. Debug-Positionierung kennzeichnen.
- Bericht trennt implementiert / produktiv angeschlossen / komponentengeprüft /
  Runtime-geprüft / offen, je Mechanismus mit Datei:Zeile.
- Build nur im eigenen Verzeichnis (z. B. `build/a01-combat`), nie in Peer-
  Buildverzeichnissen; CMake nicht ändern (Test ggf. direkt kompilieren).
