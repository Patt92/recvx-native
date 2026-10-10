# T5 — Subplayer-Pfad (Begleit-/Szenenfiguren) vollständig

Stand 2026-10-10. Besitz: `native/subplayer.c`, `native/subplayer.h`.
Neu angelegt: `tests/native/subplayer_test.cpp` (bislang nicht in CMake, siehe (b)).
Kein Commit, kein Push, kein Startpaket. Keine fremde Datei geändert.

`python3 tools/check_no_original_core.py` → **PASS**, vor der Arbeit und nach
jeder Änderung ausgeführt. Keine der gesperrten Bezeichner steht in Code,
Kommentar oder String; `gap`-Meldungen nennen ausschließlich Originalfundstellen
(`subpl:…(subpl.c:NNN)`, `Motion.c:21`, `MdlPut.c:240`).

---

## 0. Messung der tatsächlich vorkommenden Identitäten

Selbst aus den 205 Raumdateien bestimmt, nicht aus Berichten übernommen.
Quelle `data/archives/RDX_LNK/*.bin`; jede Datei ist ein Expand-Strom (die
Python-Messung bildet `native/expand.cpp` nach), danach Header `u32(b,16)`,
Anzahl bei `+0x88`, Satzzeiger bei `+8`, 36-Byte-Sätze, Filter `flg & 1`,
Identität `u16(+4)`, Typ `u16(+6)`, `mdlver u8(+9)`.

```
files=205 bad=0
active enemy works total=759
SUBPLAYER distinct ids=45  works=195
  id  91 works  38 rooms 38      id  92 works  22 rooms 22
  id  93 works  17 rooms 17      id  67 works  14 rooms 13
  id  43 works   8              id  97 works   8
  id  59 works   7 (mdlver 0/1/2)  id  69 works   7 (mdlver 0/1/2)
  id  95 works   6   id  96 works 6   id  77 works 5   id  98 works 5
  id  60/76 works 4   id 58/83 works 3
  id  52/62/65/73/75/81/87/88/94 works 2
  id  42/44/47/49/51/61/63/66/68/70/72/74/78/79/82/84/85/89/90/99 works 1
```

**45 Identitäten, 195 Works** — identisch zur Auftragsangabe, einschließlich
der Spitzenwerte. Alle 195 haben `type == 0`; `mdlver` ist 0 außer bei 52, 59,
66, 69, 76, 96 (dort auch 1 bzw. 2).

Drei Befunde aus derselben Messung, die den Port betreffen:

1. **Die kleinste vorkommende Identität ist 42, die größte 99.** Der
   Originalindex `id - 40` (subpl.c:91) bleibt damit in allen 205 Räumen
   innerhalb der 120 Tabelleneinträge. Die Identitäten 31..39, die
   `eneset.c:84` ebenfalls auf diesen Apparat leitet, ergäben im Original
   einen **negativen** Index; kein Raum löst das aus. Der Fall ist im Port
   als Fehler gemeldet, nicht geraten.
2. **Nur 7 der 195 Works haben einen Motionzeiger** in der Raum-Motiontabelle
   (`u32(b,24) + i*4`): die Identitäten 61, 65, 75, 88. Die anderen 188 haben
   einen genullten `MN_WORK`. Damit ist die CLAUDE.md-Aussage „leere
   Original-MN_WORK-Tabellen behalten die letzte Pose" **am Original belegt**:
   `bhSetMotion` (Motion.c:29) hat seinen gesamten Rumpf hinter
   `mnwP[mtn_no].datP != NULL`; ohne Daten passiert nichts und der Frame
   bleibt stehen. Für diese 188 Figuren kommt die Motion nur über die
   Raumliste (`sys->plmthp`) oder gar nicht.
3. **Keine per-ID-Logik im Original.** `subpl.c` liest `epw->id` an genau zwei
   Stellen: `subpl.c:91` (Tabellenindex) und `subpl.c:112` (`id == 199`). Alle
   120 Einträge von `subpl_init_tbl` zeigen auf dieselbe Routine. Die 45
   Identitäten unterscheiden sich daher **ausschließlich** in Raumdaten
   (Modell/Motionbibliothek) und Skriptparametern (`mode0/mode1/ct0..ct3`),
   nicht im Code. Es gibt also keine 45 Einzelportierungen zu leisten; die
   „Abarbeitung nach Häufigkeit" fällt zu einem einzigen Apparat zusammen.
   Zusatzunterschied außerhalb von `subpl.c`: `eneset.c:372` ruft für
   Identitäten **> 90** vorher `bhControlMask` — das trifft 91..99, also 110
   der 195 Works, und gehört dem Masken-/Lippenbesitzer (`native/face.*`).

Messskripte liegen unter `$TMPDIR/t5meas/` (Wegwerfwerkzeuge, nicht im Baum).

---

## (a) Implementierung

