# T3 — Grosse Einzelgegner: Zwischenstand

Auftrag: EN03, EN05, EN09, EN15 (+EN53), EN19, EN22, EN23, EN26.
Vorgehen laut Auftrag: jede ID einzeln, lieber wenige vollstaendig und belegt
als alle halb.

**Pflichtlektuere erledigt:** `AGENTS.md` Abschnitt „Verbindliche
Architekturvorgabe fuer alle Agenten" (Zeile 1226 ff.), `QUALITY.md`.
`python3 tools/check_no_original_core.py` vor der Arbeit und nach jedem
Abschnitt ausgefuehrt: jedes Mal
`PASS: audited source/build/binary scope has no forbidden core`.

## Stand in einem Satz

Fertig und belegt sind **zwei gemeinsame Fundamente**, von denen alle acht
Identitaeten abhaengen: die geteilte Bibliothek (zonzon.c/zonzon1.c) und die
EN03-Kollisionsfamilie (der Dienst `collision_works`). **Keine der acht
Identitaeten selbst ist portiert.** Das ist ausdruecklich kein „EN03 fertig".

---

## (a) Implementierung

### A1 — `native/en03_lib.c` (neu, 2402 Zeilen): die geteilte Bibliothek

Der vorhandene Vertrag `native/en03_lib.h` hatte null Funktionskoerper. Alle
dort deklarierten Mechanismen sind jetzt nativ eigenstaendig bestimmt. Kein
Originalkoerper, kein Generator, keine zweite Workwelt, kein `sys`/`plp`/
`rom`/`ene`/`eff`.

Portiert (Fundstellen im Dateikopf vollstaendig aufgelistet):

* Dreh-/Winkelapparat: `ikou`, `ikou3`, `NitenDir_ck`, `bhCdirCheck`,
  `bhDGCdirCheck`/`2`/`3`, `bhEne_AngleCheck`, `bhArcTan2`,
  `bhEne_DirTarget`, `bhEne_CheckDirTarget`, `bhEne_DirCheck`,
  `bhEne_DGDirCheck`.
* Motion/Pose: `bhEne_GetTranslateMtn`/`2`, `bhEne_CalcPartsPos`,
  `bhEne_GetPartsPos`, `bhEne_AddNullTrans`/`Dir`, `bhEne_ChgMtn`.
* Kollision/Abstossen: `bhEne_CollisionCheckWall`/`2`, `bhEne_CheckDirWall`
  `/2/3`, `bhEne_CheckSideWall`/`2`/`3`, `bhEne_PosCheck`,
  `bhEne_EnemyAtariCheck`, `bhEne_CheckEventAtr`, `bhKaidanAtrCheck`,
  `bhEne_CheckEnemiesBall`.
* Schaden: `bhEne_CalcDamage`, `bhEne_HitCheckParts`,
  `bhEne_AttackHitCheck`.
* Audio/Eingabe: `bhEne_CallSE`/`_EX`, `bhEne_CallEffectSE`,
  `bhEne_CheckPlayEffectSE`, `bhEne_PlayerSePlay`, `bhEne_CallPlayerVoice`,
  `bhEne_SetVibration`, `bhEne_LeverCheck`.
* Effektfamilie: `bhEne_SetBlood`, `SetBlood2` (alle Typen 0..11),
  `SetBlood3`, `SetBlood4`, `SetNikuhenEffect`/`2`, `SetDFireEffect`/`2`,
  `SetSanEffect`/`2`, `SetMinceEffect`/`2`, `SetBloodEffect`/`2`/`4`/`5`,
  `SetBloodEffectBurst`, `SetBloodstain`, `SetFireEffect`, `SetAcidEffect`,
  `bhEne_EraseArrow`, `bhEne_QuickSort`, `bhEne_GetShapeCnt`.
* Spieldaten: `BloodType[31]` (zonzon1.c:9), `BloodCol[4][2]` (:43),
  `bld_tbl[20]` (zonzon.c:708), `blood_n[4]` (:731).

