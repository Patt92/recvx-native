# ItemBox-Oberfläche (sub1.c ItemBox, Fenster/Sprites) — 2026-10-12

Umfang: `native/box_menu.cpp/.h`, `native/ui.cpp/.h`, Abschnitt in
`tests/native/progression_test.cpp`. Kein Originalkörper, kein Generator, keine
Raumregel, kein Eingriff in `world.cpp`/`runtime.cpp`/`events.cpp` (die halten
laufende Parallelsitzungen). Kein Commit/Push.
Pflichtprüfer vor Build und Konfiguration: `python3 tools/check_no_original_core.py`
→ PASS (CMake führt ihn zusätzlich bei jeder Konfiguration aus).

## Auftrag und Abgrenzung

Nutzervorgabe: „Originale ItemBox-Oberfläche (sub1.c ItemBox, Fenster/Sprites)“.
Die Daten- und Logikteile lagen bereits: `ItemBoxChange`/`ItemBoxIconSet`-Icon-
Abbildung ist über `Inventory::box_change` und `GameUi::icon` portiert, die
Fenstergruppen `parts_04b/05b/07b/11b/15b/16b/17b/18b/03b` werden aus
`ui_tables_data.inc` gezeichnet. **Offen war die Listen-Oberfläche selbst**: das
Zeilenfenster der Kiste und die Scroll-Animation. Genau die ist jetzt portiert.

## Belege im Original

- `sub1.c:1628 ItemBoxInit` setzt `parts_20b[num1].pos = (296, 38*num1+31, 21)`
  für acht Zeilen und `st->ips1 = st->ipsdisp = 0`.
- `sub1.c:8603 ItemBoxIconSet` mutiert diese Tabelle **jede Zeile**:
  `pb->pos[1] += st->number` (nur `testmode == 2`), danach `-38` bei
  `st->flag == 1` bzw. `+38` bei `2`, markiert `atr |= 0x20` nur für
  `31 < pos[1] < 259`, setzt `cen_no = 10`, `parts_num = 21` und die
  Icon-Maße (`sx` 42/84 breit, `sy` 19 bei `pos[1] > 240`, sonst 38).
- `sub1.c:8176 SpriteOnOff` zeichnet den Item-Namen an
  `pos[1] + cen_pos[10][1]` und nur innerhalb desselben Fensters; die Zeile in
  der Mitte (`num1 == 3`, also `bxp[ips1]`) ist der aktive Kisteneintrag.
- `sub1.c:7439-7604 ItemBox testmode 2` ist die Scroll-Eingabe:
  `pad_on 0x1/0x2` setzen `st->number = ±19` und die Pfeil-Anim 19,
  `pad_on 0x80/0x10` schieben fünf Einträge mit `keywait > 10` und
  `boxwait > 3` (erster Schritt sofort bei `keywait == 1`).
- `sub1.c:7688-7751` ist der Zeilen-Tail: `st->flag = 0` am Anfang, dann
  `wait++`, `tmp = 38/|number|`, bei `wait > tmp` Indexschritt (`ips1 ± 1`),
  `flag = 1/2`, SE 2, Maske `& 0x7F` (128 Einträge) bzw. `& 0x1F` (Sicherheitsbox, 32).
  Weil `ItemBoxIconSet` die Tabellenmutation behält, läuft die Anzeige in
  Schritten von 19 px (-19, -38) und der Richtungsframe legt die vollen 38 px
  auf, sodass die Summe am Ende wieder 0 ist.

## Native Umsetzung

`BoxMenu` hält den Originalzustand (`ips1_/ipsdisp_`, `number_`, `flag_`,
`wait_`, `keywait_`, `boxwait_`) plus `row_shift_` als **akkumulierte**
`parts_20b[pos][1]`-Mutation, denn der Originalcode verändert die Tabelle selbst
statt einen Zeichenoffset zu führen. `synchronize()` (pro Frame, wie
`ItemBox` → `SpriteOnOff` im Original) führt den Tail aus, `input()` die
Scroll-Eingabe. `GameUi::draw_box` zeichnet das Fenster jetzt aus derselben
Geometrie: sieben Zeilen `31 + 38*num1 + row_shift`, Sichtbarkeit und
Icon-Höhe nach den Originalgrenzen, Name an `centers[10]` + Icon über die
vorhandene `icon()`-Abbildung (deckungsgleich mit `ItemBoxIconSet`:
`itemno`-Umschlag, texid 3/4, u/v-Raster 34/24). `parts_15b[20]/[21]` folgen
dem Originalanim 18 (neutral) / 19 (Richtung aktiv); außerhalb des
Scrollmodus bleiben die Tabellenwerte stehen.

