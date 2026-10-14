# Event-Effekte: Hubschrauber (RM_0020) und Steve/Doppelwaffe (RM_0030)

Subagent `event-effects`, Thema `event_effects_heli_steve`, 2026-10-14. Zwei
gebündelte Event-Effekt-Fehler. Kein Commit/Push, kein Paket. Geänderte Dateien
und Ledger-Vorschläge am Ende.

## Kurzfassung

- **Steve/Doppelwaffe (RM_0030): behoben.** Der Feuerpfad der Waffe 21
  (`bhSetGunFire` case 21, Wandfunken), die Anschlussbindung
  (`bhCheckL2Wall` im 0x49-Weg) und die Weitergabe der Arbeit aus
  `WpnEffTab` (vorher nur id 3) sind nachgezogen; das Doppelwaffen-
  Mündungsfeuer **bhEff229** ist von Hand portiert. Die gemeldeten Lücken
  `bhEff2`, `bhEff229` und `bhSetGunFire Waffe 21` sind weg.
- **Hubschrauber (RM_0020): behoben.** Die Heli-Szene ist die
  Raum-Eintrittsszene von **RM_0020**. Der fehlende Effekt **bhEff352** (die
  Flamme am brennenden Wrack, erzeugt vom Gegner-Feuer
  `bhEne_SetSanEffect`/`zonzon.c:939`) ist von Hand portiert; die Lücke
  `bhEff352` ist weg und 14 Flammen zeichnen sichtbar (Bild 400, Flammenfläche
  links). Eine Regression der jüngsten Effektkernarbeit erreicht diesen Raum
  nicht (keine `flg 0x20000`, keine verlinkten Raumeffekte).

## 1. Steve/Doppelwaffe RM_0030

### Vorher/Nachher der Effektlückenliste (Replay `m93r-after-steve.txt`)

| Stand | Lücken |
|---|---|
| vorher (Binary 18:02, vor dieser Arbeit) | `Common motion: enemy initialization/exp1 missing`, `bhEff2`, `bhEff229`, `bhSetGunFire Waffe 21 (Laser/Wandfunken, pwksub.c:3156)` |
| nachher | `Common motion: enemy initialization/exp1 missing` |

Die verbleibende Lücke ist fremd (Subplayer-/Gegnerinit, nicht Effekt).

### Änderungen (Originalfundstellen)

1. **`native/player_combat_core.c` `cb_set_gun_fire` case 21** — ersetzt die
   Lücke. Nach `pwksub.c:3156-3240`: Schusslinie aus `WpnTab[wo].wp_fps1`
   durch `owP[jno]` (Y +36, X −1547 auf der Gelenkmatrix), dann
   `bhCheckL2Wall(&gline,0x400,&len)`; bei Wandtreffer und
   `bhCheckWallAttrB89` ein `bhSetEffGunSpark(wps, -gline.v, 0xFFF8F8A0,
   0x4CE04010, WpnTab[wo].hiteff-4)`, auf einer Wasserfläche (`prm0 == 1`)
   zusätzlich vier bhEff011-Tropfen in zwei Winkeln (±16384) und zwei
   Größen (`ef_scale`, `0.5*ef_scale`), Reihenfolge und `lkono` wie im
   Original. Ausschließlich vorhandene Dienste (`l2_wall`, `gun_spark`,
   `effect`).
2. **`native/world.cpp` `RoomWorld::gun_fire`** — bindet `s.l2_wall`
   (`bhCheckL2Wall`, pwksub.c:718) wie der Kampfpfad `combat.cpp:176`. Ohne
   diese Bindung meldete case 21 nur `bhCheckL2Wall (pwksub.c:718)`.
3. **`native/world.cpp` `RoomWorld::gun_fire` `s.effect`** — reicht die
   `bhSetEffectTb`-Anforderung aus `WpnEffTab` jetzt wie `combat.cpp:193`
   durch: `id 3` bleibt der eigene Zeichenweg (`spawn_weapon_flash`), jede
   andere Kennung geht in den gemeinsamen `eff[512]`-Pool. Vorher verwarf
   dieser Zweig (der 0x49-/Skriptweg) alles außer id 3, deshalb blieben
   `bhEff2` (Mündungsrauch) und `bhEff229` (Doppelwaffenfeuer) unerreichbar.
