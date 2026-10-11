# C03 — EN01 (Zombie): Unterzustände vollständig portiert (Claude, 2026-10-11)

Fortsetzung von C03a. `native/enemies/en01_core.c` ist die einzige Datei, die
dieses Paket anfasst; dazu kommt der eigene Test
`tests/native/en01_core_test.c`. Gemeinsame Anschlussdateien
(`CMakeLists.txt`, `native/world*.cpp`, `native/enemies/enemy_work.h`,
`AGENTS.md`) bleiben beim Integrator und bei der laufenden C02-Sitzung; für
sie liegen Patches bei. **Kein Commit, kein Push, kein Paket.**

Pflichtprüfer vor jedem Build ausgeführt: `tools/check_no_original_core.py`
→ *PASS: audited source/build/binary scope has no forbidden core*.

## Stand

`en01_core.c` ist von 6330 auf **11492 Zeilen** gewachsen; 113 öffentliche
`recvx_en01_*`-Funktionen, **2975 Zeilenverweise** auf `en01.c` bzw.
`en01b.c`. **Die Linklücke ist geschlossen** — `nm -u` gegen
`en01_core.o` + `en01_tables.o` lässt nur noch libc und die Ninja-Mathematik
offen, kein `recvx_*`-Symbol mehr.

### Neu in diesem Durchgang: 32 Funktionen

| Original | Nativ | Was der Zustand tut |
|---|---|---|
| `bhEne01_DG00` en01.c:8143 | `recvx_en01_dg00` | Treffer im Stehen, MTN 20/21 je exp0-Bit 0x2000 |
| `bhEne01_DG01` :8192 | `recvx_en01_dg01` | Treffer mit Rückkehr in Lauf oder Renndrehung |
| `bhEne01_DG02` :8255 | `recvx_en01_dg02` | wie DG01, zusätzlich Kappenwurf (FlyingCap 2/3) |
| `bhEne01_DG03` :8342 | `recvx_en01_dg03` | Rückwärtstaumel mit ZulzulCheck-Sturz; kein `case 4` |
| `bhEne01_DG04` :8570 | `recvx_en01_dg04` | Vorwärtstaumel, Klippensturz, Schubphasen; kein `case 2` |
| `bhEne01_DG05` :8865 | `recvx_en01_dg05` | Oberkörper abgetrennt, Übergabe an DG05B |
| `bhEne01_DG06` :8912 | `recvx_en01_dg06` | Konterbiss auf Frame 26 oder Abbruch in mode2 10 |
| `bhEne01_DG07` :9006 | `recvx_en01_dg07` | Treppensturz, Etage aus der Sturzhöhe |
| `bhEne01_DG08` :9145 | `recvx_en01_dg08` | langer Treppensturz: Anlauf, Flug, Landung |
| `bhEne01_DG09` :9381 | `recvx_en01_dg09` | Oberkörper im Lauf ab, Rumpfmotion nach laufender MTN |
| `bhEne01_DG10` :9467 | `recvx_en01_dg10` | Niederschlag mit Erholung |
| `bhEne01_DG11` :9587 | `recvx_en01_dg11` | Niederschlag ohne Erholung |
| `bhEne01_DG14` :9657 | `recvx_en01_dg14` | Kantensturz mit Abtastung des Fallbogens |
| `bhEne01_DG15` :9807 | `recvx_en01_dg15` | kurzes Weiterlaufen nach dem Sturz |
| `bhEne01_DG16` :9837 | `recvx_en01_dg16` | kurzer Treffer im Stehen (MTN 129/130) |
| `bhEne01_DD00` :9934 | `recvx_en01_dd00` | Tod mit Blutlache oder Absinkbahn im Wasser |
| `bhEne01_DD01` :10106 | `recvx_en01_dd01` | Tod ohne Lache |
| `bhEne01_DD02` :10132 | `recvx_en01_dd02` | Tod aus Knoten 1, Wasserversatz −1.0 |
| `bhEne01_DD03` :10218 | `recvx_en01_dd03` | Verschwinden nach unten (py −100) |
| `bhEne01_NG00` :6753 | `recvx_en01_ng00` | Griff + Biss, Hebelrütteln, Rückstoß, Sturzvarianten |
| `bhEne01_NG01` :7125 | `recvx_en01_ng01` | abgebrochener Griff |
| `bhEne01_NG02` :7325 | `recvx_en01_ng02` | Nackenbiss, immer tödlich, Kopf ab |
| `bhEne01_MV13B` en01b.c:211 | `recvx_en01_mv13_b` | Oberkörper dreht auf das ikou-Ziel |
| `bhEne01_NG03B` :247 | `recvx_en01_ng03_b` | Oberkörper hält den Spieler, Kopfabtrennung |
| `bhEne01_DG05B` :402 | `recvx_en01_dg05_b` | Oberkörper löst sich, Abtrennmotion 224 |
| `bhEne01_DG09B` :510 | `recvx_en01_dg09_b` | wie DG05B ohne eigenen Motionwechsel |
| `bhEne01_DG12B` :656 | `recvx_en01_dg12_b` | kurzes Taumeln, hp-Zweig in den Tod |
| `bhEne01_DG13B` :707 | `recvx_en01_dg13_b` | wie DG12B, immer tödlich |
| `bhEne01_DD02B` :739 | `recvx_en01_dd02_b` | Tod des abgetrennten Oberkörpers |
| `bhEne01_PlyDG00` en01.c:7509 | `recvx_en01_ply_dg00` | Spielerseite des Griffs, 9 Teilzustände |
| `bhEne01_PlyDG01` :7784 | `recvx_en01_ply_dg01` | Spielerseite des Nackenbisses |
| `bhEne01_EnemyPushChk` :8008 | `recvx_en01_enemy_push_chk` | Losreißen schiebt umstehende Zombies |

