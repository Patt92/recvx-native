# Briefings für manuell gestartete Gegner-Subagenten

Stand: 2026-10-10. Jeder Block ist eigenständig und kann unverändert als
Agentenauftrag eingefügt werden. Modellwahl steht jeweils in der Kopfzeile.

Status der Arbeitsteilung: T1 (Vertrag/Dispatch/EN00/EN04/EN07/EN56) und
T2 (EN01-Familie) laufen bereits auf Opus. Offen sind T3, T4, T5.

## WICHTIG: Der gemeinsame Vertrag ist fertig (T1 Stufe A)

Er liegt in `native/enemies/enemy_work.h` (+ `native/enemies/enemy_work.c`) und ist für T3/T4/T5
VERBINDLICH. Jeder dieser Agenten muss ihn vollständig lesen, bevor er
implementiert — kein eigenes Work-/Mode-/EXP-/Child-Interface mehr entwerfen.
Die Absätze „REIHENFOLGE" in den Briefings unten sind damit erfüllt: die
Wartebedingung entfällt, es darf direkt gegen `enemy_work.h` implementiert
werden.

Kernpunkte, die jeder Agent kennen muss:

- `RECVX_ENEMY_WORK` ist ein **transparenter** Feldsatz, der als EIN Member in
  der vorhandenen WorldEntity liegt: keine eigene Allokation, keine eigene
  Lebensdauer, keine Tabelle/Arena, kein sys/plp/rom/ene/eff. Das grenzt ihn vom
  verbotenen Muster in `native/enemies/en01_damage.h` ab (dort ein 16-KiB-opaker, per
  shared_ptr getrennt allokierter Block = zweiter Besitzer).
- Feld-Autorität: Der Satz besitzt alle Originalfelder, die WorldEntity nicht
  hat. Für gemeinsame Felder (px/py/pz, ax/ay/az, flr_no, hp, mtn_no, frm_no,
  mtn_add, flg 0x2 → dead, stflg 0x1000000 → inactive, mdflg 0x200 →
  Zeichen-Gate) ist **während des Gegnerpasses** der Satz Autorität;
  `recvx_enemy_publish()` gibt sie EINMAL je Pass ab, genau dort wo das Original
  `bhCalcModel` ruft. Entity → Work nur über `recvx_enemy_set()` (Raumladen) und
  `recvx_enemy_adopt(mask)` (nach Skriptbefehlen).
- Dienste über `RECVX_ENEMY_SERVICES` (Funktionszeiger). NULL = ungebunden →
  `gap(name)` aufrufen und nichts tun, **niemals** ein plausibler Ersatzwert.
- `player_state` liefert derzeit nirgends einen gebundenen Spieler. Jeder
  Angriffs-/Griff-/Spielerschadenszweig bleibt damit eine namentlich gemeldete
  Lücke — dort weder HP noch Pose noch ein Griffende erfinden. Das war der
  bereits belegte Fehler des alten `recvx_en01_player_state`.

Vom Orchestrator an den Originalquellen nachgeprüft und damit verbindlich:

- `bhEne_InitDamage` steht in **`zonzon1.c:806`** (nicht zonzon.c). Semantik:
  `mlwP != NULL` → `dam[0..obj_num-1] = 0`; `comb_timeout != 0` → dekrementieren;
  `comb_flg &= ~0x70`; `flg2 &= ~0x4`. Läuft nach JEDEM `bhJumpEnemy[id]`.
- Das `flg`-0x10000-Gate von `bhEne_CallSE` (`zonzon1.c:828`) liegt im
  Mechanismus, nicht im Dienst.
- Original-Quirk (`eneset.c:302`): `clp_jno[0]=1` und `clp_jno[1]=-1` stehen
  NACH dem id-Zweig und überschreiben die Sätze für 41..48 / 91..98. Der
  `-1`-Terminator an Index 1 macht 2..6 für eine terminatorgesteuerte Suche
  unerreichbar. **Unverändert übernehmen, nicht korrigieren.**
