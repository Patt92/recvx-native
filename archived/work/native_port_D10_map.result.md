# D10 — Kartenbildschirm (map.c) nativ portiert: Ergebnis

Stand 2026-10-10. Auftrag: `archived/work/native_port_D10_map.md`. Verbindlich bleiben
`AGENTS.md` (Abschnitte „Verbindliche Architekturvorgabe“, „FORBIDDEN_DONTUSE“,
„Originalkern: nicht kompilieren, nicht linken, nicht aufrufen“) und `QUALITY.md`.

`src/ps2/veronica/prog/map.c` (3028 Zeilen), `include/ps2/veronica/prog/map.h`,
`system.c`, `sdfunc.c` und das ELF wurden ausschliesslich als Semantikreferenz
gelesen. Es wurde kein Funktionskörper kopiert, extrahiert, generiert,
kompiliert oder gelinkt. `archived/FORBIDDEN_DONTUSE` wurde nicht gelesen,
nicht durchsucht und nicht genutzt. `python3 tools/check_no_original_core.py`
lief vor der Arbeit und vor jedem Build: jeweils
`PASS: audited source/build/binary scope has no forbidden core`.

Eigene Dateien, nichts Fremdes geändert, nichts zurückgesetzt, gestasht oder
gelöscht; kein Commit, kein Push, kein Paket:

| Datei | Inhalt |
| --- | --- |
| `native/map_tables.h` | Typen und Enums (`mp_spr`, `mp_set`, `mp_mod`, `enum_2`) |
| `native/map_tables.c` | 19 Originaldatentabellen aus map.c |
| `native/map_screen.h` | Schnittstelle, Host-/Dienstverträge, Zeichenliste |
| `native/map_screen.cpp` | Mechanismus |
| `tests/native/map_screen_test.cpp` | Komponententest |
| `tools/verify_map.py` | Runtime-Prüfung (braucht den Integrator-Patch) |
| `archived/work/native_port_D10_map.patch` | Anschlüsse an fremde Dateien |

---

## 1. Datenquelle und Format (erster Auftragsschritt, belegt)

`bhReadMapData` (map.c:208) liest über `RequestReadInsideFile(mwP->prti_no, …)`
(sdfunc.c:3053). `mwP->prti_no = sys->dor_partid` (map.c:164), und
`sys->dor_partid = SoundAfsPatDef[5].PartitionId` (sdfunc.c:441) zeigt in
`SoundAfsPatDef` (sdfunc.c:92) auf **`MRY.AFS`**, Partition 5, maximal 160
Innendateien. Im Projekt liegt das entpackt als **`data/archives/MRY/`** mit
157 Dateien (`0000.bin` … `0156.bin`).

Angefordert werden genau zwei Dateiarten:

* **Indexpaket** `MapEntryTask(FtskMapRead, MP_MOD_IDX_ANALYZE, sys->ply_id)`
  (map.c:371) → Datei-ID = Spieler-ID 0…4. In `data/archives/MRY` sind genau
  die Dateien `0000`–`0004` von dieser Bauart (Header-Offset 4, erste Marke
  `MDL`), die übrigen nicht — das bestätigt die Zuordnung.
* **Etagenpaket** `MapEntryTask(FtskMapRead, MP_MOD_DAT_SET,
  (mwP->map_flr + mwP->stg_no * 6) + 5)` (map.c:460) → Datei-IDs
  `5 + flr + stg*6`. Mit `MapCncInit(11, 6)` (map.c:343) sind das die IDs
  5…70, also 11 Karten × 6 Etagen = 66 Dateien. Genau diese 66 Dateien haben
  Header-Offset 24; davon sind 33 leer (Etagen ohne Karte → originaler Pfad
  `MP_MOD_WAIT_NOMAP`, map.c:522) und 34 enthalten einen Opcodestrom.

**Containerformat** (nativ geparst in `MapScreen::analyze_index` /
`analyze_stage`): 4 Byte Magic `'MPAC'` (0x4341504D, map.c:378/470), danach ein
32-Bit-Byteoffset auf den ersten Chunk. Jeder Chunk beginnt mit einem 32-Bit-Wort
`next` = Byteabstand vom Chunk-Nutzdatenanfang zum nächsten Chunk-Kopf; `-1`
beendet die Kette.