4. **`native/effect_sprite_procedures.cpp` `step_eff229`** neu —
   `effsub5.c:3036` **bhEff229** und `:3117` **bhDrawWeaponEffect2**. Das
   Original reiht den Satz in `sys->ef_fnc` ein und zeichnet mit `op->func`
   im Gelenkraum des verlinkten Werks (`lkwkp->mlwP->owP[lkono].mtx`,
   Texturgruppe 3). Nativ als ef_fnc-Satz: drei Passes (`flr_no 3`), ein
   zentrales Quad plus Waagerecht-/Senkrechtbalken (Blend 8/10), danach sechs
   um Z gedrehte Balkenpaare (Blend 8/6); die Eckpunkte werden über
   `op->mtx = Linkmatrix · T(lox,loy,loz) · R(ax,ay,az)` und die Skalierung
   `(sx,sx,sz)` in Weltkoordinaten gerechnet (`world_space`-Vierecke). UV-Bank
   `uvinfo_t01[mdlver][ani*3+k]` und Bild-Offset `akpal[k][ani]` in
   `native/effect_sprite_procedure_tables.inc` (`eff229_t01`, `eff229_akpal`).

### Tests

- `tests/native/effect_room_sprites_test.cpp` `muzzle_weapon229`: Init
  (tex_id 3, Blend 8/10), drei Passes, `spd` nach `ct0`, 15 Vierecke je Pass
  (3 + 6·2), `world_space`/`category`/`frame`, erstes Eckpunkt-`x` numerisch
  gegen `T(10,20,30)·4·(-0.25)`, Ende nach `flr_no`.
- `tests/native/combat_state_test.cpp` `test_dual_wall_spark` (Fixture um
  `gun_spark`-Mitschnitt erweitert): Waffe 21 feuert, die Wandlinie trifft,
  der Kern legt Funken mit Art `WpnTab[21].hiteff-4 == 1`.
- Die zwei Lückenlistentests (`effect_debris_test.cpp`,
  `progression_test.cpp`) führten 229 als offen; 229 ist in die
  Portiert-Liste verschoben (`progression_test` prüft die negative Lücke
  weiter mit der offenen Kennung 21). Keine Erwartung abgeschwächt.
- **`./build.sh --test`: 91/91 grün.** `tools/verify_combat.py --runtime
  --data`: Tabellen, Steves M93R-Geschenk→Ausrüsten→zwei Schüsse, Zielen in
  RM_0080 bestanden. `check_no_original_core.py` PASS.

### Mündungslicht (rom->lgtp[0]) — Befund, nicht behoben

`pwksub.c:2684-2733` füllt `rom->lgtp[0]` (flg 3, `lkflg` an die Hand,
`type = WpnTab[wno].ltp`). Für die **M93R (Waffe 4)** gilt `WpnTab[4].ltp ==
100`; `light.c:279 bhSetEasyDirLight` behandelt Typ 100 als **kurzen globalen
Helligkeitsblitz** (mode 0: Intensität ×3, mode 1: `flg &= ~0x2`), Typ 101 als
Kosinus-Ausblendung. Der Host bindet `set_gun_light` (`world.cpp:1309`) als
**Punktlicht** (source 4) an der Hand — es rendert, aber als kleiner Glanz
statt des Originalszenenblitzes; `step_light_color` kennt nur Typen 1..6, der
Typ-100/101-Einmalblitz fehlt. Waffe 21 hat `WpnTab[21].flg 0x10000040` **ohne
Bit 0x4**, erzeugt im Original also gar kein Mündungslicht. Benannte Lücke
(Renderer-Kanal).

## 2. Hubschrauber RM_0020

### Raum/Event

Das Hubschrauber-Event ist die Eintrittsszene von **RM_0020** (Stufe-0 =
Prologbereich). `tests/native/replays/graveyard-walk.txt` nennt es
ausdrücklich: Bodenfläche 0 (`x 129.69..136.72 / z 124.49..143.83`) setzt
`cb_flg 0x200` mit `flr_idx 0`, scd1 startet **Event 0 (Helikopter)**. Die
Repohistorie (`work/history/2026-10-13-instructions/CLAUDE.before.md:1028`)
nennt dieselbe Szene „RM_0020 Heli (Bild 400–900)“.

### Fehlende Prozedur bhEff352 — portiert

`bhEff352` = `bhEff_Sub352` (`effsub4.c:654`, `bhJumpEffect4[2]`). Erzeuger ist
das Gegner-Feuer `bhEne_SetSanEffect`/`bhEne_SetSanEffect2`
(`zonzon.c:939`, ebenso `en01.c:11272`/nativ `native/enemies/en01_core.c:7775`):
`sys->ef.id = 352` mit verlinktem Werk, danach `eff[n].zn/.ct3/.shp_ct`.