`native/subplayer.c` war eine 32-zeilige Lücke mit neutralen Rückgaben
(`recvx_is_subplayer` lieferte 0, daher band `world.cpp` überhaupt keine
Begleitfigur). Jetzt ist der ganze Apparat eigenständig nativ bestimmt.

### Gerüst

| Original | nativ | Anmerkung |
|---|---|---|
| `subpl.c:77` Rahmen | `recvx_subplayer_tick` | Statusbits, Dispatch, Abschluss |
| `subpl.c:12` mode0-Tabelle (7) | `subpl_dispatch_mode0` | Index 2/3/4 sind NULL |
| `subpl.c:23` Init-Tabelle (120) | `recvx_subplayer_init_index` + `subpl_shared_init` | alle 120 Einträge gleich |
| `subpl.c:38` mode2-Tabelle (1) | `subpl_move` | nur mode2 0 ist belegt |
| `subpl.c:39` ct2-Tabelle A (15) | `subpl_mv00_tbl` | echte Zeigertabelle |
| `subpl.c:58` ct2-Tabelle B (15) | `subpl_mv01_tbl` | echte Zeigertabelle |
| `subpl.c:131/137/143/150` | `subpl_init_entry` / `subpl_event` / `subpl_event2` | 5/6 teilen einen Zweig |
| `subpl.c:95` Initialisierung | `subpl_shared_init` | Motion 0, Schritt 32768, mode3 3 |
| `subpl.c:153…392` elf Zweige | `subpl_mv00 … subpl_mv10` | einzeln, nicht zusammengefasst |
| `subpl.c:371` Winkelgrenze | `recvx_subplayer_dir_target` | öffentlich, weil `pl_evt.c:577/624` sie am Spielerwork nutzt |
| `pwksub.c:133` `bhAddSpeed` | `subpl_add_speed` | rein lokal, daher hier enthalten |
| `subpl.c:410` Identität 28 | `recvx_subplayer_enemy28` | **bleibt leer**, kein Versehen |

Die Tabellen bleiben echte Zeigerfelder mit ihren Mehrfachbelegungen
(Tabelle A: Plätze 7, 8, 11..14 teilen den Zweig von Platz 0; Tabelle B hat
dieselben 15 Plätze mit anderer Belegung). Die NULL-Plätze der mode0-Tabelle
werden **nicht** zu einem stillen No-op: `mode0` 2/3/4 meldet
`subpl:mode0-null-slot (subpl.c:15)` und gibt `RECVX_SUBPL_UNHANDLED` zurück,
weil das Original dort auf die Adresse null springt und `bhCalcModel`
(subpl.c:84) folglich nie erreicht. Dasselbe gilt für `mode0 > 6`,
`mode2 != 0` und `ct2` außerhalb 0..14.

### Erhaltene Eigenheiten des Originals (kommentiert, nicht korrigiert)

- **Durchfallen.** Sieben Zweige verlassen den Eintrittszustand ohne `break`
  und laufen im **gleichen** Pass noch durch den laufenden Zustand
  (`subpl_mv00/00_ex/01/03/03_0/05/06/00_1`); vier brechen ab
  (`subpl_mv02/02_0/10` und der Vollständigkeit halber `subpl_mv02`). Die
  Prüfung deckt genau das ab.
- **Reihenfolge mit Seiteneffekt.** In `subpl_mv01` wird zuerst `ay` gedreht,
  dann `spd` aus `ct3` gesetzt, dann `bhSetMotion`, dann `bhAddSpeed` — der
  Versatz benutzt also den in diesem Pass bereits geänderten Winkel und die
  neue Geschwindigkeit. `subpl_mv03/03_0` lassen `bhSetMotion` weg: die Figur
  gleitet in gehaltener Pose.
- **Vorzeichenasymmetrie in `ct0`.** `subpl.c:201` liest das untere Halbwort
  des gepackten Ziels **vorzeichenlos** (0..65535), `subpl.c:260` und `:284`
  **vorzeichenbehaftet**. Ein negatives Z-Ziel ist daher nur über
  `subpl_mv03/03_0` erreichbar. Beides ist getrennt implementiert
  (`subpl_target_z_unsigned` / `subpl_target_z_signed`) und wird geprüft.
- **Nur `subpl.c` benutzt `>> 16`.** `pl_evt.c:577/624` rufen dieselbe
  Winkelgrenze mit `(short)(ct0 / 65536)` auf. Für negative `ct0` mit
  nichtleerem unteren Halbwort unterscheiden sich Division und Verschiebung;
  der Port behält je Aufrufstelle die dortige Form. Das obere Halbwort wird
  über den vorzeichenlosen Schub gewonnen, damit das Ergebnis nicht von der
  implementierungsabhängigen Verschiebung negativer Werte abhängt — der
  Halbwortinhalt ist identisch.