Damit sind alle vier mode2-Verteiler echt besetzt. Es bleiben genau die drei
**echten NULL-Plätze des Originals**: `DamageMode2W[12]` und `[13]`
(en01.c:2340-2341) sowie `NageMode2W[3]` (:2303); sie melden sich im Wortlaut
als `== NULL`, unterschieden von unportierten Stellen. `MoveMode2W[13]`
(:2270) war schon vorher so behandelt.

Die vier B-Tabellen sind im Original fast vollständig mit `bhEne01_Dummy`
(en01b.c:144, leerer Rumpf) belegt; diese Plätze führen den Dummy aus und
melden ausdrücklich **nichts**.

## Originaldefekte, die erhalten bleiben (nicht geglättet)

- `if (&gpos == NULL)` / `if (&pd == NULL)` — Adresse einer lokalen Variablen,
  nie wahr. Der Zweig bleibt unerreichbar (en01.c:10017, :10161, en01b.c:776).
- `DD00` hat **kein** `case 3`, `DG03` kein `case 4`, `DG04` kein `case 2`,
  `NG00` kein `case 7`. Übernommen, nicht aufgefüllt.
- Fehlende `break` sind absichtlicher Durchfall, in jedem Fall mit
  Zeilenverweis kommentiert. Besonders `DG12B` (en01b.c:683) und `DG10`
  (:9510): der Durchfall geht auf den **textuell** nächsten `case`, auch wenn
  der hp-Zweig `mode3` gerade auf 2 gesetzt hat. Dort ist die Framebedingung
  direkt nach dem Motionwechsel nie erfüllt, der Pass bleibt wirkungslos.
  Ein `goto` auf `case 2` wäre eine Verhaltensänderung gewesen.
- `DG08` setzt `spd` zweimal hintereinander (0.41 dann 0.375, en01.c:9212;
  0.45 dann 0.34444442, :9220); die zweite Zuweisung gilt. Beide bleiben.
