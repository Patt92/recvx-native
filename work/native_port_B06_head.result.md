# B06 — bhControlPlayerHead vollständig nativ portiert

Stand 2026-10-10. Eigene Dateien: `native/player_head.c`, `native/player_head.h`,
`tests/native/player_head_test.cpp`, `tools/verify_head.py`, dieser Bericht.
Abgestimmte Fremdanschlüsse: `native/player_stairs.c/.h` (eigene B01-Dateien),
`tests/native/continuity_test.cpp` (eigener B01-Test), `tools/verify_stairs.py`
(eigenes B01-Werkzeug), `CMakeLists.txt` (eine Testgruppe, eine Quelldatei) und
kleine Hunks in `native/runtime.cpp`.

Vor der Arbeit und vor jedem Build: `git status --short` und
`python3 tools/check_no_original_core.py` →
`PASS: audited source/build/binary scope has no forbidden core; not a gameplay acceptance.`
Quarantäne (`archived/FORBIDDEN_DONTUSE/`) wurde nicht gelesen und nicht benutzt.
Originalquellen ausserhalb der Quarantäne ausschliesslich als Semantikreferenz;
kein Originalkörper, kein Generator, keine PS2-Nachbildung, keine Ersatzlogik.
Kein Commit, kein Push, kein Paketupdate.

## 1. Was portiert ist

| Original | Datei:Zeile | Native Stelle |
|---|---|---|
| `bhControlPlayerHead` | player.c:6746-7006 | `recvx_head_tick` |
| `bhLookNearEnemy` | player.c:7009-7050 | `head_look` |
| `bhSetHeadRotation` | player.c:7053-7204 | `head_rotation` |
| `bhSearchNearEnemy2` | pwksub.c:395-503 | `head_search` |
| Rückführpfad eines Winkels | player.c:6879-6958 | `head_return` |
| `PlyEyeTab` | player.c:206-209 | `static const signed char PlyEyeTab[8]` |
| `bhSetPlayer`-Vorgabe | player.c:787-789 | `recvx_head_begin` |

Der Zustand ist eine Sicht auf den einen vorhandenen Spieler-/Actor-Besitzer:
die Laufzeit übergibt je Bild die lokalen Knotenwinkel 0..5 aus dem
Posebesitzer, `plp->ax/ay/az` aus `SceneSpawn` und die Weltmatrix des
Kopfknotens; sie schreibt die veränderten Knoten 4 und 5 dorthin zurück. Es
gibt keine zweite Spielerwelt und keine zweite Pose. `plp->exp1` ist im
Original ein typloser Puffer; jedes Feld in `RECVX_HEAD_STATE` ist mit seinem
Originaloffset kommentiert (`EXP1_I/S/F/UC`, macros.h:50-54).

Übernommene Originalgrössen, keine geschätzten Werte: Rückführschwelle 182
(ein Grad), Viertelschritt `(ziel-gehalten)/4`, Neigungsgrenze ±6144,
Giergrenze ±8192, Halbierung des Ergebniswinkels, Suchfenster 23665,
Einrastzeit 30, Zähler­decke 1024, Punktnachführung 1.6, Blinkschwelle 0.97,
Blinkbereitschaft 30, Blinkruhe 60, Winkel aus `atan2` mit 10430.381.

Alle Zwischenergebnisse laufen wie im Original über `short`: die dreistufige
Winkelsumme (Knoten 0..3 + `ax/ay/az`, dann Knoten 4, dann Knoten 5) kürzt auf
`int16_t` und läuft über, sie sättigt nicht.

## 2. Vollständige Kopfflags, Schreiber und Verbraucher

`EXP1_I(0)`:

* `0x1` Augentextur überhaupt tauschen — gesetzt von `bhSetPlayer` (787),
  gelöscht nirgends; gelesen player.c:6986.