Erhaltene Originalquirks, jeweils im Code kommentiert:

1. `bhEne_LeverCheck`: die Richtungsgruppe **setzt** 4 (keine Addition), die
   Knopfgruppe `0x2d00` legt 3 dazu.
2. `bhEne_HitCheckParts`: die Schleife laeuft `1 .. obj_num-2`, laesst also
   Wurzel **und** letztes Gelenk aus.
3. `bhKaidanAtrCheck`: die Probepunkte werden **in der Schleife** mit den
   Kanten der gerade geprueften Flaeche ueberschrieben; Folgerunden arbeiten
   mit den geklemmten Werten.
4. `bhEne_SetBlood2` Typen 3/6/7/8/10/11 nullen `ofp->z` im Speicher des
   **Aufrufers**. Das ist beobachtbar (en01sub.c:292/462/512 uebergeben
   `&epw->xn`, dort wird also `epw->zn` genullt) — daher ist der Parameter
   nicht `const`.
5. `bhEne_SetBlood2` Typen 4/6/11 **schreiben** `dvx/dvy/dvz` des Works neu.
6. `bhEne_SetSanEffect2`: die Richtungsprobe nutzt Gelenk `no`, der Effekt
   haengt aber an `ltbl[no].lnk_obj`.
7. `bhEne_CheckEnemiesBall`: bei genau gleicher Lage ist `ln == 0`; das
   Original teilt ungeschuetzt und erzeugt dort NaN. Unveraendert.
8. `bhEne_SetBloodstain` legt den Fleck **viermal** an.
9. `bhEne_SetFireEffect`: `sz` bleibt 1.0 unabhaengig von `size`.
10. `ikou` behandelt negatives `add_dir` (Drehung vom Ziel weg), `ikou3`
    nicht.

### A2 — `native/en03_collision.c/.h` (neu, 681 + 78 Zeilen): Dienst `collision_works`

Die EN03-Kollisionsfamilie ist **nicht EN03-eigen**: `enemy_work.h:365`
fuehrt sie als gemeinsamen Dienst, den en03sub.c:286/389, en05sub.c,
en06.c:1840, en07.c:1863, en08.c, en11.c:852, en23.c:3066, en24.c:512 und
en30.c:84 benutzen. Deshalb eigene Datei, damit der Integrator sie **einmal**
bindet.

Portiert: `bhEne03_Collision` (en03.c:5220), `Collision2` (:5310),
`CollisionWallBox` (:5399), `CollisionWallGround` (:5528),
`CollisionWallCylinder` (:5549), `CollisionWallSlope` (:5594),
`CollisionWallTriangle` (:5770), `CollisionBoxEdge` (:5938) sowie die
Hilfsgeometrie `njDistanceP2P`/`P2L`/`P2PL` (ps2_NaColi.c:65/77/125),
`njGetPlaneNormal` (:301) und `bhInOutCheck` (hitchkl.c:682).

Erhaltene Quirks:

1. `CollisionWallBox` gibt **1** zurueck, sobald die Lage im aufgeweiteten
   Kasten liegt — auch wenn kein Zweig sie verschiebt (tief im Kasten ist
   genau das der Fall).
2. Der Flaechenzweig des Kastens besteht aus **drei unabhaengigen** Abfragen,
   nicht aus einer Auswahl; bei gleichen Abstaenden greift keine.
3. `CollisionWallGround` baut einen Kasten mit **negativer** Hoehe ab der
   Oberkante. Das wirksame y-Band ist `(py+h-ar, py+ar)` und damit nur bei
   duennen Platten (`h < 2*ar`) nicht leer. Keine Korrektur.
4. `CollisionWallTriangle`: das `return 1` steht **nach** dem innersten
   Block. Liegt die Lage im weiten y-Band, aber nicht im engeren, meldet die
   Routine einen Treffer **ohne** zu verschieben; ein erfolgloser Kastenzweig
   endet ebenfalls dort.