- `DG08` mode3 1 wechselt im `ct0 > 0`-Zweig die Motion **zweimal** auf
  denselben Index: erst mit `~0x3000000` (:9226), dann noch einmal mit
  `~0x2000000 | 0x1000000` (:9271, außerhalb des `if/else`). Erhalten.
- `DG03` mode3 3 setzt `mode1` erst auf 0 und unmittelbar danach auf 5
  (:8517/:8520); `NG02` mode3 3 genauso auf 0 dann 2 (:7494/:7497). Die
  erste Zuweisung ist wirkungslos und bleibt stehen.
- `NG02` mode3 2 setzt `plp->ct1` in beiden Zweigen auf 5 — die Abfrage auf
  `ct0 == 0` davor ändert nichts (:7430-7437).
- `NG00` prüft `frm == 20` zweimal hintereinander in getrennten `if`
  (:6898 und :6903).
- `DG04` mode3 5 prüft `(frm >= 0) && (frm < 22)` auf einem vorzeichenlosen
  Wert; die linke Hälfte ist immer wahr (:8752).
- `npCutSkin` (njplus.c:1746) hat einen **leeren** Originalrumpf. Die zwei
  Aufrufe in DG05B/DG09B bleiben als Kommentar stehen und sind **keine**
  Portierungslücke.
- `bhEne01_SePlay` nimmt im Original eine Position entgegen und benutzt sie
  nicht (en01.c:11368). Der native Aufruf lässt sie weg.
- `DG14` sucht die Zieletage mit `for (i = 0; TRUE; i++)` **ohne eigene
  Obergrenze**; die Schleife endet allein über die Bodendaten des Raums. Das
  bleibt unverändert — eine erfundene Obergrenze wäre ein Sonderfall-Fix.
  Fehlt einer der beiden Dienste (`bhGetGroundPosition`, `bhCheckFloorNum`),
  wird gemeldet und **gar nicht gesucht**, damit der Lauf nicht auf einem
  fehlenden Dienst dreht.

## Nachweise

macOS arm64, ohne CMake gebaut (die Datei ist noch nicht registriert, siehe
`work/native_port_C03_states.patch`):

- **Strenge Übersetzung sauber**, keine Warnung:
  ```
  cc -c -Wall -Wextra -Wno-unused-parameter -I native -I native/enemies \
     native/enemies/en01_core.c -o en01_core.o
  ```
- **Linklücke geschlossen.** `nm -u` auf `en01_core.o`, abgezogen die
  definierten Symbole von `en01_core.o` + `en01_tables.o`, lässt nur
  `njSin/njCos/njCalcPoint/njCalcVector/njRotateXYZ/njRotateY/njTranslate/`
  `njUnitMatrix/njUnitVector/njInnerProduct/njGetTranslation/njSqrt`,
  `atan2f`, `memcpy/memset/bzero` und die Stack-Protector-Symbole offen.
  Vor diesem Durchgang fehlten noch `recvx_en01_ply_dg00/01`.