- **Winkelkonvention.** `atan2f(px - x, pz - z)` zeigt vom Ziel weg; dieselbe
  Minus-Konvention benutzt `bhAddSpeed`. Unverändert übernommen.
- **`mode3 = 3` nach der Initialisierung** ist ein Wert, den **kein** Zweig
  behandelt. Eine frisch geladene Begleitfigur hält also ihre Pose, bis ein
  Raumskript `mode3` auf 0 setzt. Das ist Absicht, kein Stillstand-Bug.
- **Bytebreite der Modi** (types.h:238). `RECVX_SUBPLAYER` führt sie als
  `int`, weil der vorhandene Besitzer sie so schreibt; jeder Lesezugriff
  maskiert auf ein Byte. `mode0 = 256` ist damit derselbe Zustand wie 0 —
  geprüft.
- **Statusbits** (subpl.c:78-81), mit belegter Wirkung:
  `0x8000` nimmt die Figur aus dem Zielbarkeitsfilter (`pwksub.c:445` prüft
  genau dieses Bit) — Begleitfiguren sind nicht anzielbar;
  `0x0100` wählt die Wandkollisionsvariante „von oben" (hitchk.c:469/562/629
  und gleichlautende Stellen);
  `~0x2` löscht in **jedem** Pass das Schadens-/Totbit.

### Keine zweite Workwelt

Der Apparat arbeitet auf `RECVX_SUBPLAYER`, das als Member **in** der
vorhandenen `WorldEntity` liegt (`native/world.h:29`). Für die Gegnerseite
(`RECVX_ENEMY_WORK`) wird derselbe Feldsatz für die Dauer **eines** Aufrufs
auf dem Stapel übernommen und zurückgeschrieben — dieselbe einseitige
Richtungsregel, die `native/enemy_work.h` mit `publish`/`adopt` festlegt.
Keine Tabelle, keine Allokation, keine Lebensdauer, kein `sys`/`plp`/`rom`.

### Fremddienste statt Ersatzlogik

Zwei Dienste gehören nicht zu `subpl.c` und werden als Funktionszeiger im
Work geführt: `set_motion` (Motion.c:21) und `calc_model` (MdlPut.c:240).
Nicht gebunden → `work->gap` nennt die Originalfundstelle; es wird kein Frame,
keine Pose und kein Rückgabewert erfunden. `advanced` meldet dem vorhandenen
Motionbesitzer unverändert, dass der Motionschritt in diesem Pass fällig war.

---

## (b) Produktiver Anschluss und Patchbedarf

`native/subplayer.c` ist bereits in `CMakeLists.txt:101` (`recvx_game`), und
`native/world.cpp` ruft alle drei öffentlichen Funktionen bereits auf
(Zeilen 190-196 beim Raumladen, 807-809 je Tick). Der Apparat ist damit
**ohne jeden Patch produktiv wirksam**: `recvx_is_subplayer` klassifiziert
jetzt, `world.cpp` bindet die 195 Works, und `recvx_subplayer_tick` liefert
für jeden belegten Originalzustand `1`, so dass der bestehende
`throw std::runtime_error("unbound original Subpl dispatch mode")`
(world.cpp:808) nur noch bei einem tatsächlich unbelegten Zustand auslöst.

Geprüft: `native/world.cpp` und `tests/native/continuity_test.cpp`
übersetzen unverändert gegen den erweiterten Header (Befehle in (c)).

Trotzdem bleibt der Anschluss **unvollständig**. Patchbedarf, nach Dringlichkeit:

### P1 — `tests/native/continuity_test.cpp:21-24` prüft die geschlossene Lücke

Diese vier Zeilen behaupten weiter, es dürfe keine Figur als Begleitfigur
erkannt werden. Mit dem Port schlagen sie zwangsläufig fehl (`recvx_is_subplayer(91)`
liefert 1, `recvx_subplayer_init` setzt `mode0 1`/`mtn_add 32768`/`mode3 3`,
`recvx_subplayer_tick` liefert 1). Ich habe sie **nicht** angefasst. Ersatz
(derselbe Platz, Kommentar mitersetzen):

```cpp
    // Begleitfiguren (Auftrag T5, native/subplayer.c): eneset.c:84-152 leitet
    // 31..52/57..70/72..99 auf den Apparat aus subpl.c; 53..56 und 71 nicht.
    check(recvx_is_subplayer(91)&&recvx_is_subplayer(93)&&!recvx_is_subplayer(54),
          "companion identities misclassified");
    {RECVX_SUBPLAYER sub{};sub.id=93;recvx_subplayer_init(&sub);
     check(sub.mode0==1&&!sub.mtn_no&&sub.mtn_add==32768&&sub.mode3==3,
           "subpl.c:95 initialization");
     sub.ct2=0;
     check(recvx_subplayer_tick(&sub)==RECVX_SUBPL_HANDLED&&sub.mode3==3,
           "state 3 is handled but untouched");}
```

