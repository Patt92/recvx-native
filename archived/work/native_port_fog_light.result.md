# Nebel und Lichtabschwächung: Zelle vor dem Feuerzeug (2026-10-12 nachts)

Nutzermeldung: Die Zelle in RM_0000 ist ohne Feuerzeug deutlich zu sehen; im
Original ist sie so dunkel, dass Rodrigo nicht erkennbar ist. Auftrag: im
Originalquelltext suchen, nicht raten.

## Befund am Original

Der Host hatte zwei Abweichungen von der Ninja-Beleuchtung und den Nebel gar
nicht. Alle drei sind im Quelltext belegt:

| Mechanismus | Original | Vorher im Host | Jetzt |
|---|---|---|---|
| Punktlicht-Abschwächung | `njCnkCvVn` (ps2_NinjaCnk.c:1240-1262): Reichweiten als Quadrate (`njCnkSetEasyMultiLightRange` :352-363), Licht nur bei `fFarRR > d²`, Abfall `fNearRR/d²` hinter `nr`, Schnitt bei `fr`; `nr 0` trägt nichts bei. VU0-Form `njCnkCvVnPs2` (:1316-1324) bekommt dieselben Werte. | Lineare Rampe `(fr-d)/(fr-nr)`; Licht 25 der Zelle (nr 14, fr 180) trug auf 56 Einheiten noch 75 % statt 6 %. | `native/cnk_light.h`, von `RoomWorld::append` benutzt. |
| Richtungslicht | `njCnkSetEasyMultiLightVector` speichert `-v` (:328-336), Term `N·fC·fI`, `v` unnormalisiert, `fI` 1 auf dem Multi-Satz (:260 schreibt nur das Einzellicht). | `N·v` (Vorzeichen falsch), normalisiert. | `cnk_directional_light_term`. |
| Nebel | Keyframe 0 der Ereigniskamera 37 (RM_0000) trägt Bit 0x8 mit `fog_col 0x000a0f`, `nr 128`, `fr -31`. `bhSetEventCamera` übernimmt die Werte (cut.c:1694-1699), `bhControlEOR` (sync.c:143-255) installiert sie je Bild: `njSetFogColor`, `njGenerateFogTable3`, `njFogEnable`, `st_flg 0x2`. Jeder CNK-Streifen zeichnet mit FGE (ps2_NinjaCnk.c:1880), `F` je Vertex aus `njCalcFogPower(fVSZ)` (:1196), GS mischt `Cv=(Cs·F+Cf·(255−F))>>8`. | Tabelle/Abtastung (`native/fog.h`, parallele Sitzung) vorhanden, aber nie installiert; Renderer nutzte sie nur für den Schattenverbund. | `RoomWorld::cut_fog_handover` / `control_fog` / `install_fog`, `Scene::fog_color`, Renderer-Mischung auf `world_lit`-Dreiecken. |

Weiter gebunden: `bhSetCut` (cut.c:349-377) und `bhSetFixedCut` (:463-484)
übergeben den Schnittnebel mit `fog_ct = fog_cngct − 3` (bei 2 und 3 gleich 0;
`ef_flg 0x10` einmal weniger), `bhControlEventCamera` splinet `fog_nr/fog_fr`
(`cam.ipf`) und `fog_col` (`cam.icr`, Kanalklemmung) je Bild (cut.c:1899-1909,
2074-2098), Modus 2 nimmt den Endkey; Opcode `0x4c` `bhFogColorSet`
(event.c:4067, Tabellenindex 76) setzt `sys->fog_col` und `st_flg 0x100000`.
Der Zweig `gm_flg 0x40` (Spieler-Augenkamera, cut.c:28-65, sync.c:149) ist
portiert, hat im Host aber keinen Setzer.

## Messung (RM_0000, Vorspann übersprungen, Frame 600, 640×480, mittlere RGB)

| Stand | Mittel RGB | Abweichung zum Bild `build/packages/cell-v05-final.bmp` |
|---|---|---|
| vorher | 40 / 42 / 42 | 30,1 |
| Abschwächung nach ps2_NinjaCnk.c | 26 / 31 / 33 | 17,8 |
| plus Nebel | 18 / 17 / 12 | 10,0 |