* Indexpaket: n × `MDL`-Chunk (Markermodelle, `bhMlbBinRealize` in `MrkMdl[8]`,
  map.c:388), dann die Sprite-Textur (`bhSetMemPvpTexture`, map.c:400, setzt
  Status 0x8), dann 10 Nachrichtenblöcke (`mes_bufPP`, map.c:408), dann die
  Etagentabelle für `MapGetFloorNo`.
* Etagenpaket: Opcodestrom (`map_cdeP`, `short`-Werte), dann das Kartenmodell
  (`MDL` → `MapPurgeTree`), dann dessen Textur (map.c:482, Status 0x10).
* Etagentabelle (`FT_WRK`, map.h:353): `short[2]` = Byteoffset der Tabelle,
  `short[3]` = Anzahl; Einträge `{float label, short map_no, short flr_no}` mit
  `label = 100 * rom_no + pos_y`, binär durchsucht (map.c:1207).

**Nicht von map.c angefordert:** die Dateien 71…156 (86 Stück, alle mit
`MDL`-Chunk). Beide Lesepfade decken nur 0…4 und 5…70 ab. Das ist als offene
Frage notiert (siehe Abschnitt 6), nicht geraten.

---

## 2. Implementiert (in eigenen Dateien, vollständig)

Reihenfolge, Zustände, Flags, Tabellen, Timer und Seiteneffekte folgen dem
Original; jede Routine mit Fundstelle.

**Lebenszyklus** — `bhInitMap` (map.c:64) mit `status |= (1 << set_mod) & 7` für
`MP_SET_EVENT`/`MP_SET_GAME`/`MP_SET_SUB` (system.c:785/789/853), `mp_prm[0..3]`
→ `bnk_mde/stg/flr/rom` (map.c:112), SE 3 nur ohne Statusbit 2 (map.c:166),
`bhSetMap` (map.c:173, im Original leer), `bhExitMap` (map.c:179) mit
Texturfreigabe nach Status 0x8/0x10 und `gm_flg &= ~0x80000`. Die
PS2-Texturspeicherrettung (`bhCopyTexmem2Mainmem`, map.c:130) ist nativ
Ressourcenerhalt — die Raumtexturliste bleibt geladen, kein Speicherkopie-Nachbau;
`restores_room_texture()` meldet den Originalzweig.

**Zustandsmaschine** — `bhControlMap` (map.c:319) mit allen zwölf `mp_mod`-Fällen
einschliesslich des Durchfalls `MP_MOD_IDX_ANALYZE → MP_MOD_MAP_READ` (map.c:445)
und `MP_MOD_WAIT_NOMAP → WAIT_NORMAL/WAIT_ZOOM` (map.c:522), der
Ansichtsinterpolation mit Faktor 0,5 und dem Schliesszweig `pad_ps & 0x3000`
(SE 0, Abblenden 0xFF000000, Status 0x400, `FtskMapExit`). Abschluss jedes Ticks
in Originalreihenfolge: `time++`, `MapPadMain`, `MapViewMain`, `MapLightMain`,
`MapPaletteMain`, `MapFuncExec` (map.c:555-562).

**Pad, Ansicht, Licht, Palette** — `MapPadMain` (map.c:569) inklusive
Totzone ±40 und vollständiger Sperre bei Status 0x400; `MapViewMain` (map.c:605);
`MapLightMain` (map.c:625) setzt `lgt_scale = map_scale`; `MapPaletteMain`
(map.c:651) mit den vier Pulsgruppen 1024/2048/4096 und der vierten Gruppe,
die zusätzlich Alpha skaliert.

