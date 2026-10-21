# E03b — Raum-Effektprozeduren nativ (Claude, 2026-10-11)

Nutzerauftrag: "grafisch fehlt vieles, z. B. Flammen beim Helikopter,
Pistolenschuss, Leuchtschilder über Türen". Dieses Paket portiert die
zugehörigen Raum-Effektprozeduren eigenständig auf den vorhandenen
`RoomWorld`-/`effect_sprites`-Besitzer. Kein Generator, kein Originalkern,
keine kopierten Funktionskörper, keine Raum-Sonderfälle.

## Ausgangslage (gemessen, nicht hergeleitet)

`supported_effect_sprite` ließ genau eine Prozedur zu (bhEff009, Feuerzeug).
Jede andere Effektarbeit des Raumes wurde weder getickt noch gezeichnet:
`tex_id` blieb −1, `visible` false. Baseline mit dem Binary **vor** dieser
Änderung, gleiche Replays und Räume:

| Ansicht | sichtbare Effektarbeiten | Screenshot |
|---|---|---|
| RM_0100, Brandstelle | 0 | `build/validation/room-effects-20261011/heli-vorher.png` |
| RM_7230, Schilderwand | 0 | `.../schilder-vorher.png` |
| RM_0020 / RM_0090 | 0 | — |

## Portierte Prozeduren

Alle acht laufen in `native/effect_sprites.cpp` auf der bestehenden
`EffectSprite`-Arbeit; Reihenfolge, Flags, Zähler, Zufall (gemeinsamer
EE-`rand`), Blendpaare und Freigaberegeln sind aus den Originalroutinen
übernommen, die Datentabellen liegen als reines Datenmodul
`native/effect_sprite_tables_data.inc` im Baum.

| Prozedur | Original | Mechanik | Texturgruppe |
|---|---|---|---|
| bhEff122 | effsub1.c:8158 | weltfeste Leuchtplatte, `ani_ct = type`, UV-Kachel aus `mode1`, Blend 8/6 | 41 |
| bhEff123 | effsub1.c:8208 | Blinklampe: 4 Bilder, Pause `20 + 10·rand`, Farbe je `type`, `az = ct0·4096`, Blend 8/10 | gemeinsame 2 |
| bhEff125 | effsub1.c:8458 | Erzeuger; verlangt `mode1 ≠ 0`, fordert bhEff126 mit `mdlver = type%4`, Viertelskala, Lagejitter | — |
| bhEff126 | effsub1.c:8488 | aufsteigende Wolke, vier UV-Bänke, `spd`/`yn`-Verlauf, Abbruch am Boden (`bhGetGroundPosition`) | 35 |
| bhEff181 | effsub0.c:1916 | Raucherzeuger, Stoßabstand `floor(sz)`, Einzelstoß bei `floor(sz) == 0` | — |
| bhEff182 | effsub0.c:1952 | Rauchwolke, `effinit`/`effset`, Farbsatz `type%10`, Form `type/10`, Vorschub `njFraction(sz)` | 400..407 |
| bhEff218 | effsub5.c:1609 | Feuer, 17 Varianten, zufälliges Startbild, Bildumlauf, UV in Pixeln/256, Blend 8/3 | 70..79 |
| bhEff230 | effsub5.c:3333 | animierte Leuchttafel, UV-Bank aus `lkono % 4`, Tafel `(type−1) % 6` | 414 |

Anschluss (klein gehalten, gemeinsame Dateien nur punktuell):
- `native/world.cpp`: Texturgruppen der portierten Prozeduren werden dekodiert
  (`ported_effect_atlas`) und behalten das dekodierte Ninja-Alpha, weil sie mit
  ihrem Originalblendpaar gezeichnet werden; die noch unportierten Sprites
  behalten unverändert ihre additive Annäherung. Zweite Stelle: die Prozedur
  besitzt `op->ax/ay/az`, deshalb übernimmt der Host die Winkel der Arbeit vor
  dem Zeichnen (bhEff123 dreht die Lampe je Bild).
- `native/effect_sprites.cpp`: `set_effect_ground` hält jetzt den vorhandenen
  `bhGetGroundPosition`-Anbieter des Raumes (vorher verworfen); der
  Zeichenpfad nutzt für die portierten Effekte `njColorBlendingMode`-Paare.

## Originalfall statt Annäherung: bhEff122 mit mode1 = 3