* `0x2` Blinkfolge läuft — gesetzt 6976, gelöscht 6996, gelesen 6990.
* `0x4` Blicksuche aktiv. **Gesetzt** in den mode3-0-Blöcken der sieben
  Laufaktionen (std 2112, srt 2176, sta 2237, wlk 2312, run 2438, bak 2575,
  bk2 2727), im Laufwende-Übergang von `bak`/`bk2` (2617/2752, `mode2=6` mit
  `mode3=1`), in den vier Skriptaktionen pl_evt.c:75/242/321/401 sowie
  en01.c:7613 und en07.c:2209/2251. **Gelöscht** von den sechs Treppen-/
  Stufenroutinen (2891/3153/3360/3522/3948/4143), `psh` 3741, `cro` 3897,
  `rpsh` 4336, dem Angriffseinstieg in `bhCPM0_action` (1800), `knf` 6234,
  `bhCPM0_damage` 6288, `bhCPM0_die` 6373, 6480, `bhCPM0_nage` 6598,
  `bhCPM0_enedam` 6637, `bhCPM0_enedie` 6676, playpch.c:46 (`~0x1E4`) sowie
  en01.c:7541/7657, en04.c:3508, en07.c:2197/2228. Gelesen 6598/6637/6676/6822.
* `0x8` Blickpause. **Befund: kein Schreiber im ganzen Spielcode.** Gelesen
  player.c:6824, gelöscht playpch.c:73 — gesetzt wird es nirgends. Der Zweig
  ist damit in der ausgelieferten Fassung unerreichbar; er ist mitportiert und
  nur im Komponententest auslösbar.
* `0x10` Ziel aufgeschaltet — gesetzt 7028, gelöscht 7040, gelesen 6829.
* `0x20/0x40/0x80/0x100` die vier Rückführungen — stets gemeinsam als `0x1E0`
  gesetzt (19 Stellen), einzeln gelöscht 6892/6909/6955 und im Knoten-5-Pfad.

`EXP1_UC(120)` Skriptfolge: `0xF0` aktiv, `0x20` Gierstufe abwärts, `0x40`
Neigungsstufe abwärts (6776-6818). `EXP1_I(64)` Zielart: **einziger Schreiber
im ganzen Spielcode ist `bhLookNearEnemy` (7030) mit dem Wert 2.** Die fünf
anderen Zweige (Punkt, Objekt, Item, Effekt, Raumposition) sind mitportiert,
aber in der ausgelieferten Fassung unerreichbar — wie `flg2 0x100` beim Haar.

## 3. Originale Aufrufreihenfolge

Belegt aus `bhControlPlayer`: `bhCtrPly_mode0[plp->mode0]()` (player.c:1417,
Tabelle 1333-1343) → `bhCPM0_action` (1777) → `bhCPM1_act_bas` (1957) →
`bhCPM2_act_*`; danach Kollision/Boden, dann `bhControlPlayerHead` (1685), dann
`bhCalcModel(plp)` (1688). `bhCalcTree` schreibt dort `owP[i].mtx`
(MdlPut.c:264). Erst anschliessend liest `bhObjClpn` dieselbe Matrix
(game.c:53 `bhControlObjItm` nach game.c:52 `bhControlPlayer`).

Die Laufzeit hält genau diese Folge in einem Durchgang:
`locomote()` (Aktion, Motionfortschreibung) → `head_hair_step()` mit
Kopfsteuerung → Modellmatrizen (`pose`/`sample_player` mit den Kopf-Overrides)
→ Haarsteuerung mit derselben `owP[5].mtx` → Darstellung
(`head_draw_overrides` legt die Kopfwinkel über die Kampf-Overrides).

Messnachweis für „dieselbe gültige Pose, keine zusätzliche Verzögerung":
`EXP1_F(12..20)` (Kopfpunkt, am Ende der Kopfsteuerung aus `owP[5].mtx`) und
die Haarwurzel (`njCalcPoint(owP[5].mtx, obwp[2].lo*)`) liegen in **jedem**
Bild exakt die Verknüpfungslänge 1.7659 auseinander (gemessen 1.7657–1.7659 in
RM_1120, Bilder 2/5/30/60/70). Ein Bild Versatz würde diese Distanz sofort
aufbrechen.

## 4. Anbindung des Flags an die Aktionsmaschine