Nativ **`native/effect_sprite_procedures.cpp` `step_eff352`** (eigenständig,
mit `bhEff_SetSpriteAnime`/`bhEff_SetSprite`, `effsub4.c:600/586` inline):
- Tore `sys->pt_flg & 0x2` (`EffectContext::pt_flg`) und
  `!(lkwkp->stflg & 0x1000000)`.
- `mode0` 0..3: Init (`flg 0x4180000`, `tex_id 8`, Blend 8/10, Eckpunkte
  (−1,−1,5)…), `sxb` wächst um `yn = sxb/8` bzw. schrumpft ×0,96; `type>=2`
  verzögert den Start (ct1 5, Skalierung 1,2, Z 0,05).
- UV-Umlauf `anim00_tbl` über `uvinfo[10]` (rohe 255er-Werte).
- Weltlage: `owk * (ofx,ofy,ofz) − cam.v * zn`, über `njCalcVector(inv(owk))`
  als lokale `lox/loy/loz`; die Flamme liegt damit um `cam.v*zn` zur Kamera
  versetzt am Werk-Gelenk.
- Abschlussprüfung `ct2 != 0 && shp_ct > op->mtx[13]` (Y des
  Zeichenursprungs des vorherigen Bildes; nativ `EffectSprite::mtx_world_y`).

Neue Felder am vorhandenen `EffectSprite` (`native/effect_sprites.h`):
`ofx`/`ofz` (zu `ofy`), `shp_ct`, `mtx_world_y` (O_WRK 0x3A4/0x3AC, 0x34C,
`op->mtx[13]`). Die Gegner-Feuer-Anschlüsse
(`native/world_enemies.cpp:1026/1092`) setzen `s.shp_ct` jetzt statt eine
Lücke zu melden (`zn`/`ct3` liefen schon). Feldbelegung per Disassembly
bestätigt (`python3 tools/disasm_ps2.py --elf data/files/SLUS_201.84
bhEff_Sub352`: `ofx 0x3A4/…/ofz 0x3AC`, `shp_ct 0x34C`, `owk->mtx[12..14]`,
`op->mtx[0][13] = *(float*)(op->mtx + 0x34)`).

### Nachweis

- **Lückenliste:** mit `graveyard-walk.txt`, `--enemy-ai on`, 400 Bilder war
  vorher `Enemy gaps: 1 / bhEff352`; jetzt gibt es die Zeile `Enemy gaps`
  gar nicht mehr, und der Zustand führt **14** lebende `id 352`-Sätze
  (Texturgruppe 8, `visible: true`).
- **Bild:** Bild 400 der Szene zeigt die Flammenfläche links
  (`build/validation/.../f400.png`, angesehen). Vorher fehlte sie.
- **Fester Test** `flame352` in `tests/native/effect_room_sprites_test.cpp`:
  Tor `pt_flg 0x2` (ohne Bit läuft nichts), Init (tex 8, Blend 8/10,
  `flg 0x4180000`, `xn=sxb`, `yn=sxb/8`), Eintritt mode0 2/mode2 1,
  `sxb += yn`, numerische Weltlage (`owk T(5,10,15)`, `cam.v (0,0,-1)`,
  `zn 1` → `lox/loy/loz (0,0,1)`, `mtx_world_y 10`), UV-Größe, Bodenabschluss.

### Flackern — Regression geprüft, keine gefunden

Die jüngsten Effektkernänderungen (Gruppen 7–10 in
`work/native_port_effsub0_handport.result.md`) sind uncommittet im Baum. Für
RM_0020/RM_0100 relevante Wirkungen:

- **Rückseitentor `flg 0x20000`**: keine Raumeffektliste in RM_0020/RM_0100
  setzt das Bit (alle `flg=1`) → das neue Tor greift dort nicht.
- **Effektlink `flg 0x80`**: RM_0020/RM_0100 führen **keine** verlinkten
  Raumeffekte (`lkflg` überall 0).
- **Benutzerdefinierte Zeichnung / Blendzustand / Nachbilder**: betreffen nur
  `ef_fnc`-Sätze; RM_0020 hat keine. `append_effect_sprite` (der normale
  Spriteweg) wurde im jüngsten Diff in Blend/Tiefe nicht angefasst.
- Der sichtbare Wechsel zwischen den Bildern der Szene (z. B. 500 → 29
  sichtbare Sätze, 700 → 9, 900 → 30) ist ein Kameraschnitt, kein Flackern.

Damit ist keine der jüngsten Effektkernänderungen als Regression für diesen
Raum belegbar. Das im Skript absichtliche Blinken von `bhEff179` (RM_0020
Event 18/19 setzen `bhEffTypeSet` 48× zwischen Art 0/1 mit Verzögerungen)
erzeugt in der Szene ein Flackern der Funken emittierenden Sätze; ob der
Nutzer das meint, ist ohne seinen Spielstand nicht entscheidbar. Mit der jetzt
zeichnenden bhEff352-Flamme ist der „fehlende Effekt“ der Szene geschlossen.