### P2 — Prüfung registrieren

`tests/native/subplayer_test.cpp` existiert, ist aber nicht gebaut. In
`CMakeLists.txt` neben die übrigen `recvx_game`-Tests (z. B. hinter Zeile 240):

```cmake
  add_executable(recvx-subplayer-tests tests/native/subplayer_test.cpp)
  target_include_directories(recvx-subplayer-tests PRIVATE native "${RECVX_TABLES}")
  target_link_libraries(recvx-subplayer-tests PRIVATE recvx_game)
  add_test(NAME original_subplayer_dispatch COMMAND recvx-subplayer-tests)
```

### P3 — `ct0` wird dem Work nie zugewiesen → jede Figur läuft nach (0,0)

Vollständige Bestandsaufnahme der Subplayer-Feldzugriffe in `world.cpp`
(`w.*` und `e.subplayer.*`): `mtn_no, mode0..3, ct1, ct2, ct3, mtn_md,
mtn_add, frm_no, frm_mode, hokan_count, hokan_rate, px, pz, ay, id, advanced`.
**`ct0` fehlt — und `flg` ebenfalls.** `ct0` ist aber das gepackte Ziel, das
`subpl_mv01/mv03/mv03_0` auswerten; ohne es drehen alle laufenden
Begleitfiguren zum Weltursprung. Die Originalquellen dafür:

- **`Common_controll` (Opcode 0x69) Unterbefehl 140 = 0x8c**, `event.c:12516-12559`:
  `mode2 = 0; mode3 = 0; mode1 = <Motion>;`
  `mode0 = (<Byte> == 0) ? 1 : 5;` und im selben Zweig
  `*(unsigned char*)&ct2 = (mode0 == 1) ? 1 : 3;`
  dann zwei `u16` mit den Vorzeichenflags `bhCetask->bpx`/`bpz`:
  `ct0 = (x << 16)` bzw. `(x * -1) << 16`, danach `ct0 |= z` bzw. `|= z * -1`.
- **Unterbefehl 132 = 0x84**, `event.c:12495-12515`: `mode0 = 1/5`,
  `mode2 = 0`, `mode3 = 0`, `mode1`, `ct1`, `ct2` (voller `int`) — wählt jeden
  der 15 ct2-Plätze direkt.
- **Unterbefehl 141 = 0x8d**, `event.c:12561-12571`: `mode0 = 5`, `mode2 = 0`,
  `mode3 = 0`, `mode1`, `ct0` (ein Byte), `ct1`, `ct2`.

`world.cpp` behandelt bei `op == 0x69` die Befehle 2..6, 7, 9, 11..19, 21,
23..34, 36, 38, 40, 44, 45, 48..50 — **0x84, 0x8c und 0x8d nicht**. Das sind
genau die drei Befehle, die den Bewegungsapparat ansteuern. Besitzer:
events/world.

Eigenheit, die der Patch mitnehmen muss: 0x8c schreibt `ct2` **nur als
unterstes Byte** (`*(unsigned char*)&ct2`), die oberen drei Bytes bleiben
stehen. `Common_controll` Fall 1 (`event.c:11035/11039`) schreibt dagegen den
**vollen** `int`. Mein Bereichstest liest den vollen `int`; ein stehen
gelassenes oberes Byte würde deshalb als `ct2-out-of-range` gemeldet — genau
das Verhalten, das den Fehler sichtbar macht, statt ihn zu verstecken.

### P4 — `flg` des Subplayer-Works wird nicht veröffentlicht

`world.cpp` liest und schreibt `subplayer.flg` nie (die einzige Fundstelle
`w.flg` in world.cpp:664 ist ein unbeteiligtes `RECVX_EN00_WALL`). Damit haben
die drei Statusbits aus `subpl.c:78-81` heute keine Wirkung: Begleitfiguren
bleiben anzielbar (`pwksub.c:445`), benutzen die falsche
Wandkollisionsvariante (`flg 0x100`) und das Totbit wird nicht gelöscht.
Patch: `e.subplayer.flg` vor dem Tick aus dem Entity-Zustand füllen
(mindestens Bit 0 = belegt, Bit 1 = `dead`) und nach dem Tick
`e.dead = (w.flg & 2) != 0` sowie das Zielbarkeits-/Kollisionsbit an den
vorhandenen Filter weiterreichen.

### P5 — `set_motion` / `calc_model` binden