5. `CollisionBoxEdge` vertauscht fuer type 7 `h1`/`h2`, wodurch die
   senkrechte Hoehe negativ wird.
6. Divisionen durch `len`/`ln` bleiben ungeschuetzt (Mittelachse, Kante).

### A3 — Korrekturen am vorhandenen Vertrag `native/en03_lib.h`

Diese Datei gehoert mir; vier davon sind **belegte Fehler des ersten
Entwurfs**:

1. **`RECVX_ENEMY_PROC` war nicht deklariert.** `set_call_func` brauchte den
   Typ aus `enemy_dispatch.h`, der nicht eingebunden war — die Datei liess
   sich fuer sich allein nicht uebersetzen (genau die im Auftrag genannte
   Include-Falle). `#include "enemy_dispatch.h"` ergaenzt.
2. **Kein `extern "C"`.** Fuer C++-Verbraucher (world.cpp) waren alle
   Deklarationen falsch dekoriert. Guard ergaenzt.
3. **`RECVX_T3_COMBWEP` hatte die Feldreihenfolge vertauscht**
   (`timeout, pt[3], crit, bonus`). Belegt ist `zonzon1.h:28 COMBWEP_WORK`,
   Groesse 0x18: `crit 0x00, pt[3] 0x04, timeout 0x10, bonus 0x14`. Mit den
   Originaltabellen als Initialisierer waere **jeder Wert verschoben** gelesen
   worden. Korrigiert.
4. **`recvx_t3_set_blood2` hatte `const RECVX_ENEMY_VEC*`** — unvereinbar mit
   dem belegten Schreibzugriff auf `ofp->z` (Quirk 4 oben). Auf non-const
   geaendert.

Ergaenzt (jeweils mit Fundstelle, alle duerfen NULL bleiben):
`RECVX_ENEMY_EXT.player_joint_matrix`, `player_get`/`player_set`,
`sphere_capsule`, `vibration`, `player_voice_free`, `effect_se`,
`effect_se_playing`, `wal_count`/`wal_at`, `room_height`;
`recvx_t3_erase_arrow`, `recvx_t3_kaidan_atr_check`, `recvx_t3_quick_sort`,
`recvx_t3_call_effect_se`, `recvx_t3_check_play_effect_se`,
`recvx_t3_get_shape_cnt`; Typen `RECVX_T3_ATR`, `RECVX_T3_SORT`,
`RECVX_T3_PKEY`.

---

## (b) Produktiver Anschluss

### Was der Integrator binden muss

1. `recvx_enemy_ext_bind(&ext)` **einmal** (world.cpp), mit den Zeigern aus
   `RECVX_ENEMY_EXT`. Jeder nicht gebundene Zeiger ist zulaessig: der
   Mechanismus meldet dann die Luecke namentlich und **tut nichts**. Es gibt
   keinen Ersatzwert.
2. `RECVX_ENEMY_SERVICES.collision_works = recvx_en03_collision`-Adapter
   (die Signatur im Vertrag ist `int (*)(RECVX_ENEMY_WORK*)`, meine Funktion
   liefert zusaetzlich den Trefferindex; ein Einzeiler-Adapter genuegt, der
   `hit_index` verwirft — genau das tun en07.c:1863 und die uebrigen
   Aufrufer auch).

### Patchbedarf an fremden Dateien (von mir NICHT angefasst)