- Zeichenfilter `bhDrawEnemy` (`eneset.c:807`): `flg&1`, `!(flg&0x80000000)`,
  `!(stflg&0x41000000)`, `(st_flg&0x20 || !(mdflg&0x200))`, und
  `(!(flg&0x80) || !(lkwkp->stflg&0x1000000))`.
- `bhCheckEneWorkNum` (`eneset.c:325`): `ewk_n` = höchster belegter Index + 1,
  **Lücken eingeschlossen**, kein Abbruch am ersten freien Slot.

Offene Punkte aus T1, die nicht verdeckt werden dürfen: `motion_frames` braucht
die MN_WORK-Framezahlen je Identität (RoomWorld hat sie für 1 und 4, nicht für
0/7/56); der O_WRK-gegen-BH_PWORK-Cast bei `pwksub.c:445` gegenüber
`eneset.c:362` liest `stflg 0x1000000` aus verschiedenen Strukturoffsets und
bleibt als Portierungslücke vermerkt.

Gemessene Datenlage (aus den 205 RDT in `data/rooms`, Header bei `u32(data,16)`,
`ene_count` bei `header+0x88`, `enep` bei `header+8`, Enemy-Slots 36 Byte, aktiv
bei `flg&1`): **759 aktive Enemy-Works auf 78 IDs** — 564 auf 33 IDs mit eigener
`bhEneNN`-Routine, 195 auf 45 `bhSubpl`-IDs. IDs 18 und 27 kommen in keinem Raum
vor; `bhEne28` ist im Original eine leere Funktion (`subpl.c:410`).

---

## T3 — Große Einzelgegner (Modell: opus)

Teilaufgabe T3 des Nutzerauftrags „vollständiger nativer Port ALLER
Originalgegner": eigenständige native Portierung der großen Einzelroutinen EN03,
EN05, EN09, EN15 (einschließlich EN53 aus `en15.c:3726`), EN19, EN22, EN23, EN26
— gemessen 67 aktive Enemy-Works in den Raumdaten.

PFLICHTLEKTÜRE VOR ARBEIT: `AGENTS.md` VOLLSTÄNDIG (3109 Zeilen, seitenweise
lesen), `QUALITY.md`, `docs/THIRD_AGENT_C01_TASK.md`. Dann `git status --short`
und `python3 tools/check_no_original_core.py` (vor Arbeit und vor jedem Build;
derzeit PASS).

HARTE GRENZEN:
- `archived/FORBIDDEN_DONTUSE` niemals lesen, durchsuchen, kopieren oder
  indirekt nutzen.
- `src/ps2/veronica/prog/en03*.c`, `en05*.c`, `en09.c`, `en15.c`, `en19.c`,
  `en22.c`, `en23.c`, `en26.c` (zusammen ~41k Zeilen) und das Original-ELF sind
  AUSSCHLIESSLICH Semantikreferenz. Keine Funktionskörper kopieren, extrahieren,
  generieren, kompilieren, linken oder aufrufen. Kein Generator.
- Kein zweites sys/plp/rom/ene/eff, keine BH_PWORK-Spiegeltabelle. Einziger
  Besitzer bleibt die vorhandene WorldEntity/RoomWorld-Quelle.
- Keine Heuristik, geratene Konstante, Raum-/Gegner-Sonderregel, stiller No-op,
  abgeschwächte Testerwartung. Ungeklärte Semantik als Portierungslücke mit
  Fundstelle offenlegen.
- Gemeinsamer Arbeitsbaum mit aktiven Peers: nichts zurücksetzen, stashen,
  löschen. Kein Commit, Push, Paket.