Beide Zeiger sind in `world.cpp` ungesetzt. Folge: jeder Pass setzt
`work->gap` auf `subpl:motion-step (Motion.c:21)` bzw.
`subpl:publish-model (MdlPut.c:240)`, der Frame wird weiter von
`world.cpp:811` per Modulo fortgeschaltet. Das ist die heutige Näherung des
**Motionbesitzers**, nicht meine: `bhSetMotion` (Motion.c:81-111) kennt
zusätzlich das Pendeln über `mtn_md` Bit 0 mit `frm_mode`, den
Rückwärtsumlauf mit Rückgabe −1 und die Interpolationsrate aus
`hokan_count`/`hokan_rate` (`mode & 0x20` gegen sonst). Vorschlag:
`e.subplayer.set_motion`/`calc_model` auf den vorhandenen Actor-/Motion-Pfad
legen und `w.frm_no`/`w.frm_mode` nach dem Tick zurücklesen, statt in
`world.cpp:811` modulo zu rechnen. Solange das offen ist, bleibt
`work->gap` der ehrliche Melder; `world.cpp` wertet ihn noch nicht aus
(Vorschlag: in das vorhandene `unbound_enemy_`-Set eintragen).

### P6 — Gegnerseitiger Vertrag

- `native/enemy_dispatch.c:30`: `d->jump[28] = recvx_subplayer_enemy28;`
  Heute meldet `recvx_enemy_control` für Identität 28 die Lücke `bhEne28`,
  obwohl die Originalroutine (`subpl.c:410`) **leer** ist. Identität 28 kommt
  in keinem der 205 Räume vor, die Meldung ist also nur kosmetisch — aber
  falsch.
- `native/enemy_dispatch.c`: die 64 Subplayer-Indizes können jetzt mit
  `recvx_subplayer_enemy` belegt werden, dann entfällt die Sonderbehandlung
  `recvx_enemy_is_subplayer_id` in `recvx_enemy_control:106`.
- `native/enemy_work.h`: `sv->event` kann auf `recvx_subplayer_enemy_event`
  gebunden werden (`subpl.c:143`), der mode0-5-Zweig von 34 Gegnerdateien.
  **Der Kommentar dort ist sachlich falsch** und sollte korrigiert werden: er
  behauptet, `bhEne_Event` „setzt ct2 aus ct0, läuft den Subplayer-
  Bewegungsapparat und schaltet bei ct2 > 2 auf Mode0 um". `subpl.c:143` hat
  genau eine Anweisung — die Tabellenauswahl über `ct2`. Es gibt keine
  ct2-Zuweisung und keine mode0-Umschaltung; `bhEne_Event` ist nur in
  `subpl.c:143` definiert (geprüft über alle `src/ps2/veronica/prog`).
- `native/enemy_work.h` `sv->set_motion` hat **keinen vierten Parameter**
  (`datP`). `subpl.c` übergibt dort überall `mtn_tp`; in diesem Apparat ist
  das Feld an jeder setzenden Stelle NULL (`subpl.c:106/197/312/336`), ein von
  außen gesetztes `mtn_tp` wäre also nicht übertragbar. Die Brücke meldet das
  (`subpl:motion-data-parameter (Motion.c:21)`) statt es zu verwerfen; saubere
  Lösung ist der vierte Parameter im Vertrag.

### P7 — weitere belegte Abweichungen im Anschluss (nicht meine Dateien)

Beim Nachprüfen der CLAUDE.md-Aussage „Sub_controll 80/8f/90/92/93 … sind
angebunden" gefunden. Alles mit Originalfundstelle, nichts davon geändert:

- `Sub_controll` Fall 128 (0x80), `event.c:9762-9775`: setzt zusätzlich
  `mnwP = sys->emtp[id]`, `stflg &= ~0x10000` und `flg &= ~0x200000`, und
  **fällt ohne `break` in Fall 139 durch** (`bhCetask->mode1 = 0; mode0 = 0`).
  `world.cpp:540-544` setzt nur `mode0/mode2/ct2`.
- `Sub_controll` Fall 144 (0x90), `event.c:9787-9798`: dieselben zwei
  Flagbits zusätzlich zu `mode3 = 3`.
- `Common_controll` Fall 1, `event.c:11043-11065`: die Kette ist
  `t1 == 6 → hokan_count = 0, mtn_md |= 8`;
  `t1 > 8 → hokan_count = t1`;
  sonst `hokan_count = 8, mtn_md &= ~8`; danach **`frm_no = 0`**.
  `world.cpp:596-599` setzt nur den Zweig „sonst" (`hokan_count = 8`), nimmt
  `mtn_md` Bit 3 aus dem **Geschwindigkeitsbyte** `v.u8(2)` statt aus `t1`,
  und setzt `frm_no = start` statt 0. Besonders der fehlende Zweig `t1 > 8`
  ist funktional: `ct2` wird dann 10, und Platz 10 der ersten Tabelle ist
  genau der Zweig, der `hokan_count` **bewusst stehen lässt** (`subpl.c:172`)
  — er soll den Skriptwert bekommen und erhält heute einen Altwert.
  Im Original kommt `mtn_md`/`hokan_rate` in Fall 1 aus dem Taskrecord
  (`event.c:11001/11002`), nicht aus dem Skriptbyte.
