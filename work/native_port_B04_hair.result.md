# B04 Haarkette des angehaengten Spielerobjekts — nativ portiert

Stand 2026-10-10. **Implementiert und in der produktiven Runtime abgenommen.**
Die Kette von `obwp[2]` (Claires Pferdeschwanz) laeuft wieder mit der
Originalmechanik: Initialisierung, Fortschreibung, Knoten-/Winkelwirkung,
Reset und beide im Original vorhandenen Varianten sind zusammen umgesetzt.

`native/player_hair.c` ist von 28 auf 292 Zeilen neu geschrieben,
`native/player_hair.h` von 18 auf 88. Es wurde **kein Funktionskoerper
uebernommen**, kein Generator benutzt, und `FORBIDDEN_DONTUSE` wurde weder
gelesen noch verwendet. Der vorherige Stand war eine dokumentierte
Portierungsluecke: `recvx_hair_tick` gab unabhaengig von der Eingabe Nullwinkel
und `return 0` zurueck, das Haarmodell stand in seiner Bindepose.

## Quellenbelege (Semantikreferenz, kein Koerper uebernommen)

| Gegenstand | Fundstelle |
|---|---|
| Objekttask der Kette, mode0 0/1 | `src/ps2/veronica/prog/objitm.c` 2122–2383 (`bhObjClpn`) |
| Winkelsumme und Wurzelgeschwindigkeit | `player.c` 7207–7298 (`bhCalcHair`) |
| Verlaufsfelder der Kette | `include/ps2/veronica/prog/objitm.h` 7–25 (`HAIR_WORK`) |
| Anfangszustand des Objektwerks | `objitm.c` 154–213 (`bhSetObject`) |
| lkmtab-Eintrag, `lkono` 5, `lox/loy/loz` | `player.c` 22–26, 756–785 (`bhSetPlayer`) |
| dieselbe Bindung nach Figurenwechsel | `player.c` 962–991 (`bhResetPlayer`) |
| Matrix des angehaengten Objekts | `MdlPut.c` 240–263 (`bhCalcModel`) |
| Objektverteiler ueber `op->id - 1200` | `objitm.c` 125–140, 355–358 (`bhJumpObject2`) |
| Vorabsetzung der Objektposition | `objitm.c` 336–338 (`bhControlObjItm`) |
| Setzer des Reset-Bits `plp->flg2 0x2` | `event.c` 6026 (`bhCyoutenHenkeiSet`, case 0) |

## Beide Varianten — und ein gemessener Originalbefund

`bhJumpObject2` wird mit `op->id - 1200` indiziert (`objitm.c:357`). Die zwei
lkmtab-Eintraege stehen in `player.c:24-25`:

* **id 1211** (`ply_id` 0, Claire) trifft Index 11 → `bhObjClpn`, die
  vollstaendige Kette. Versatz `lox/loy/loz` = `0 / 1.5869 / 0.7747`.
* **id 1212** (`ply_id` 3) trifft Index 12 → **`bhObjWssg`, und dessen Koerper
  ist leer** (`objitm.c:2386-2389`). Spieler 3 hat im Original **keine**
  Haarphysik, obwohl `bhSetPlayer` das Objekt bindet (Versatz
  `0 / 0.85 / -1.03`).

Das ist ein Originalzustand, keine Luecke dieses Ports, und wird als eigene
Variante `RECVX_HAIR_VARIANT_WSSG` gefuehrt, die nie Winkel erzeugt. Eine
frueher an dieser Stelle vermutete zweite Haarmechanik existiert nicht. Die
uebrigen `ply_id` haben keinen lkmtab-Eintrag und ergeben
`RECVX_HAIR_VARIANT_NONE`.

## Portierte Verfahren

**`bhCalcHair`** bildet je Bild die Summe der lokalen Winkel der Spielerknoten
0..5 plus `pp->ax/ay/az` (im Original ueber `short`, die Kuerzung auf 16 Bit ist
uebernommen), merkt die Wurzel des Vorbildes und rechnet die Wurzelverschiebung
der XZ-Ebene durch die Umkehrdrehung `(-ay, -ax, -az)` in den Kopfraum. Das
Original schreibt das Ergebnis nach `&hair->spx`, also `spx/spy/spz` in Folge;
`spy` wird unmittelbar danach durch die rohe Hoehendifferenz ersetzt. Alle drei
werden auf ±1 begrenzt.