DATEIBESITZ (exklusiv, neu anzulegen): `native/enemies/en03*`, `en05*`, `en09*`,
`en15*`, `en19*`, `en22*`, `en23*`, `en26*`, `en53*` (.c/.h). NICHT anfassen:
`CMakeLists.txt`, `native/world.*`, `native/runtime.cpp`, `native/events.cpp`,
`native/enemies/enemy_spawn.*`, `native/enemies/en01_damage.*`, `native/subplayer.*`,
`native/player_combat_core.c`, `native/targeting.cpp`,
`native/effect_particles.cpp`, `native/effect_sprites.*`, `native/renderer.*`,
`native/scene.h`, neue `native/enemies/enemy_dispatch.*`/`enemy_work.*` und alle übrigen
enNN-Dateien (andere Teilaufgaben).

ZIEL PRO ID: vollständige originale Initialisierung, Variantenauflösung über die
type-Tabellen, Bewegungs-/KI-Zustandsmaschine, Angriff/Griff, Schaden/Tod,
Child-Lebenszyklus, Motion-/Posefortschreibung, Draw-Gate und SFX, soweit beim
jeweiligen Originalgegner vorhanden. Pro ID die Dispatchtabellen
(mode0/type/mode2/mode3), HP-Tabellen, SE-Aufrufe und benötigten Dienste mit
Datei:Zeile belegen.

Vorhandene Dienste WIEDERVERWENDEN statt neu entwerfen:
`native/geometry_motion.h`, `native/effect_collision.h`, `native/audio.h`,
`native/actor.h`, `native/core_random.*`, `native/combat_tables_data.inc`,
`native/room_table.*`.

QUELLENGEPRÜFTE FLAG-REGEL (gilt für alle Gegner): `bhSetEnemy`
(`eneset.c:170/183`) kopiert `etp->flg` der EGG_WORK-Vorlage — dort kommt `flg`
Bit 0 her; `id > 40` ergänzt `mdflg |= 0x20` und `flg |= 0x8000`
(`eneset.c:187-229`). `bhEneSetCk` (`event.c:2715/2738`) setzt NUR
`stflg |= 0x1000000`. `bhControlEnemy` (`eneset.c:344`) läuft nur bei
`sys->sp_flg & 2`, sichert pxb/pyb/pzb und axb/ayb/azb, ruft für `id > 90` zuerst
`bhControlMask`, dann `bhJumpEnemy[id]`, dann `bhEne_InitDamage`, und sammelt
`flg & 0x80000000` in `sys->en_obj`.

REIHENFOLGE: Eine parallele Teilaufgabe (T1) erarbeitet gerade den gemeinsamen
Enemy-Work-/Service-Vertrag auf der vorhandenen WorldEntity/RoomWorld; vorher
KEIN eigenes Work-/Mode-/EXP-/Child-Interface implementieren. Nutze die Zeit für
die belegte Quellen-, Varianten-, Asset-, Motion- und Abhängigkeitsmatrix pro ID.
Schnittstellenbedarf an gemeinsamen Dateien als Patch-Beschreibung melden, nicht
selbst ändern.

KEINE Builds, kein CTest, keine Runtime-Läufe bis das Gesamtpaket aller fünf
Teilaufgaben integriert ist (Nutzervorgabe: erst alles implementieren, dann
gemeinsam testen und gemeinsame Ursachen beheben). Policy-Prüfer bleibt Pflicht.

BERICHT: `work/native_port_T3_enemies.result.md`, getrennt nach (a)
implementiert, (b) produktiv angeschlossen, (c) komponentengeprüft, (d)
Runtime-geprüft, (e) blockiert/offen, je ID mit Originalfundstellen Datei:Zeile.

---

## T4 — Kleine und mittlere Einzelgegner (Modell: deep4flash-sonnet)

Teilaufgabe T4 des Nutzerauftrags „vollständiger nativer Port ALLER
Originalgegner": eigenständige native Portierung der kleineren und mittleren
Einzelroutinen EN02, EN06, EN08, EN10, EN11, EN12, EN13, EN14, EN16, EN17, EN18,
EN20, EN21, EN24, EN25, EN27, EN29, EN30, EN54, EN55, EN71.