Im Original gehört `0x4` der Aktionsmaschine: jede der sieben Laufroutinen
setzt es in ihrem `mode3`-0-Einstiegsblock und weist im selben Block
`plp->mtn_no = PlMtnAct[wpntp][dmlvl][n]` zu (Tabelle player.c:223-235,
`PlMtnAct[0][0] = {42,39,0,4,16,52,9}`). Der Einstieg ist also daran erkennbar,
dass die Spielerarbeit mit einer neuen `PlMtnAct`-Motion gefahren wird. Der
Host erreicht vier der sieben Routinen: 42 = `[0]` `act_std`, 0 = `[2]`
`act_wlk`, 4 = `[3]` `act_run`, 16 = `[4]` `act_bak`. `srt`, `sta` und `bk2`
hat der Host als eigene Zwischenaktionen nicht — ihr Flaggeneffekt ist
identisch, es fehlt also kein Flagverhalten.

`bhResetPlayer` schliesst mit `*(int*)&plp->mode0 = 1` (player.c:1152), also
`mode2=0`, `mode3=0`: der erste Dispatch nach einem Raumwechsel läuft
`act_std` case 0 und setzt `0x4`. Der Host reproduziert das mit einem
Sentinel-Startwert, der beim Raumreset zurückgesetzt wird.

Die Gegenrichtung, jeweils im Einstiegsblock der Spezialaktion:

* die sechs Treppen-/Stufen-/Leiterroutinen über den neuen Dienst
  `RECVX_STAIR_HOST::release_head_look` in `native/player_stairs.c`
  (`stair_open`, alle sechs Aufrufstellen),
* `bhCPM2_act_psh` (3741) auf der Flanke von `world->pushing()`,
* der Angriffseinstieg `bhCPM0_action` (1793-1801) auf der Flanke, mit der
  `CombatController` die Spielerarbeit übernimmt.

Weil eine Spezialaktion `mode2` aus der Laufmenge herausnimmt, ist die
Rückkehr immer ein frischer `mode3`-0-Einstieg; der Sentinel wird dabei
verworfen. Gemessen in RM_1120: Bild 60 Treppe aktiv, `flags 0x1`; Bild 70
Arbeit zurückgegeben, `motion 42`, `flags 0x5`. Genau ein Bild Abstand, wie im
Original, weil `act_std` case 0 erst im nächsten Dispatch läuft.

`RECVX_STAIR_UNBOUND_HEADWORK` (0x100) wird dadurch nicht mehr erhoben:
`stairs.unsupported` fällt im Treppenraum von 0x380 auf 0x280
(HOKAN 0x80 | MTN_ATTR 0x200). Das Bit und seine Vorsaat bleiben als
Meldepfad für einen Host bestehen, der `release_head_look` nicht stellt — der
Komponententest `core_continuity` prüft beide Seiten. Die beiden anderen
Treppendienste bleiben ausdrücklich offen und werden weiter gemeldet.

## 5. Gesammelte Prüfung

### Komponententest `recvx-player-head-tests` (75 Prüfungen)

Winkelsumme und Kürzung inklusive Überlauf; Kopfpunkt und Vorbildpunkt; keine
Suche ohne `0x4`; Einrasten erst nach 30 Bildern; Sichtprüfung blockiert;
fehlender Wanddienst sperrt das Ziel und wird gemeldet; ungeeigneter Gegner und
Gegner hinter dem Spieler werden übersprungen; der nähere von zwei gewinnt;
**Zielwechsel** setzt den Zähler zurück; Zielverlust startet alle vier
Rückführungen und leert den Index; **Pause** (`0x8`) stoppt Suche und
Zielwechsel und friert den gehaltenen Punkt; einzelner und vierfacher
Rückführpfad samt Ein-Grad-Schwelle; gehaltene Winkel folgen der Motion;
Skriptfolge hat Vorrang, beide Richtungen, beide Grenzen; Blinken mit
`PlyEyeTab`, Modell 3 und `mode0 4` unterdrückt; Treppenbindung in beiden
Richtungen.

### Produktive Laufzeit — `tools/verify_head.py`, 53 Prüfungen, alle bestanden

Ohne Diagnosepositionierung, mit echten Raumdaten:

* **RM_0120, natürlicher Lauf vorwärts.** Bild 1: `flags 0x5`, der
  `act_std`-Einstieg hat `0x4` gesetzt, `look_timer 1`. Bilder 40/90/140/200:
  Blick weiter aktiv, Suche lief in jedem Bild, kein erfundener Halt und keine
  erfundene Aufschaltung, Pose der Motion unverändert (`overrides 0`).
* **RM_0120, Drehen auf der Stelle und Stillstand.** Blick bleibt aktiv,
  `PlMtnAct`-Motion 42.
* **RM_0120, Blinken im selben Lauf.** Bilder 25–52 Ruhezähler, Bild 53 Start
  (`0x7`, Zähler zurückgesetzt), Bilder 53–59 `PlyEyeTab`
  `0,0,1,1,2,2,1`, Bild 60 Abschluss mit erzwungener Textur 0. Das ist ein
  vollständiges Augenschliessen und -öffnen in der laufenden Partie.
* **RM_1120, Raumskript-Treppe (`bhPlayerKaidanMotion`, ohne jede Eingabe).**
  Bild 1: Einstieg gibt `0x4` ab, setzt `0x1E0`, eine Rückführung läuft noch
  (`flags 0x21`) und schreibt einen Knoten-Override. Bild 2: die Rückführung
  unter 182 löscht ihr Bit und hört auf zu überschreiben. Bilder 5/30/60:
  Blick bleibt die ganze Aktion über aus. Bild 70: Arbeit zurückgegeben,
  `motion 42`, Blick wieder an, keine Rückführung offen, Treppenmeldung ohne
  Kopfbit.
* **Zusammenspiel mit dem Haar**, Abschnitt 3: gleiche Matrix, gleiches Bild.

### Regressionen

`ctest` im eigenen Verzeichnis `build/hair-claude-20261010`: **52/52 bestanden**
(inklusive `player_head`, `player_hair`, `core_continuity`, `player_push`).

Zustandsvergleich gegen das echte Vorher-Binary
`build/doors-claude-20261009/recvx-runtime` (01:27, vor B04/B06), gleiche Daten,
gleiche Eingaben:

| Szene | Unterschied |
|---|---|
| RM_0020, 120 Bilder Stillstand | nur die drei im alten Binary nicht vorhandenen Schlüssel `hair`, `head`, `render_clip` |
| RM_0120, 200 Bilder Lauf | dieselben drei Schlüssel |
| RM_1120, 100 Bilder Treppe | dieselben drei Schlüssel **plus** `stairs.unsupported` 896 → 640 |

Jeder andere Zustandswert ist identisch. Die eine Abweichung ist das
beabsichtigte Verschwinden von `RECVX_STAIR_UNBOUND_HEADWORK`.

Weitere Suiten gegen dasselbe Binary: `verify_stairs` (alle Prüfungen, nach
Anpassung der B01-Erwartung von 0x380 auf 0x280), `verify_continuity`,
`verify_motion_collision`, `verify_events`, `verify_progression`,
`verify_audio_state`, `verify_snapshot` — bestanden.

`verify_push` scheitert an `motion==49 && push.phase==4`. **Gemessen, nicht
geschlossen:** dieselbe Zusicherung scheitert mit demselben Zustand
(`push.active false`, `phase 0`) auf dem Vorher-Binary von 01:27. Der Befund ist
älter als diese Arbeit und liegt im Schiebemechanismus (nicht meine Datei).

### Nachtrag: Besitzer von `epw->flg` Bit 0 quellengeprüft

Der Kommentar an der Kandidatenschleife (`native/runtime.cpp`) nannte als
Besitzer von Bit 0 zunächst `bhEne01_Init`/`bhEne_SetWork` und für `0x8000`
`bhEneSetCk`. Beides ist am Quelltext nachgeprüft und **korrigiert**:

* Bit 0 stammt aus der Werkanlage `bhSetEnemy`, die `epp->flg = etp->flg` aus
  der `EGG_WORK`-Vorlage übernimmt (eneset.c:183) — also aus dem
  `ene_n`-Eintrag des Raums oder einer gegnerischen Statik wie
  `static EGG_WORK child = { 0x8001, 31, 0 }` (en02.c:223). `bhEne_SetWork` ist
  der Name dieser Vorlage, nicht einer zweiten Schreibstelle.
