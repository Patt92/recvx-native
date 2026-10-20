# en16_laser_hits — Ergebnis

## Originalbefund

- `en16.c:211-277` (`bhEne16_MV02` mode3 4) ist der einzige Aufrufer der Trefferfolgen, nicht
  der Effekt: `bhEff_E16_LaserSightDraw` (effsub2.c:2390) schreibt nur `exp0` (getroffene Flaeche
  aus `bhCollisionCheckLine2(..., 0x4400, -1)`), `lox/loy/loz`, `ct0`
  (`bhEff_E11_CheckCollisionPlayer`, Objektindex oder -1) und `gpx/gpy/gpz`. Der Draw laeuft in
  `bhDrawEffect` (game.c:340), also nach `bhControlEnemy` (game.c:48): MV02 liest die Werte des
  Vorbildes.
- Spielertreffer (`en16.c:217-248`, Disassembly 0x1EA218-0x1EA384): `eff->ct0 > 0` und
  `!(plp->stflg & 0x30)` -> `plp->dv = epw->n - epw->p`, `bhEne_SetBloodEffectBurst(plp, 2,
  eff->ct0, &eff->gpx, 1)` (zonzon1.c:243-298), HP `>150: -= hp>>1`, `>50: -= hp/3`, sonst -1;
  bei hp >= 0 `flg |= 4`, mode0 2, mode1 `bhEne_DGDirCheck(plp)`, mode2 1, mode3 0, sonst
  `epw->ct1 = 0`.
- Wandtreffer (`en16.c:250-266`, 0x1EA388-0x1EA454): `hp = eff->exp0`, bei
  `bhCheckWallAttrB89(hp)` (hitchk.c:5881) `v = epw->p - epw->n`, `pos = eff->lo`,
  `bhSetEffGunSpark(&pos, &v, 0xFFF8F8A0, 0x4CE04010, WpnTab[13].hiteff - 4)`; ELF 0x3560D6
  (WpnTab 0x355B30 + 13*0x68 + 0x5E) = 5, also `Eff308Prm[1]`. Danach `gap.wpn_no = 0xD`,
  `gap.ay = bhArcTan2(v.x, v.z)`, `bhSetGunSplash(&gap, &pos, hp->prm0)` (lbu 0x20).
- **Neuer Befund:** `bhSetGunSplash` (weapon.c:1636-1640, 0x28DA58-0x28DA80) liest
  `gap->vx/vy/vz` (GA_WORK +0xC..+0x14) als Spritzrichtung (lox bzw. px/pz des Effekts 11/10).
  `bhEne16_MV02` legt `gap` bei sp+0x50 an; ihre einzigen Stackstores sind 0x0/0x4/0x8
  (Register), 0x78 (`wpn_no`), 0x90 (`ay`) und 0xA0..0xB8 (`v`, `pos`). sp+0x5C..0x64 wird nie
  beschrieben: die Richtung ist im Original uninitialisierter Stackinhalt, nicht `v`.

## Nativer Stand vorher

Entgegen der Ledger-Zeile waren alle Dienste bereits gebunden (Commit 0350aed):
`bhCheckWallAttrB89` (`en16.wall_attr_b89`, world_enemies.cpp:1932, gleiche Abbildung
st_flg = Bank 5, cam.ncut = values[2] wie world.cpp:3461/combat.cpp:268), `bhSetEffGunSpark`
(`RoomWorld::gun_spark` -> `start_gun_spark`), Blut ueber `recvx_t3_set_blood_effect_burst`
(en03_lib.c:2322, plp-Link, BloodType[0]) und der Spielerzustand ueber `player_set`. Der einzige
inhaltliche Fehler: en16.c uebergab `&v` als `gvec` an `gun_splash` — eine erfundene Richtung.

## Aenderungen

- `native/enemies/en16.c`: Wandzweig ruft `bhSetGunSplash` nicht mehr mit `v`, sondern meldet
  die benannte Luecke `bhSetGunSplash (en16.c:265): GA_WORK vx/vy/vz im Original
  uninitialisiert` (mit Disassembly-Beleg im Kommentar). Funken unveraendert; Kopfkommentar
  zum Effekt 270 aktualisiert.
- `native/enemies/en16.h`: Feld `gun_splash` aus `RECVX_EN16_SERVICES` entfernt (Begruendung im
  Kommentar), veralteter Satz „Effekt nicht portiert“ korrigiert.
- `work/en16_laser_hits.integration.patch` (world_enemies.cpp, `git apply --check` OK):
  entfernt die nicht mehr genutzte `en16.gun_splash`-Bindung. **Muss zusammen mit en16.h
  angewendet werden**, sonst kompiliert world_enemies.cpp nicht.