PFLICHTLEKTÜRE VOR ARBEIT: `AGENTS.md` VOLLSTÄNDIG (3109 Zeilen, seitenweise
lesen), `QUALITY.md`, `docs/THIRD_AGENT_C01_TASK.md`. Dann `git status --short`
und `python3 tools/check_no_original_core.py` (vor Arbeit und vor jedem Build;
derzeit PASS).

HARTE GRENZEN:
- `archived/FORBIDDEN_DONTUSE` niemals lesen, durchsuchen, kopieren oder
  indirekt nutzen.
- `src/ps2/veronica/prog/en02.c`, `en06.c`, `en08.c`, `en10.c` … `en71.c` samt
  zugehöriger `*sub.c` und das Original-ELF sind AUSSCHLIESSLICH
  Semantikreferenz. Keine Funktionskörper kopieren, extrahieren, generieren,
  kompilieren, linken oder aufrufen. Kein Generator.
- Kein zweites sys/plp/rom/ene/eff, keine BH_PWORK-Spiegeltabelle. Einziger
  Besitzer bleibt die vorhandene WorldEntity/RoomWorld-Quelle.
- Keine Heuristik, geratene Konstante, Raum-/Gegner-Sonderregel, stiller No-op,
  abgeschwächte Testerwartung. Ungeklärte Semantik als Portierungslücke mit
  Fundstelle offenlegen.
- Gemeinsamer Arbeitsbaum mit aktiven Peers: nichts zurücksetzen, stashen,
  löschen. Kein Commit, Push, Paket.

DATEIBESITZ (exklusiv, neu anzulegen): `native/enemies/en02*`, `en06*`, `en08*`,
`en10*`, `en11*`, `en12*`, `en13*`, `en14*`, `en16*`, `en17*`, `en18*`, `en20*`,
`en21*`, `en24*`, `en25*`, `en27*`, `en29*`, `en30*`, `en54*`, `en55*`, `en71*`
(.c/.h). NICHT anfassen: `CMakeLists.txt`, `native/world.*`,
`native/runtime.cpp`, `native/events.cpp`, `native/enemies/enemy_spawn.*`,
`native/enemies/en01_damage.*`, `native/subplayer.*`, `native/player_combat_core.c`,
`native/targeting.cpp`, `native/effect_particles.cpp`,
`native/effect_sprites.*`, `native/renderer.*`, `native/scene.h`, neue
`native/enemies/enemy_dispatch.*`/`enemy_work.*` und alle
en03/en05/en09/en15/en19/en22/en23/en26/en53-Dateien (andere Teilaufgaben).

KLARE VORGABE: Jede ID folgt im Original demselben Muster — Dispatch über
`bhEneNN` → `mode0`-Tabelle → `type`-Tabelle → `mode2`/`mode3`-Tabellen. Arbeite
ID für ID: zuerst alle Originaltabellen und ihre Zweige mit Datei:Zeile belegen,
dann als eigenständige native Mechanik auf dem gemeinsamen Vertrag umsetzen.
Vollständig heißt: Initialisierung, Variantenauflösung, Bewegung/KI, Angriff,
Schaden, Tod, Child-Lebenszyklus, Motionfortschreibung, Draw-Gate, SFX — soweit
beim jeweiligen Originalgegner vorhanden.

Vorhandene Dienste WIEDERVERWENDEN statt neu entwerfen:
`native/geometry_motion.h`, `native/effect_collision.h`, `native/audio.h`,
`native/actor.h`, `native/core_random.*`, `native/combat_tables_data.inc`.

GEMESSENE DATENLAGE (aus den 205 RDT in `data/rooms`, nicht raten): diese IDs
haben zusammen 170 aktive Enemy-Works. EN18 und EN27 kommen in keinem Raum vor —
trotzdem vollständig implementieren, aber ausdrücklich als „ohne Raumvorkommen,
keine Runtime-Abnahme möglich" berichten.

