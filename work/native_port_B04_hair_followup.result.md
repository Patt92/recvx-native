# B04 Nacharbeit — die drei offenen Bindungen

Stand 2026-10-10. **Alle drei geschlossen.** Die Verzoegerung ist am
gemeinsamen Besitzer behoben, nicht durch eine Haar-Sonderregel;
`plp->ax`/`az` und der `pv`-Verbraucher sind vollstaendig geklaert. Zwei der
drei Punkte endeten in einem belegten Originalbefund statt in neuem Code — das
steht unten mit Fundstelle.

Schreibbereich eingehalten: `native/player_hair.h`, `tests/native/player_hair_test.cpp`,
acht Zeilen in `native/scene.h` (der gemeinsame Spielerbesitzer) und drei
Haar-Hunks in `native/runtime.cpp`. **B05/Itembox und C01/Fallen nicht
angefasst**; `native/world.*`, `native/events.*`, `native/renderer.*` und
`native/view_clip.*` ebenfalls nicht (dort arbeitet Codex parallel).

## 1. Originale Reihenfolge — und die Ursache der Verzoegerung

Die Reihenfolge ist jetzt am Original nachvollzogen, nicht vermutet:

| Schritt | Fundstelle |
|---|---|
| `bhControlPlayer` laeuft | `game.c:52` |
| …und endet mit `bhCalcModel(plp)` | `player.c:1688` |
| `bhCalcModel` → `bhCalcTree` schreibt `owP[i].mtx` | `MdlPut.c:264`, `:268-296` |
| **danach** `bhControlObjItm` → `bhObjClpn` | `game.c:53`, `objitm.c:357` |
| …die Objektposition wird vorher aus derselben Matrix gesetzt | `objitm.c:336-338` |
| erst am Schleifenende `bhAllDrawModel` | `game.c:69`, `:318` |

Damit ist belegt: **`bhObjClpn` liest die Kopfmatrix desselben Bildes**, die
`bhControlPlayer` unmittelbar davor geschrieben hat. Es gibt im Original keine
Verzoegerung.

**Ursache im Host** war die Reihenfolge am gemeinsamen Besitzer: `hair_step()`
stand **vor** `locomote()`. `locomote` ist der Host-Gegenpart zu
`bhControlPlayer` — es bewegt den Spieler und setzt `actor.position`,
`actor.yaw`, `motion` und `animation` (`runtime.cpp:577`, `:683`), fuehrt
`tick_stairs` und die Schiebearbeit. Die Kette las also die Pose des Vorbildes.

**Behoben** durch Verschieben von `hair_step()` hinter `locomote()` und vor
`third_person_tick()` (die Kameraarbeit, im Original `bhControlCamera` nach
`bhControlObjItm`, `game.c:60`). An beiden Aufrufstellen — headless und GUI —
mit demselben Kommentar auf `game.c:52-53`. **Keine Haar-Sonderregel**: die
Haarsteuerung hat keine eigene Ausnahme bekommen, sie steht jetzt an der
Stelle der Bildfolge, an der das Original sie hat.

**Gemessener Vorher/Nachher-Beleg** (RM_0020, `keys forward`, Bild 60, derselbe
Lauf): der Versatz zwischen Spielerlage und Haarwurzel sinkt von **+0.599** auf
**+0.220**. Die Differenz von 0.379 ist fast genau ein Bewegungsschritt
(gemessen 0.39 je Bild) — also exakt das eine Bild, das zuvor fehlte.

| Bild | Spieler x | Wurzel x | Versatz |
|---|---|---|---|
| 20 | 91.770 | 91.875 | +0.105 |
| 60 | 76.214 | 76.435 | +0.220 |
| 100 | 60.573 | 60.757 | +0.184 |

Die Kette selbst bleibt richtig: `seg_dx` +0.45 / +0.88 / +1.33 / +1.77 entgegen
der Laufrichtung, Abstaende weiter innerhalb der Originalgrenzen.

## 2. `plp->ax` / `plp->az` — am gemeinsamen Besitzer angebunden

**Gemessener Originalbefund:** `plp->ax` und `plp->az` werden im **gesamten
Original nirgends geschrieben**. Alle Zugriffe sind Lesezugriffe:

* `map.c:1074` — `njRotateXYZ(NULL, plp->ax, plp->ay, plp->az)` im Kartenbild
* `player.c:1375`/`:1377` — Sicherung nach `plp->axb`/`azb`
* `player.c:6760`/`:6762` — `bhControlPlayerHead`
* `player.c:7219`/`:7221` — `bhCalcHair`

