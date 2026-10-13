# Alpha-Domäne der Texturen: Ursache und Fix (Punkt 1 des Transparenzplans)

Stand 2026-10-13, Sitzung „alpha domain". Kein Commit, kein Paket. Der Arbeitsbaum enthielt
parallel Änderungen anderer Sitzungen (u. a. `renderer.cpp`, `scene.cpp`, `map_render.*`);
die hier beschriebenen Änderungen sind davon getrennt.

**Ausgeliefert in die Dev-App** (Nutzerauftrag): `tools/build_native.py --platform macos
--no-test` hat `build/macos-arm64` neu gebaut und `build/start/macos-arm64/RECVX Native.app`
ersetzt (arm64-only, `codesign --verify --deep --strict` bestanden, SHA256 des installierten
Runtime a55478ed…23d6). Smoke mit den App-Optionen inklusive `--mods` (HD-Paket) über
SDL-Dummy: 30 presents, kein Fehler. Verify-Suiten gegen das App-Binary: detector, scanner,
lighter, computer, frontend, progression, pickups PASS. Vorher-Binär:
archived/releases/alpha-domain-before-20261013-224118 (SHA256 f9614470…), zusätzlich als
`build/start/macos-arm64/.previous-*/RECVX Native.app` erhalten. Der Dev-App-Build enthält
den Arbeitsstand der parallelen Karten-/Schatten-/Fog-Sitzungen. CTest wurde deshalb dort
nicht gefahren (`--no-test`, sonst blockiert `original_map_render` die Auslieferung); die
Suite lief vollständig in `build/alphadomain-claude-20261013` (79/81, beide Roten erklärt).

## Ursache

Die Engine hatte zwei Alpha-Domänen für dieselbe Sache:

- **Dekodierte Originaltexturen** (`decode_texture_bundle`, native/scene.cpp): die Palette
  wurde mit `Ps2AlphaIsHalf` halbiert, also im GS-Bereich **0..128** gespeichert
  (0x80 = voll deckend). Der Renderer rechnete den CNK-Blendfaktor deshalb mit **÷128**
  (native/renderer.cpp).
- **Gekochte TGA-Dateien** (`write_tga` schrieb `min(255, a*2)`, `read_tga` las unverändert):
  hier standen **0..255** in der Datei, also 0xFF = voll deckend. Alle Host-2D-Pfade
  (UI-Blits, Effekt-Sprites, Computer-Terminal) rechneten mit **÷255**.
- Weil zur Laufzeit immer die TGA gewinnt (`apply_texture_files`), bekam der CNK-Pfad
  **verdoppelte Alphas** und teilte sie erneut durch 128: jede partiell transparente
  Fläche wurde bis zu **2× zu deckend**, und alles ab GS-Alpha 0x40 (= 0x80 in der TGA)
  wurde durch das `min(1.0)`-Klemmen **vollständig deckend**.
- Drei Stellen haben die Verdopplung von Hand nachgeholt (`ui.cpp` Menü/Titel,
  `computer.cpp` Terminal-Scheibe) — Beleg dafür, dass die Host-Domäne 0..255 ist; der
  gemeinsame Dekoder machte sie aber nicht.

Messung (RM_0090, Textur 32): dekodiert `0,9,17,26,34,43,51,60,68,77,85,94,102,111,119,128`,
in der gekochten TGA `0,18,34,52,68,86,102,120,136,154,170,188,204,222,238,255` — exakt ×2
(256 auf 255 geklemmt).

## Fix (eine Domäne)

Die Host-Domäne **0..255** gilt jetzt überall; der Blendfaktor ist `alpha/255`. Das ist
derselbe Faktor wie das originale `As/128`, weil `As = Ps2AlphaIsHalf(alpha)` ist:
`(alpha/2)/128 = alpha/256 ≈ alpha/255`. `Ps2AlphaIsHalf` wird damit nicht als Halbierung
in den Palettendaten gespeichert, sondern als Domänenkonvention — die Semantik bleibt.

- `native/scene.cpp`: Palettenalpha ohne `>>1` (nur noch Color-Key `Ps2AlphaIs000` und
  `Ps2Alpha4to8` für ARGB4444).
- `native/asset_io.cpp`: `write_tga` schreibt das Alpha unverändert → Export und Import
  sind identisch, ein erneutes Kochen erzeugt dieselben Dateien (8888) bzw. korrigiert
  eine 1-Stufe-Rundungslücke bei 4444/ungeradzahligen Alphas.
- `native/renderer.cpp`: ein Teiler für alle Blendarten (`coverage = alpha/255`), auch im
  Texturfilter-Pfad und bei `njDrawLine3D`; `SceneBlend::Ninja`/`Alpha`/`Additive` sind
  damit nur noch Modusunterschiede, keine Domänenunterschiede.
- `native/ui.cpp` (2×), `native/computer.cpp` (1×), `native/world.cpp` (Effektatlanten,
  1×): die vier handgeschriebenen ×2-Verdopplungen entfallen.