Der 205-Raum-Sweep brach in **RM_4020** ab: das Raumskript setzt per
`bhEffModeSet` `mode1 = 3`, bhEff122 liest aber `exp0[mode1]` aus einer Tabelle
mit drei Zeilen. Geklärt an der Originalbinary statt geraten:
`tools/disasm_ps2.py --elf data/files/SLUS_201.84 bhEff122` zeigt bei
0x00233CE0 `lui 0x32 / addiu 0x6B60`, also `exp0 = 0x00326B60`. Ab dort liegt im
US-Release ein zusammenhängender `UV_WORK`-Bereich: `uvinfo_p0` von bhEff122
(3 Zeilen), bhEff123 (4) und bhEff124 (15). Alle 22 Zeilen sind byte-identisch
gegen `data/files/SLUS_201.84` geprüft und liegen als Datenfenster im
Datenmodul; `mode1 = 3` liefert damit die tatsächlich gelesene Originalzeile
(0.0, 0.875, 0.0625, 0.0625). Jenseits des belegten Fensters meldet die
Portierung statt zu raten.

## Nachweise

- **Neuer CTest `effect_room_sprites`** (`tests/native/effect_room_sprites_test.cpp`):
  Zustandsautomaten aller acht Prozeduren, Originaltabellen, Blendpaare,
  Erzeugerkadenz, Bodenabbruch, Slotfreigabe; dazu produktive Prüfungen mit
  Originalräumen (RM_7230 Schilder, RM_0100 Feuer, RM_0090 Dampf) inklusive
  gerenderter Pixel aus echten Atlanten.
- **A/B mit identischem Buildstand** (nur `supported_effect_sprite` getauscht,
  danach byte-identisch zurückgebaut):
  - RM_0100 Brandstelle: 0 → 39 sichtbare Effektarbeiten, **13,97 %** der Pixel
    geändert (`heli-vorher.png` / `heli-nachher.png`).
  - RM_7230 Schilderwand: 0 → 10 sichtbar, 356 Pixel geändert
    (`schilder-vorher.png` / `schilder-nachher.png`).
- **Reichweite** aus dem vollständigen Sweep (600 Ticks, keine Eingaben):
  128 Räume führen portierte Effektarbeiten, 82 Räume zeigen sichtbare.
  bhEff122 268/178, bhEff182 282/224, bhEff218 206/113, bhEff230 39/23,
  bhEff123 58/3 (blinkt), bhEff125 472 Arbeiten (warten auf Skriptfreigabe).
- **Sweep:** 205/205 Räume laden, 0 blockierte Eventbefehle, 0 Abbrüche.
- **CTest macOS arm64:** 55/57. Die zwei roten sind vorbestehend und nicht aus
  diesem Paket: `effect_pool_lifetime` (E02-Partikel, in AGENTS.md dokumentiert)
  und `engine_progression_resources` (Not Run, weil `recvx-progression-tests`
  wegen `EnemySpawnContext::random` nicht übersetzt).
- **verify-Suiten:** `verify_lighter`, `verify_detector`, `verify_events`,
  `verify_continuity`, `verify_progression` bestanden.
- **Policy:** `check_no_original_core.py` gegen Quellen, Buildverzeichnis und
  tatsächlich genutztes Binary PASS; `tests/port_policy_test.py` 24/24,
  `tests/check_build_coverage_test.py` 17/17.
- **Windows x64:** `recvx_game`, `recvx-runtime.exe` und der neue Test
  kompiliert und gelinkt, PE-Linkaudit PASS. **Nicht ausgeführt** (kein
  Windows-/Wine-Lauf).

## Zwei fremde Testerwartungen korrigiert (nicht abgeschwächt)

- `effect_debris_test.cpp`: Die Lückenliste behauptete, 122/218/230 seien
  ungebunden. Sie sind portiert; die Liste führt jetzt nur noch die wirklich
  offenen Prozeduren und prüft zusätzlich positiv, dass die acht portierten sich
  als gebunden melden.
- `lighter_test.cpp`: Der Test hielt den Slot der Objektflamme fest. Seit die
  Raumprozeduren desselben Raumes aus dem gemeinsamen `eff[512]`-Pool
  allozieren (bhEff181 fordert bhEff182 an), kann der Platz vergeben sein. Die
  Flamme wird jetzt an ihrer Originalkennung gesucht; alle Zusicherungen
  bleiben.

## Mündungsfeuer (Nutzerpunkt 2) — Befund, nicht behoben