**Funktions- und Taskliste** — `MapFuncInit/Alloc/Free/Del/Ins/Exec`
(map.c:1291-1372) als originaler Ring aus `func_num + 2` Plätzen: Einfügen
direkt hinter dem Kopf, der jüngste Eintrag läuft zuerst; der Nachfolger wird
vor dem Aufruf gesichert, damit eine Prozedur sich selbst freigeben kann.
`MapEntryTask` (map.c:1719) und `FsubTaskMain` (map.c:1744) mit der originalen
Überlagerung der 64-Byte-Plätze: `w[0..2]` = `param0..2`, `w[3..11]` =
`FreeWrk[0..8]` (map.h:109), so dass `FT_WORK`, `FS_WORK`, `FG_WORK`, `FZ_WORK`,
`FI_WORK`, `FS_WRK`, `FC_WORK` und `FM_WORK` dieselben Wörter teilen wie im
Original. Tasks: `FtskMapWait` (`st_flg & 8`), `FtskMapExit` (`cb_flg & 2`),
`FtskMapRead` (zweiphasig: anfordern, dann auf den Abschluss warten),
`FtskMapNormal` (map.c:1811, SE 5, Richtungspfeile nach `MapCheckNextMap`),
`FtskMapZoom` (map.c:1867, SE 5, Status 0x200, `bnk_tag_pal` 3 / `_wal` 9,
Zielzoom 0,5 × `vew_zom_bak`, Tagwechsel mit SE 2, Zoomgrenzen `vew_min/max.y`).

**Codeinterpreter** — `MapCodeProcess` (map.c:718) mit allen Opcodes 0…18:
Objektmatrix (3), Modellzeichnung mit Palettenumschaltung für die Modi
1/2/24 bzw. 7/8/25 bei Status 0x200 (4), Marker (5, 17), Rotation/Translation
der laufenden Matrix (6, 7), Raumvergleich (11), Flaggenprüfung (12),
Spielerbox (13), bedingte und unbedingte Sprünge (8, 9, 10), Flaggensetzung
(14), einmalige Ansichtsinitialisierung mit Status 0x40 (15), Tageintrag mit
Status 0x100 (16) und der zeitgetaktete SE über `SndTbl` (18). Abbruch auf dem
vollen Wert 2 — nicht auf dem maskierten Opcode —, Boolmodus aus dem oberen
Nibble über `MapBoolSet` (map.c:1022). Die Argumentreihenfolge von Opcode 12/14
ist an den Originaldaten belegt: `MRY/0005` führt 8 dann 48, und eine
Flaggenmatrix 48 existiert nicht — also `(Gruppe, Index)`.

**Flagumrechnung** — `MapCnvStatus2Flag` (map.c:256) auf den vorhandenen
Besitzern: Gruppe 8 über `EventVm::set_flag`, Gruppe 9 aus `ItmDat` gegen
`Inventory`-Slots (`&sys->itm[ply_id * 16]`, map.c:287), Spielerfaltung
0/1/2 bleibt, 3 → 2, 4 → 0. Keine zweite `sys`/`plp`/`rom`-Welt.

**Tag- und Verbindungsgraph** — `MapTagInit/Entry/Connect/Center`
(map.c:2195-2318) mit `njCalcPoint` in den Objektraum, der Quadrantenbildung
`((atan2 … + 8192) / 16384) & 3` und dem Schwerpunkt-Rückfall;
`MapCncInit/Get/Cnc/Connect` (map.c:2795-2935) und `MapCheckNextMap`
(map.c:2936) mit `AdjMapA`, der 512 → 896-Korrektur und den vier Ketten;
`GetGameMode` (map.c:3018) wählt `CncDatA` bzw. `CncDatB`.

**Darstellung** — `MapDrawMarker` (map.c:1039, inklusive Spielermarker nur bei
`ply_flr == map_flr` und Marker 6 als Titelsprite), `MapDrawBackground`
(map.c:1101, Platte plus 32-px-Gitter in beiden Richtungen),
`MapDrawSprite` (map.c:1142, `TexAnm`, `stg_texP` ab Sprite 17, Farbe als
Konstantmaterial), `MapDrawLine`/`Line2`/`Fill`/`PolyFill` (map.c:2319-2387 mit
den Attributen 0/0x40/0x60 und der Pixelmittenrundung), `FsprSpriteDraw`,
`FsprSilhouetteDraw` (map.c:1475, Alpha steigt mit **zwei** Inkrementen je Tick,
wie im Original), `FsprArrowDraw` (map.c:1514, Schwingung 1,9 × sin, Zähler
+910 mit Umbruch bei 32768, Kartenwechsel), `FsprArrowDraw2` (map.c:1649,
Modus −1 ohne Nachbartag), `FsubGaugeDrawZ/X/Draw` (map.c:1992-2194, Teilung
alle 5 Einheiten, lange Marke alle 10, mittlere alle 2, Seitenverhältnis
1,174), `FsubZoomCursor` (map.c:2394), `FsubZoomInfomation` (map.c:2488),
`FsubZoomScreen` (map.c:2576), `FsubCompass` (map.c:2712, +1456 je Tick),
`FsubModeMessage` (map.c:2759 nach `sys->keytype`), `MapCnvArgb2Color`
(map.c:2563), `MapPurgeTree` (map.c:1245).