**`bhObjClpn` case 0** oeffnet die Puffer (`flg |= 0x1000`, `0x100000`), nullt
die 128 Verlaufseintraege, setzt `ct0`/`xn`/`yn`/`zn`/`spd` auf 0 und geht nach
mode0 1. Dieses Bild erzeugt noch **keine** Winkel.

**`bhObjClpn` case 1** traegt die mit `-0.25` skalierte Geschwindigkeit in den
Ringpuffer (128 Eintraege, Maske `0x7F`) und glaettet sie mit Faktor `0.333`.
Die vier Segmente lesen den Puffer je drei Bilder weiter zurueck
(`ct0 - 3*i`), drehen den Wert um `hair->ay` in den Weltraum und haengen ihn an
die Position des Vorgaengers. Dazu der Fallanteil `spd` (begrenzt auf ±0.5),
zwei Mindestabstaende zum Kopf (`1.0` um `owP[5]*(0,1,0)`, `1.3` um
`owP[5]*(0,1.8,-0.3)` mit der Hoehe des ersten Punktes), jeweils nur in der
XZ-Ebene, und zwei Hoechstabstaende von `0.5` zum Vorgaenger. Aus dem
Restvektor entstehen ueber `atan2` die beiden Knotenwinkel; `rx`/`ry` nehmen die
von den Vorgaengersegmenten bereits aufgebrachte Drehung wieder heraus, damit
die Winkel lokal zum Elternknoten entstehen. `ax`/`ay`/`rx`/`ry` sind im
Original `short` (`objitm.c:2127-2128`), die Kuerzung wirkt also **vor** der
Ruecknahme — das ist so uebernommen.

**Reset** (`flg2 0x2`): nullt Verlauf **und** `HAIR_WORK`. Eine Eigenart des
Originals ist dabei uebernommen und im Test festgehalten: `objitm.c:2189` nullt
`HAIR_WORK`, und erst danach laeuft `bhCalcHair` (`objitm.c:2206`). Der
Vorgaenger der Wurzel ist in diesem einen Bild also der Ursprung, nicht die
vorige Lage; die Geschwindigkeit dieses Bildes laeuft folglich in die
Begrenzung. Gemessen: `oxb/ozb` = 0, `spx/spy/spz` auf ±1 gesaettigt, und die
Kette haengt trotzdem an der neuen Raumlage.

**Kein zweites Werk**: Pose, Knotenmatrizen und Gier kommen vom vorhandenen
Actor-/Posebesitzer (`PlayerAsset::pose`, `::local_pose`, `RoomWorld::sample_player`).
`RECVX_HAIR_STATE` haelt nur die Felder, die `bhObjClpn` selbst fuehrt
(`flg`, `mode0`, `ct0`, `xn/yn/zn`, `spd`, `HAIR_WORK`, die drei Puffer und
`pv`). Keine geratene Federphysik, kein kosmetischer Ersatz.

## Anschluss an die Laufzeit (kleiner abgestimmter Patch)

Sechs Zeilen in `native/runtime.cpp`, nur an den Haar-Hunks:

* `recvx_hair_begin(&hair, (args.player-10)%4)` beim Start — `bhSetObject` plus
  die Variantenwahl von `bhSetPlayer`.
* derselbe Aufruf beim Raumwechsel (`bhResetPlayer` bindet `obwp[2]` neu) samt
  Loeschen der zuletzt gelieferten Winkel.
* `hair_step()` uebergibt die Kopfmatrix `owP[5]`, die lokale Pose der Knoten
  0..5, `plp->ay` und den Versatz `lox/loy/loz`; die Wurzel bildet der Port
  selbst wie `bhCalcModel`.
* `"hair"` im `--state`-Ausgang (Variante, mode0, Ringpuffer, Fallanteil,
  Geschwindigkeit, geglaettete Geschwindigkeit, Wurzel, Giersumme, die vier
  Winkelpaare und die vier Segmentpunkte) — damit die Haarpose belegbar ist.