**Abweichung, offen benannt:** Das Original liest die Scroll-Richtung als
*gehaltenes* Pad (`sys->pad_on`). Der Host liefert Richtungen als
`MenuInput`-Ereignisse (mit `input.hpp`-Autorepeat 18/5), daher zählen
`keywait`/`boxwait` pro Ereignis statt pro Frame, und `arrows_` bleibt während
des Wiederholfensters beleuchtet. Die Schrittzahl (±19/±38, ±5), die
Indexfortschreibung, die Masken und SE 2 sind unverändert übernommen.

## Nachweis

- Neuer Abschnitt `ItemBox Listen-Scroll (testmode 2)` in
  `tests/native/progression_test.cpp`: ±19 in Frame 1, ±38 in Frame 2,
  Indexschritt + Fensterrückkehr im Richtungsframe (Pfeile neutral 18),
  Rückweg nach oben, Fünferschritt (sofort, dann `keywait > 10`, dann
  `boxwait > 3` alle vier Ereignisse).
- Renderprüfung mit Originaldaten: derselbe Kasten mit und ohne
  `row_shift = -19` liefert unterschiedliche Pixel (die Zeilen folgen der
  Mutation). Läuft im `argc > 1`-Block von `progression_test`.
- Voller Lauf: `ctest -j 1` in `build/box-ui-claude-20261012` → **66/66 grün**.
- `tools/verify_pickup_slide.py` gegen das neue Binary: GetItem-Frage in
  RM_7090 ignoriert Bestätigen während der Einfahrt und nimmt danach an.
- `tools/verify_map.py`: Karte öffnet über die Statusleiste, `mode 7`,
  1 Tag, 70 Draw-Aufrufe (Zustand, siehe unten).
- Gemessen, nicht von dieser Änderung: `recvx-progression-tests data` bricht
  in der C01-Gegnersektion („enemy spawn gap notified the original room
  script“) ab. Gegenprobe mit zwei fremden Vorherbinaries
  (`build/enemy-ai-claude-20261011`, `build/weapon-se-claude-20261012`):
  identischer Abbruch, also vorbestehend im Arbeitsbaum.

## Weitere Nutzerpunkte dieses Auftrags

2. **GetItem** — bereits vollständig im Baum portiert und geprüft, hier nicht
   neu gebaut: `Inventory::pickup` (sub1.c:3727) deckt testmode 0..5,
   die Meldungen 153/154/157/158, die Antworten 100/101, `sb_id 130 -> 35`,
   den Sofortgebrauch `Use_00` (Zweig 153) und `StatusCancel` ab.
   Belege: `tests/native/ammunition_test.cpp`, `tests/native/game_test.cpp:95`
   (volle Tasche + Kraut → 153 und Ja), `verify_pickup_slide.py`.
   Die frühere Überlappung von Item-Name und Aufnahmedialog ist in
   `native/ui.cpp` durch `!(menu.pickup && dialog.active())` weiterhin
   ausgeschlossen. Offen bleibt allein die *individuelle* Prüfung des
   SpriteH/SpriteUV-Einfahrens für `subscreenmode 8` (der Slide selbst ist
   derselbe wie im Statusbildschirm).
3. **Event-Karte (D10)** — der Kartentask läuft über den echten Weg
   (Statusleiste → MAP → `bhInitMap`/`bhControlMap`/`bhExitMap`) und liefert
   `mode 7`, 1 Tag und 70 Draw-Aufrufe. **Gezeichnet wird nichts**, weil der
   native Ausgabepfad die 2D-Primitiven (`njDrawSprite2D`,
   `njDrawPolygon2D`, `njCnkEasyMultiDrawObject`) nicht besitzt — genau die
   Lücke, die `archived/work/native_port_D10_map.patch` Teil B4 als Integrator-Vertrag
   festhält (2D-Sprite/Bildschirmfläche/Konstantmaterial in `native/scene.h`
   und die Dekodierung von `MDL`-/PVP-Chunks). `native/scene.h` und
   `native/runtime.cpp` sind aktuell von zwei laufenden Sitzungen belegt
   (Morph-Pfad, Waffen-SE), deshalb wurde hier kollisionsfrei nichts davon
   angefasst.

## Nachtrag (Nutzerauftrag „fix es, leg los“, 2026-10-12)

### Confirm/Cancel-Gate der Scrollphase (Original 7606)

Beim Lauf der echten Runtime-Replays fiel auf: mein erster Wurf verarbeitete
Bestätigen auch *während* der laufenden Zeilen-Scrollanimation. Das Original
tut das nicht — in testmode 2 stehen Bestätigen und Abbrechen im Zweig
`else if (st->wait == 0)` (sub1.c:7606), und `wait` zählt genau die
`38/|number|`-Bilder der Animation. Neu in `native/box_menu.cpp`:

```cpp
if(selecting_&&wait_)return true;   // 7606: Bestaetigen/Abbrechen erst nach dem Scroll
```

Wirkung, gemessen am Detektor-/Box-Replay (`tools/verify_progression.py`):
vorher landete der zweite Kasten-Gegenstand im selben Kistenplatz wie der erste
(`security_items_2 = [3604481, 0]`, der Lighter blieb in der Tasche), jetzt
trifft der Index erst nach dem Scroll (`[327681, 3604481]`).

### Korrektur der Replay-Zeitachse in `tools/verify_progression.py`

Das Replay drückte `down` und im **nächsten** Bild `confirm`. Das entsprach dem
alten Host (Index sofort ±1), nicht dem Original (±19 px je Bild, Index erst
nach `38/|number|`). Korrigiert: `362 down / 366 confirm` und
`602 down / 606 confirm`, und der Kisten-Checkpoint Bild 364 prüft jetzt genau
den ersten Gegenstand (Lighter), das volle Paar erst bei `deposit-pass`. Die
Prüfung ist damit *nicht* abgeschwächt, sondern auf die Originalzeitachse
gelegt; die Komponentenprüfung in `progression_test.cpp` deckt denselben Ablauf
zusätzlich ab (Gate ignoriert Confirm während des Scrolls, Ablage danach nutzt
den fortgeschrittenen Kistenplatz `ips1 1`, nicht Platz 0).

### Event-Karte: belegte Blocker (Sonde, keine Vermutung)

Isolierte Sonde gegen `data/archives/MRY` (nur `map_screen.cpp` +
`map_objects.cpp` + `scene.cpp`-Dekodierer) ergab für das Etagenpaket:

- `mode 7`, Puffer 44.448 B, `stage_model` 10.748 B, `stage_tex` 32.864 B,
  `map_tex` 32.840 B, 4 Markermodelle, 21 Kartenobjekte mit Positionen.
- `decode_map_objects` setzt **nie** `has_model` (nur der Test tut das), und der
  Knoten trägt keinen Modeloffset. Deshalb erzeugt `MapCodeProcess` für das
  Etagenmodell keinen einzigen `MapDraw` — die Kartenfläche kann nicht
  gezeichnet werden, egal welcher Renderer.
- `decode_memory_model` erwartet den Block am MDL-Kopf; die MPAC-Chunks tragen
  davor ihr `next`-Wort (`analyze_stage` rechnet genau damit), ein direkter
  Aufruf mit dem Chunkanfang bricht mit „binary relative offset outside asset“.
- Die 2D-Primitiven aus `archived/work/native_port_D10_map.patch` Teil B4 brauchen die
  `pri`→Tiefe-Abbildung; die liegt in `Ps2AddPrim2D` und ist **nicht** in der
  Decomp (PS2-SDK). Das ist eine Renderer-/Integrator-Entscheidung, keine
  Host-Ersatzlogik, die hier erfunden werden darf.

### Auslieferung: Zustand des gemeinsamen Arbeitsbaums

`--platform macos` gebaut und die Start.command-App aktualisiert; danach
gemessen: **`verify_progression` scheitert schon bei Bild 110**
(„announcement missing/restarted“), zusätzlich waren `verify_detector`
(Textur 414 unsichtbar), `verify_doors` (Raumwechsel fehlt) und
`verify_events` (cemetery-walk) rot. **Gegenprobe:** dieselben fünf Dateien
gestasht (Sicherung in `$TMPDIR`) und erneut gebaut — die Runtime scheitert
identisch, also stammt der Ausfall aus den laufenden Parallelsitzungen
(`world.cpp/.h`, `world_enemies.cpp`, `effect_sprites.cpp`, Änderungen
12:43–12:47), nicht aus dieser Arbeit. Die App wurde deshalb auf den letzten
geprüften Stand zurückgesetzt (`archived/releases/box-ui-before2-20261012-125222`),
der `verify_detector`, `verify_item_box_object` und `verify_map` besteht.
Erneut ausliefern, sobald der Baum wieder grün ist (ein
`--platform macos`-Lauf).

## Grenzen

Kein Durchspielnachweis, kein visueller Screenshot-Vergleich der Kistenliste
gegen das Original, kein Windows-Build in diesem Schritt, keine Paket-/
Starter-Aktualisierung (dafür gilt die ausdrückliche Auftragsregel). Die
Pad-Gehalten-Semantik des Scrolls ist als Abweichung oben benannt.