* `bhEne01_Init` setzt `flg |= 0x178` und `flg &= ~2` (en01.c:2712-2713),
  `bhEne00` Fall 0 `flg |= 0x8120` und `flg &= ~0x6` (eneset.c:401-402). Beide
  lassen Bit 0 unberührt; Bit 0 muss beim Eintritt in `bhEne01_Init` bereits
  stehen.
* `bhEneSetCk` (event.c:2715) schreibt ausschliesslich `stflg |= 0x1000000`
  (event.c:2738). Es setzt kein `flg`-Bit und löscht auch keines. Dass
  `0x1000000` in der Ausschlussmaske `0x41000000` der Suche liegt, ist der
  Zusammenhang — nicht `flg 0x8000`.

`0x8000` setzt die Werkanlage selbst für `id > 40` (eneset.c:229); der
Treffer `epw->flg & 0x8000` in der Suchmaske schliesst also die
Kind-/Anhangarbeiten aus, die keine eigene suchbare Arbeit sind.

Nach der Kommentarkorrektur neu gebaut und neu geprüft:
`cmake --build build/hair-claude-20261010` → **52/52 CTest bestanden**,
`tools/verify_head.py` **53/53 bestanden**, `tools/verify_stairs.py` alle
Prüfungen bestanden, `tools/check_no_original_core.py` PASS. Es wurden nur
Kommentare geändert; das Verhalten ist unverändert.

### Windows

**Build:** `build/windows-hair-claude-20261010d` (Ninja + zig, x86_64-w64-mingw32)
neu gebaut, `recvx-runtime.exe` 2 315 776 Byte, alle Testbinaries einschliesslich
`recvx-player-head-tests.exe` erzeugt. Das PE-Audit
(`tools/windows_link_audit.py` über `check_no_original_core.py --build-dir`)
hat den Linkbefehl, alle Eingaben und den bytegleichen Vergleichslink geprüft:
PASS. Der zig-Cache musste über `ZIG_GLOBAL_CACHE_DIR`/`ZIG_LOCAL_CACHE_DIR`
nach `$TMPDIR` gelegt werden, weil die Sandbox `~/.cache/zig` nicht beschreibbar
macht.

**Lauf:** **kein tatsächlicher Windows-Lauf.** `wine64` liegt vor (wine-7.7,
Game Porting Toolkit 1.1), aber das Präfix ist in dieser Sandbox nicht
beschreibbar: `touch /tmp/.wine-501/probe` → `Operation not permitted`. Es gibt
keine Windows-Hardware. Build und Lauf sind damit getrennt berichtet; es wird
kein Windows-Lauf behauptet.