QUELLENGEPRÜFTE FLAG-REGEL (gilt für alle Gegner): `bhSetEnemy`
(`eneset.c:170/183`) kopiert `etp->flg` der EGG_WORK-Vorlage — dort kommt `flg`
Bit 0 her; `id > 40` ergänzt `mdflg |= 0x20` und `flg |= 0x8000`. `bhEneSetCk`
(`event.c:2715/2738`) setzt NUR `stflg |= 0x1000000`. `bhControlEnemy`
(`eneset.c:344`) läuft nur bei `sys->sp_flg & 2` und ruft `bhJumpEnemy[id]`,
danach `bhEne_InitDamage`.

REIHENFOLGE: Eine parallele Teilaufgabe (T1) erarbeitet gerade den gemeinsamen
Enemy-Work-/Service-Vertrag auf der vorhandenen WorldEntity/RoomWorld; vorher
KEIN eigenes Work-/Mode-/EXP-/Child-Interface implementieren. Nutze die Zeit für
die belegte Tabellen- und Abhängigkeitsmatrix pro ID. Schnittstellenbedarf an
gemeinsamen Dateien als Patch-Beschreibung melden, nicht selbst ändern.

KEINE Builds, kein CTest, keine Runtime-Läufe bis das Gesamtpaket aller fünf
Teilaufgaben integriert ist. Policy-Prüfer bleibt Pflicht.

BERICHT: `work/native_port_T4_enemies.result.md`, getrennt nach (a)
implementiert, (b) produktiv angeschlossen, (c) komponentengeprüft, (d)
Runtime-geprüft, (e) blockiert/offen, je ID mit Originalfundstellen Datei:Zeile.

---

## T5 — Subplayer-Pfad (Modell: deep4flash-sonnet)

Teilaufgabe T5 des Nutzerauftrags „vollständiger nativer Port ALLER
Originalgegner": der vollständige `bhSubpl`-Pfad (kinematische Gegner-/
Darsteller-IDs) nativ.

PFLICHTLEKTÜRE VOR ARBEIT: `AGENTS.md` VOLLSTÄNDIG (3109 Zeilen, seitenweise
lesen), `QUALITY.md`, `docs/THIRD_AGENT_C01_TASK.md`. Dann `git status --short`
und `python3 tools/check_no_original_core.py` (vor Arbeit und vor jedem Build;
derzeit PASS).

HARTE GRENZEN:
- `archived/FORBIDDEN_DONTUSE` niemals lesen, durchsuchen, kopieren oder
  indirekt nutzen.
- `src/ps2/veronica/prog/subpl.c`, `subpl.h`, `eneset.c` und das Original-ELF
  sind AUSSCHLIESSLICH Semantikreferenz. Keine Funktionskörper kopieren,
  extrahieren, generieren, kompilieren, linken oder aufrufen. Kein Generator.
- Kein zweites sys/plp/rom/ene/eff, keine BH_PWORK-Spiegeltabelle. Einziger
  Besitzer bleibt die vorhandene WorldEntity/RoomWorld-Quelle.
- Keine Heuristik, geratene Konstante, Raum-/ID-Sonderregel, stiller No-op,
  abgeschwächte Testerwartung. Ungeklärte Semantik als Portierungslücke mit
  Fundstelle offenlegen.
- Gemeinsamer Arbeitsbaum mit aktiven Peers: nichts zurücksetzen, stashen,
  löschen. Kein Commit, Push, Paket.