- **Komponententest: 196 Prüfungen, 0 Fehler.**
  `tests/native/en01_core_test.c` gegen eigene Dienststummel, ohne
  Originaldaten. Neu darin: `test_damage_states` (DG00 vorne/hinten/ohne
  Oberkörper, DG02-Modusgruppe, DG16 mit dem anderen Motionwechsel,
  DG10/DG11-Motionwahl, DG10 hp<0, die NULL-Plätze 12/13, der +10-Versatz von
  DGType02), `test_b_family` (MV13B-Flags, DG12B hp-Zweig und die
  lkwkp-Lücke, DG13B, DG05B-Ablösung mit Lageübernahme, Dummy meldet nichts)
  und `test_grab_states` (NG02-Spielerzustand und Startframe, Kopfabtrennung
  MTN 36/35 mit SE 4877/4869, das fehlende `plp->ct1`, NageMode2W[3],
  PlyDG00 mode3 0/2 und ply_id außerhalb der Tabelle).

  Drei Prüfungen des vorbestehenden Tests waren **falsch erwartet** und sind
  gegen die Originaldaten korrigiert, nicht abgeschwächt:
  1. `en01_PersonalType[10].add_hp` ist **0**, nicht 20 (en01.c:1072) — die
     HP bleiben bei 40, nicht 48. Nur Subtyp 9 trägt dort 20.
  2. `en01_mtn_tbl3` hat 15 Zeilen (8, 85, 120, 121, 9, 43, 86, 36, 6, 35,
     5, 7, 34, 46, 111); **MTN 50 steht nicht darin**. Der Test prüft jetzt
     MTN 7 Frame 13 und zusätzlich, dass `mode0 >= 5` die Tabelle sperrt
     (en01.c:10709) und MTN 50 nichts auslöst.
  3. Der NULL-Platz `MoveMode2W[13]` ist nur über **Typ 8** erreichbar:
     `MVType00` überspringt für `type == 8` Brain00 und ActionModeCheck
     (en01.c:4873); bei jedem anderen Typ setzt ActionModeCheck `mode2`
     vorher um. Außerdem fehlte dem Test der Stummel für
     `plp->mlwP->owP[0]`, den `bhEne01_Move` für den Spielerabstand braucht
     (:2840) — ohne ihn kam der Verteiler nie an.

- **Pflichtprüfer PASS** nach jedem der vier Blöcke (B-Familie, DG00–DG04,
  DG05–DG09, DG10–DG16, NG, PlyDG).

**Nicht nachgewiesen:** kein Laufzeitnachweis, kein Spielverhalten, kein
Raumscan, kein Paketbau. `jump[1]` ist weiterhin **nicht** umgelegt; die
World fährt EN01 noch über den Legacy-Store. Das ist C03c und braucht erst
die beiden Spielerfelder unten.

## Offen / blockiert — konkrete Anschlussaufgaben

### A. Zwei Felder in `RECVX_ENEMY_PLAYER` (`native/enemies/enemy_work.h`, C02)

Im Original ist der Spieler derselbe `BH_PWORK` wie ein Gegner.
`RECVX_ENEMY_WORK` führt beide Felder schon (enemy_work.h:183/196); die
Spielersicht noch nicht.

1. **`int32_t ayp`** — Drehratenspeicher **über Frames hinweg** für
   `PlyDG00`/`PlyDG01` (en01.c:7545/7559/7566, :7851/7883/7903). Ohne das
   Feld bleibt der Drehanteil des Griffs aus; alles andere der beiden
   Zustände läuft. Gemeldet als `plp->ayp lesen/schreiben`.
2. **`int32_t ct1`** — `NG00` (en01.c:6876) und `NG02` (:7432/:7436) setzen
   ihn. Gemeldet als `plp->ct1 = 0/5`.

Der `ayp`-Bedarf ist bei der laufenden C02-Sitzung eingereiht
(message_id 53b4c2d6-1f5e-4b65-846e-a6a0471a2948, Zustellung bestätigt,
keine Antwort). `ct1` ist heute dazugekommen und noch nicht gemeldet.
**Es wurde kein zweites Spielerwerk gebaut und kein Ersatzwert erfunden.**

### B. Fehlende Dienste im EN01-/Gegnervertrag