### RM_0100 (Brandstelle) zur Abgrenzung

RM_0100 führt **keine** Raumevents und keine Effekt-Skriptbefehle; sein
Wrackfeuer ist statisch (BhEff218/181/182). Mit `--enemy-ai on` keine
Gegner-Effektlücke. Es ist damit nicht das beschriebene „Explosions“-Event.

## Geänderte Dateien

- `native/player_combat_core.c` — case 21 bhSetGunFire.
- `native/world.cpp` — `RoomWorld::gun_fire`: `s.l2_wall` und generischer
  `s.effect`-Pfad.
- `native/effect_sprites.h` — `ofx/ofz`, `shp_ct`, `mtx_world_y`.
- `native/effect_sprite_procedures.cpp` — `step_eff229`, `step_eff352` +
  Tabelleneinträge.
- `native/effect_sprite_procedure_tables.inc` — `eff229_t01`, `eff229_akpal`,
  `eff352_uv`, `eff352_anim`.
- `native/world_enemies.cpp` — `shp_ct` der Gegner-Feuer-Anschlüsse statt
  Lückenmeldung.
- `tests/native/effect_room_sprites_test.cpp` — `muzzle_weapon229`, `flame352`.
- `tests/native/combat_state_test.cpp` — `gun_spark`-Mitschnitt + Test.
- `tests/native/effect_debris_test.cpp`, `tests/native/progression_test.cpp` —
  229 in die Portiert-Liste.

## Ledger-Vorschläge (TAB-Format, der Hauptagent führt zusammen)

```
pwksub.c:bhSetGunFire	case 21	partial	Waffe-21-Wandfunken und -Wassertropfen (pwksub.c:3156-3240) in native/player_combat_core.c cb_set_gun_fire, auf l2_wall/gun_spark/effect; Waffe 11/20 und der ATR-Satz von bhCheckWallAttrB89 bleiben offen. tests/native/combat_state_test.cpp test_dual_wall_spark. work/native_port_event_effects_heli_steve.result.md
effsub5.c:bhEff229	done	Doppelwaffen-Muendungsfeuer (bhEff229 + bhDrawWeaponEffect2, effsub5.c:3036/3117) in native/effect_sprite_procedures.cpp step_eff229, 3 Passes, 15 Quads, Blend 8/10 und 8/6, Texturgruppe 3, Linkmatrix. tests/native/effect_room_sprites_test.cpp muzzle_weapon229.
effsub4.c:bhEff_Sub352	done	Flamme am verlinkten Werk (RM_0020 Heli-Szene, Erzeuger zonzon.c:939/en01_core.c:7775). native/effect_sprite_procedures.cpp step_eff352 mit bhEff_SetSpriteAnime/-SetSprite und shp_ct/op->mtx[13]-Abschluss; EffectSprite um ofx/ofz/shp_ct/mtx_world_y erweitert, world_enemies.cpp traegt shp_ct. Runtime RM_0020: 14 lebende Saetze, Luecke weg. tests/native/effect_room_sprites_test.cpp flame352.
```

## Offen / Grenzen

- **Mündungslicht**: `rom->lgtp[0]` (Typ 100/101 `bhSetEasyDirLight`) fehlt als
  globaler Helligkeitsblitz; der Host zeigt ein Punktlicht. Waffe 21 hat im
  Original kein Mündungslicht.
- **Waffe 11/20** in `bhSetGunFire` bleiben Lücken.
- **ATR-Satz** (`sys->ahtp`) wird von `combat_wall_distance` nicht geliefert;
  `bhCheckWallAttrB89` sieht attr 0 und lässt jeden Wandtreffer durch
  (dieselbe Grenze im bestehenden Kampfpfad).
- **bhEff352-Zeitigkeit**: die Originalkontrolle liest `op->mtx[13]` aus dem
  Zeichenpass des **vorherigen** Bildes; nativ steht der Wert aus dem
  vorherigen Steuerpass (`mtx_world_y`). Unterschied nur bei einem Sinken des
  Satzes unter den Boden innerhalb eines Bildes.
- Das **Flackern** der Hubschrauber-Szene ist nicht auf einen konkreten
  Mechanismus zurückgeführt; ohne Spielstand/Zeitpunkt nicht entscheidbar.
- Kein Commit/Push/Paket. Prüflauf-Binary: `build/macos-arm64` (Build 9,
  91/91).