**P1 — `native/enemy_work.h` (T1): `RECVX_ENEMY_ATR` fehlt das Feld `id`.**
`ATR_WORK` (types.h:498) hat an Offset 0x02 ein `unsigned char id`. Der
gemeinsame Satz in `enemy_work.h:102` fuehrt es nicht. Genau dieses Feld
waehlt in `bhEne03_CollisionWallSlope` (en03.c:5638) die hohe Kante der
Schraege und in `bhEne03_CollisionWallTriangle` (en03.c:5820/5879) die Lage
des rechten Winkels — **ohne `id` ist beide Geometrie nicht bestimmbar.**
Vorschlag: `uint8_t id;` zwischen `type` und `flr_no` ergaenzen (Originallage).
Bis dahin fuehre ich in `en03_lib.h` eine eigene vollstaendige Sicht
`RECVX_T3_ATR` und nutze sie fuer `wal_at`. Beim Patch kann `RECVX_T3_ATR`
entfallen.

**P2 — `native/ninja_matrix.h` / `native/ninja_math.h`: `njRotate`
(Achse+Winkel) und `njArcSin` fehlen.** Beide braucht EN03s (en03sub.c:142/
178/181/474). `njRotate` ist ein allgemeiner Ninja-Dienst und gehoert nicht
in meine Dateien. Solange er fehlt, ist EN03s nicht portierbar (siehe d).

**P3 — `CMakeLists.txt`:** `native/en03_lib.c` und `native/en03_collision.c`
sind noch **nicht** in der Build-Liste (die Datei gehoert mir nicht). Die
beiden Pruefprogramme in `work/` sind ebenfalls nicht registriert und werden
von Hand gebaut (Befehle unter c).

### Erledigte Fremdmeldung

`native/enemy_spawn.cpp:12` notierte, `recvx_t3_pos_check` sei „pending from
the body". Der Koerper existiert jetzt (`en03_lib.c`, zonzon.c:402) und ist
geprueft. Ich habe die Datei nicht angefasst; der Kommentar kann beim
naechsten Anfassen entfallen.

---

## (c) Komponentenpruefung — tatsaechlich ausgefuehrte Befehle

Zwei eigenstaendige Pruefprogramme, **kein Teil des Spiels**:
`work/t3_en03_lib_check.c`, `work/t3_en03_collision_check.c`.
Sie pruefen gegen hergeleitete Originalwerte und -zustandsuebergaenge,
einschliesslich der oben genannten Quirks und der Lueckenmeldungen.

### Einzelpruefung je Datei (Form aus dem Auftrag)

```
/usr/bin/cc -DRECVX_NATIVE=1 -Inative -Ibuild/macos-arm64/generated \
  -Ibuild-tools/ninja -Iinclude/ps2/veronica/prog -O1 -DNDEBUG -std=gnu11 \
  -arch arm64 -mmacosx-version-min=11.0 -Wall -Wextra -fsyntax-only <datei>
```
Ausgabe:
```
OK  native/en03_lib.c
OK  native/en03_collision.c
OK  C++ include        (beide Header unter -std=gnu++17)
```
Keine Warnung, keine Meldung. (Hinweis fuer Nachpruefer: die Shell ist zsh,
dort wird eine unquotierte Variable **nicht** in Woerter geteilt — Flags in
einem Array oder literal uebergeben, sonst scheitert `-I`.)

### Lauf der Pruefprogramme

```
R=/Users/patrickhoffmann/Projects/recvx-decomp
CF=(-DRECVX_NATIVE=1 -I$R/native -I$R/build/macos-arm64/generated \
    -I$R/build-tools/ninja -I$R/include/ps2/veronica/prog -O1 -DNDEBUG \
    -arch arm64 -mmacosx-version-min=11.0 -Wall -Wextra)
/usr/bin/cc  "${CF[@]}" -std=gnu11   -c $R/native/en03_lib.c            -o a1.o
/usr/bin/cc  "${CF[@]}" -std=gnu11   -c $R/native/en03_collision.c      -o c1.o
/usr/bin/cc  "${CF[@]}" -std=gnu11   -c $R/native/ninja_math.c          -o a3.o
/usr/bin/cc  "${CF[@]}" -std=gnu11   -c $R/native/ninja_matrix.c        -o a4.o
/usr/bin/c++ "${CF[@]}" -std=gnu++17 -c $R/native/core_random.cpp       -o a5.o
/usr/bin/cc  "${CF[@]}" -std=gnu11   -c $R/work/t3_en03_lib_check.c       -o a2.o
/usr/bin/cc  "${CF[@]}" -std=gnu11   -c $R/work/t3_en03_collision_check.c -o c2.o
/usr/bin/c++ -arch arm64 -mmacosx-version-min=11.0 -o t3check a1.o a2.o a3.o a4.o a5.o
/usr/bin/c++ -arch arm64 -mmacosx-version-min=11.0 -o t3coll  c1.o c2.o a1.o a3.o a4.o a5.o
```
Ausgabe:
```
en03_lib:       236 Pruefungen, 0 Fehler
en03_collision:  89 Pruefungen, 0 Fehler
```

