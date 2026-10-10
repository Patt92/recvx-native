# Enemy-Id-Asset-Inventar — Bestandsaufnahme 2026-10-10

**Auftrag:** Bestandsaufnahme aller Original-Gegner-IDs (Varianten + Child-
Zuordnungen) aus den zulässigen Originaltabellen, Vergleich mit den vorhandenen
nativen Bindungen, Beleg pro Id (Datei/Zeilenzahl). Kein Code geändert, kein
Generator, keine Funktionskörper übernommen. `archived/FORBIDDEN_DONTUSE` nicht
gelesen. KI-/Gameplay-Vollständigkeit nicht behauptet: ein bestehender
Work-Strang bedeutet „Daten/Spawn belegt", nicht „Gegner funktioniert".

## Ursprung des Enemy-Ids

`bhSetEnemy` (`src/ps2/veronica/prog/eneset.c:170`) kopiert die
Raum-Eid (`EGG_WORK`) in einen globalen `BH_PWORK`-Slot (`ene[]`, 180224 Byte,
`bhInitEnemy` at :166). Der globale Feind-Durchlauf `bhControlEnemy`
(`eneset.c:344`) ruft je Slot `bhJumpEnemy[ep->id](ep)` (`eneset.c:377`).
Damit ist `ep->id` der einzige Anker: er kommt aus den Raumdaten (RDT) und
steuert gleichzeitig die Sprungtabelle.

Der Raum-Lade-Pfad im Host: `native/world.cpp:139` liest `enep` aus dem Raum-
Header und bindet pro Slot (`&1`-Flag). `native/world.cpp:171` liest aus
`enep + i*36 + 4` die Enemy-Identity (Motion-Bibliothek), `:176` die Variante
aus `enep + i*36 + 6` (`epw->type`).

## Sprungtabelle `bhJumpEnemy[100]`