Sie bleiben also auf dem Wert, den `bhSetObject` per `npSetMemory` setzt (0).
Das ist keine Luecke, sondern der Zustand des Originals — eine erfundene
Neigungsquelle waere genau der verbotene Ersatz.

**Anbindung** trotzdem vollstaendig, weil der Wert dem Spielerwerk gehoert und
nicht lokal angenommen werden darf: `SceneSpawn` (`native/scene.h:103`) ist der
eine Spielerbesitzer des Hosts (`RoomWorld::player_` ist eine Referenz darauf,
`world.h:228`) und fuehrt jetzt `pitch` und `roll` neben `yaw` — die
`ax`/`ay`/`az` von `BH_PWORK` (`types.h:94-96`). `hair_step` liest sie von dort
(`in.ax=actor.pitch; in.ay=actor.yaw; in.az=actor.roll;`) statt 0 anzunehmen.
Sobald ein Besitzer sie setzt, wirken sie ohne weitere Aenderung.

Im Komponententest geprueft: beide gehen in die Winkelsumme ein
(`ax` 2000 + Knoten 300 → `hair.ax` 2300; `az` −1500 − 100 → −1600), und beide
erreichen die Umkehrdrehung von `bhCalcHair`. Gemessen an einer Bewegung
unterhalb der ±1-Begrenzung, weil gesaettigte Werte den Unterschied verdecken:
`ax` 8192 aendert `spz` von 0.3747 auf 0.2650. Dass `az` eine reine
z-Bewegung **nicht** aendert, ist `njRotateZ` (dreht x/y) und keine fehlende
Bindung — mit einer x-Bewegung ist die Wirkung belegt und mitgeprueft.

## 3. `op->pv` — Verbraucher geklaert, kein aktiver vorhanden

**Gemessener Originalbefund:** der einzige Leser von `op->pv` ist
`bhDrawLinEffect` (`effect.c:1120-1129`), das zwischen `pv[0]` und `pv[1]` eine
gefaerbte Linie zeichnet. Dieser Zeichner bezieht seine Objektliste
**ausschliesslich** aus `sys->ef_lin`/`ef_linn` (`effect.c:871-873`) — und
diese Liste wird **in keiner Originalquelle gefuellt**; die beiden einzigen
Vorkommen sind genau diese Lesezugriffe plus die Felddefinition
(`types.h:746`, `:758`). `bhDrawLinEffect` ist damit unerreichbar.

Die Haarobjekte nehmen ohnehin einen anderen Zeichenweg: wegen `id >= 1210`
landen sie in `sys->ob_hlg` (`objitm.c:366-371`) und werden von `bhDrawObject`
als Modell gezeichnet (`objitm.c:526-528`), nicht als Linie. Der Zeichenpass
nimmt also die **Knotenwinkel**, die `bhObjClpn` nach `objP[0..3].ang` schreibt.

`pv` wird deshalb geschrieben wie im Original (`objitm.c:2374-2376`), hat aber
keinen aktiven Verbraucher. Das ist im Header an der Felddefinition belegt und
im Test geprueft: `pv[i]` haelt genau den gefilterten Kettenpunkt `point[i]`,
und die Kette haengt Segment an Segment innerhalb der 0.5-Grenze.

## Konkrete Abhaengigkeiten (gemeldet, nicht ersetzt)

* **`bhControlPlayerHead` (`player.c:6746`) ist nicht portiert.** Die Routine
  laeuft im Original direkt vor `bhCalcModel` (`player.c:1685`), summiert die
  Knoten **0..3** und schreibt `objP[4].ang` und `objP[5].ang` — also genau die
  beiden Knoten, die die Haarkette danach als Teil ihrer Summe 0..5 liest, und
  den Kopfknoten 5, dessen Matrix die Wurzel bildet. Solange sie fehlt, stehen
  diese zwei Knoten auf ihren Motionwerten statt auf der Kopfsteuerung. Die
  Kette verarbeitet, was da ist; sie erfindet nichts. Derselbe Dienst fehlt
  B01 als `RECVX_STAIR_UNBOUND_HEADWORK` (`player_stairs.h:52`) — es ist eine
  uebergreifende Abhaengigkeit, kein Haar-Thema.
* **Ein Besitzer fuer `plp->ax`/`az` gibt es weiterhin nicht**, weil das
  Original keinen hat. `SceneSpawn` fuehrt die Felder jetzt; wer sie
  irgendwann setzt (etwa eine portierte Kartenansicht oder eine
  Neigungsmechanik), braucht an der Kette nichts zu aendern.
* **`sys->ef_lin`/`ef_linn` und `bhDrawLinEffect`** bleiben unportiert, und das
  ist richtig: im Original wird die Liste nie gefuellt. Falls sich spaeter ein
  Fueller in noch nicht gelesenem Code findet, ist der Anschlusspunkt `pv`
  bereits vorhanden.