### Was die 325 Pruefungen abdecken

`en03_lib` (236): Sinustabelle wirkt (`_Make_SinTable`); `NitenDir_ck` und
`bhArcTan2` an vier bzw. fuenf bekannten Richtungen einschliesslich des
`(short)`-Umlaufs des Spiegelzweigs; `bhCdirCheck`-Fenstergrenzen beidseitig;
`ikou` in allen vier Zweigen plus `flg 0x80` plus negatives `add_dir`;
`ikou3` dreht nicht selbst; `bhDGCdirCheck` laesst den Eingabevektor
unberuehrt, `2` ignoriert die Matrixtranslation, `3` in allen vier
Quadranten; `AngleCheck` normalisiert in-place; `PosCheck`-Kanten;
`ChgMtn` alle Felder und Flagloeschungen plus `-1`-Zweig;
`LeverCheck` alle sieben Padkombinationen (deckt die Verwechslung
`= 4` gegen `+= 4` auf); `CheckDirWall`/`SideWall` inkl. `both`-Rueckgabe
und `aox`-Versatz; `CollisionCheckWall` stellt Lage/Radius/Hoehe/`flg 0x100`
wieder her und ruft Stufenanhebung und Wandprobe **je genau einmal**;
`QuickSort`; `GetShapeCnt`; `CalcDamage` mit Gelenk-0-Auslassung,
`correct`-Prozenten, `flg 1`-Gelenk ohne Kombozaehlung, Waffenwechsel
gegen laufenden Timeout, crit+Bonus, `comb_flg 0x2`, Trefferseite 0x4/0x8
und der Startmaske `~0xD`; `HitCheckParts`-Quirk (Wurzel und letztes Gelenk
aus, Abstand 0 greift); `CheckEnemiesBall` Abstossen auf `car+car`,
`stflg 0x2` am anderen Work, `flg 0x8`-Gate, `flg2`-Abschalter und
Spielerplatz erhaelt 0x2 (nicht 0x4); zehn Stichproben von `BloodType` und
vier von `BloodCol`; `SetBlood2` Typen 2/3/4/11 samt der `ofs->z`-Nullung
und dem `dv`-Schreibzugriff, Typ 9/12 tun nichts; `SetBlood`-Tabellenzweig
mit fester Zufallsfolge (Reihenfolge x-dann-y belegt) und Waffe 10/`bt==NULL`
im Zweig ohne Tabelle; `SetBlood4` Blockgrenzen 9/5/3+1 und `ct0`/`ct1`;
Fire/Acid-`flg 0x81`-Unterscheidung; `Bloodstain` legt vier an;
`GetTranslateMtn` Gate, Frame-0- gegen Differenzschluessel, `mode`-Hoehe und
`GetTranslateMtn2`-Betrag; `AddNullTrans` Frameauswahl, Drehung und
**Spielerzweig ueber den Spielerbesitzer**; sowie zehn Faelle, in denen ein
fehlender Dienst eine namentliche Lueckenmeldung erzeugt statt eines Wertes.