DATEIBESITZ (exklusiv): `native/subplayer.c`, `native/subplayer.h` sowie neue
`native/subpl_*.c/.h`. NICHT anfassen: `CMakeLists.txt`, `native/world.*`,
`native/runtime.cpp`, `native/events.cpp`, `native/enemies/enemy_spawn.*`,
`native/enemies/en01_damage.*`, `native/player_combat_core.c`, `native/targeting.cpp`,
`native/effect_particles.cpp`, `native/effect_sprites.*`, `native/renderer.*`,
`native/scene.h`, neue `native/enemies/enemy_dispatch.*`/`enemy_work.*` und alle
enNN-Dateien (andere Teilaufgaben).

KLARE VORGABE (Originalbelege selbst nachlesen und zitieren): `bhSubpl`
(`subpl.c:76`) ruft `Subpl_tbl[epw->mode0](epw)` mit `Subpl_tbl[7] =
{init_subpl, move_subpl, NULL, NULL, NULL, em_sce, em_sce}` (`subpl.c:12`).
`init_subpl` (`subpl.c:87`) setzt `mode0 = 1` und ruft
`subpl_init_tbl[epw->id - 40]` (120 Einträge, `subpl.c:22`). `move_subpl`
(`subpl.c:131`) ruft `subpl_mv_tbl[epw->mode2]`; `em_sce` (`subpl.c:137`) ruft
`subpl_mv01_tbl[epw->ct2]`. Auch `subpl_mv00_tbl` (`subpl.c:38/56`) gehört dazu.
`bhEne28` ist im Original eine LEERE Funktion (`subpl.c:410`) und bleibt leer. In
`eneset.c:51` sind die `bhSubpl`-Einträge die IDs 31..52, 56..70 und 72..99.

GEMESSENE DATENLAGE (aus den 205 RDT in `data/rooms`, nicht raten): 45 dieser IDs
kommen tatsächlich in Räumen vor, zusammen 195 aktive Works; häufigste sind ID 91
(38 Works), 92 (22), 93 (17), 67 (14). Der heutige Host bindet nur
`identity > 90` als Aktor und `recvx_is_subplayer`; `recvx_subplayer_tick`
liefert für die meisten Modi 0 — das ist die zu schließende Lücke. Bestimme die
real vorkommenden IDs selbst aus den Raumdaten (Header bei `u32(data,16)`,
`ene_count` bei `header+0x88`, `enep` bei `header+8`, Enemy-Slots 36 Byte, aktiv
bei `flg&1`) und berichte sie belegt.

QUELLENGEPRÜFTE FLAG-REGEL: `bhSetEnemy` (`eneset.c:170/183`) kopiert `etp->flg`
der EGG_WORK-Vorlage — dort kommt `flg` Bit 0 her; `id > 40` setzt zusätzlich
`mdflg |= 0x20`, die clp_jno-Sätze für 41..48 bzw. 91..98 und `flg |= 0x8000`
(`eneset.c:187-229`); `id > 90` setzt `mdflg |= 0x100`. `bhControlEnemy`
(`eneset.c:344`) ruft für `id > 90` zuerst `bhControlMask`, dann
`bhJumpEnemy[id]`.

REIHENFOLGE: Eine parallele Teilaufgabe (T1) erarbeitet gerade den gemeinsamen
Enemy-Work-/Service-Vertrag auf der vorhandenen WorldEntity/RoomWorld; vorher
KEIN eigenes Work-/Mode-Interface implementieren und `RECVX_SUBPLAYER` nicht
einseitig umbauen. Nutze die Zeit für die ID-/Tabellen-/Abhängigkeitsmatrix.
Schnittstellenbedarf an gemeinsamen Dateien als Patch-Beschreibung melden, nicht
selbst ändern.

KEINE Builds, kein CTest, keine Runtime-Läufe bis das Gesamtpaket aller fünf
Teilaufgaben integriert ist. Policy-Prüfer bleibt Pflicht.

BERICHT: `work/native_port_T5_subplayer.result.md`, getrennt nach (a)
implementiert, (b) produktiv angeschlossen, (c) komponentengeprüft, (d)
Runtime-geprüft, (e) blockiert/offen, mit Originalfundstellen Datei:Zeile.