`native/view_clip.*`, Renderer und Kamera-/Clipanschluss sind **nicht**
angefasst; die laufende Treppenarbeit (`native/player_stairs.*`) ebenfalls
nicht.

## Abnahme in der produktiven Runtime

Alle Werte aus `build/hair-claude-20261010/recvx-runtime` gegen die
Auslieferungsdaten, **ohne diagnostische Platzierung** (RM_0020, `--in-game`,
nur Tasteneingaben).

**Stillstand** (60 Bilder, keine Eingabe): Variante 1, mode0 1, Ringpuffer bei
59, Giersumme 16380, Winkel erzeugt. Die Segmente haengen gestreckt unter der
Wurzel `(100.154, 16.744, 149.950)`: y = 16.300 / 15.801 / 15.301 / 14.801,
also genau die vier `0.5`-Schritte.

**Vorwaerts** (`keys forward`): die Figur laeuft nach −x, die Kette streckt
sich entgegen der Laufrichtung. Abstand Segment zu Wurzel in +x:

| Bild | Spieler x | seg0 | seg1 | seg2 | seg3 |
|---|---|---|---|---|---|
| 20 | 91.77 | +0.459 | +0.902 | +1.346 | +1.787 |
| 60 | 76.21 | +0.440 | +0.856 | +1.280 | +1.709 |
| 100 | 60.57 | +0.450 | +0.872 | +1.293 | +1.716 |

`velocity` saettigt bei −0.34 bis −0.43, `fall` liegt an seiner Grenze −0.5.

**Rueckwaerts** (`keys back`, 80 Bilder): das Haar haengt hinter dem Kopf
(`loz` +0.7747), Rueckwaertslaufen treibt es **gegen** den Kopf — und genau
dafuer gibt es die beiden Mindestabstaende. Gemessen bleibt die Kette auf der
Grenze stehen (`seg_dx` 0.278 / 0.366 / 0.373 / 0.374) statt durch den Kopf zu
wandern. Im Komponententest liegt sie dort auf beiden Grenzen zugleich
(1.0 zum Kopfpunkt, 1.3 zum Schulterpunkt).

**Drehen** (`keys left`, 80 Bilder): die Giersumme folgt von 16385 auf 15545,
und die Kette schwingt in +z heraus: `seg_dz` 0.161 / 0.313 / 0.457 / 0.597.

**Raumwechsel** durch eine echte Tuer (`tests/native/replays/door-roundtrip.txt`,
kein Teleport): RM_0000 → RM_0010 → RM_0000. Die Wurzel folgt jedes Mal der
neuen Spielerlage statt an der alten zu haengen —
Bild 4200 `actor (30.60, 0, 33.90)` / `root (31.24, 16.74, 33.53)`,
Bild 4760 `actor (10.04, 0, −35.90)` / `root (10.04, 16.74, −35.17)`,
Bild 5200 `actor (29.56, 0, 68.17)` / `root (29.56, 16.75, 68.89)`.
mode0 bleibt 1, der Ringpuffer laeuft weiter (94 → 66 → 37).

**Gerenderte Wirkung**, gegen das Nachbar-Binary `build/doors-claude-20261009/recvx-runtime`
(01:27, enthaelt B04 nicht — echter Vorher-Stand): derselbe Lauf unterscheidet
sich in **545 von 921 600 Pixelbytes (0.059 %)**, und die 204 geaenderten Pixel
liegen in **einem** Bereich x 159..184, y 149..175 (26 × 27 von 640 × 480).
Eine oertlich begrenzte Aenderung in Kopfhoehe, nichts gestreut.

**Unveraenderte uebrige Spielzustaende**: derselbe Zustandsvergleich
(vier Laeufe: vorwaerts, rueckwaerts, drehen, Stillstand) gegen dasselbe
Vorher-Binary ergibt pro Lauf **genau ein** abweichendes Feld, und das ist
`render_clip` — Codex' laufende `view_clip`-Neubestimmung, die im alten Binary
noch gar nicht existierte (`None` → `[2, 20000]`). Alle anderen Felder
(Position, Gier, Motion, Kamera, Audio, Eventzustand, Inventar, HP, Tasks,
Gegner, Effekte, blockierte Befehle) sind identisch. Das `hair`-Feld ist beim
Vergleich ausgenommen, weil es die gemessene Groesse selbst ist.