`src/ps2/veronica/prog/eneset.c:51`. Eindeutige („individuelle") Feind-Routinen
tragen ihre eigene `bhEneNN`-Dispatch-Funktion, `bhSubpl`-Einträge werden über
`bhSubpl` (`src/ps2/veronica/prog/subpl.c:76`) zum subpl-Pfad geleitet.

| id | Routine | Herkunft |
| --- | --- | --- |
| 0 | `bhEne00` | `eneset.c:53` (Ratte) |
| 1 | `bhEne01` | `eneset.c:54` (Zombie) |
| 2 | `bhEne02` | `eneset.c:55` |
| 3 | `bhEne03` | `eneset.c:56` |
| 4 | `bhEne04` | `eneset.c:57` (Hund) |
| 5..30 | `bhEne05`..`bhEne30` | `eneset.c:58..84` |
| 31..52 | `bhSubpl` | `eneset.c:85..106` (kinematisch) |
| 53 | `bhEne53` | `eneset.c:106` |
| 54 | `bhEne54` | `eneset.c:107` |
| 55 | `bhEne55` | `eneset.c:108` |
| 56..70 | `bhSubpl` | `eneset.c:110..124` |
| 71 | `bhEne71` | `eneset.c:124` |
| 72..99 | `bhSubpl` | `eneset.c:125..152` |

Alle 100 Einträge in `eneset.c` belegt. Es sind keine id-spezifischen
Sonderwege im Host sichtbar; der Host hat `bhJumpEnemy` nicht nachgebaut (kein
`JumpEnemy_proc`-Zeiger im `native/`).

## Subpl-Pfad (id >= 31, id != 53..55, id != 71)

`bhSubpl` (`subpl.c:76`) ruft `Subpl_tbl[epw->mode0](epw)` mit
`Subpl_tbl[7]` = `{init_subpl, move_subpl, NULL, NULL, NULL, em_sce, em_sce}`
(`subpl.c:12`).
- `init_subpl` (`subpl.c:87`) setzt `mode0 = 1` und ruft
  `subpl_init_tbl[epw->id - 40](epw)`. Die init-Tabelle hat 120 Einträge
  (`subpl.c:22`); die ersten 120 (`em60_init`, `subpl.c:24`) sind bekannt,
  für höhere Einträge fehlt die Belegstelle. **Keine native Bindung** des
  subpl-Init-Pfades im Host gefunden.
- `move_subpl` (`subpl.c:131`) ruft `subpl_mv_tbl[epw->mode2]`
  (`subpl.c:37`).
- `em_sce` (`subpl.c:137`) ruft `subpl_mv01_tbl[epw->ct2]`.

Der Host bindet im Raumladen nur `identity > 90` als Aktor (`world.cpp:134`)
und ruft `recvx_subplayer_init` für `recvx_is_subplayer(identity)`
(`world.cpp:190`). Für die übrigen cinematic ids (31..99, nicht >90) fehlt die
native subpl-Zuordnung.

## en01 (id 1) — Variante/Child-Auflösung

`bhEne01` (`src/ps2/veronica/prog/en01.c:2467`) ist die zentrale Routine;
`bhEne01_MainLoop` (`en01.c:2504`) wählt `bhEne01_Mode0[epw->mode0]`
(`en01.c:2217`).

**Varianten-Tabelle `ene01_typ_tbl[31][2]`** (`en01.c:2541`) löst die
Raum-`mdlver` (erste Spalte) in die interne en01-Sonder-Variante (zweite
Spalte) um; `bhEne01_Init` (`en01.c:2696`) findet den ersten passenden Eintrag
und schreibt `EXP0_I(0x4C)` (die en01-eigene „type"-Id).

| mdlver (Spalte 0) | en01-Sub-Variante (Spalte 1) |
| --- | --- |
| 0 | 0 |
| 1 | 1 |
| 31 | 2 |
| 18 | 0 |
| 2 | 3 |
| 3 | 4 |
| 4 | 5 |
| 32 | 6 |
| 33 | 7 |
| 34 | 8 |
| 21 | 9 |
| 19 | 3 |
| 5 | 10 |
| 6 | 11 |
| 35 | 12 |
| 36 | 13 |
| 22 | 14 |
| 20 | 10 |
| 7 | 15 |
| 8 | 16 |
| 37 | 17 |
| 38 | 18 |
| 24 | 15 |
| 9 | 19 |
| 10 | 20 |
| 25 | 21 |
| 39 | 22 |
| 40 | 23 |
| 41 | 24 |
| 42 | 25 |
| 43 | 26 |

Beleg: `en01.c:2541` (definiert) und `en01.c:2696`. Keine dieser 31 Zeilen ist
im Host nachgebaut (kein `ene01_typ_tbl`-Ersatz). Child-/Link-Referenzierung
(`bhEne01_SetLinkEnemy`, `en01.c:2815`) und die mit `exp0` verknüpften Kinder
(`en01.c:2600..2670`) sind separate Work-Referenzen ohne eigene id.

**Weitere en01-Tabellen** (`en01.c:2207..2460`): `bhEne01_InitType[16]`,
`MoveTypeW[16]`, `MoveMode2W[17]`, `NageTypeW[16]`, `NageMode2W[4]`,
`DamageTypeW[16]`, `DamageMode2W[17]`, `DieTypeW[16]`, `DieMode2W[4]`,
`PlyDmgMode[2]`, `DmgCheckType[16]` — alle indiziert über `epw->type`/`mode0`/
`mode2`/`mode3`. **Keine native Bindung** dieser en01-Zustandstabellen im Host.

## en04 (id 4) — Variante über `epw->type`

`bhEne04` (`src/ps2/veronica/prog/en04.c:516`) ruft
`bhEne04_MainLoop` (`en04.c:558`) → `bhEne04_Mode0[epw->mode0]`
(`en04.c:414`). `bhEne04_Move/Nage/Damage/Die` (`en04.c:1020..1044`) indizieren
jeweils über `epw->type`.

**`en04_motion_by_type[19]`** (`en04.c:372`) ordnet `type 0..18` einem
Initial-Motion-Wert zu (Beleg: Tabelle bei `en04.c:372`, erste Zeile `{0,...}`,
letzte `{18,...}`). `bhEne04_Init` (`en04.c:939`) schreibt
`epw->mtn_md = 0` (`en04.c:974`) und nutzt diese Tabelle.

Beleg für `en04` im Host: `native/world.cpp:181` (`motion = type?49:47`) und
`native/world.cpp:176` (`type==15 ? 13 : ...`) für den Initial-Pose-Pfad.
**Keine native Bindung** von `en04_by_type[19]`, `bhEne04_MoveType[3]`,
`bhEne04_BrainMode2[12]`, den `Nage/Damage/Die`-Tabellen (`en04.c:424..498`) im
Host. `en04`-Sub-Dispatch (`en04.h`) nicht im Host abgebildet.

## en07 (id 7) — Bat

`bhEne07` in `src/ps2/veronica/prog/en07.c`: `bhEne07_Mode0[6]`
(`en07.c:66`), `bhEne07_BrainType[2]` (`en07.c:76`),
`bhEne07_MoveMode2[9]` (`en07.c:81`). Der Work-Strang
`work/en07_native_link_audit.c` ist archiviert (`archived/en07-bat-draft-20261008`),
kein produktiver Kern. **Keine native Bindung** von en07 im Host.

## en53/en54/en55/en71 (individuell)

`eneset.c:106..108` bzw `:124`. Diese ids sind individuell (`bhSubpl`-Einträge
rundherum), aber im Host nicht als eigene Feind-Aufgabe belegt. Beleg nur über
`eneset.c:51`.

## Zusammenfassung native Bindungen vs. Original

| Original-Bereich | Beleg | Native Implementierung | Status |
| --- | --- | --- | --- |
| `bhJumpEnemy[100]` | `eneset.c:51` | — | **offen** (kein Ersatz) |
| `bhSubpl`-Einträge 31..52, 56..70, 72..99 | `eneset.c:85..106`, `:110..124` | subpl-Init-Pfad (`subpl.c:87`) **nicht** an RoomWorld gebunden; nur `identity > 90` als Aktor (`world.cpp:134`) | **offen** |
| `ene01_typ_tbl[31][2]` (mdlver → Sub-Variante) | `en01.c:2541` | — | **offen** |
| en01 Init/Move/Nage/Damage-Tabellen | `en01.c:2207..2460` | — | **offen** |
| en04 Motion-by-Type | `en04.c:372` | `world.cpp:181`, `world.cpp:176` (Initial-Pose-Pfad) | **teilweise**: nur Start-Pose |
| en07 (Bat) | `en07.c` | archiviert, kein Kern | **offen** |
| Raum-Id → RoomWorld-Entity | `eneset.c:377` | `world.cpp:139` | **belegt** (Daten-/Spawnlage) |

## Offene Zuordnungen (ausdrücklich ungeklärt)

- Welche Raum-`mdlver` konkret welche en01-Sub-Variante (`ene01_typ_tbl`)
  erzeugt, ist in den Raumdaten nicht geprüft; die 31 Zeilen sind belegt, aber
  ihre reale Belegung je Raum unbestätigt.
- Die 120-entry `subpl_init_tbl`-Einträge für ids > 119 und die 15-entry
  `subpl_mv00_tbl`/`subpl_mv01_tbl` (`subpl.c:38`, `:56`) sind im Original
  vorhanden, im Host aber nicht zugeordnet.
- `en04`-`type 15` als Sonderfall (`world.cpp:176` nutzt `mtn_md` 13) ist eine
  Host-Regel, keine aus `en04.c` hergeleitete; als ungeklärt behandelt.
- Für ids 2..3, 5..30, 56..70, 72..99 gibt es keine gesonderte Host-Abfrage;
  ob diese ids jemals in den geladenen Räumen vorkommen, ist aus den RDT nicht
  geprüft (RDT nicht im Workspace vorhanden).
- Der en04-Sub-Dispatch (`en04.h`) und en07-Sonder-Varianten sind im Original
  vorhanden, im Host aber nicht abgebildet.

## Datenlage

- Zulässige Original-Quellen: `src/ps2/veronica/prog/eneset.c`, `en01.c`,
  `en04.c`, `en07.c`, `subpl.c`, `subpl.h`, `en01.h`, `en01sub.h`, `en04.h`.
- `archived/FORBIDDEN_DONTUSE` nicht verwendet.
- Raum-Eigen-Daten (RDT) sind im Workspace nicht vorhanden; Raum-Id-Vorkommen
  bleibt ungeprüft. Ein bestehender Work-Strang belegt nur die Daten-/Spawnlage,
  nicht die Laufzeitkorrektheit.