`en03_collision` (89): die vier Geometrieprimitiven inkl. unbegrenzter
Gerade, Sonderfall 0 und vorzeichenlosem Ebenenabstand; `bhInOutCheck`
innen/aussen/Kante/`num<3`/Normale-selbst-bestimmen; `wall_box` Mitte-Quirk,
beide x-Seiten, Eckzweig (Abstand genau `ar`, Richtung nach aussen), aussen,
`h==0`-Raumhoehe in beide Richtungen und Lueckenmeldung ohne Raumhoehe;
`wall_ground` duenne Platte trifft, dicke ergibt das leere Band,
`h==0` nutzt **nicht** die Raumhoehe; `wall_cylinder` radialer Schub auf
`w+ar`, aussen, unter der Grundflaeche, Sehnenzweig an der Deckelkante
(Radius kleiner als `w+ar`), Raumhoehe; `wall_slope` Flaechenzweig setzt den
Ebenenabstand auf `ar`, Torwaechter in x, und `id 0` gegen `id 2` belegt die
`id`-Auswertung; `wall_triangle` beide y-Tore, der Treffer-ohne-Verschiebung-
Quirk, Grundriss aussen, Hypotenusenband (Schub auf die **aeussere**
Bandkante `w+ar*sqrt(2)`, Hoehe unberuehrt), jenseits der Hypotenuse und
`id 1` kehrt die Seite um; `box_edge` obere Umrandung, Kasteninneres trifft
nichts, weit weg, senkrechte Ecke und type-7-Quirk; `collision` letzte
Trefferflaeche gewinnt, `collision2` beide Ausschlussrichtungen,
`flg 1`-Gate der Flaeche, `type 1` mit/ohne Work-`flg 0x400` und `type 0`
trotz 0x400, echte Verschiebung des Works, und ohne gebundene Flaechenliste
Luecke + unveraenderte Lage.

### Korrigierte Erwartungen (nicht abgeschwaecht)

Neun Erwartungen meiner Pruefprogramme waren im ersten Lauf falsch; in allen
neun Faellen war die **Implementierung** richtig und die Erwartung wurde
gegen die Originalsemantik belegt korrigiert, nicht gelockert:
`PI*10430.381` schneidet auf 32768 ab; `bhArcTan2`s Spiegelzweig liefert
`(short)(32768-ang)`, fuer `ang 0` also **-32768**; `bhCdirCheck`s Fenster ist
`[-0x4000, 0x3FFF]` (untere Kante inklusive, obere nicht); vier
`CalcDamage`-Faelle brauchten `comb_wep == wpnr_no` **und** laufenden Timeout,
damit die Punkte den Waffenwechsel ueberleben; die Trefferseite war in meinem
Test vertauscht (`dv` zeigt vom Trefferpunkt **zum** Koerper, ein Treffer von
vorn ergibt daher `+Z` und `comb_flg 0x4`); und der Dreiecksschub geht auf die
**aeussere** Bandkante, nicht auf die Hypotenuse.

### Was ausdruecklich UNGEPRUEFT bleibt

* **Kein Laufzeit-/Spielnachweis.** Die beiden Dateien sind nicht in
  `CMakeLists.txt`, laufen in keinem Spielbild und in keinem Replay. Es gibt
  keinen Nachweis gegen echte Raumdaten, echte Gegnerwerte oder PCSX2.
* **Keine Windows-Pruefung.** Kein mingw-Crosscompiler im PATH
  (`x86_64-w64-mingw32-gcc not found`). Es wird nichts fuer Windows behauptet.
* **Kein ASan/UBSan**, kein CTest-Lauf, kein Startpaket, kein Commit/Push.
* **Die Effektwege sind nur bis zur Dienstgrenze geprueft.** Was ein echtes
  `effect_tb`/`effect_particle` daraus macht, ist nicht geprueft — die Tests
  pruefen die uebergebenen Vorlagensaetze, nicht die Darstellung.