Die Primitiven werden mit Originalgeometrie, Priorität, Blendattribut und
aufgelöstem Material in `MapDrawList` aufgezeichnet (siehe Lücke 6.1), nicht
still verworfen.

**SFX** — ausschliesslich `CallSystemSe` über den vorhandenen `AudioMixer`:
3 beim Öffnen, 5 beim Wechsel in Normal- und Zoomansicht, 2 bei Tag- und
Kartenwechsel, 0 beim Schliessen, `SndTbl[0] = 0x80000247` über Opcode 18.
Keine Ersatztöne.

### Belegte Originalbefunde (keine erfundene Logik)

* **`MapDrawMessage` (map.c:2388) hat im Original einen leeren Körper.** Weder
  die „keine Karte“-Meldung (map.c:523) noch der Raumname im Zoom (map.c:2436)
  erzeugen Ausgabe, und die zehn Nachrichtenblöcke `mes_bufPP` bleiben ungenutzt.
  Der Port übernimmt das unverändert und erfindet keinen Raumnamen-Renderer.
* **`map_wrk::cur_rom` (map.h:181)** wird von keiner Routine in map.c gelesen;
  der Port führt dafür kein Feld.
* **`MapCnc` indiziert `AdjMapA[3]` mit dem 0…15-Slot der Karte 10**
  (map.c:2852). Beide Originaltabellen referenzieren Karte 10 nur mit den Slots
  0, 1 und 2 (`CncDatB` überhaupt nicht), der Originalzugriff bleibt also in
  Reichweite. Ein Slot ≥ 3 wäre im Original ein Lesezugriff über die Tabelle
  hinaus; der Port meldet das, statt es in einen erfundenen Wert zu maskieren.
  Der Test sichert die Reichweite beider Tabellen ab.
* **`MapPadMain` läuft am Ende von `bhControlMap`** (map.c:556). Der
  Schliesszweig im `switch` sieht deshalb den Padstand des vorigen Ticks, der
  Zoomeintritt in `MapFuncExec` dagegen den aktuellen. Diese Ein-Tick-Latenz ist
  originalgetreu übernommen und im Test festgehalten.

---

## 3. Produktiv angeschlossen

**Nichts.** Der Kartenbildschirm ist noch nicht in der laufenden Runtime
aktiv. Die fremden Dateien `CMakeLists.txt`, `native/runtime.cpp`,
`native/ui.*`, `native/input.*`, `native/world.*`, `native/renderer.*`,
`native/scene.h` wurden auftragsgemäss **nicht** geändert — dort arbeiten Peers.
Die Anschlüsse liegen als `archived/work/native_port_D10_map.patch`:

* **Teil A, anwendbare Hunks:** `CMakeLists.txt` (zwei Quellen in `recvx_game`,
  neues Testziel `recvx-map-tests` mit `add_test original_map_screen`),
  `native/ui.h` (`bool map_requested`), `native/ui.cpp:257` (MAP, `maincsr == 2`,
  fordert den Kartentask an statt des Fehler-SE).
* **Teil B, Schnittstellenvertrag für `native/runtime.cpp`:** Objekt und
  Include, Bindung von `MapServices`/`MapHost` an die vorhandenen Besitzer,
  Eintritt über `bhInitMap(MP_SET_SUB)` mit `gm_flg |= 0x80000`, Tick mit
  Vorrang wie beim Terminaltask, Austritt über `bhExitMap`, Modell-/Texturdekodierung
  mit den vorhandenen Dekodern plus `set_map_objects`, Zeichnen aus
  `draw_list()` und die `"map"`-Sektion der `--state`-Ausgabe für
  `tools/verify_map.py`. Teil B ist bewusst als Vertrag und nicht als
  Kontext-Diff geschrieben: die betroffenen Anweisungen in `runtime.cpp` sind
  einzelne Zeilen von über 2000 Zeichen, an denen ein Diff bei parallel
  arbeitenden Peers nicht zuverlässig anwendbar bleibt.