**Paket:** nicht aktualisiert, dem Auftrag entsprechend
(„Kein Commit, Push oder Paketupdate"). Die Arbeit liegt damit noch nicht in
`build/packages/macos-arm64-v0.5` und ist nach der Projektregel
`deliver-to-start-command-package` noch nicht ausgeliefert.

## 6. Fehlende Abhängigkeiten — benannt, nicht ersetzt

1. **`epw->flg` Bit 0 hat keinen nativen Besitzer.** `bhSearchNearEnemy2`
   prüft `epw->flg & 1`, `!(flg & 0x8000)` und `!(stflg & 0x41000000)` an der
   Gegnerarbeit selbst. Die Bits gehören dem Gegnerkern, und zwar genauer:
   Bit 0 setzt **die Werkanlage** `bhSetEnemy` (eneset.c:183), die das `flg`
   der `EGG_WORK`-Vorlage übernimmt — den `ene_n`-Eintrag des Raums oder eine
   gegnerische Statik wie `static EGG_WORK child = { 0x8001, 31, 0 }`
   (en02.c:223). `bhEne01_Init` setzt danach `flg |= 0x178` und
   `flg &= ~2` (en01.c:2712-2713), `bhEne00` Fall 0 `flg |= 0x8120`,
   `flg &= ~0x6` (eneset.c:401-402) — beide lassen Bit 0 unberührt.
   `bhEneSetCk` (event.c:2715) schreibt `stflg |= 0x1000000` (event.c:2738)
   und **niemals** `flg`; es löscht auch nichts. `native/en01_damage.c` ist
   eine erklärte Portierungslücke und meldet `flg 0`. Ein treuer Lesevorgang
   findet darum **keine** suchbare Arbeit. Folge: in der produktiven Laufzeit
   rastet der Blick nie ein, obwohl die Suche in jedem Bild läuft. Das ist
   kein Fehler dieser Portierung, sondern die ehrliche Konsequenz der
   Gegnerlücke. Die Laufzeit meldet es in `head.unbound`:
   `bhSearchNearEnemy2: epw->flg bit 0 has no native owner (en01_damage.c gap), so no enemy is searchable`.
   Es wurde **kein** Hostfeld als Ersatz für das Bit eingesetzt; die vorher an
   dieser Stelle stehende Ersatzbedingung aus `hidden`/`inactive`/`dead` ist
   entfernt. Gegnerarbeiten ganz ohne `en01`-Zustand haben überhaupt keinen
   Träger für `flg`/`stflg` und werden getrennt gemeldet.
2. **Kein öffentlicher Zugriff auf die Weltmatrix eines Gegnergelenks.**
   `bhEne01_Init` setzt `epw->lok_jno = 0xa`; `bhSearchNearEnemy2`
   (pwksub.c:455-457) und `bhSetHeadRotation` (player.c:7132-7134) lesen dann
   `owP[lok_jno].mtx`. `RoomWorld::link_matrix`/`link_point` sind privat
   (world.h, hinter Zeile 188), eine öffentliche Gelenkabfrage fehlt. Der
   Kandidatenaufbau nimmt deshalb die Arbeitsposition — also den
   Originalzweig `lok_jno == 0` — und meldet die Lücke. Kein erfundenes
   Ersatzgelenk. Für den Integrator: ein öffentlicher
   `enemy_node_point(index, node)` würde den Vertrag schliessen.
3. **`bhControlPlayerPch` besitzt beim Zielen die Knoten 4 und 5.**
   playpch.c:46 setzt `owP[4].flg |= 2` und `owP[5].flg |= 2` und löscht
   `~0x1E4`, playpch.c:73 löscht `~0x8`. Diese Routine liegt im
   quarantänebedingt offenen Spielerkampfkern
   (`native/player_combat_core.c`, erklärte Lücke: `recvx_combat_tick` meldet
   den Neutralzustand). Der Angriffseinstieg selbst liegt in player.c und ist
   gebunden (Abschnitt 4); die Knotenübernahme durch `Pch` bleibt offen und
   wird hier gemeldet, nicht nachgebaut.
4. **`sys->ewk_n`.** `bhSetHeadRotation` begrenzt den Zielindex gegen die Zahl
   der Gegnerwerke, nicht gegen die Kandidatenzahl (player.c:7075-7078). Die
   Laufzeit übergibt dafür `RECVX_HEAD_INPUT::enemy_count`. Solange der
   Gegnerkern offen ist, ist das die Länge der Raumliste.
5. **`rand()`-Folge.** Das Blinken zieht `-rand()/-2.1474836e9f`. Die Laufzeit
   hat keinen gemeinsamen Originalzufallsstrom; `std::mt19937(12345)` liefert
   die Verteilung, nicht die Originalfolge. Die Schwelle 0.97 und beide Zähler
   sind original. Ein gemeinsamer Originalzufallsstrom wäre ein
   übergreifender Mechanismus und bleibt offen.
6. **`plp->mode0 == 4`** (Schadenszustand) unterdrückt den Blinkstart. Die
   Laufzeit setzt `mode0` auf 3 beim Schieben und sonst 1; der Schadenszustand
   `bhCPM0_damage` ist nicht portiert (derselbe Gegnerkern), also wird 4 nie
   erreicht. Gemeldet, nicht ersetzt.

B05/Itembox und C01/Fallen wurden nicht angefasst. `view_clip.*`, Renderer und
Kamera-/Clipanschluss wurden nicht angefasst. Die laufende Treppenarbeit wurde
nur um den einen abgestimmten Kopfdienst erweitert.