- `tests/native/enemy_beam_test.cpp`: neuer Block `en16_room_shots` mit echten Raeumen.

## Testfaelle (enemy_beam_effects)

- RM_1020: EN16 ist dort type 0 (Init mode2 1 = MV01, kein Brain), Strahl mode0 2; MV02 und
  damit die Trefferzweige sind dort ohne Ereignis nicht erreichbar. Der schiessende EN16
  (type 1) steht in **RM_3010**.
- RM_3010, Diagnose-Vorbedingungen (gekennzeichnet): stflg 0x1000000 geloescht (statt der
  Ereignisfreigabe), Spielerin auf Boden 0 bei (80,0,40) in prm0-16-Flaeche, fuer den Wandzweig
  plp->stflg 0x10.
- Spielertreffer: HP nach en16.c:224-236 (160 -> 80), Spielerzustand flg 4/mode0 2/mode1 aus
  DirCheck/mode2 1/mode3 0, ct1-1, genau ein eff 297 mit type 2, lkono = eff->ct0 (Vorbildwert),
  Spielerlink, mdlver 0, lox = Inverse(owP[jno]) * (gp - owP[jno].t), xn = -plp->dv.
- Wandtreffer: kein HP-Abzug, genau ein eff 308 mit vtx_pos = eff->lo, vtx_dir = p - n,
  Farben, gnd_hgh = y - 100, Parametersatz Eff308Prm[1]; kein eff 11/10; Luecke gemeldet;
  keine weitere Luecke.

Privater Lauf unter $TMPDIR (Kopien der Bibliotheken, `ar d`/`ar r` fuer en16.c.o und
gepatchtes world_enemies.cpp.o): PASS. Vorherstand (unveraenderte Bibliothek, neuer Test): rot
mit „bhSetGunSplash darf ohne belegte Richtung keinen Spritzer erzeugen“.

## Gegenproben (Kopie von en16.c sabotiert, Test rot, Original unberuehrt)

| Sabotage | Meldung |
|---|---|
| `hp -= hp/2` -> `/3` | plp->hp folgt nicht en16.c:224-236 |
| Blut `&eff.gpx` -> `&eff.lox` | Blut 297: lox/loy/loz nicht der Gelenkversatz |
| Blut jno `eff.ct0` -> `eff.ct0 - 1` | Blut 297: type 2 am Spielergelenk eff->ct0 |
| Funken `hiteff13 - 4` -> `- 3` | Funken: nicht Eff308Prm[1] |
| Funken `v = p - n` -> `n - p` | Funken: vtx_dir != epw->p - epw->n |
| mode1 DirCheck invertiert | Spielerzustand nach Treffer weicht ab |
| stflg-0x30-Tor entfernt | Wandzweig darf keinen Schaden machen |

## Ledger-Vorschlag (docs/port_ledger.tsv, Zeile effsub2.c:bhEff_E16_LaserSight)

```
effsub2.c:bhEff_E16_LaserSight	partial	step_e16_laser samt Draw (Linie 1500, Wand-/Spielertreffer, bhEne16_AddEffPos, Spurlinien bei type != 0, 16 Kegelpolygone). Trefferfolge en16.c:217-266 (MV02 mode3 4) am RoomWorld-Besitzer: bhEne_SetBloodEffectBurst(plp,2,eff->ct0,&eff->gpx,1), HP/Spielerzustand, bhCheckWallAttrB89, bhSetEffGunSpark(Eff308Prm[1]) in RM_3010 geprueft. Offen: bhSetGunSplash (en16.c:265) liest im Original nie beschriebene GA_WORK vx/vy/vz (Disassembly bhEne16_MV02 0x1EA430-0x1EA454) -> benannte Luecke, kein Spritzer. enemy_beam_test.
```

## Grenzen

- Kein Build in build/macos-arm64, kein CTest-Lauf ueber CMake; nur privater Link gegen Kopien.
- Der sichtbare Wandspritzer (Effekt 11/10) faellt gegenueber dem bisherigen Stand weg, weil er
  auf einer erfundenen Richtung beruhte. Ob und wie die undefinierte Originalrichtung behandelt
  werden soll, ist eine Nutzerentscheidung.
- `line_wall` ist eine Kopie der Flaeche zum Draw-Zeitpunkt, das Original liest den ATR-Satz
  live im naechsten Bild (nur relevant, wenn ein Ereignis attr/prm dazwischen aendert).
- Ist `EXP0_I(0)` NULL, liest das Original im else-Zweig Adresse 0x2F0; nativ wird der Zweig
  uebersprungen.
- Die Ereignisfreigabe des Scharfschuetzen in RM_3010 und das Fortsetzen der Spielerin aus dem
  Schadenszustand sind nicht Teil dieses Tests.