## Tatsaechliche Tests

**`player_hair`** (neu, `tests/native/player_hair_test.cpp`, 265 Zeilen,
CTest). Geprueft werden: Variantenwahl aller vier `ply_id`; Anfangszustand
(`flg` 0x81 aus dem lkmtab-Eintrag, genullte Puffer); die leere Variante
erzeugt auch bei Bewegung nie Winkel und veraendert den Zustand nicht; das
Initialbild liefert keine Winkel und geht nach mode0 1; Stillstand haelt beide
Mindestabstaende am Zielpunkt und haengt gestreckt (Abstand genau 0.5), und ist
nach 120 Bildern stabil; Vorwaerts saettigt `spz`, die geglaettete
Geschwindigkeit laeuft gegen die Bewegung, und jedes Segment liegt weiter hinten
als sein Vorgaenger; Rueckwaerts saettigt `spz` mit umgekehrtem Vorzeichen und
die Kette bleibt auf den Grenzen; Drehen fuehrt die Giersumme aus `plp->ay`
**und** den sechs Knotenwinkeln nach; Reset loescht Verlauf und `HAIR_WORK`,
uebernimmt die neue Wurzel, haelt die Geschwindigkeit begrenzt und laesst die
Kette an der neuen Raumlage haengen; der Ringpuffer laeuft bei 128 mit Maske
`0x7F` um und zaehlt danach weiter; der Fallanteil bleibt in ±0.5; die
Geschwindigkeit bleibt in ±1 auch bei einem Sprung ueber den halben Raum; die
Winkel liegen im `short`-Bereich und stimmen mit dem Zustand ueberein; mode0 2
und ein nicht angehaengtes Objekt halten den Task an.

**Gesamte Suite**: **50/50 bestanden**, sowohl `-j1` (9.40 s) als auch `-j8`
(4.36 s). Das sind die 49 Gruppen des Nachbarstands plus `player_hair`.

**Originaldaten-Suiten** gegen diese Runtime, jede einzeln ausgefuehrt:
`verify_stairs` (82 Pruefungen), `verify_doors`, `verify_events` und
`verify_progression` — **alle vier bestanden**. (Ein erster Sammelaufruf von mir
meldete sie faelschlich rot; das war ein Fehler meines Schleifenaufrufs, die
Einzellaeufe zeigen das Gegenteil.)

**`tools/check_no_original_core.py`**: PASS vor der Arbeit, vor jedem Build und
nach dem letzten Build. `git diff --check` ist sauber.

## Offene Abhaengigkeiten (gemeldet, nicht ersetzt)

* **`pp->flg2 0x100`** (`objitm.c:2242`) laesst die Kette senkrecht unter die
  Wurzel fallen. Das Bit wird **in keiner Originalquelle gesetzt** — der Zweig
  ist gebunden, aber im Spiel unerreichbar. Als Eingabe `ground` gefuehrt und
  nur im Port ansprechbar.
* **`op->ct1`/`ct2`/`ct3`** setzt `bhObjClpn` case 0 auf 0 und liest sie nie
  wieder; sie sind daher nicht Teil des Zustands.
* **`op->pv[0..3]`** wird hier gefuehrt, weil `bhObjClpn` sie schreibt. Gelesen
  werden sie im Original von `effect.c:1120-1129` (Effektpunkte); dieser
  Verbraucher ist nicht Teil von B04 und bleibt offen.
* **Ein Bild Verzoegerung**: die Knotenmatrizen werden vor dem Task abgetastet,
  wie schon beim bestehenden `hair_step`. Die Kette liest damit die Kopfmatrix
  des laufenden Bildes, aber die Pose, die die Laufzeit danach zeichnet. Im
  Original laeuft `bhControlObjItm` nach `bhControlPlayer` ueber derselben
  Matrix; die Abweichung ist nicht sichtbar, aber eine Abweichung.