| Original | Gebraucht von | Lage |
|---|---|---|
| `bhSetBloodPoolLnk` effsub3.c:145 | DD00, DD02, DD02B | Der EN01-Vertrag kennt nur `bhLinkBlood` (effect.c:736), `bhEne_SetBlood3` und `bhEne_SetBlood4` — andere Originalroutinen. Es entsteht keine Ersatzlache. |
| `bhSetEffParticle` effsub3.c | DG09B (zweimal, en01b.c:565) | Kein Dienst. Die Feldschreibungen (`dvx/dvy/dvz`, `djnt_no`) darüber laufen originalgetreu. |
| `eff[shadow_slot].jno[3] = -1` | DG05 (:8878), DG09 (:9437) | Der Vertrag hat nur `set_shadow` (effect.c:644) und keinen Schreibweg in einen bestehenden `eff[]`-Slot. Das Original rechnet dort außerdem ungeprüft mit `shadow_slot == -1` und schreibt dann **vor** den Pool; nativ wird das gemeldet, nicht nachgebaut. |
| `sys->pad_on &= ~0xF` | DG06 (:8956), PlyDG00 (:7643/7755/7767), PlyDG01 (:7928) | Nur das Bundle `player_hurt_state` deckt das Padwort ab, kein eigener Weg. |
| `mlwP->objP[0].pos[1] = 0` | MV08 (:6311) | Nur Leseweg `node_local` / `node_local_clear_xz` vorhanden. |
| `plp->exp0 + 0x48 / + 0x50` | PlyDG01 (:7920) | Felder des Spieler-exp0; kein Weg im Vertrag. |
| `bhSetMotion(plp, …)` | PlyDG01 (:7943) | `set_motion` nimmt einen Gegnerwork, nicht die Spielersicht. |
| `sepw->exp0[0/4] = &objP[11].evalflags / &owP[11].flg` | NG03B (en01b.c:328) | Das Kopf-Kindwork speichert dort **Adressen** von Feldern des Hauptkörpers. Der Vertrag hat nur `model_node_hidden` (evalflags Bit 0x8) und keinen Weg, eine fremde Feldadresse zu übergeben. |
| `mlwP->owP[0].flg \|= 3` | FlyingCap (:11910) | wie oben: nur `model_node_hidden`. |
| `stair_area` muss zusätzlich den ATR-Satz liefern | MV08 (:6115-6143) | Der Dienst gibt nur den Index; der Zweig liest `prm0/prm1/prm2` aus dem Originalzeiger. Schon in en01_work.h:200-206 vermerkt. |

Alle diese Stellen melden ihren **Originalnamen** über `en01_gap()` und
ändern nichts. Insgesamt stehen 478 solche Meldungen in der Datei; die große
Mehrheit sind Dienste, die mit C03c gebunden werden (`plp`, `owP`-Matrizen,
`bhAddSpeed`, `bhCheckFloorNum`, `rom->grand`, die Blut- und Effektdienste),
nicht Vertragslücken.

### C. Nächste Schritte dieses Pakets

1. **C03c** — `native/world_enemies.cpp`: `RECVX_EN01_CONTEXT` setzen
   (`table`, `services`, `en01`, `game_mode`), `recvx_en01_set_data`
   (`recvx_en01_default_data()`), die ~40 Felder von
   `RECVX_EN01_SERVICES` an die vorhandenen nativen Anbieter binden, dann
   `d->jump[1]` umlegen. Nachweis soll ein Zustandsabgleich pro Frame
   zwischen Legacy und nativ unter `--enemy-ai` plus `verify_rooms.py`
   („Enemy gaps“: `bhEne01` muss verschwinden, keine neuen Namen) werden.
   Liegt als Patch vor, sobald A.1/A.2 im Vertrag stehen.
2. **C04** — `native/enemies/en04.c` gegen den fertigen `en04.h`-Vertrag;
   die zwölf Tabellen als Statics übertragen (nur `en04_hp_tbl` ist über
   `set_data` vorhanden), dann `d->jump[4]`.
3. **CMake** — `work/native_port_C03_states.patch`, drei Teile.

## Grenzen dieser Meldung

Komponentennachweis (strenge Übersetzung, Linkprüfung, 196 Testprüfungen,
Pflichtprüfer) ist erbracht. **Laufzeit- und Plattformnachweis nicht:** kein
CMake-Build, kein CTest-Lauf, kein Raumscan, kein Windows-Build, kein
Spieldurchlauf, kein Paket. EN01 läuft produktiv weiterhin über den Legacy-
Store; dieses Paket macht die Zustandsmaschine vollständig und linkbar, nicht
produktiv aktiv.