Die Vergleichsaufnahme vom 5. Oktober (Mittel 12/12/12) ist ein älterer
Hoststand, kein Originalbild; sie dient nur als Richtung. Mit Feuerzeug
(Ereigniskamera 1, Key 0 ohne Bit 0x8) bleibt der Nebel aus; das steht so in
den Raumdaten. Rodrigo ist in dieser Phase nicht geladen (kein Kind-1-Work im
Zustand), das Bild ist also richtig dunkel und leer.

## Nachweise

- `tests/native/lighting_fog_test.cpp` (CTest `lighting_fog_semantics`): Punktlicht-
  Term mit den Zahlen von RM_0000-Ereignislicht 24 (1.0 innen, 0.49 bei d 5,
  0 bei fr, 0 bei nr 0, Rückseite 0, d 0), Richtungslicht `N·(−v)`,
  GS-Mischung gegen die Formel mit `fog_sample`, keine Mischung auf
  Nicht-CNK-Dreiecken, Nebel aus = unverändert.
- `tools/verify_fog.py` (Laufzeit, Originalräume): RM_0000 Frame 600 Nebel an mit
  genau 0x000a0f/128/−31 und Zellenhelligkeit < 24/255; Feuerzeugszene Nebel
  aus; RM_0080 Schnitt 1 Nebel 0x17212b/128/−117; keine blockierten Befehle,
  keine `njGenerateFogTable3`-Lücke. `--state` trägt jetzt `fog`.
- Diagnose: `RECVX_LIGHT_DBG=1` schreibt die mittlere Vertexbeleuchtung des Raumes.

## Auslieferung und Umfeld

`build/start/macos-arm64/RECVX Native.app` (von `Start.command` gestartet) ersetzt,
ad hoc signiert, `codesign --verify --deep --strict` OK, SHA256 e59e142b…; Backup
`archived/releases/fog-light-before-20261012-231113`. Normaler Starterlauf
(SDL-Dummy) RM_0000 mit übersprungenem Vorspann: 700 Present, 350 Rendering,
349 Ticks, 0 blockierte Befehle. `tools/verify_fog.py` PASS gegen das
installierte Binary (`build/validation/fog-20261012/installed-clean`).
Benutzer-INIs unverändert.

Zwei Beobachtungen zum Arbeitsbaum, die nicht aus dieser Änderung stammen:

- `script_motion` (CTest) scheitert an „drag leaves the original motion's
  position range". Gemessen: mit und ohne die hier eingeführten Nachbarkeys
  (`RECVX_NO_MTN_TAIL`) identisch rot, und mit abgeschaltetem
  `EventVm::initialize_enemies`-Hook der parallelen Gegner-Sitzung ebenfalls
  rot. Keine dieser beiden Ursachen; HEAD selbst lässt sich nicht als
  Vergleich bauen (siehe nächster Punkt). Offen, nicht mir zuzuordnen.
- Commit `7023885` (Parallelsitzung, 21:42) enthält `x.set_effect=…` in
  `native/world_enemies.cpp`, also eine halb angewandte Änderung aus einer
  laufenden Sitzung: **HEAD übersetzt nicht**. Der Arbeitsbaum schon, weil die
  Korrektur (`s.set_effect`) dort liegt und uncommittet ist.

## Offen

Bildschirm-Clipping der Punktlichter (light.c:794-808, `ci->flg & 0x10`),
Lichttypen 7..10, Specular, `bhSetHalfLight` (0,5-Faktor für Gegner/Spieler),
Nebel auf `njDraw*`-Primitiven (Effekte; ps2_NaDraw.c:351/355 wählen FGE je
Aufruf), `sys->fog_cngct` aus `ss_flg` Bit 1 (kein Host-Besitzer; `fog_ct`
ist bei 2 und 3 ohnehin 0), `gm_flg 0x40`-Setzer (Augenkamera),
`sys->fog_col`-Override nur über Opcode 0x4c. Keine Vollport- oder
Durchspielbehauptung.