- Bestätigt **original** und korrekt nachgebildet: der `ct2`-Deckel
  `> 8 → 10` (`event.c:11033-11040`) und das Passgate `sp_flg & 2`
  (`eneset.c:352`); `world.cpp` bekommt es über
  `set_simulation_flags(events->flags[6][0])`, und Bank 6 ist `sp_flg`
  (`native/events.cpp:29/448`).
- `Common_controll` Fall 0 (`event.c:10960-10988`) setzt `mode0 = 7` und
  `mode2 = 8` und hängt `mnwP` an `sys->plmthp`. Das sind **Spieler**zustände
  (`player.c:1417` `bhCtrPly_mode0[]`, `player.c:1744` prüft `mode0 == 7`);
  für einen Begleitwork wäre `Subpl_tbl[7]` ein Griff hinter die 7-Einträge-
  Tabelle. Mein Port meldet das (`subpl:mode0-out-of-range`) statt fremden
  Speicher zu lesen.

### Zur geprüften CLAUDE.md-Aussage

„Common 14/15/16/21 und Sub_controll 80/8f/90/92/93 sowie die originalen
subpl.c-Bewegungs-/Dispatchroutinen sind angebunden" — **teils überholt**.
`Common 14/21` schreiben tatsächlich `ct3`/`ct1` des Subplayer-Works
(`world.cpp:560`); `Common 15/16` schreiben Knotenwinkel, nicht den
Subplayer-Work. Die Bewegungs-/Dispatchroutinen waren bis zu dieser Arbeit
**nicht** angebunden (`native/subplayer.c` war die dokumentierte Lücke) — die
Aussage stammt aus der Zeit vor dem Rückbau vom 2026-10-09.
„Leere Original-MN_WORK-Tabellen behalten die letzte Pose" ist **bestätigt**
(Motion.c:29 und die Messung oben: 188 von 195 Works ohne Motionzeiger).

---

## (c) Komponentenprüfung — ausgeführte Befehle und Ausgaben

### 1. Pflichtprüfer (vor der Arbeit und nach jeder Änderung)

```
$ python3 tools/check_no_original_core.py
PASS: audited source/build/binary scope has no forbidden core; not a gameplay acceptance.
```

### 2. Einzelprüfung der Datei

```
$ /usr/bin/cc -DRECVX_NATIVE=1 -I build/macos-arm64/generated -I build-tools/ninja \
    -I native -I include/ps2/veronica/prog -O1 -DNDEBUG -std=gnu11 -arch arm64 \
    -mmacosx-version-min=11.0 -Wall -Wextra -fsyntax-only native/subplayer.c
SYNTAX OK            (keine Warnung)
```

Includeauflösung geprüft (`-H`), damit kein Originalheader einspringt:

```
. native/subplayer.h
. native/enemy_work.h
. native/ninja_math.h
```

Portabilität (für den Windows-Zweig relevant, aber **nicht** dessen Nachweis):

```
$ /usr/bin/cc ... -std=c99 -pedantic -Wall -Wextra -Wconversion -fsyntax-only native/subplayer.c
C99 PEDANTIC EXIT=0  (keine Warnung)
```

### 3. Bestehende Verbraucher übersetzen unverändert

```
$ /usr/bin/c++ -DRECVX_NATIVE=1 -I build/gapcheck-claude/generated -I build-tools/ninja \
    -I native -I include/ps2/veronica/prog -std=gnu++17 -arch arm64 \
    -mmacosx-version-min=11.0 -fsyntax-only native/world.cpp
WORLD EXIT=0
$ ... -fsyntax-only tests/native/continuity_test.cpp
CONTINUITY SYNTAX EXIT=0
```

### 4. Schrittweiser Abgleich aller zwölf Routinen gegen die Semantikreferenz

Eigenes Prüfskript (`$TMPDIR/t5meas/xcheck.py`): extrahiert aus Referenz und
Port je die geordnete Folge aus Feldschreibzugriffen (Feld, Operator, Wert),
`case`-Marken, `break`-Stellen und Dienstaufrufen und vergleicht sie.

```
OK  mv00_subpl0      == subpl_mv00           (9 Schritte)
OK  mv00_subpl0Ex    == subpl_mv00_ex        (8 Schritte)
OK  mv00_subpl1      == subpl_mv01           (12 Schritte)
OK  mv00_subpl2      == subpl_mv02           (6 Schritte)
OK  mv00_subpl2_0    == subpl_mv02_0         (6 Schritte)
OK  mv00_subpl3      == subpl_mv03           (9 Schritte)
OK  mv00_subpl3_0    == subpl_mv03_0         (9 Schritte)
OK  mv00_subpl5      == subpl_mv05           (9 Schritte)
OK  mv00_subpl6      == subpl_mv06           (10 Schritte)
OK  mv00_subpl0_1    == subpl_mv00_1         (7 Schritte)
OK  mv00_subpl10     == subpl_mv10           (5 Schritte)
MISMATCH em60_init vs subpl_shared_init
  >> ('CALL', '', 'setmotion')                None
Abweichungen: 1
```