* Die Zufallsreihenfolge ist nur in `SetBlood` mit fester Folge belegt; in
  `SetDFireEffect2` und `SetSanEffect2` ist sie im Code dokumentiert
  (Zeit, Groesse, x, y), aber nicht durch einen Test festgenagelt.
* `recvx_t3_calc_parts_pos` und `recvx_t3_get_parts_pos` sind **nicht**
  durch einen Test gedeckt (sie brauchen `node_transform`); nur ihr
  Lueckenpfad ist belegt.

---

## (d) Offene Portierungsluecken, namentlich mit Fundstelle

### D1 — Keine der acht Identitaeten ist portiert

| ID | Quelle | Zeilen | Stand |
|----|--------|--------|-------|
| EN03 | en03.c | 7432 | nur die Kollisionsfamilie (:5203-6072). Zustandsmaschine offen: `bhEne03` (:478), `_Init` (:625), `_Brain`/`BR00..02` (:886-1144), `_Move`/`MV00..MV19` (:1145-2700), `_Nage`/`NG00..01` (:2701-3215), `_Damage`/`DG00..DG12` (:3216-4228), `_Die`/`DD00..DD05` (:4229-4824), Helfer `_MakeMatrix` (:4951), `_HideParts` (:5037), `_CollisionWalls` (:5064), `_CollisionBoxEdge2` (:6074), `_CollisionLine`/`2` (:6199/:6216), `_DiveSpace` (:6278), `_DamageInit` (:6366), `_CheckJumpSpace` (:6715), `_PlayerControl` (:6798), `_CallSE` (:7117), `_Acid` (:7275), `_HitMark` (:7353) |
| EN03s | en03sub.c | 484 | offen, **blockiert** (siehe D2) |
| EN05 | en05.c + en05sub.c | 6425 | offen |
| EN09 | en09.c | 7741 | offen |
| EN15 + EN53 | en15.c (EN53 ab :3726) | 3745 | offen |
| EN19 | en19.c | 4825 | offen |
| EN22 | en22.c | 3071 | offen |
| EN23 | en23.c | 4028 | offen |
| EN26 | en26.c | 3450 | offen |

Die 67 Works aus den Raumdaten sind damit weiterhin unbedient; `bhEne03`
bleibt im Dispatch NULL, und der Laufzeittausch von Slot 31 auf EN03s
(`bhEne_SetCallFunc`, eneset.c:933) ist nicht ausgefuehrt.

### D2 — EN03s ist durch zwei fehlende Ninja-Dienste blockiert

`njRotate` (Achse+Winkel) und `njArcSin` fehlen in `native/ninja_matrix.h`
bzw. `native/ninja_math.h`. En03sub.c braucht beide an vier Stellen
(:142 `njRotate(NULL, &EXP0_F(0xB8), epw->azp)`, :178 `njArcSin(out)`,
:181 `njRotate(NULL, &ov, ang / epw->ct3)`, :474 `njRotateX` mit
`NJM_DEG_ANG(360)*njRandom()`). Beides sind allgemeine Ninja-Dienste und
liegen ausserhalb meiner Dateien — siehe Patchvorschlag **P2**. Ich habe
keinen Ersatz erfunden und die ID nicht angebrochen.

### D3 — Ungebundene Dienste, die meine fertigen Teile namentlich melden

Alle folgenden sind bewusst NULL-faehig; der Mechanismus meldet die Luecke
und tut nichts (keine Heuristik, kein geratener Wert, kein stilles No-op,
sondern eine benannte Meldung mit Originalfundstelle):

* Effektsystem: `effect_tb` / `effect_patch` / `effect_particle` /
  `effect_particle_mk2` / `effect_erase_arrow` (effect.c, effsub3.c) —
  **die gesamte Blut-/Feuer-/Saeure-/Hackfleischfamilie zeichnet ohne sie
  nichts.**
* Modell/Knoten: `node_transform` (`mlwP->objP[n].pos/.ang`),
  `model_texp`, `model_texp_index`.