Die Mechanik ist vorhanden und korrekt: `WpnEffTab[4][0]` fordert bhEff003 an,
`CombatController::fire` legt die Arbeit an, die gemeinsame Texturgruppe 3 ist
geladen, und die Originalgeometrie erzeugt mit der echten Textur Pixel (neuer
Prüfabschnitt `muzzle_flash`). Im einzigen vorhandenen Schuss-Replay
(`combat-handgun.txt`) ist trotzdem nichts zu sehen, und zwar belegbar an
anderer Stelle: zum Schusszeitpunkt steht `cb_flg 4` (Cinematic) noch, und der
Zeichenpfad behandelt das als "scripted" — dann zeichnet er bloße Hände statt
Waffe und Mündungsfeuer (`runtime.cpp:411`, `combat.cpp` `append`). Gemessen:
`cinematic` ist bei Frame 3501 (Schuss) true und erst ab ~3700 false. Die Figur
kann also schießen, während der Zeichenpfad eine laufende Sequenz annimmt.
Das ist ein Widerspruch zwischen Kampf- und Zeichenbesitzer (A0x/F0x), nicht
Effektarbeit; `native/combat.cpp` wurde nicht verändert (temporäre Diagnose
vollständig zurückgebaut, `git diff` leer).

## Ausdrücklich offen

- Alle übrigen Effektprozeduren (Explosion, Blut, Trümmer, Funken, Eff137,
  bhEffFil/bhEffBG-Filter/Hintergrund) bleiben ungebunden.
- bhEff125 erzeugt nur nach Skriptfreigabe; in den eingabefreien Sweeps bleibt
  der größte Teil der 472 Arbeiten still. Kein Durchspielnachweis.
- Abweichung: Der Host leert die Effektanforderungen **nach** der Pool-Schleife,
  das Original führt eine im selben Pass angelegte Arbeit mit höherem Index
  sofort aus. Das erste Bild einer erzeugten Wolke läuft damit einen Tick
  später. Im Test benannt, nicht kaschiert.
- `op->bank` (`sys->ef_pbkb[tex_id]`, PVP-Palettenbank) hat im nativen
  Zeichenpfad keinen Abnehmer, weil dort dekodierte RGBA-Texturen gezeichnet
  werden.
- `njDrawTextureSub3D` verwirft im Original eine Fläche, sobald ein Eckpunkt
  hinter der Kamera liegt (bhEff123 nutzt dagegen den 1P-Pfad ohne diese
  Verwerfung). Der native Renderer clippt stattdessen regulär.
- Kein Commit, kein Push. Geprüftes Binary der Testläufe:
  `build/effects-claude-20261011/recvx-runtime`.

## Auslieferung (auf Nutzerauftrag, 2026-10-11)

`python3 tools/build_native.py --platform macos` neu gebaut
(`build/macos-arm64`, arm64-only, Deployment 11.0) und über dessen
`sync_macos_start_package` die Start.command-App erneuert:
`build/start/macos-arm64/RECVX Native.app`. Policy vor Build, gegen das
Buildverzeichnis und gegen das installierte Binary PASS; `lipo -archs` meldet
`arm64`; `codesign --verify --deep --strict` bestanden.

Prüfung **über Start.command selbst** (nicht über das Testbinary):
RM_0100 mit demselben Replay zeigt 39 von 50 Effektarbeiten sichtbar,
0 blockierte Eventbefehle. Gegen die vorherige Start.command-App (Stand 00:13,
gesichert unter `archived/releases/room-effects-before-20261011-091843`)
ändern sich **14,10 %** der Pixel, sichtbare Effektarbeiten 0 → 39.
Bilder: `build/validation/room-effects-20261011/start-command-vorher.png`,
`start-command-nachher.png`, `start-command-schilder.png`.

Die ausgelieferte App enthält den gesamten aktuellen Arbeitsbaum, also auch die
parallele Peer-Arbeit (u. a. der Schiebekisten-Port vom selben Tag), nicht nur
dieses Paket. Das v0.5-Standalone-Paket unter
`build/packages/macos-arm64-v0.5` ist seit dem 10.10. 23:42 ohne Runtime
(leeres `Contents/MacOS`); Start.command nimmt darum ohnehin die
Entwicklungs-App. Ein neues Standalone-Paket wurde **nicht** erzeugt, weil
`--package` nur in ein neues Verzeichnis schreibt und das Packaging-Layout
sonst eigenmächtig geändert würde.