Die **einzige** Abweichung ist gewollt und benannt: der erste der beiden
Motionschritte in `subpl.c:114-126` läuft im Original auf einem um acht Knoten
verkürzten Objektbaum (Identität 199). Ohne gebundenes Knotenfenster würde er
auf dem vollen Baum laufen und damit einen **falschen** Seiteneffekt erzeugen;
er wird deshalb nicht ausgeführt, sondern als
`subpl:init-node-window-199 (subpl.c:114)` gemeldet. Der zweite Schritt läuft
regulär. Erreichbarkeit: `id - 40 = 159` liegt außerhalb der 120 Einträge, und
keine der 205 Raumdateien führt eine Identität über 99.

### 5. Verhaltensprüfung `tests/native/subplayer_test.cpp`

```
$ /usr/bin/cc ... -c native/subplayer.c -o subplayer.o
$ /usr/bin/c++ ... -std=gnu++17 -c tests/native/subplayer_test.cpp -o test.o
$ /usr/bin/cc  ... -c native/ninja_math.c -o math.o
$ /usr/bin/c++ -O1 -arch arm64 subplayer.o test.o math.o -o subplayer_test && ./subplayer_test
subplayer apparatus: all checks passed
EXIT=0
```

Abgedeckt: alle 100 Identitäten der Dispatchtabelle einzeln plus 100..259; die
45 gemessenen Raumidentitäten namentlich; die vier Tabellengrenzen des
Init-Index (39/40/159/160 und 199); die neun Feldwerte von `subpl.c:95` samt
Aufrufzahl, Schritt und Modus; `mode3 = 3` als unbehandelter Zustand; die
Statusbits einschließlich „Bit 0 bleibt stehen"; die NULL-Plätze 2/3/4 und
`mode0` 7/8/255 mit ihrer Meldung und ohne Abschlussaufruf; die Bytebreite
(`mode0 = 256`); `mode2 = 1`; `ct2` 15/16/−1 auf allen drei mode0-Wegen;
**alle 15 Plätze beider ct2-Tabellen** (einmal über `mode0 1`, zweimal über
`mode0 5` und `6`), jeweils über die Feldsignatur des Zweigs identifiziert;
das Durchfallen gegen den abbrechenden Eintritt; Winkelgrenze, Viertel-
drehung und 16-Bit-Umlauf; die Vorzeichenasymmetrie des unteren `ct0`-
Halbworts zwischen `mv01` und `mv03`; `bhAddSpeed` in zwei Richtungen; die
Meldung beider ungebundener Dienste; und die Gegnerbrücke (Statusbits am
echten Work, Dienstnutzung, Frame auf dem echten Work, Platz 10 beider
Tabellen getrennt, Lückenmeldung über `sv->gap`, Identität 28 bitidentisch
unverändert).

### 6. Die Prüfung kann falsche Bindungen aufdecken (Mutationsprobe)

Fünf Mutationen im Port, jeweils gebaut und gelaufen; Port danach
unverändert wiederhergestellt:

```
A  Durchfallen in subpl_mv00 durch break ersetzt
   -> mode0 1 ct2 0: erwartet MV00, erhalten UNKNOWN          EXIT=1
B  hokan_count von subpl_mv06 von 16 auf 8
   -> mode0 1 ct2 5: erwartet MV06, erhalten UNKNOWN          EXIT=1
C  unteres ct0-Halbwort in subpl_mv01 vorzeichenbehaftet
   -> "the unsigned and signed reading ... must differ"        EXIT=1
D  Statusbit 0x8000 nicht gesetzt
   -> "the pass makes the figure untargetable"                 EXIT=1
E  mode0 2/3/4 als stiller No-op (HANDLED ohne Meldung)
   -> "null slots are not states"                              EXIT=1
F  subpl_mv01_tbl Plätze 6 und 7 vertauscht
   -> mode0 5 ct2 6: erwartet MV02_0, erhalten MV03_0          EXIT=1
```

### 7. Was ausdrücklich NICHT geprüft ist

- **Kein Laufzeitnachweis im Spiel.** Weder ein CTest-Gesamtlauf noch ein
  Runtime-Replay noch ein Screenshot. Die Prüfung ist eine
  Komponentenprüfung; dass eine Begleitfigur im Raum ihren Skriptweg geht,
  ist damit **nicht** belegt — und kann es nicht sein, solange P3 (`ct0`) und
  P5 (`set_motion`) offen sind.