Die Kontextzeilen des Patches stammen aus dem Arbeitsbaum vom 2026-10-10 und
sind vor dem Anwenden gegen Peer-Änderungen zu prüfen.

---

## 4. Komponentengeprüft (Mac arm64, eigenes Buildverzeichnis)

Build ausschliesslich in `build/d10-map`, kein Peer-Verzeichnis berührt,
keine parallelen Builds:

```
python3 tools/check_no_original_core.py
clang -std=c11  -Inative -c native/map_tables.c   -o build/d10-map/map_tables.o
clang -std=c11  -Inative -c native/ninja_math.c   -o build/d10-map/ninja_math.o
clang -std=c11  -Inative -c native/ninja_matrix.c -o build/d10-map/ninja_matrix.o
clang++ -std=c++17 -Inative -Wall -Wextra -o build/d10-map/map_screen_test \
    tests/native/map_screen_test.cpp native/map_screen.cpp \
    build/d10-map/map_tables.o build/d10-map/ninja_math.o build/d10-map/ninja_matrix.o
./build/d10-map/map_screen_test . data
```

Ergebnis: `PASS` des Prüfers, Übersetzung **ohne Warnung** unter
`-Wall -Wextra`, Test **`map screen: all checks passed`** (Rückgabe 0).

Geprüft wird:

1. **Datentabellen gegen die Quelle.** Der Test liest
   `src/ps2/veronica/prog/map.c`, schneidet den Initialisiererblock jeder
   Tabelle heraus und vergleicht **jeden einzelnen Wert** in Reihenfolge mit
   `native/map_tables.c`: `MapPal` (128 Werte), `MapCol`, `ItmDat` (44),
   `CncDatA` (81), `CncDatB` (49), `SndTbl`, `MrkAtr` (24), `TexAnm` (180),
   `SprSet` (55 Zahlen plus Enum- und Prozedurnamen in Reihenfolge),
   `dst_pos0/1/2`, `ScrOut`, `ScrIn`, `set_pos`, `CmpArw` (16), `NmlMes`,
   `AdjMapA`. Eine fehlende oder nicht auffindbare Tabelle ist ein Fehlschlag,
   keine stille Auslassung.
2. **Flagumrechnung.** Alle fünf Spieler-IDs mit der Originalfaltung; Gruppe 8
   behält genau `id+1`; Gruppe 9 wird für alle 22 `ItmDat`-Einträge gelöscht;
   für jedes der 22 Items einzeln: vorhanden → genau eine Flaggennummer gesetzt
   (die Einträge 59 und 86 teilen `flg_no` 96, map.c:44 — der Test zählt
   deshalb Flaggennummern, nicht Tabellenzeilen); fremde Item-ID → keine
   Flagge; letzter von 16 Slots wird geprüft.
3. **Verbindungsgraph.** 66 Knoten mit korrekten `map_no`/`flr_no`/Status 0;
   `MapCncConnect` ohne Gruppe-8-Flaggen → `MapCheckNextMap` meldet in **keiner**
   Richtung ein Ziel (Nichttrefferfall); mit allen Flaggen → Knoten mit Status;
   `MapCnc` richtet `flr_prev`/`flr_next`/`map_prev`/`map_next` nach den vier
   Originalzweigen; Reichweite beider Tabellen gegen `AdjMapA[3]`, `cnc_map`
   und `cnc_flr`.
4. **Etagenwahl und Originaldaten.** Gegen die echten Dateien in
   `data/archives/MRY`: die Indexdatei der Spieler-ID wird angefordert, das
   `MPAC`-Paket geparst, Markermodelle werden gefunden, und ein Schlüssel
   unterhalb der Originaltabelle liefert `-1` statt eines erfundenen Treffers.
