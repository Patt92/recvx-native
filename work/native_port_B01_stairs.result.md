# B01 Treppen-, Leiter- und Stufenaktionen — nativ portiert

Stand 2026-10-10. **Implementiert und gemessen.** Die sechs Originalroutinen
`kdu`/`kdd`/`dnu`/`dnd`/`hsu`/`hsd` laufen als ein zusammenhängendes Paket:
Auswahl aus dem ETC-Bereich, Originalzustandsübergänge, Motion-/Bildfolgen,
Wurzelbewegung, Fußfixierung, Etagenwechsel, Schrittgeräusche, Flags und
Steuerungsfreigabe. Auch `bhPlayerKaidanMotion` (Skriptopcode 0xa4) ist
angebunden. Kein Sofortabschluss, keine Teleportation, keine erfundene
Kletterbewegung.

`native/player_stairs.c` (839 Zeilen) ist neu geschrieben; der alte Generator
`tools/generate_player_stairs.py` wird nicht mehr benutzt. Es wurde **kein
Funktionskörper übernommen**, der Quarantänebereich nicht gelesen und
`FORBIDDEN_DONTUSE` nie benutzt.

## Quellenbelege (Semantikreferenz, kein Körper übernommen)

| Gegenstand | Fundstelle |
|---|---|
| Aktion `kdu` (Treppe auf) | `src/ps2/veronica/prog/player.c` 2872–3134 |
| `kdd` (Treppe ab) | ebd. 3135–3348 |
| `dnu` (Stufe auf, „dansa") | ebd. 3349–3507 |
| `dnd` (Stufe ab) | ebd. 3508–3933 |
| `hsu` (Leiter auf) | ebd. 3934–4128 |
| `hsd` (Leiter ab) | ebd. 4129–4321 |
| Bildvorlauf, `flg 0x400000`, `mtn_md` | `Motion.c` 21–120 (`bhSetMotion`) |
| Wurzelschlüssel des Objekts 0 | `Motion.c` 919–975 (`bhGetObjMotion`) |
| Grundnummer/-setzung, Tempomarke | `pwksub.c` 85–141 |
| Schrittgeräusch der Fläche | `hitchk.c` 5434–5467 (`bhCheckFloorSound`) |
| Wasserprobe | `hitchk.c` 5532–5558 (`bhCheckWater`) |
| Effektanforderung | `effect.c` 331 ff. (`bhSetEffect`), 436 ff. (`bhSetEffectTb`) |
| Auswahl der Aktion, Sperren | `hitchk.c` 4588 ff., besonders 4640–4730 (`bhCheckExmAtari`) |
| Sprungtabelle in die Aktion | `player.c` 1165 ff. (`bhKaidanPlayerMotion`), `event.c` 0xa4 (`bhPlayerKaidanMotion`) |
| Schrittstimmenplatz und Tonhöhe | `sdfunc.c` 1426–1500 (`GetPlayerActionSeSlotNo`, `CallPlayerFootStepSeEx`) |
| `rtspd` vor dem Aktionsdispatch | `player.c` 1956–1991 (`bhCPM1_act_bas`) |
| Bewegungstabellen (Originaldaten) | `player.c` 216–281 (`PlMtnAct`, `PlFootSnd`, `PlKDU`) |

## Auswahl der Aktion — Korrektur einer früheren Lesart

`bhCheckExmAtari` entscheidet in zwei Schritten, und **beide** Parameter sind
früher verwechselt worden:

* **`prm0` (`parameters[0]`) wählt auf/ab**, nicht die Richtung der Sonde:
  `prm0 == 0` → aufsteigend, `prm0 != 0` → absteigend. Beim Absteigen setzt das
  Original zusätzlich `pp->flr_no = bhCheckFloorNum(pp->py - 2.0f * prm2)`.
* **`attr & 1` wählt die Routinenfamilie**: 0 → Treppensatz (`kdu`/`kdd`,
  mode2 14/15), 1 → Leitersatz (`hsu`/`hsd`, mode2 20/21).

Beide Zweige setzen `pp->stflg |= 0x80010010`, `pp->flg |= 0x10400`,
`pp->flg &= ~0x110`, `sys->st_flg |= 4` und `sys->pl_htp = exp`; der Typ-1-Zweig
ruft zusätzlich `bhSetUseKaidanFlag`. Der `case 2`-Zweig (Stufe) setzt
`0x80010020` statt `0x80010010` und **kein** `st_flg 0x4`; die abschließende
danf-Sonde setzt ebenfalls nur `0x80010020`. Jeder Treffer setzt vorher
`sys->cb_flg 0x100` und `sys->etc_idx`; beide Zweige des `case 2` enden mit
`return`, die weiteren Sondeeinträge werden nicht mehr geprüft.

Sondentabelle (aus dem Originalcode): Typ 1 bei `0.75` vor dem Spieler, danf
bei `4.5` (Eintritt) und dann `6.0` Vortritt mit `9.1 + py` Wandsonde bzw.
`3.0 + ar` / `py − 8.9` in der Abschlusssonde.

## Portierte Verfahren

**Die sechs Aktionsroutinen** sind je als eigener Durchlauf formuliert:
Startblock (`flg &= ~0x28C0008`, `flg2 |= 1`, Ruhe-/Anlaufmotion aus
`PlMtnAct[wpntp][dmlvl]`, `hokan_rate`, `mtn_add`, `arp = 0`, Startbild 0),
Giernäherung an `ayp` (Schwelle 1310, `182.04445 * 7.2 * rtspd`), Laufphase,
Höhen-/Wandprüfung, Schrittstimmen, Abschlussblock und Ruhemotion
`PlMtnAct[wpntp][dmlvl][0]`. Die Tabellen `PlMtnAct`, `PlFootSnd` und `PlKDU`
sind unveränderte Originaldaten.

**`bhSetMotion`** ist am Ende jedes Durchlaufs gebunden: `frm_no += mtn_add`
bzw. `−= mtn_add` im Rückwärtsmodus, die Bereichsprüfung
`frame <= 0 || frame >= total − 65536`, der `frm_mode`-Umschlag, und daraus
`plp->flg 0x400000` für den nächsten Durchlauf.

**Abschlussblock** (am Beispiel `dnd`, `player.c` 3508 ff.): `flg |= 0x200000`;
nur wenn `flg & 0x400000` steht, wird der Knoten gelesen, `py -= 9.0`,
`status &= ~0x80010020`, `flg &= ~0xD0400`, `flg |= 0x118`, die Grundnummer
neu gesetzt, `rom->grand` aufgeschlagen, mode0 = 1, Ruhemotion, Hokan 8 Bilder,
`mtn_md |= 0x100` und die Arbeit freigegeben. Das Original setzt `frm_no` in
demselben Block zuerst auf 65536 und danach auf 0 — die erste Zuweisung ist
wirkungslos und ist im Port als solche kommentiert.

**Bodendienste**: `bhCheckFloorNum`/`bhSetFloorNum`/`bhAddSpeed` über den
Originaltabellen, `bhCheckFloorSound` als exakter Aufruf mit den
Originaloperanden (Flächenliste der Raumdaten, danach `bhObj001`-Liste),
Schrittstimmen über `CallPlayerFootStepSeEx` (Bank 2, Liste
`FootDef[FloorType]` bzw. 5, Platz aus `GetPlayerActionSeSlotNo`) und
`bhCheckWater` als `1` innerhalb eines Typ-3-Gebiets mit `prm0 == 1` samt
`top = py + h`.

**Wurzelbewegung**: `bhGetObjMotion` wird über die Originalmotion des
Spielerkörpers gelesen, nicht geschätzt. Der `frame` ist der 16.16-Zähler der
Spielerarbeit; die Laufzeit teilt **vor** dem Schlüsselzugriff durch 65536.

## Anschluss an die Laufzeit (kleiner Patch)

`native/runtime.cpp` enthält nur die Verbindungen, keine eigene Logik:
Opcode 0xa4 wird abgefangen und im selben Bild nach `events->tick()` über
`drain_script_stairs()` abgearbeitet; `begin_stairs`/`tick_stairs` liefern die
Originaldienste; `stairs.rtspd` wird vor jedem Durchlauf nach `bhCPM1_act_bas`
gesetzt; der erreichte ETC-Index geht als `sys->etc_idx` (`values[20]`) und
`cb_flg 0x100` zurück in die eine Spielerarbeit.

Wesentlich: `flg`, `status` (`stflg`), `flg2`, `mode0`, `ax`/`ay`/`az`,
`motion`, `frame`, `floor`, `rtspd`, `arp` und `mtn_*` werden **vor** dem Aufruf
hinein- und **danach** zurückgeschrieben. Der C-Zustand ist eine Sicht auf die
einen vorhandenen Spielerarbeit; die Buchführung des Originalwerks
(`spx`/`spz`, `bpx`/`bpy`/`bpz`, `pxb`/`pyb`/`pzb`) gehört der vorhandenen
Bewegungs-/Kollisionsarbeit und wird hier nicht doppelt geführt.

## Gemessene Originalwerte

**Leitersatz im Auslieferungsdatenbestand nicht erreichbar** (gemessen, nicht
vermutet): über alle **205** Raumdateien gibt es **150** Typ-1-Bereiche
(Treppen-/Leitersatz), davon **0** mit `attr & 1`, sowie **87** Typ-2-Bereiche
(Stufen). In allen fünf Spielerkörpern (`data/archives/SYSTEM/0010..0014.bin`,
je 100 Bewegungsplätze) sind die Plätze **62 bis 67 leer** — genau die
Bewegungen, die `hsu` (62/63/64) und `hsd` (65/66/67) anfordern. Die Aktionen
20/21 sind damit **nur im Komponententest** darstellbar; das ist das ehrliche
Maximum und wird so berichtet, nicht als „nicht geprüft" verschwiegen.

**Skriptpfad `bhPlayerKaidanMotion`** (RM_1120 und RM_1121, **ohne jede
Eingabe und ohne Diagnoseplatzierung**). Dass es wirklich der Skriptpfad ist,
beweist der Code: `native/input.cpp:172` erreicht den Interaktionshaken
ausschließlich bei einem Bestätigungsdruck (`MenuInput::Confirm`), und
`begin_stairs` ist nur über `gameplay_input.interact` erreichbar
(`runtime.cpp:307`, Zuweisung `:511`). Ein Bild-1-Lauf ohne Replay und ohne
Tastendruck kann die Aktion also nur über Opcode 0xa4 gestartet haben.

| Bild | Messung |
|---|---|
| 1 | `active`, mode2 15 (kdd), mode3 1, Motion 39, Grund 1, y = 20, `st_flg 0x4` |
| 5 | mode3 5, Motion 32, y = 20 |
| 40 | y = 12 |
| 60 | y = 4 (16 Einheiten herab) |
| 75 | freigegeben, mode0 1, Motion 42, Grund 0, y = 0, Ende (29.760, 0, −40.025), `st_flg 0x4` gelöscht |
| 900 | unverändert gegenüber 75 |

Klangbelege: `MULTSPQ1/0032/2/0000` viermal und `MULTSPQ1/0032/2/0004` einmal;
`audio_unavailable` und `sequence_unavailable` bleiben leer,
`blocked_commands` ist 0.

**Steuerungsfreigabe** (RM_1120): mit `100 keys forward` läuft der Spieler
z −40.0253 (Bild 100) → −16.8149 (160) → +14.4408 (240). Mit
`2 keys forward` — Eingabe **während** der Aktion — ist die Bahn
bitgleich zur eingabefreien: y 20 → 12 (Bild 40) → 4 (Bild 60). Die Bewegung
ist also während der Aktion gesperrt und danach frei, wie im Original.

**Vier Interaktionsläufe** (RM_0050/RM_2050). Die Positionierung stammt aus
dem Replay und ist eine **diagnostische Platzierung**; geprüft wird die
Aktion selbst.

| Aktion | ATR-Satz | mode2 | Grund | ungebundene Dienste | Endlage |
|---|---|---|---|---|---|
| `kdu` | RM_0050 idx 3, Typ 1, `prm2 = 4` | 14 | 2 | `0x380` | (−184.9390, 8.0000, 88.0045), Gier 49152 |
| `kdd` | RM_0050 idx 4, Typ 1, `prm2 = 4` | 15 | 0 | `0x380` | (−196.5710, 0.0000, 85.0588), Gier 16384 |
| `dnu` | RM_2050 idx 7, Typ 2 | 16 | 1 | `0x380` | (40.0015, 9.0000, 41.0497) |
| `dnd` | RM_2050 idx 9, Typ 2 | 17 | 0 | `0x384` | (35.0000, 0.0000, 26.3016) |

Alle vier enden auf Motion 42, melden `blocked_commands = 0` und liefern
`audio_unavailable` leer. Die Höhendifferenzen entsprechen den Daten:
`prm2 = 4` ergibt bei `kdu`/`kdd` genau 8 Einheiten, bei `dnu`/`dnd` greift die
im Original fest verdrahtete `9.0`.

**Keine Teleportation** (`dnd`, RM_2050, Bild für Bild aus der Laufzeit):

| Bild | mode3 | Motion | Grund | aktiv | Lage der Arbeit | Kamera |
|---|---|---|---|---|---|---|
| 62 | 2 | 39 | 1 | ja | (35, 9, 35) | (19.8266, 10.7823, 10.4082, −2760) |
| 64 | 4 | 22 | 1 | ja | (35, 9, 35) | unverändert |
| 66 … 86 | 5 | 22 | 0 | ja | (35, 9, 35) | unverändert |
| 88 | 5 | 42 | 0 | nein | (35, 0, 26.3016) | unverändert |
| 90 | 5 | 42 | 0 | nein | (35, 0, 26.3016) | (42.0527, 19.3726, 52.9238, 2371) |

Gerenderte Bilddifferenzen (jeweils zwei Bilder Abstand, 640×480):
84→86 **7.45 %**, 86→88 **7.17 %** (der Freigabebild), 88→90 **99.98 %**.
Der große Sprung liegt also **einen Bild nach** der Freigabe und gehört dem
Kameraverfolger, der nach dem Etagenwechsel umschaltet — nicht der
Positionsübernahme. Die Übernahme selbst bewegt die Figur um die gewöhnliche
Strecke.

## Tatsächliche Tests

**`core_continuity`** (CTest, ohne Originaldaten, `tests/native/continuity_test.cpp`
ab Zeile 26). Fährt alle sechs Routinen je einmal vollständig durch, mit
echten Motionlängen (27 Bilder für 31/33/35/37, gemessen aus
`0010..0014.bin`), einer Wurzelbewegung in Originalform und den sechs
Hostdiensten. Geprüft: der Einsprungszustand von `bhCheckExmAtari` (`case 1`
setzt `0x80010010`, `case 2`/danf setzen `0x80010020`), `st_flg 0x4` nur in den
beiden `case`-Zweigen, Freigabe genau in dem Bild, in dem der Abschlussblock
zurückgibt (ein `0` ohne Freigabe ist ein Fehler), Rückkehren auf die
Originalgrundtabelle, `PlMtnAct[..][..][0]`, `flg 0x118` gesetzt und
`0xD0400`/`0x80010030` gelöscht, Hokanarbeit geöffnet, Kaidanmarke gelöscht
und — je Treppenfamilie unterschiedlich — Schrittstimmen vorhanden bzw. nicht.
Dazu die danf-Stufe mit `st_flg 0x40`: Wasserprobe an Knoten 19, Effekt 108 mit
`lkono 15`, sechs `bhSetEffectTb` 11 und `CallPlayerActionSe 522`.

**`tools/verify_stairs.py`** (neu, 250 Zeilen, **82 Prüfungen, 0 Fehler**)
gegen die echte Laufzeit und die Originalressourcen. Es prüft unabhängig:
den Datenbestand (205 Räume, 150/0/87 Bereiche, leere Leitermotionen), den
Skriptpfad in RM_1120 **und** RM_1121, die Steuerungsfreigabe, die Sperre
während der Aktion, die vier Interaktionsläufe und die Freigabebild-Trace.
Die vier Interaktionsläufe sind im Skript ausdrücklich als diagnostische
Platzierung ausgewiesen.

**`ctest`**: **49/49 bestanden**, sowohl mit `-j1` (9.14 s) als auch mit `-j8`
(4.21 s) in `build/doors-claude-20261009`. Ein früherer Stand dieses Pakets
hatte sieben rote Nachbarsuiten gemeldet; nach dem Neubau ist das nicht
reproduzierbar und wird hier als **gemessene Änderung** festgehalten, nicht
wegerklärt. Die genannten Namen (`ammunition_pickup_zero`,
`combat_weapons_ammunition`, `player_push`) sind echte CTest-Fälle und bestehen.

**`tools/check_no_original_core.py`**: PASS, vor und nach dem Bauen.

**205/205 Räume laden** weiterhin; die blockierten Befehle bleiben auf dem
Stand der Nachbarpakete.

## Offene Bindungen

* **Drei Dienste haben keinen nativen Besitzer** und werden als
  `unsupported_services` geführt, nicht stillschweigend verworfen:
  `0x80` SetMtnNormalHokan-Blend, `0x100` Kopfwerk `EXP1_I(0)`, `0x200`
  bildweises `mtn_attr`. Sie werden von **jedem** Lauf gesetzt (`0x380`).
  `dnd` erhöht auf `0x384` durch `StartVibrationEx(0, 7)` im mode3-4-Block.
* **`AudioMixer::footstep` weicht ab** (Codex' Datei, nur gemeldet, nicht
  geändert): dort stehen die Paare `{{4,5},{14,15},{16,17}}` und ein
  `floor > 4`-Riegel, der die Aktionsstimmen ausschließt; das Original hat
  `SlotDef = {11,12,13,14,15,16}` mit `(Id*2) + PlayerFootStepSwitch[Id]` für
  die Typen 0/1/4/5 und Platz 6 für 2/3/6. Der Treppenaufruf geht deshalb
  direkt über `AudioMixer::play_room_sound`.
* **Nicht portiert**: die Tonhöhenstreuung `WalkPitchTbl {0,256,256,0}` /
  `RunPitchTbl {512,768,768,512}` (Zufallsindex `4.0f * (-rand() / -2.1474836e9f)`)
  und `PitchDelayTime = -2` für die Typen 2/3.
* **Analogzweig von `rtspd`**: `bhCPM1_act_bas` liest im zweiten Zweig
  `sys->pad_dx`/`pad_ax`. Während einer Stufenaktion trägt `stflg` immer
  `0x80010010`/`0x80010020`, also greift stets der erste Zweig
  (`dmlvl 2 → 0.8`, sonst `1.0`) — dieser ist gebunden. `InputFrame` führt
  keine Achsenbeträge; der Analogzweig bleibt offen gemeldet.
* **Fünf Sondeeinträge der Typen 0/3/4** stützen sich auf `EXP_WORK fpx/arn`,
  das keinen nativen Besitzer hat; diese Einträge bedient weiterhin
  `EventVm::interact`. Die Typ-1- und Typ-2-Suche liegt jetzt in `begin_stairs`
  und läuft dort zuerst.
* **`sys->pl_htp`** (die gewählte Fläche geht als `htp` in die Spielerarbeit),
  `plp->flg2 0x8`, `plp->ax`/`plp->az` und die Buchführung `spx/spz`,
  `bpx/bpy/bpz` haben **keinen nativen Besitzer** außerhalb dieses Pakets.
  `flg2 0x1` (Hokanarbeit) wird gesetzt und zurückgeschrieben.
* **`plp->flg = 0x119`** stammt aus `bhSetPlayer` (player.c:691) und gehört dem
  Spielermodusbesitzer, nicht B01. Der Abschlussblock setzt `flg |= 0x118`;
  die beiden stimmen bis auf Bit 0 überein. Es wird hier nichts nachgebaut.
* **Ein Bild Verzögerung**: die Knotenlagen `nodes` werden **vor** dem
  Durchlauf abgetastet, die Laufzeit übernimmt die Lage aber erst nach dem
  Bild. Die Fußfixierung liest dadurch die Lage des Vorbilds. Sichtbar
  unbedeutend, aber eine Abweichung und deshalb genannt.
* **Interaktionshaken nur bei Bestätigung**: `native/input.cpp:172` ruft
  `interact` ausschließlich bei einem `Confirm`-Druck. Das Original prüft die
  Sonden in `bhControlPlayer`; der Unterschied ist nur über die Eingabe
  sichtbar.
* **danf-Wasserzweig** braucht `sys->st_flg & 0x40`; ohne dieses Bit läuft die
  Stufe ohne Wasserprobe, wie im Original.
* **`bhSetDansaLimitAtari`/`bhCheckDansaAtari`** liegen in Codex' B00-Datei
  `native/geometry_motion.cpp`; B01 liest sie nicht.
* **Nicht Teil dieses Pakets**: Gegner-KI, Effekt-Emitter, Waffe/Mündungsfeuer,
  Tür-/Objekt-SE und die Box-Oberfläche. B01 bindet nur die eigene Aktionsfolge
  samt Schrittstimmen, Grundwechsel und Freigabe.

## Getrennte Befunde macOS / Windows

* **macOS arm64 — gebaut und gelaufen.** Prüfer PASS,
  `build/doors-claude-20261009`, **49/49 CTest** in `-j1` und `-j8`,
  `tools/verify_stairs.py` 82/82. Die Messwerte dieses Berichts stammen
  ausschließlich aus diesem Verzeichnis.
* **Windows x64 — nur kreuzgebaut, nicht gelaufen.** Eigener Werkzeugkasten in
  `build/windows-doors-claude-20261010`: der vollständige Bau läuft durch,
  **51 ausführbare Dateien** liegen vor, `recvx-runtime.exe` ist 2 297 856 Byte
  groß, der eingebaute Prüfer meldet PASS. Der echte dreistufige PE-Audit
  (`tools/windows_link_audit.py`, Linkbefehl aus dem Buildsystem, jede Eingabe
  einschließlich aller Archivmitglieder, danach ein Vergleichslink Byte für
  Byte gegen das ausgelieferte Binary mit ausschließlich aus dem PE geparsten
  Metadatenfeldern) liefert **0 Befunde**. Die zugehörigen Negativprüfungen
  laufen als CTest-Fall `windows_link_audit` mit. **Ein Windows-Lauf hat nicht
  stattgefunden**: Wine ist in dieser Umgebung nicht startbar (siehe unten),
  Windows-Hardware gibt es nicht.
* **Kein Windows-/Wine-Lauf, gemessene Ursache**: jeder Wine-Aufruf bricht mit
  `wineserver: mkdir /tmp/.wine-501/server-…: Operation not permitted` ab. Das
  ist eine **Sandbox-Grenze** — `touch /tmp/.wine-501/f` scheitert mit
  `Operation not permitted`, während `mkdir -p /tmp/claude/sbtest` gelingt.
  Wine legt seinen Sitzungsordner fest unter `/tmp/.wine-<uid>` an; ein
  Umlegen von `TMPDIR` hilft nicht. Das Abstellen der Grenze ist eine
  Entscheidung des Nutzers, nicht meine.
* **Buildwerkzeug-Fallstrick, gemessen und dokumentiert**: im frischen
  Windows-Verzeichnis war `CMAKE_C_FLAGS_RELEASE` leer, weil CMake die
  Compilerkennung von zig nicht kennt und deshalb keine Vorgabeflags setzt.
  C-Dateien wurden dadurch mit `-O0` übersetzt, und **`zig cc` 0.16.0 schaltet
  bei `-O0` UBSan von sich aus ein, bei `-O3` nicht** (an einer zwei Zeilen
  langen Probe nachgewiesen). Der Link scheiterte an
  `__ubsan_handle_*`. Mit `-DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG"` (und
  entsprechend für C++) baut es durch.

## Noch rot, ausserhalb dieses Pakets

`verify_detector`, `verify_pickups`, `verify_ammunition`, `verify_continuity`
und `verify_combat` bleiben offene Projektaufgaben. `verify_motion_collision`
und `verify_stairs` sind neu hinzugekommen.

## Nicht angefasst

`world.*`, `events.*`, `actor.*`, `audio.*`, die Effektdateien und CMake sind
unverändert; `native/geometry_motion.*` und `native/geometry_motion.cpp` sind
Codex' B00-Arbeit und wurden nicht berührt. Geändert wurden
`native/player_stairs.c` (neu), `native/player_stairs.h`,
`tests/native/continuity_test.cpp`, `tools/verify_stairs.py` (neu) und dieser
Bericht; in `native/runtime.cpp` kamen nur die oben beschriebenen
Anschlusspunkte dazu.

**Kein Commit, kein Push, kein Paketupdate.** `build/packages/macos-arm64-v0.5`
enthält diesen Stand **nicht**; nach der Regel `deliver-to-start-command-package`
zählt B01 erst nach Einspielen in das Paket, Neusignieren und Prüfung über
`Start.command`. `build/windows-doors-claude-20261010` ist ein eigenes
Verzeichnis und bleibt vorerst liegen.