- `docs/port_ledger.tsv`: Zeile `ps2_dummy.c:Ps2CheckTextureAlpha` mit der Konvention.

## Nachweis

- `tests/native/chunk_blend_test.cpp`: neuer Fall „doubled TGA alpha does not render the
  original GS half" — TGA-Alpha 0x80 (= GS 0x40) muss die **halbe** Mischung ergeben
  (0x707070) statt 0x484848 (Viertel) oder 0xc0c0c0 (deckend). **Gegenprobe:** derselbe
  Test gegen eine Kopie von `renderer.cpp` mit dem alten Teiler fällt genau dort
  („doubled TGA alpha does not render the original GS half", Exit 1).
- `tests/native/scene_test.cpp` (Palettenalpha/`ClutChange`-Tor, jetzt über den
  ARGB4444-Zweig beobachtbar), `tests/native/game_test.cpp` (TGA-Rundlauf verlustfrei,
  HD-Vorrang), `tests/native/lighter_test.cpp` (Filterpfad mit vollem Alpha).
- CTest in `build/alphadomain-claude-20261013`: **79/81**. Rot: `script_motion`
  (vorbestehend, gleiche Meldung mit dem Binary von 22:00 in
  `build/mapinput-claude-20261013` belegt) und `original_map_render` (neue, noch
  unfertige Parallelarbeit der Kartensitzung: 11 Füllungen statt 1, 0 Objekte, Licht 0,
  Farben mit RGB 0/A 0x80 — keiner der Fehlschläge liegt auf meinem Pfad; die
  Alpha-Domäne ist dort der `SceneBlend::Alpha`-Füllpfad, den die Änderung nicht berührt).
- Laufzeit gegen `data` mit dem neuen Runtime: verify_detector (RM_0090), verify_lighter,
  verify_scanner, verify_computer, verify_frontend, verify_progression, verify_pickups,
  verify_events, verify_doors, verify_stairs, verify_combat, verify_snapshot,
  verify_continuity, verify_fog PASS; Raumscan **205/205 lädt, 0 blockierte Befehle**.
- `tools/check_no_original_core.py` PASS.
- Bildvergleich RM_0090 Kamera 1/2: die Änderung verschiebt das Bild in Richtung der
  GS-Referenz (7494 Pixel näher heran, 1407 weiter weg bei Kamera 1; Kamera 2 1792 gegen 7).

## Ausdrücklich nicht gelöst

- **Das Scanner-/Detektorfenster in RM_0090 bleibt unerklärt.** Gemessen: das Raummodell
  hat nur 14 transluzente Dreiecke (Textur 32: 10, Textur 49: 4), und deren Alpha ist
  **binär** (Textur 49: nur 0 und 128; Textur 32: 79 % Alpha 0 plus Gradient). Binäres
  Alpha ist von der Domänenänderung nicht betroffen (0 → 0, 128/128 = 255/255 = 1.0), und
  die gemessene Bildänderung dieser Kamera ist mit 776 Pixeln (Delta ≥ 8) klein. Punkt 1
  ist also **nicht** die Ursache des grauen Fensters.
- Auch der Texturfilter ist es nicht: `--nearest` gegen gefiltert ändert Kamera 1 in
  149606 Pixeln, aber **kein** Pixel um mehr als 23 Stufen — es entsteht keine deckende
  Scheibe. Offen bleibt der Hinweis, dass `njCnkCtTid` (Chunk-Typ 8) neben der Texturnummer
  auch den Filtermodus trägt (`(usSize/16384)&3`, Tor `!(usSize & 0x2000)`,
  ps2_NinjaCnk.c:890-901) und der Host diesen Teil nicht dekodiert.
- Betroffen von Punkt 1 sind die **Gradientflächen**: Raumglaskanten mit weichem Alpha,
  RM_1080 Textur 17 (Halo über der Tür, 8 Dreiecke, Materialalpha 0xc5), 109 von 400
  Objekttexturen und 43 von 400 NPC-Texturen mit Teilalpha. Für diese ist die Änderung die
  Korrektur (vorher bis zu 2× zu deckend, alles ≥ 50 % hart deckend).

## Offene Punkte aus dem Plan (2–4, unverändert)

2. `njColorBlendingMode`/`njColorBlendingModeSys` mit den Sonderfällen 0/1 und der
   „Register behalten"-Semantik portieren und **jeden** Zeichenpfad daran binden
   (heute: ID-Whitelist + `SceneBlend::Additive` in native/effect_sprites.cpp:629).
3. Den Nicht-CNK-Zeichenpfad (`njDrawTexture3DEx`/`njDrawPolygon3D`/`njDrawLine3D`,
   ps2_NaDraw.c) mit `trans`-Flag und gehalbierter Vertexfarbe als eigenen Pfad binden.
4. CNK-Texturfunktion (Head-Bits 0..2 → `usCnkCsTexColorCalcFunc`), `njSetConstantAttr`/
   `Ps2_njControl3D_flag & 0x8000`, `njCnkCmAs` (Typ 22) und FBA.