5. **Taskfolge an Originaldaten.** Öffnen (SE 3, Status aus `set_mod`,
   `MP_SET_SUB` **ohne** SE) → `MP_MOD_FIRST` (Abblenden mit `fade_pbk`,
   Hintergrundgitter gezeichnet, Verbindungsgraph, Wechsel nach
   `MP_MOD_IDX_READ`) → Index lesen/auswerten → Etagendatei **5** aus
   Partition **5** lesen/auswerten → Normalansicht (SE 5) → Zoom (SE 5,
   Status 0x200) → Zoom verlassen (Status 0x200 fällt) → Schliessen (SE 0,
   Abblenden nach Schwarz, Padsperre 0x400, `MP_MOD_EXIT`, `bhControlMap`
   liefert 0) → `bhExitMap` (Texturfreigabe, Raumtexturrückgabe,
   `gm_flg 0x80000` gelöscht). Die Zweitick-Haltezeit beim Schliessen ist die
   originale `MapPadMain`-Latenz und als solche kommentiert.
6. **Palette und Nichttrefferfälle.** Einträge 0…15 bleiben die Tabelle,
   16…31 pulsieren, `MapCol` unverändert; eine nicht lesbare Datei bleibt der
   Originalpfad `fil_mode 1` — der Schirm bleibt ohne Kartendaten statt eine
   Ersatzkarte zu erfinden.

Der Testaufbau benutzt zwei ausdrücklich gekennzeichnete Diagnosehilfen: ein
einzelnes übergebenes Kartenobjekt, damit `MapCodeProcess` läuft (im Betrieb
kommen die Objekte vom Integrator), und die direkte Übergabe der Itemslots.
Ohne `data/archives/MRY` werden die datengestützten Prüfungen übersprungen und
das ausdrücklich gemeldet, nicht als Erfolg gezählt.

---

## 5. Runtime-geprüft

**Nichts.** `tools/verify_map.py` ist geschrieben und syntaktisch geprüft
(`python3 -m py_compile`), kann aber erst nach dem Integrator-Patch bestehen:
es öffnet die Karte über den echten Statusmenü-Pfad (Debug-Gabe des Items 59
und Debug-Position sind als Diagnose gekennzeichnet; `inventory` → `back` →
`left` → `confirm` ist der gewöhnliche MAP-Knopf), prüft `MP_SET_SUB`,
Partition 5, die Indexdatei der Spieler-ID, Flagge 9/96, gezeichnete
Primitiven, Tags, den Ansichtsmodus, die Rückkehr ins Statusmenü und die
Freigabe von `gm_flg 0x80000`, dazu den Event-Einstieg in `RM_0060`.

Fehlt die Runtime-Bindung, bricht das Skript mit genau dieser Begründung ab
(`the runtime reports no "map" state section … Apply
archived/work/native_port_D10_map.patch`) — ein Fehlschlag kann nicht als Erfolg
missverstanden werden. Beide Abbruchwege wurden ausgeführt: ohne Binary
(`runtime not found`) und gegen ein vorhandenes Peer-Binary.

Beim Lauf gegen `build/gapcheck-claude/recvx-runtime` (nur ausgeführt, dort
nicht gebaut) bricht die Runtime **vor** jeder Kartenprüfung mit
`binary field outside asset: offset 18446744073709551615, bytes 4, asset size
1100316` ab. Das ist der in AGENTS.md beschriebene `size_t(-1)`-Sentinelfall
eines älteren Peer-Builds, enthält keine meiner Dateien und ist **nicht** durch
diese Arbeit verursacht. Eine eigene Runtime wurde nicht gebaut, weil dafür
`CMakeLists.txt` geändert werden müsste, was dieser Auftrag untersagt.

**Windows:** nicht gebaut, nicht geprüft. Kein Paket, kein Durchspielnachweis,
kein Screenshot des laufenden Kartenbildschirms.

---

## 6. Offene Lücken (benannt, mit Fundstelle)