* **`plp->ax`/`plp->az`** bleiben 0, weil der Host sie fuer den Spieler nicht
  fuehrt. `bhCalcHair` summiert sie mit; sobald ein Besitzer sie setzt, gehen
  sie ohne Aenderung ein.
* **`bhSetShadow`/`bhCalcSkin` der Kette**, die Texturbindung `skp[mdlver]` und
  `mdlver` 4 bleiben beim vorhandenen Actor-Besitzer; B04 liefert nur die
  Knotenwinkel, die `PlayerAsset::sample_rest` schon verarbeitet.

## Getrennte Befunde macOS / Windows

* **macOS arm64 — gebaut und gelaufen.** Eigenes Verzeichnis
  `build/hair-claude-20261010`, Pruefer PASS, **50/50 CTest** in `-j1` und
  `-j8`, vier Originaldaten-Suiten bestanden, alle Messwerte dieses Berichts
  stammen von hier.
* **Windows x64 — nur kreuzgebaut, nicht gelaufen.** Eigenes Verzeichnis
  `build/windows-hair-claude-20261010d` (Ninja, zig): Bau laeuft durch,
  **52 ausfuehrbare Dateien** (51 des Nachbarstands plus der neue Haar-Test),
  `recvx-runtime.exe` ist **2 303 488 Byte**, meine beiden Dateien uebersetzen
  fuer Windows ohne Fehler. Das echte dreistufige PE-Audit
  (`tools/windows_link_audit.py` ueber `audit.audit(...)`, Linkbefehl aus dem
  Buildsystem, alle Eingaben einschliesslich Archivmitglieder, Vergleichslink
  Byte fuer Byte) liefert auf genau diesem Binary **0 Befunde**.
* **Kein Windows-/Wine-Lauf, gemessene Ursache.** `wine64` ist vorhanden
  (`/opt/homebrew/bin/wine64`), aber sein Sitzungsordner ist nicht
  beschreibbar: `touch /tmp/.wine-501/probe` scheitert mit
  `Operation not permitted`, waehrend `mkdir -p /tmp/claude/sbok` gelingt. Wine
  legt `/tmp/.wine-<uid>` fest an; ein Umlegen von `TMPDIR` hilft nicht. Das
  Abstellen dieser Sandbox-Grenze ist eine Entscheidung des Nutzers.
* **Zwei Buildwerkzeug-Fallstricke, gemessen und dokumentiert**, damit der
  naechste Lauf sie nicht wiederholt: (1) ein frisches Windows-Verzeichnis
  braucht `-DRECVX_FETCH_SDL2=OFF` und
  `-DCMAKE_PREFIX_PATH=build/dependencies/SDL2-2.32.10/x86_64-w64-mingw32`,
  sonst bricht die Generierung mit `IMPORTED_IMPLIB not set for imported target
  "SDL2::SDL2"` ab. (2) Der Generator muss **Ninja** sein: das PE-Audit
  rekonstruiert den Linkbefehl ueber `ninja -t commands` und blockiert unter
  Make mit `link command for recvx-runtime.exe is not recoverable from the
  build system`. Das ist der Pruefer, nicht ein Fehler des Codes.

## Nicht angefasst

`native/view_clip.*`, Renderer, Kamera-/Clipanschluss, `native/world.*`,
`native/events.*`, `native/actor.*`, `native/audio.*`, die Effektdateien und die
laufende Treppenarbeit `native/player_stairs.*`. Geaendert wurden
`native/player_hair.c`, `native/player_hair.h`,
`tests/native/player_hair_test.cpp` (neu), neun Zeilen in `CMakeLists.txt`
(Testregistrierung) und sechs Haar-Hunks in `native/runtime.cpp`. Das im
Arbeitsbaum ebenfalls geaenderte `render_clip` in `runtime.cpp` und die
`view_clip`-Neubestimmung gehoeren Codex.

**Kein Commit, kein Push, kein Paketupdate.**
`build/packages/macos-arm64-v0.5` enthaelt diesen Stand **nicht**; nach der
Regel `deliver-to-start-command-package` zaehlt B04 erst nach Einspielen in das
Paket, Neusignieren und Pruefung ueber `Start.command`.