* Welt: `wal_count`/`wal_at` (rom->walp + sys->mwalp), `room_height`
  (rom->h), `flr_count`/`flr_at`, `etc_count`/`etc_at`, `dansa`
  (hitchk.c:5391), `water_line` (bhCheckL2Water).
* Spieler: `player_get`/`player_set`, `player_joint_matrix`,
  `player_obj_num`, `player_hit_set`/`player_hit_get`, `player_watr`,
  `player_way`, `player_id`, `player_body`, `player_stflg_or`,
  `sphere_capsule` (njplus.c npCollisionCheckSC).
* Eingabe/Audio/Haptik: `pad_ps`, `vibration`, `player_voice_free`,
  `effect_se`, `effect_se_playing`.
* Gegnerliste: `ene_count`/`ene_at`/`ene_is_player`.
* Kinder/Dispatch: `spawn_enemy` (eneset.c:170), `set_call_func`
  (eneset.c:933).
* Licht: `light_get`/`light_set` (rom->lgtp, von EN22/EN15 geschrieben).

### D4 — Zwei Stellen, an denen das Original ungesetzten Speicher liest

Ich erfinde dort keinen Wert, sondern melde die Luecke:

1. `bhEne_SetDFireEffect2` (zonzon.c:910): bei erfolgloser Wasserprobe liest
   das Original eine ungesetzte Stapelgroesse (der anschliessende Test auf die
   Adresse einer lokalen Variablen kann nie greifen). Mein Port meldet
   `zonzon.c:910 ... ohne Treffer` und laesst `shp_ct` **ungeschrieben**.
2. `bhEne03_CollisionWallSlope` (en03.c:5739): zwei gesetzte Randmarken, die
   kein x/z-Paar bilden (bei negativer Breite/Tiefe moeglich), lassen den
   Eckindex im Original ungesetzt. Mein Port meldet
   `en03.c:5739 Eckindex der Schraege ungesetzt` und gibt 0 zurueck.

### D5 — Semantische Restunsicherheit

* `bhEne_GetPartsPos`/`CalcPartsPos` benutzen im Original `njRotateEx` auf
  dem Ex-Matrixstapel; ich verwende lokale `NJS_MATRIX` mit `njRotateXYZ`.
  Rotationsreihenfolge und Rechenweg sind gleich, der globale
  Zwischenspeicher entfaellt. Das ist als technische Anpassung dokumentiert,
  aber **nicht** gegen die Disassembly abgeglichen.
* `bhEne_CheckEnemiesBall`s Zweig `epw == plp` (setzt `stflg 0x4` am anderen
  Work, zonzon1.c:578) ist von meinem Eintrittspunkt aus unerreichbar und
  gehoert dem Spielerbesitzer. Er ist **nicht** portiert.
* `recvx_t3_calc_damage` begrenzt auf `dam[64]` (types.h:354) und meldet ein
  Modell mit mehr Knoten. Mit gueltigen Daten kann das nicht eintreten
  (die Effektwege begrenzen auf 42), aber es ist eine Grenze, die das
  Original nicht hat.

---

## Dateien

Neu von mir:
* `native/en03_lib.c` — die geteilte Bibliothek
* `native/en03_collision.c` / `native/en03_collision.h` — Dienst
  `collision_works`
* `work/t3_en03_lib_check.c`, `work/t3_en03_collision_check.c` —
  Pruefprogramme (nicht Teil des Spiels)

Geaendert von mir: `native/en03_lib.h` (mein Vertrag, vier belegte
Korrekturen + Ergaenzungen, siehe A3).

Nichts anderes angefasst. Kein Commit, kein Push, kein Startpaket.
`git status --short` zeigt ausser meinen Dateien nur parallele Fremdarbeit
(`subplayer.*`, `enemy_spawn.*`, `effect_particles.cpp`, `runtime.cpp`,
`tools/check_no_original_core.py`), die ich nicht beruehrt habe.