1. **2D-Primitiven im nativen Ausgabepfad fehlen.** `native/scene.h:58`
   (`SceneTriangle`) und `native/scene.h:72` (`SceneLine`) bieten kein
   2D-Sprite (`njDrawSprite2D`, map.c:1203), keine Bildschirmfläche
   (`njDrawPolygon2D`, map.c:2385) und keinen Konstantmaterial-Modellaufruf
   (`njCnkEasyMultiDrawObject`, map.c:1070/1086). Der Port zeichnet deshalb
   nicht selbst, sondern liefert `MapDrawList` mit Originalgeometrie,
   Priorität, Attribut und Material. Anschlussvertrag in Patch Teil B4;
   `renderer.*`/`scene.h` wurden nicht geändert.
2. **Modell- und Texturdekodierung** der `MDL`- und PVP-Chunks
   (`bhMlbBinRealize`, map.c:388/478; `bhSetMemPvpTexture`, map.c:400/482)
   bleibt beim Integrator, weil die vorhandenen Dekoder in `world.cpp`/`scene.h`
   liegen. `MapSchreen` liefert die Chunk-Spannen und führt `MapPurgeTree`
   selbst aus. Ohne diese Bindung bleibt `MapCodeProcess` ohne Objekte.
3. **Kamera- und Lichtvertrag.** `njClipZ(-2, -20000)`, `njSetAspect(1.174, 1)`,
   `njSetBackColor(0,0,0)` und `njSetScreen` (dist 500, 640 × 480, Mitte
   320/240) aus map.c:68-71 und map.c:356-369 sowie die
   `njCnkSetEasyMulti*`-Lichtinstallation aus `MapLightMain` (map.c:625) sind
   Renderer-Zustand; der Port hält die Werte, installiert sie aber nicht.
   `MapScreen::light_main` führt deshalb nur `lgt_scale` nach.
4. **MRY-Dateien 71…156** (86 Dateien mit `MDL`-Chunk) werden von keinem der
   beiden Lesepfade in map.c angefordert (map.c:371 und map.c:460 deckt 0…4 und
   5…70 ab). Zweck ungeklärt — nicht geraten, nicht benutzt.
5. **`GetReadFileStatus`-Zeitachse.** Der native Archivdienst liefert eine
   Datei in einem Aufruf; der Port behält die zweiphasige Abfolge
   (anfordern, dann auf den Abschluss warten, map.c:208-250), aber nicht die
   DMA-Laufzeit des Originals. Der Abbruchpfad `fil_mode 1` / `status -1` ist
   abgedeckt, `CheckOpenTray`/Soft-Reset (sdfunc.c:3110) nicht.
6. **`MapDrawMessage` bleibt leer**, weil es im Original leer ist (map.c:2388).
   Sobald belegt ist, woher Raumnamen tatsächlich kommen (die Blöcke
   `mes_bufPP`, map.c:408, sind im Original unbenutzt), ist das eine eigene
   Aufgabe — hier wurde nichts erfunden.
7. **Zweiter Spielmodus.** `GetGameMode` (map.c:3018) wählt `CncDatB` ab
   Stage 7; dieser Zweig ist nur über die Tabellen und den Graphen geprüft,
   nicht an einem gespielten Antarktis-Stand.
8. Nicht geprüft, weil ohne Integrator-Patch nicht erreichbar: Zusammenspiel
   mit `StatusSlide`/`StatusCancel` beim Zurückkehren, Verhalten bei
   Raumwechsel während offener Karte, `MP_SET_EVENT`-Einstieg aus einem
   Skript (system.c:785).

---

## 7. Regeln, Einhaltung

* Keine Heuristik, keine geratene Konstante, keine Raumsonderregel, kein
  stiller No-op, keine abgeschwächte Testerwartung. Die einzige ursprünglich
  eingebaute Maskierung (`AdjMapA`-Index) wurde an den Originaldaten geprüft
  und durch den Originalzugriff mit gemeldeter Grenze ersetzt.
* Keine zweite `sys`/`plp`/`rom`-Welt: Flaggen aus `EventVm`, Items aus
  `Inventory`, Stage/Raum/Spieler/Pad aus `MapHost` (vom Integrator aus den
  vorhandenen Besitzern gefüllt), Ton über `AudioMixer`.
* Kein Originalkern kompiliert, gelinkt oder aufgerufen; nur Datentabellen
  übernommen, mit Regressionstest gegen die Quelle.
* Fremde Dateien unverändert; Build nur in `build/d10-map`; kein Commit, Push
  oder Paket.
