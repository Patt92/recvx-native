# Blendzustand der Original-Engine: Punkt 2 des Transparenzplans

Stand 2026-10-13, Sitzung „alpha domain". Kein Commit, kein Paket, keine Dev-App ersetzt.
Der Arbeitsbaum enthielt parallel Änderungen anderer Sitzungen (Karte, Schatten, Fog).

## Was portiert wurde

`ps2_NaSystem.c:169-425` ist **Zustand, keine Funktion des Paares**:

- `njColorBlendingMode(target, mode)` schreibt `Ps2_nj_save_current.mode_bk[target]`, setzt
  `set_last = target` und ruft `njColorBlendingModeSys(mode_bk[0], mode_bk[1])`.
- `njColorBlendingModeSys(s, d)` schreibt `Ps2_gs_save.mode_bk[0..1]`, übernimmt
  `set_last` aus dem **nj**-Save und wählt dann das GS-ALPHA-Register:
  1. `mode_bk[set_last] < 2` → Register = `0x01` (Index 0) bzw. `0x44` (Index 1),
     **ohne** Tabellenzugriff;
  2. sonst, falls einer der beiden Indizes `< 2` ist → **Rückkehr ohne Update**, das
     Register behält seinen Wert;
  3. sonst `alpha_tbl[mode_bk[0]][mode_bk[1]]`; ist der Eintrag `0xFFFFFFFF`, bricht das
     Original mit „CALL ISHIKAWA ! ALPHA ERROR" ab.

Umgesetzt in `native/chunk_blend.cpp` als
`color_blending_mode`/`color_blending_mode_sys`/`color_blending_alpha` mit **zwei** Saves
(nj- und gs-Save getrennt, weil der Chunk-Pfad `njColorBlendingModeSys` direkt aufruft und
dabei den `set_last` des nj-Saves liest). Ungültige Paare werden in
`color_blending_unavailable()` gesammelt (Ausgabe als `blend_unavailable` in `--state`) und
lassen das Register stehen, statt einen Ersatzmodus einzusetzen oder abzubrechen.

Angebunden: `append_effect_sprite` (Effektsprite/Primitive), `append_custom_effect` (id 302
über dieselbe Funktion), `effect_particles.cpp`, `weapon_flash.cpp`. Die ID-Whitelist in
`effect_sprites.cpp` ist entfallen — das Paar des Ops entscheidet. `ninja_blend_register`
ist damit produktiv ungenutzt und wurde entfernt (die Tests prüfen jetzt den
Zustandsautomaten bzw. die Registerliterale).

Nicht angebunden, weil sie in den **noch unportierten Effektprozeduren** entschieden werden:
die Kategorieauswahl `ef_trs/ef_pnc/ef_opq/ef_thl/ef_ntx/ef_lin/ef_trs2d` mit ihren
`njControl3D`-Flags, dem `trans`-Argument und dem Sonderfall `(0,0)` = deckend im
`bhDrawThlEffect3D`-Pfad (`njDrawTexture3DHEx(...,0)`). Ein Sprite ohne portierte Prozedur
behält deshalb seine dokumentierte additive Annäherung und wird **nicht** mit einem
untergeschobenen Register gezeichnet.

## Nachweis

- `tests/native/chunk_blend_test.cpp`: Paare (8,6)→0x44, (8,10)→0x48, (11,3)→0x46,
  (9,6)→0x44; Index 0 → 0x01 und Index 1 → 0x44 ohne Tabellenzugriff; ein Paar mit Index 0
  (`bhEff111` 0/0) lässt das Register stehen; das ungültige Paar 9/3 wird gemeldet und das
  Register bleibt — genau eine Meldung `"9/3"`.
- `weapon_flash_test`, `lighter_test`, `effect_particles_test` prüfen die Register der
  echten Zeichenpfade (0x44).
- CTest in `build/alphadomain-claude-20261013`: **79/81**, rot nur `script_motion`
  (vorbestehend, mit dem Binary von 22:00 identisch) und `original_map_render`
  (Parallelarbeit der Kartensitzung).
- Laufzeit: verify_lighter, verify_combat, verify_detector, verify_scanner, verify_doors,
  verify_continuity, verify_events PASS. `verify_enemy_ai` bricht wie mit dem
  Vorher-Binary bei RM_0080 „kein Biss" ab (gemessen identisch, fremde Sitzung).
- **Echte Szenen treffen die Sonderfälle nicht:** `--state` in RM_0000 (Lighter-Replay),
  RM_0030 (Steve), RM_0080 (Kampf-Replay) und RM_0200 (Friedhof) meldet
  `blend_unavailable: []` — die abbruchgefährdeten Paare kommen im Datenbestand nicht vor.

## Ehrliche Einordnung: kein Pixel geändert

Gemessen mit dem Runtime von 22:41 (Alpha-Domäne, **ohne** Punkt 2) gegen den Stand mit
Punkt 2: RM_0090 Kamera 1 und 2 sowie die Lighter-Szene sind **pixelgleich** (0 von 307200
Pixeln, Delta 0). Das ist erwartet: die portierten Pfade benutzten ihre Register schon über
`ninja_blend_register`, die Werte sind identisch. Punkt 2 ersetzt eine ID-Whitelist und eine
reine Tabellenfunktion durch den **Originalmechanismus** und macht die Lücke sichtbar — die
sichtbaren Transparenzfehler der Effekte kommen aus den fehlenden Effektprozeduren
(Paar + Kategorie + Geometrie), nicht mehr aus dem Blendzustand.

## Nächster Schritt

Effektprozeduren portieren (Kategorie + Paar + Zeichenweg), beginnend bei denen, die der
Nutzer sieht: Scanner-/Fräs-Effekte in RM_0090, Blut-Pool, Mündungsfeuer-Zusätze. Danach
Punkt 3 (Nicht-CNK-Primitiven `njDrawTexture3DEx`/`njDrawPolygon3D`/`njDrawLine3D` mit
`trans`-Flag und gehalbierter Vertexfarbe) und Punkt 4 (CNK-Texturfunktion head bits 0..2,
`njSetConstantAttr`, `Ps2_njControl3D_flag & 0x8000`, `njCnkCmAs`, FBA).