- **Kein Gesamtbuild.** `cmake --build` wurde nicht ausgeführt; geprüft sind
  Einzelübersetzung von `subplayer.c`, `world.cpp` und `continuity_test.cpp`.
  `tests/native/continuity_test.cpp:21-24` wird nach P1 **fehlschlagen** —
  das ist angekündigt, nicht übersehen.
- **Kein Windows-Lauf.** Nur `-std=c99 -pedantic -Wconversion` auf arm64; kein
  MSVC-/Cross-Build, kein `tools/windows_link_audit.py`.
- **Keine Sanitizer**, kein Paket, kein Commit.
- „Compiliert" ist kein Verhaltensnachweis; Verhalten belegt ausschließlich
  Abschnitt 5 zusammen mit der Mutationsprobe in 6.

---

## (d) Offene Portierungslücken, namentlich mit Originalfundstelle

| Fundstelle | Lücke | Besitzer | Folge |
|---|---|---|---|
| `Motion.c:21` `bhSetMotion` | Frameabtastung, Interpolationsrate (`mode & 0x20`), Pendeln über `mtn_md` Bit 0 mit `frm_mode`, Rückwärtsumlauf (Rückgabe −1), `mtn_attr` aus `atrP[frm_no >> 16]`, die vier Abtastvarianten `SetMtnNormal/Fast/Slow(+Hokan)` | Motion-/Actor-Besitzer | `work->gap`; der Frame läuft weiter über `world.cpp:811` per Modulo, also ohne Pendeln und ohne Interpolationsmischung |
| `MdlPut.c:240` `bhCalcModel` | Veröffentlichung in den Objektbaum am Ende jedes Passes (`subpl.c:84`) | Objektbaum-Besitzer | `work->gap`; `world.cpp:809` schreibt Lage/Winkel direkt zurück |
| `subpl.c:114-126` | verkürztes Knotenfenster (`objP`/`owP` um 8 versetzt, `obj_num` um 8 kleiner) für Identität 199, inklusive des ersten Motionschritts darauf | Modell-/Objektbaum-Besitzer | gemeldet; unerreichbar, weil Index 159 außerhalb der 120 Einträge liegt und keine Raumidentität über 99 geht |
| `subpl.c:12` Index 2/3/4 | im Original NULL — kein Zustand | — | gemeldet als `subpl:mode0-null-slot`; Original stürzt dort ab |
| `subpl.c:23` Index < 0 bzw. ≥ 120 | Identitäten 31..39 (negativ) und ≥ 160 | — | gemeldet als `subpl:init-table-index`; kein Raum löst es aus |
| `subpl.c:38` `mode2 != 0` | Tabelle hat genau einen Eintrag | — | gemeldet |
| `subpl.c:143/150` `ct2` außerhalb 0..14 | ungeprüfter Tabellenzugriff im Original | — | gemeldet |
| `eneset.c:372` `bhControlMask` | Masken-/Lippensteuerung für Identitäten > 90 — betrifft 110 der 195 gemessenen Works (91..99) | `native/face.*` | läuft im Original **vor** dem Pass; `recvx_enemy_control:101` meldet es bereits |
| `event.c:12495/12516/12561` | Common-Unterbefehle 0x84/0x8c/0x8d, die `mode0/mode1/ct0/ct1/ct2` des Begleitworks setzen | events/world | **ohne sie kein `ct0`** → laufende Figuren drehen zum Weltursprung (P3) |
| `event.c:9762/9787` | `Sub_controll` 0x80/0x90 setzen zusätzlich `mnwP`, `stflg 0x10000`, `flg 0x200000`; 0x80 fällt in Fall 139 durch | events/world | Teilbindung (P7) |
| `event.c:11043-11065` | `Common_controll` Fall 1: Zweig `t1 > 8 → hokan_count = t1`, `mtn_md` Bit 3 aus `t1`, `frm_no = 0`, `mtn_md`/`hokan_rate` aus dem Taskrecord | events/world | `subpl.c:172` (Platz 10) bekommt den Skriptwert nicht (P7) |
| `native/world.cpp` | `subplayer.flg` wird nie gefüllt oder gelesen | world | Begleitfiguren bleiben anzielbar (`pwksub.c:445`), falsche Wandvariante, Totbit nicht gelöscht (P4) |
| `native/enemy_work.h` `sv->set_motion` | vierter Parameter `datP` fehlt im Vertrag | T1 | `mtn_tp != NULL` wäre nicht übertragbar; Brücke meldet es (P6) |

Nicht als Lücke zu führen: **Identität 28**. `subpl.c:410` ist im Original
eine leere Funktion. Sie bleibt leer (`recvx_subplayer_enemy28`), und
`enemy_dispatch` sollte sie binden, damit die falsche Lückenmeldung entfällt
(P6). Identität 28 kommt in keiner der 205 Raumdateien vor.