* **`op->ct1`/`ct2`/`ct3`** setzt `bhObjClpn` case 0 auf 0 und liest sie nie
  wieder — unveraendert kein Zustandsfeld.

## Gezielte Runtime-Regressionspruefungen

Die Aenderung beruehrt die Bildreihenfolge und den gemeinsamen
Spielerbesitzer, also sind beide gezielt gegengeprueft.

* **Vollstaendiger Zustandsvergleich** gegen `build/doors-claude-20261009/recvx-runtime`
  (01:27, echter Vorher-Stand ohne B04), vier Laeufe in RM_0020 ueber 80 Bilder
  (vorwaerts, rueckwaerts, drehen, Stillstand): pro Lauf **genau ein**
  abweichendes Feld, und das ist `render_clip` — Codex' `view_clip`-Arbeit, die
  im alten Binary noch nicht existierte. Position, Gier, Motion, Kamera, Audio,
  Eventzustand, Inventar, HP, Tasks, Gegner, Effekte und `blocked_commands`
  sind identisch. Die Reihenfolge-Korrektur und die beiden neuen Felder
  veraendern **keinen** anderen Spielzustand.
* **Raumwechsel** durch eine echte Tuer (`door-roundtrip.txt`, kein Teleport):
  RM_0000 → RM_0010 → RM_0000 mit identischen Lagen wie vor der Nacharbeit und
  `blocked_commands` 0. Die Wurzel folgt jedes Mal der neuen Spielerlage. Der
  Ringpuffer steht einen Schritt weiter (86/38 statt 66/37), weil `hair_step`
  jetzt spaeter im Bild laeuft — die erwartete Folge der Korrektur.
* **Gerenderte Wirkung**: 207 von 307 200 Pixeln (0.067 %) gegenueber dem
  Vorher-Binary, alle in **einem** Bereich x 159..184, y 149..175
  (26 × 27 von 640 × 480). Weiter oertlich begrenzt, nichts gestreut.
* **Originaldaten-Suiten**, jede einzeln: `verify_stairs`, `verify_doors`,
  `verify_events`, `verify_progression` und `verify_snapshot` — **alle fuenf
  bestanden**. `verify_snapshot` nimmt kein `--output`; ein erster Sammelaufruf
  von mir meldete es deswegen faelschlich rot.
* **Gesamte Suite**: **51/51 bestanden** (die 50 des aktuellen Nachbarstands
  plus `player_hair`; Codex hat `item_box_object` ergaenzt).
* **`tools/check_no_original_core.py`**: PASS vor der Arbeit und vor jedem
  Build. `git diff --check` sauber.

## Getrennte Befunde macOS / Windows

* **macOS arm64 — gebaut und gelaufen.** `build/hair-claude-20261010`,
  51/51 CTest, fuenf Originaldaten-Suiten, alle Messwerte dieses Berichts.
* **Windows x64 — kreuzgebaut, nicht ausgefuehrt.**
  `build/windows-hair-claude-20261010d`: Bau laeuft durch, **53 ausfuehrbare
  Dateien**, `recvx-runtime.exe` **2 304 000 Byte**, Pruefer PASS. Das echte
  dreistufige PE-Audit liefert auf genau diesem Binary **0 Befunde**. Das ist
  **ein Windows-Build, kein Windows-Lauf**: nichts davon wurde unter Windows
  ausgefuehrt.
* **Kein Windows-/Wine-Lauf, gemessene Ursache unveraendert.** `wine64` ist
  vorhanden, aber `touch /tmp/.wine-501/probe` scheitert mit
  `Operation not permitted`, waehrend `mkdir -p /tmp/claude/...` gelingt. Wine
  legt `/tmp/.wine-<uid>` fest an. Das Abstellen dieser Sandbox-Grenze ist eine
  Entscheidung des Nutzers.

## Geaenderte Dateien

`native/scene.h` (acht Zeilen: `pitch`/`roll` am gemeinsamen Besitzer, mit
Originalbeleg), `native/runtime.cpp` (drei Haar-Hunks: `hair_step` an die
Originalstelle verschoben, `ax`/`az` vom Besitzer gelesen),
`native/player_hair.h` (Beleg am `pv`-Feld),
`tests/native/player_hair_test.cpp` (drei neue Pruefgruppen). `player_hair.c`
brauchte **keine** Aenderung — die Mechanik war richtig, falsch war die
Einordnung im Bild und die lokale Annahme der Winkel.

**Kein Commit, kein Push, kein Paketupdate.**
`build/packages/macos-arm64-v0.5` enthaelt diesen Stand nicht.
