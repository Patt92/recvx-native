# Effektprozeduren: Handfassung statt Übersetzer (2026-10-14)

Auftrag (Nutzer): die regex-übersetzten Originalkörper in
`native/effect_procedures.cpp` schrittweise durch eigenständige native
Implementierungen ersetzen („weg vom Generatorkram“) und die noch
unportierten Original-Effektprozeduren benennen.

## Ergebnis

- **Neu:** `native/effect_sprite_procedures.cpp` mit allen 60 vormals
  übersetzten Prozeduren, die nicht schon in `native/effect_sprites.cpp` lagen:
  effsub0.c 150..164, 166..169, 171..173, 175..176, 178..180, 183..185,
  187..188, 190, 192, 194; effsub1.c 8, 10, 11, 14, 15, 29, 101, 103, 104, 116,
  117, 119, 121, 128, 131, 132; effsub5.c 206, 208, 214, 215, 222, 224, 231,
  241; effsub6.c 144. Dazu `native/effect_sprite_procedure_tables.inc` (reine
  UV-/Bild-/Pausentabellen mit Originalzeilen) und
  `native/effect_sprites_internal.h` (gemeinsame Dienste aus
  `native/effect_sprites.cpp`: `effinit`, `effset`, `setentry`,
  `request_template`/`submit_template` als `sys->ef`, `latched_uv`,
  `quad_uv`, `quad_color`, `effect_random`).
- `EffectUv` liegt jetzt in `native/effect_sprites.h`; `step_effect_sprite`
  prüft die Handtabelle `hand_effect_procedures[]` vor den älteren
  Schalterfällen (0/2/5/9/17/122/123/125/126/181/182/218/230). Der
  kontextfreie Aufruf `step_effect_sprite(sprite, &queued)` läuft für die
  Tabellenprozeduren (sie lesen keine Passflags) und lehnt die
  kontextabhängigen Schalterfälle weiterhin ab.
- **Entfernt:** `native/effect_procedures.cpp`, `native/effect_core.{h,cpp}`,
  `tools/generate_effect_procedures.py`; die toten Generat-Duplikate der schon
  handportierten Kennungen 2/5/122/123/181/182 damit ebenfalls. CMake-Ziel
  `recvx_game` bindet nur noch die Handfassung.

## Befunde am Generat (in der Handfassung korrigiert)

1. `rand()` → `effect_random()` (bereits normiert) bei stehen gelassener
   Division `/ -2.1474836E9f`: jeder Zufallsausdruck ≈0. Folgen: bhEff152/184
   ohne Streuung, bhEff154 deterministische Funkenart, bhEff159 fester Drift
   -sz/16, bhEff187/117 immer kürzeste Pause bzw. ct1 1, bhEff103/104/015/222
   ohne Lagestreuung und bhEff103 Art 0 feuerte immer, bhEff119 ohne Drift,
   bhEff123 (schon handportiert) Pause immer 20.
2. bhEff132 Art 3: der Versatz `&op->xn` wurde über das native Feldlayout als
   `(xn, zn, sx)` gelesen; das Original (types.h O_WRK 0x3C8..0x3D0) meint
   `(xn, yn, zn)`.
3. Die Reihenfolge zweier `rand()` in einem Ausdruck ist per Disassembly
   belegt (bhEff152 0x0021F538/0x0021F568, bhEff184 0x00221E60/0x00221E90:
   erster Aufruf ist der Minuend).

## Prüfung

- Differenzläufe Generat gegen Handfassung vor dem Entfernen (je Kennung
  Typen × Aufträge × mdlver × 40 Passes, gleicher Zufallsstrom; Zustand,
  Queue und Anforderungen verglichen), Logs und Prüfprogramm in
  `build/validation/effproc-handport-20261014/`:
  - effsub0 150..164: 210 Läufe, 178 identisch; Abweichungen nur Zufall
    (152/154/159) und die Tabellengrenze 153 (type ≥ 12, Handfassung meldet).
  - effsub0 166..194: 280 Läufe, 258 identisch; Abweichungen nur Zufall (184/187).
  - frühere Handports 181/182/122 identisch, 123 nur `ct2` (Zufallspause).
  - effsub1/5/6 (25): 334 Läufe, 279 identisch; Abweichungen nur Zufall
    (15/103/104/117/119/222). Das Generat stürzte für 11/14/15/119 mit Typen
    außerhalb der Tabellen (Nullzeiger) ab und für 29 ohne Link; die Handfassung
    meldet diese Fälle als Lücke bzw. gibt nur frei.
- Feste Tests in `tests/native/effect_room_sprites_test.cpp`
  (`effsub0_producers`, `effsub0_sprites`, `effsub0_batch_b`,
  `effsub1_batch`); `effect_room_sprites` und `effect_pool_lifetime` grün in
  `build/effproc-claude-20261014` (Vorherstand ebenfalls grün). Vollständiger
  Build (inkl. `recvx-runtime`) fehlerfrei; gesamter ctest-Lauf 82/83 grün
  (`build/effproc-claude-20261014/full-ctest.log`). Die eine rote Suite
  `script_motion` („visible wrap sampled frame zero instead of final frame 84“)
  ist fremder Vorherstand: dieselbe Meldung steht in sieben fremden Build-
  verzeichnissen seit 2026-10-13 13:43 (z. B. `build/stairs0050-claude-20261013`,
  `build/msgfreeze-claude-20261014`), der Test berührt keine Effekte.
- `python3 tools/check_no_original_core.py`: PASS.

## Grenzen

- Komponentennachweis auf dem Effektbesitzer; kein Durchspielnachweis. Der
  Zeichenpfad und die Texturadressierung über `tex_id` sind unverändert.
- Ziele, die die Erzeuger anfordern, aber selbst nicht portiert sind: 107,
  191, 193, 195, 207, 209, 216, 217, 223, 225, 232, 242 (sowie 10/12 als
  Ziele von 103/104 bereits portiert).
- Die geteilte Vorlage `request_template()` ist nur das `sys->ef` der
  Effektprozeduren; andere native Erzeuger (Gegner, Waffen, Skripte) füllen
  ihre `EffectSpawn` direkt.

## Noch nicht portierte Original-Effektprozeduren (Antwort auf die Frage)

Laut `work/effect_procedures.generator.md` 103 Prozeduren, die der Übersetzer
mangels Dienstfläche nie ausgab, dazu die Familien in effsub2/3/4 (bhEff300..
30c/349, bhEff_Sub350..373/398, bhEff_E00_*), die als Partikel-/Modellpfade
andere Besitzer brauchen. Benötigte Dienste nach Häufigkeit: `sys->gm_flg/
st_flg/sp_flg/pt_flg/cb_flg` (Passkontext, teils schon in `EffectContext`),
`sys->rpb/ufo_*/ond_*/wt_*/winds/thunder/gas_py/apos` (Raum-/Wetter-/Wasser-
zustand), `sys->ef_fnc/ef_tlist/ef_opq/ef_thl/ef_2d` (weitere Zeichenlisten),
`cam`, `rom`, `eff`-Pool-Zugriffe, `bhCheckBombAtari`, `bhGetGroundPosition`,
Arbeitsfelder `aox/aoy/aoz/axp/azp/pv/ofy`. Nach Häufigkeit in den bisherigen
Prüfläufen am dringendsten: bhEff352 (effsub4), 229, 107, 003, 130, 300..309,
138, 143, 001, 124, 370, 137, 111, 108, 235..249.

## Gruppe 2 und 3 (2026-10-14, Fortsetzung „geh das an“): nie übersetzte Prozeduren

**Neu portiert (51):** effsub0.c 165, 170, 177, 186, 189, 191, 193, 195, 196, 198;
effsub1.c 016, 018, 102, 105, 109, 113, 115, 124; effsub5.c 200, 201, 202, 203,
205, 207, 209, 210, 211, 212, 213, 216, 220, 223, 225, 226, 227, 228, 232, 233,
234, 235, 237, 238, 239, 240, 242, 244, 245, 246, 247, 248, 249; effsub6.c 147, 148.
Datentabellen in `native/effect_sprite_procedure_tables.inc` (Texelkoordinaten
der EFF0UV/EFF5UV-Typen), Ledger-Zeilen mit Fundstellen in `docs/port_ledger.tsv`.

**Neue Dienstfläche auf den vorhandenen Besitzern** (keine zweite Welt):

- `EffectSpawn` trägt die Nachträge der Originalerzeuger am neuen `eff[n]`
  (`velocity` = xn/yn/zn, `tex_id_override`, `az`, `lkono`, `parent_slot` = `exp0`);
  `initialize_effect_sprite` wendet sie an. `EffectSprite::slot` setzt der
  Raumbesitzer, `parent_slot` löst er vor jedem Pass in `parent` auf
  (effsub5 203/227/248 lesen den Elternsatz).
- `EffectSprite::opaque_queued` = `sys->ef_opq` (bhDrawOpqEffect3D: festes Paar
  8/6, Tiefenschreiben im Zeichenpfad). `EffectContext::camera_angles` =
  `cam.ax/ay/az` des Vorbildes (bhControlEffect läuft vor bhControlCamera),
  `stage_no/room_no` aus dem Raumnamen.
- `effect_weather()` ist der Besitzer von `sys->winds/windr(+b)` und
  `sys->thunder` (Schreiber bhEff102/105). `EffectOutput.background_sounds`
  (CallBackGroundSe) und `bombs` (bhCheckBombAtari) gehen zum Raumbesitzer.
- `wall_reflect()` = bhCheckWallRefAngle über `effect_wall`/`reflect_effect_wall`,
  `ground_height()` = bhGetGroundPosition, `request_direct()` = bhSetEffect.
- `ported_effect_atlas()` kennt nun alle Texturgruppen der Handfassung
  (31..43, 50..69, 80, 94, 408..431); ohne Atlas bleibt ein Sprite unsichtbar.

**Belege:** Disassembly für `rand()-rand()` in bhEff246 (0x0025B62C/65C) und
bhEff248 (0x0025C160/190), `(short)(float)` als `short_of()` (erst int, dann 16 Bit).
Feste Tests `group2_producers`, `group2_sprites`, `group3_services` in
`tests/native/effect_room_sprites_test.cpp`; `effect_collision_debris` und
`engine_progression_resources` führen 16/200/201/232 nicht mehr als Lücke.
Vollständiger Build grün, ctest 82/83 (nur die fremde `script_motion`,
Vorherstand seit 2026-10-13, siehe oben).

**Grenzen dieser Gruppen (als Lücke gemeldet, kein Ersatz):** bhEff238 auf einem
verlinkten Erzeuger und bhEff248 unter verlinktem Elternsatz brauchen den
Drehanteil der Linkmatrix; bhEff148 mit `lkwkp` ebenso; das Rückseitentor
`flg 0x20000` des Zeichenpfads ist nicht gebunden; bhEff203/223 mit `ct1 0`
(sz zu groß) melden statt durch 0 zu teilen; Leser von `thunder` (bhDrawThunder,
Raum 1-09) und `winds/windr` (bhEff106 Regen) sind noch offen; die Ziele 19,
108, 114, 221, 229 der neuen Erzeuger sind noch nicht portiert.

## Gruppe 4 (2026-10-14): Prozeduren mit eigener Zeichenfunktion

**Neu portiert (11):** effsub1.c 022, 024, 025, 027, 100 (mit bhEff106), 107;
effsub5.c 217, 236, 243; effsub6.c 143, 149. Diese Sätze reihen sich im Original
in `sys->ef_fnc` ein und zeichnen mit `op->func` statt über `bhDrawTrsEffect3D`.

**Neue Dienstfläche:**

- `EffectSprite::custom_queued` ist die `sys->ef_fnc`-Mitgliedschaft; der
  Raumbesitzer überspringt für solche Sätze den Sprite-Zeichenpfad und zeichnet
  stattdessen `EffectSprite::quads`/`lines`. `Quad` trägt jetzt Texturgruppe und
  Bild, Weltkoordinaten, Punktfilter, Nebelabschaltung und „ohne Textur"
  (`njPtclDrawPolygon`). `lines` sind die `njDrawLine3D`-Aufrufe (bhEff106).
- `bhSetExtraEffectWork` ist derselbe `eff[512]`-Pool: `set_effect_extra_allocator`
  reserviert einen Platz mit `flg 2`, `EffectSprite::extra` hält die 1248 Byte des
  Hilfsarbeitssatzes, `release_extra_work` gibt beides zurück. Die Originaltypen
  `ER_WORK`, `EXP_DRAW022_WORK`, `EXP_EFF025_WORK` und `P_WRK` liegen als
  Layoutstrukturen darüber (statisch auf ihre Originalgrößen geprüft).
- `set_effect_link_lookup` liefert `pp->mlwP->owP[n].mtx` (bhEff027 Gelenk 9,
  bhEff236 Gelenk 1), `set_effect_work_angle` liefert `pp->ay` als Werkfeld,
  nicht aus der Matrix gerechnet. `floor_effect()` ist `bhCheckFloorEffect`
  auf der Bodenliste des Raumbesitzers. `EffectContext` trägt zusätzlich
  `camera_position`/`camera_view` (cam.wpx.. und cam.vx.., camera.c:45–56).

**Zeitpunkt des Zeichenteils:** die Zeichenfunktionen von bhEff022/024/025/027/
100/107 schreiben auch Zustand fort (Spurzeile, Bildzähler). Das Original führt
sie in `bhDrawEffect` aus, das `sys->pt_flg & 0x10` (game.c:338/426) gated.
Dieser Port führt den Zeichenteil im selben Pass aus, aber hinter demselben Tor;
ohne Zeichenpass bleibt der Zustand wie im Original stehen. Mehrfachdurchläufe
je Bild (Spiegelpass, game.c:340/348) hat der Host nicht.

**Prüfung Gruppe 4:**

- Feste Tests `group4_custom_draw` in `tests/native/effect_room_sprites_test.cpp`:
  Hilfsarbeitssatz samt Freigabe, Zeichenausgabe als quads/lines, das Tor
  `pt_flg 0x10`, Bodenabfrage, Wandabprall, Gelenkbindung und Lebensdauern.
- Dauerlauf gegen echte Raumdaten (RM_0050, das 100/101/102/245 und die neuen
  Gruppen enthält): 600 Ticks ohne Ausnahme, lebende Sätze stabil bei 186–199
  von 512, Hilfsarbeitssätze konstant 7–8, keine neue Effektlücke. Quelle und
  Protokoll: `build/validation/effproc-handport-20261014/soak_rm0050.{cpp,log}`.
- Vollständiger Build grün, ctest 82/83 (nur die fremde `script_motion`).

**Zwei Testerwartungen korrigiert**, weil sie eine Eigenschaft prüften, die mit
dem jetzt belegten `bhSetExtraEffectWork` nicht mehr gilt: der Feuerzeugtest
hielt den freigegebenen Flammenplatz für dauerhaft frei. `bhSetEffectTb` und
`bhSetExtraEffectWork` nehmen aber beide den ersten freien Platz des geteilten
`eff[512]`-Satzes, und RM_0050 belegt ihn im selben Pass als Hilfsarbeitssatz.
Geprüft wird jetzt, dass keine Flamme mehr lebt und dass der neue Platz
tatsächlich der erste freie war. Die Lückenlisten in `effect_collision_debris`
und `engine_progression_resources` führen die inzwischen portierten Kennungen
nicht mehr.

## Gruppe 5 (2026-10-14): Explosionsball und die übrigen Zeichenlisten

**Neu portiert (5):** effsub1.c 012, 019, 023, 118; effsub5.c 204. Damit sind die
Ziele geschlossen, die schon portierte Prozeduren selbst anfordern: 012 von
bhEff104, 019 von bhEff018/002/015, 023 von `bhSetExplosion` (weapon.c:1759/1818).

**Zwei Zeichenlisten belegt statt geraten.** `sys->ef_ntx` (bhDrawNtxEffect3D,
effect.c:1165) ist derselbe Pfad mit `njDrawPolygon3DExSetData` statt
`njDrawTexture3DEx`, also ohne Textur. `sys->ef_pnc` (effect.c:886) ist derselbe
Pfad wie `sys->ef_trs`, nur unter `njControl3D(0x4000)` — und der Texturzeichner
`ps2_NaDraw.c:280` liest `_nj_control_3d_flag_` nie; nur die Chunk-Modelle
(`ps2_NinjaCnk.c`) tun das. Der Unterschied ist damit allein die Reihenfolge der
beiden Durchgänge, kein anderer Zeichenzustand.

**Neue Dienstfläche:**

- `EffectSprite::pn` (3 oder 4) steuert, ob der Zeichenaufruf ein Dreieck oder ein
  Viereck ist; `EffectSprite::gouraud` trägt eigene Eckfarben über den
  Gouraud-Kanal des Zeichenpfads (die Flächenfarbe hält das Alpha, die Eckfaktoren
  den RGB-Anteil). Ungleiches Eck-Alpha wird gemeldet statt gemittelt.
- `EffectOutput.gaps` meldet benannte Dienste, die eine sonst portierte Prozedur
  nicht erreicht hat, an den Raumbesitzer (`unbound_enemy_`). bhEff019 nutzt das
  für `rom->lgtp[0]`; der Ledger führt die Prozedur deshalb als `partial`.
- `set_effect_pool_lookup` gibt Zugriff auf `eff[n]` des geteilten Satzes, und
  `EffectSpawn::slot_sink_work/index` schreibt den von `bhSetEffectTb` belegten
  Platz in das `pv` des Erzeugers zurück (bhEff118 führt seine acht Kinder so).
- Texturgruppe 449 ist beim Laden auf die gemeinsame Textur 7 gebunden, wie
  bhEff204 sie über `sys->ef_tn[449] = 7` setzt.

**Prüfung Gruppe 5:** feste Tests `group5_rest`, dazu der erneute Dauerlauf
RM_0050 (600 Ticks, unverändert stabil) und der vollständige ctest-Lauf 82/83.
Die Lückenliste in `effect_collision_debris` führt 23 und 204 nicht mehr.

## Gruppe 6 (2026-10-14): H-Eckpunkte mit eigener Nebelfarbe

**Neu portiert (2):** effsub1.c 114 und effsub5.c 221. Beide zeichnen mit
`njDrawTexture3DHEx` und galten vorher als offen, weil der Host nur eine
Eckfarbe kennt. Die Routine selbst klärt das:

- `njDrawTexture3DHEx` (ps2_NaDraw.c:464) kopiert die Grundfarbe `bcol` jedes
  Eckpunkts nach `ocol` und setzt `bcol` auf 0x80; `njDrawTextureSub3DH`
  (ps2_NaDraw.c:415) liest dann ausschließlich `ocol`. Die sichtbare Eckfarbe
  ist also `bcol`.
- Zusätzlich setzt `njDrawTexture3DHEx` mit `Ps2SetFogColorSys` die **Nebelfarbe
  genau dieses Aufrufs** aus `ocol` des ersten Eckpunkts und stellt sie danach
  mit `Ps2SetFogColor` zurück. Der von beiden Prozeduren pro Bild berechnete
  Grauwert (Winkel zwischen Sprite-Normale und `cam.vx`, in einen Kosinus gelegt)
  ist damit ein Nebelton, keine Eckfarbe.

Dafür trägt `SceneTriangle` jetzt eine optionale Nebelfarbe, die der Renderer
anstelle der Szenenfarbe in dieselbe GS-Mischung einsetzt. Mehr war nicht nötig.

**Weitere belegte Punkte:** die vier H-Eckpunkte liegen im Original über
`op->pv` und `op->tv` (types.h 0x404 und 0x444, zusammen 160 Byte für 4 × 28 Byte)
und erreichen kein weiteres Feld. Die Bildwahl aus `mdlver` rechnet für Werte
unter 15 mit negativen Resten und Quotienten und ergibt negative UV-Zeilen; das
ist unverändert übernommen und im Test festgehalten. Der Zeitlupenzweig
(`st_flg 0x2000000`) hat in den Originalquellen keinen einzigen Schreiber, und
der Teiler `sys->ef_slow` (event.c:836, bhSysSet 19) ist im Host nicht gebunden:
wird der Zweig betreten, meldet der Port die Lücke statt zu teilen.

**Prüfung Gruppe 6:** feste Tests `group6_h_vertices` (Bildwahl in allen drei
Zweigen samt negativer Zeilen, Flugbahn, Grundfarbe nach Raum, Nebelton grau,
Freigabehöhe, beide Lücken), Dauerlauf RM_0050 unverändert, ctest 82/83.

## Gruppe 7: Rückseitentor und die Augenlichter (145/146)

**Korrektur zweier früherer Aussagen in diesem Bericht.** Beide waren falsch:

1. `njGetPlaneNormal2` ist **nicht** undefiniert. Sie steht in
   `ps2_NaColi.c:314` (und `njGetPlaneNormal` in `:301`). Die Kantenreihenfolge
   ist damit belegt und nicht geraten: `(p1-p0) × (p2-p1)`, nicht `(p2-p0)`.
2. „Kein offener Effekt steht statisch in einer Raumdatei" ist falsch. Ein Scan
   der Effektlisten aller 205 RDT (Zeiger `room+0x14`, Zahl `room+0x94`,
   Schrittweite 68, Kennung `u16` bei `+4`) findet von den offenen Prozeduren:
   **145** (RM_1002), **127** (13×), **219** (11×), **135** (10×), **110** (3×),
   **111/112/134/085** (je 2×), **021/133/137/138** (je 1×). Ohne statischen
   Eintrag bleiben 001, 006, 007, 013, 026, 120, 130, 136, 146, 229.

### Portierte Ninja-Funktionen

In `native/ninja_matrix.{h,c}`, mit Originalfundstelle im Kopf:

- `njGetPlaneNormal` (`ps2_NaColi.c:301`) und `njGetPlaneNormal2` (`:314`).
- `njProjectScreen` (`ps2_NaMatrix.c:1572`). Das Original rechnet
  `njMulMatrixCN(&NaViewScreenMatrix, m)` in die VU-Register und liest sie mit
  `njCalcPointCN`; nativ steht dafür eine lokale Matrix. Danach `x/y` durch `z`,
  mit `_nj_screen_.dist` skaliert und um `fNaViwOffsetX/Y` verschoben. Das `z`
  des Originals landet in einer **lokalen** Variablen (`sw t1, 8(%1)` zeigt auf
  `&Point`, nicht auf `p2`) und ist deshalb kein Ergebnis — die Aufrufer
  verlassen sich darauf, dass `pa.z` der Wert aus dem vorherigen `njCalcPoint`
  bleibt.
- Dazu der Bildschirmzustand, den sie liest: `njViewScreenMatrix`
  (`ps2_NaView.c:217`), `njSetScreen` (`:27`), `njSetAspect` (`:81`) und
  `njSetPerspective` (`:55`). Vorbelegt ist der Stand nach `system.c:429/443`
  (dist 500, 640×480, Mitte 320/240, Seitenverhältnis 1.174/1.0), also
  `fNaViwOffsetX/Y = 2048`. Ohne nativen Abnehmer bleiben `Ps2CalcScreenCone`,
  `vu1SetScreenProjection/Offset`, `fNaViwHalfW/H` und `ClipDispW/H`.

### Rückseitentor `flg 0x20000`

Alle vier Zeichenpässe (`effect.c:1241` Ntx, `:1374` Trs, `:1515` Pnc, `:1651`
Opq) haben dasselbe Tor: `njCalcPoint` der ersten drei Eckpunkte füllt `pa/pb/pc`
(davon bleibt nur `z` stehen), `njProjectScreen` überschreibt `x/y` mit dem
Bildschirmpunkt, `njGetPlaneNormal2` nimmt die Normale, `njUnitVector` normiert
sie, und `vec.z < 0` heißt Rückseite: `op->stflg |= 0x10` und nicht zeichnen,
sonst `stflg &= ~0x10` und zeichnen. Nativ in
`native/effect_sprites.cpp` (`backface_culled`, gerufen aus
`append_effect_sprite`), mit der Winkelweite aus `njSetPerspective(cam.pers)`
wie in `camera.c:28`.

Die Sichtbarkeitsrichtung fällt dabei heraus, sie ist nicht gewählt: nach
`cmat` (= `njSetView`, `diag(1,-1,-1)`, `ps2_NaView.c:119`) ist `z` vor der
Kamera positiv und `y` zeigt nach unten (`njClipZ(-2,-20000)` in `main.c:172`
belegt das Vorzeichen), und der Einheitsquad aus `bhSetEffectTb`
((-1,-1),(1,-1),(-1,1),(1,1)) ergibt damit ein negatives Kreuzprodukt, wenn
seine lokale **+z**-Seite zur Kamera zeigt. Ein Weltquad ist also von seiner
lokalen **−z**-Seite sichtbar — genau die Richtung, in die eine Figur blickt.
Für ein Billboard (`flg 0x100000`) dreht `njScale(1,-1,-1)` die Reihenfolge um,
weshalb es immer durch das Tor kommt; damit ist auch das Tor von **bhEff148**
(`flg 0x4120000`) jetzt gebunden statt nur vermerkt.

`stflg 0x10` wird zurückgeschrieben: der native Zeichenpass ist `const`, deshalb
merkt er die Plätze (`RoomWorld::effect_backface_`) und `bhControlEffect` trägt
das Bit im nächsten Bild vor den Prozeduren ein. Dazwischen liest es niemand;
einziger Originalleser ist `bhDrawSmallScreenRenderTexture` (`screen.c:943`) am
Rendertextur-Satz, und der gehört zur weiter offenen Gruppe 110/111/130.

### bhEff145 / bhEff146_Init / bhEff_Sub146 (effsub6.c:75/114/160)

- **145** ist ein unsichtbarer Führungssatz (`flg 0x8000` hat in den Quellen
  keinen Leser, der Satz steht in keiner Zeichenliste). Er hält `tv[0].col` fest
  auf `0xFFFF0000`; bei `mode1 == 2` blendet er mit `col += 0xF0000000` aus, bis
  das Alpha unter `0x10000000` fällt, dann `col = 0`. Danach ruft jeder Pass
  `bhEff146_Init`.
- **bhEff146_Init** legt je Pass zwei Augen an `eye_ofs` (±0.28, 0.82, −0.90) im
  Raum der Gelenkmatrix des verlinkten Werksatzes an. Die Vorlage `sys->ef`
  bekommt nur `id/type/flg/Skalierung 0.05/mdlver` und die Lage; alles andere
  bleibt vom letzten Schreiber stehen. Nach `bhSetEffectTb` trägt das Original am
  neuen Satz nach: `ax = atan2(m[6],m[10])`, `ay = asin(-m[2])`,
  `az = atan2(m[1],m[0])` (je `× 10430.381`), `ct3 = 0`, `exp0 = op`, `mode1`
  und `tv[0].col`. Dafür hat `EffectSpawn` jetzt `mode1` und `color`.
  Geklemmt wird nur `-m[2]` gegen Gleitkommawerte knapp jenseits von ±1 — ein
  numerischer Schutz, keine andere Entscheidung.
  Ohne aufgelöste Gelenkmatrix meldet der Port die benannte Lücke.
- **bhEff_Sub146** ist das Augenlicht: Textur 4 Bild 2, Blendpaar 8/10, Weltlage
  mit Rückseitentor (`flg 0x40A0000`). `ct2` ist der Ausblendschritt nach `type`
  (0x10/0x20/0x40 Mio.). Der erste Abzug geht von `0x00FF0000` aus und läuft
  dabei **unter Null** in ein hohes Alpha — das ist die Originalrechnung und
  unverändert übernommen. `mode1 != 0` zeichnet genau ein Bild und gibt den
  Platz frei.
- Das Original schreibt nach `bhSetEffectTb` auch dann, wenn `eno == -1`
  zurückkommt (`if ((eno != -1) || (eno < 512))` ist immer wahr), also in
  `eff[-1]`. Der native Weg stellt bei vollem Pool einfach keinen Satz; dieser
  Originalfehler ist nicht nachgebildet.

**Prüfung Gruppe 7:** feste Tests `group7_backface_and_eyes` (Reihenfolge der
Matrixmultiplikation in `njProjectScreen`, `njSetPerspective`, beide
Kantenreihenfolgen von `njGetPlaneNormal2` an einem Dreieck, das sie
unterscheidet, Tor am Weltquad in beiden Lagen, Billboard kommt durch, ohne
`0x20000` wird nichts verworfen, 145-Zustandsautomat samt Ausblendung und Lücke,
146 in allen drei `type`-Varianten). Dazu `eye_lights` am Originalraum
**RM_1002**: ein 145, zwei Augen im zweiten Pass, Dauerbestand sechs (drei
gezeichnete Bilder × zwei), und aus drei der vier Originalkameras kommen genau
zwölf Augendreiecke an, aus der vierten keines — das Tor greift auf echten
Raumdaten. Dauerlauf
`build/validation/effproc-backface-20261014/soak_rm1002.log` (600 Bilder,
27 Sätze stabil, keine Ausnahme).

## Gruppe 8: die Gaswand (bhEff127)

`effsub1.c:8720`. Sie zeichnet selbst nichts, sondern stellt den schon
portierten Rauch **128** und hält die Wandhöhe.

- Tor: `type 0/1` laufen nur mit einem Auftrag in `mode1` **und** solange
  `bhCkFlg(sys->ev_flg, 2)` nicht steht (`objitm.c:1791/1830` setzt es);
  sonst `mode0 = 0`. `type 2` läuft ohne beides.
- `mode0 == 0` setzt `sx = 2`, `ct0 = (op->idx & 3) * 4` (der eigene
  `eff[]`-Platz, nativ `s.slot`), `sys->gas_py = 0` und `gpy` aus
  `bhGetGroundPosition`.
- `type 0` stellt alle 16 Zählungen eine Rauchsäule (`flg 0x4000001`,
  Skalierung `sx/0/sx`) und wächst um 0.5 bis 4.
- `type 1` lässt `sys->gas_py` um 0.018 je Pass bis 17 steigen, erzeugt ab 0.5
  alle 8 Zählungen eine Wolke (jede 32. den Deckenzweig mit `type 1` und
  `flr_no = ct1`, sonst `type 2` und `flr_no = 2..3`), streut sie ±15 um die
  Wand und setzt `plp->hp = -1`, sobald `1 + plp->mlwP->owP[5].mtx[13]` unter
  der Wandhöhe liegt — außer bei `plp->stflg 0x40000`.
- `type 2` dasselbe, aber aus der eigenen Höhe statt aus `gas_py`.

Neue Bindungen: `sys->gas_py` hat mit `effect_gas_height()` einen Besitzer im
Effektmodul (nur bhEff127 schreibt und liest sie), `EffectContext::ev_flg_2`
kommt aus Flagmatrix 1 des Eventbesitzers, und `plp->hp = -1` geht als
`EffectOutput::player_killed` denselben Weg wie ein Biss (`world.cpp:1142`):
der Verlust als Schaden an den Spielerbesitzer, der Satz gilt als getötet.

**Prüfung Gruppe 8:** feste Tests `group8_gas_wall` (beide Tore, type 2 ignoriert
sie, Init-Werte, 16er-Takt und Wachstumsdeckel 4, Anstieg 0.018 mit 28 Passes bis
über 0.5, 8er-Takt mit 2 Decken- und 6 Wolkenzweigen in 64 Passes, Streuung,
Größe 50/50, `sy` in ±1, Tod über dem Kopf, Schutz durch `stflg 0x40000`,
Deckel 17) und `gas_room` am Originalraum **RM_7210** (drei Wände vom `type 2`,
sichtbarer Rauch). Dauerläufe über alle sechs Gasräume (RM_7210, RM_0150,
RM_9000, RM_9050, RM_9120, RM_9180) in
`build/validation/effproc-backface-20261014/soak_gas_rooms.log`: je 600 Bilder,
stabiler Bestand, keine Ausnahme. RM_0150 und RM_9000 (`type 0/1`) bleiben ohne
Skriptauftrag erwartungsgemäß still.

**Gesamtprüfung nach Gruppe 8:** `tools/check_no_original_core.py` PASS,
vollständiger Build fehlerfrei, ctest 82/84. Rot sind `script_motion` (fremd,
unverändert) und `enemy_en21_component`. Letztere war in diesem Baum nie gebaut
(das Ziel hängt nicht an `all`); nach dem Bauen zeigt sie, dass
`tests/native/en21_test.cpp` `x21.effect_request` nicht belegt, während
`native/enemies/en21.c:1088` es für `bhEne21_SetMarkEff` braucht. Beide Dateien
sind fremde, noch laufende Arbeit (nach dem Anfangsstand dieser Sitzung geändert)
und bleiben unangetastet. Dauerlauf RM_0050 unverändert
(`peak live 199 peak extra 8`, dieselben vier Lücken).

## Gruppe 9: Schneefall (bhEff219) mit der Schneebibliothek

`effsub5.c:1937` und die Bibliothek `effsub5.c:5579..6150`
(`CreateEff5SnowRect`, `DeleteEff5SnowRect`, `ExecEff5SnowRect`,
`DrawEff5SnowRect` und neun Setzer), alle eigenständig portiert. 11 Raumeinträge
in RM_5800/5840/6100/9000/9090, jeder Raum führt dazu die Texturgruppe 409 und
den Wind 102.

- **Pool:** eine Box mit 850 Flocken belegt im Original elf
  `bhSetExtraEffectWork`-Sätze: Kopf, neun Punktgruppen (8×100 + 50) und das
  Vektorfeld. Nativ liegen die Daten in `EffectSnow`
  (`native/effect_sprites_internal.h`), die Plätze bleiben im `eff[512]`-Pool
  belegt wie im Original (`reserve_extra_slot`/`release_extra_slot`). Nur ein
  Raumwechsel leert sie; bhEff219 gibt nie frei. Schlägt eine Belegung fehl,
  gibt `DeleteEff5SnowRect` alles schon Belegte zurück und der Satz endet.
  `lSnowMax <= 0` lässt den Kopf belegt — das ist die Originalfolge, der einzige
  Aufrufer übergibt 850.
- **Zufallsreihenfolge:** zuerst das Vektorfeld (100 Zeilen, je x/y/z), danach
  die Punkte (je x/y/z); beide stehen als einzelne Anweisungen, die Reihenfolge
  ist also aus den Quellen eindeutig.
- **Bewegung:** Wind aus `effect_weather()` (`winds·−sin(windr)/10`) plus
  `(0,−0.33,0)` plus der Vektor der Zeile, Umbruch je Achse mit `njFraction`.
  Die Flockenzahl folgt `ax/32760` und blendet ganzzahlig über 150 Passes über.
- **Zeichnung:** jede lebende Flocke ein Billboard der Größe `SnowModel`, Gruppe
  409 Bild 0, Punktfilter. Die Z-Drehung kommt aus dem Wind mit
  `y − 0.38000003` (nicht 0.33 wie beim Bewegen) unter `cam.mtx` des
  Zeichenpasses und wird deshalb erst in `append_custom_effect` gerechnet;
  type 2 dreht zusätzlich um Y nach `((int)((z+(x+y))/sz) % 4)·16384` — auch mit
  den negativen Resten von `%` — und hebt im Zeichenteil `sz` auf mindestens 1.
- **Blendzustand:** `DrawEff5SnowRect` setzt kein Blendpaar. Ein Scan aller
  Zeichenfunktionen in effsub*/effect.c zeigt, dass alle Vorgänger in der
  `ef_fnc`-Liste am Ende 8/6 zurückstellen — außer `bhDraw107` (Regeneinschlag),
  der 8/10 setzt und stehen lässt, und zwar am Anfang, also auch ohne Viereck.
  Nativ läuft deshalb ein Blendzustand in Platzreihenfolge über **jeden**
  eingereihten eigenen Zeichensatz (`world.cpp`, `fnc_blend`), Flocken erben
  ihn (`Quad::inherit_blend`), bhEff107 hinterlässt 8/10
  (`EffectSprite::fnc_exit_*`). Weiter offen mit derselben Eigenschaft:
  `bhEff_E11_SearchLightDraw`, `bhEff_E16_LaserSightDraw` (effsub2) und
  `bhEff_PtclSpriteDraw` (effsub4, Partikelpfad), alle unportiert.
- **Kamerabox (type 1/2):** sie liest `cam.wpx`/`cam.vx`. Vor dem ersten
  gelaufenen `bhControlCamera` hat der Host diesen Stand nicht; die Box meldet
  dann die benannte Lücke und legt sich ein Bild später an. Im Runtime-Pfad
  betrifft das genau das erste Bild eines Raumes (`world->tick()` läuft vor
  `tick_camera`).
- Die Obergrenze `sys->ef_fncn < 128` hat der Host nicht (keine der eigenen
  Zeichenfunktionen zählt sie).

**Nebenbefund am Besitzer, behoben:** `RoomWorld::reserve_effect_slot` konnte
`sprites_` mitten in einem Prozedurschritt vergrößern, während die Prozedur ihren
eigenen Satz als Referenz hält. Bei einem Hilfssatz (bhEff100/107) ging das
bisher zufällig gut, bei elf in Folge nicht mehr. `sprites_` hat jetzt feste
Kapazität 512 (`eff[512]` ist im Original ein festes Feld), auch nach dem
Effektstapel-Rücksprung (Skriptbefehl 0x71).

**Prüfung Gruppe 9:** feste Tests `group9_snow` (elf Plätze in
Belegungsreihenfolge, Gruppenaufteilung, Zufallsreihenfolge, erster
Anordnungs-/Bewegungsschritt samt Umbruch, Windschritt, 850 Billboards mit
Gruppe/Bild/Punktfilter/Größe/Farbe, ohne Zeichenpass keine Vierecke, Anteil 354
bei ax 13650, Überblendung auf 850 in 150 Passes, Klemme ax < 0, type 2 mit
sz-Anhebung und Y-Drehung samt negativen Resten, type 4 → 1, Kameralücke ohne
Belegung, voller Pool gibt alles zurück, Blendvererbung 8/6 bzw. 8/10 nach
bhDraw107 auch ohne Viereck, Winddrehung am Dreieck) und `snow_room` am
Originalraum **RM_9090** (zwei Boxen vom type 3 in 22 Hilfssätzen, Flocken in
der Szene, Kamerabox meldet ohne Kameralauf die Lücke). Dauerläufe in
`build/validation/effproc-snow-20261014/`: alle fünf Schneeräume je 600 Bilder
ohne und mit Kameralauf (mit Kamera bauen alle Boxen, Hilfssätze stabil bei
22/33), dazu RM_9090 über 3000 Bilder (Bestand sättigt bei rund 150 durch die
Kinder 203 des Erzeugers 202, kein Leck).

**Gesamtprüfung nach Gruppe 9:** `tools/check_no_original_core.py` PASS,
vollständiger Build fehlerfrei, ctest **83/84**. `enemy_en21_component` ist
inzwischen grün (die fremde Arbeit hat den Testdienst ergänzt). Rot bleibt allein
`script_motion` mit unveränderter Meldung („visible wrap sampled frame zero
instead of final frame 84"), gemessen mit frisch gebautem Testbinär gegen den
aktuellen Quellenstand.

## Gruppe 10: Effektlinks (bhSetEffectLink) und Nachbilder (bhEff135/136)

### room.c:787 bhSetEffectLink — bisher nicht portiert

Beim Raumladen hängt `bhSetEffectLink` jeden Raumeffekt mit `lkflg` 1..4 an
`plp`, `ene[lkno]`, `obwp[lkno]` oder `itwp[lkno]`: `lkno` über der Raumanzahl
wird 0; ist das Werk aktiv (`flg & 1`), bekommt der Satz `flg 0x80`, `lkwkp`,
`lkono` (auf `obj_num` begrenzt) und `lox = efp->lx`, sonst bleibt `flg 0x80`
aus. Nativ fehlte das ganz: verlinkte Raumeffekte folgten zwar dem Gelenk, aber
ohne `flg 0x80`, also ohne die Gelenkdrehung im Zeichenpass. Jetzt
(`native/world.cpp`):

- `flg 0x80`, `EffectSprite::link_kind/link_index` (= `lkwkp`), `lkono`, `lox`.
- `bhControlEffect` folgt dem Link nur noch bei `flg 0x80` (effect.c:800), und
  gezeichnet wird aus `op->px` bzw. der Linkmatrix wie `bhDrawTrsEffect3D`. Ein
  Raumsatz, der seine Lage selbst fortschreibt, wird jetzt dort gezeichnet
  (vorher blieb die Zeichenlage beim Ladeort stehen).
- Fehlt dem Host die Gelenkmatrix, meldet der Zeichenpass eine benannte Lücke
  statt das Bild abzubrechen.
- Offen: `lkflg 5` (`eff[efid[lkno]]`, benannte Lücke, in keinem Raum belegt)
  und das Rückschreiben von `efp->px` in die Raumtabelle (kein nativer Leser).

Gemessen an allen 63 Räumen mit verlinkten Effekten
(`build/validation/effproc-afterimage-20261014/link_old_vs_new.log`): sichtbar
ändert sich nur die Dauerleuchte **196** in RM_4010/4011/4012/8010. Sie stand
bisher senkrecht **in** der Konsolenfläche (|cos| 0,00 zur Normalen der 0,07
entfernten Fläche) und liegt jetzt fast flach auf (|cos| 0,87). 122 und 230
bleiben unverändert.

Mit `flg 0x80` werden drei bisher nie erreichte Linkzweige aktiv, alle portiert:
**bhEff238** (Startrichtung im Drehanteil der Gelenkmatrix, sonst Lücke ohne
Kind), **bhEff248** (Drehung beginnt mit der Gelenkmatrix des Elternsatzes) und
**bhEff148** (`lkwkp` bleibt nach dem Löschen von `flg 0x80` gesetzt, der Tropfen
folgt dem Gelenk über `owP[lkono].mtx * lox`, bis `mode1` den Fall auslöst). Der
alte Testfall „238 unter Link wirft" ist durch einen Test des Zweigs ersetzt.

### Nachbilder: bhEff135 / bhEff136 / bhDraw136 (effsub1b.c:621..760)

Zehn Raumeinträge in RM_1002, 5820, 7070, 9321, 9370; die Raumskripte schalten
sie mit 0x91 an und aus (Bytesuche in den Skriptbereichen).

- **135** ist unsichtbar. Beim ersten Lauf im Raum setzt er `sys->ef_flg 0x4`
  und belegt den 2-MB-Puffer `zan_memp` (`bhInitRoomChangeSystem` leert
  `ef_flg` je Raum, system.c:195). Mit Auftrag und `flg 0x80` reserviert er acht
  Schnappschussplätze der Größe `((vlist[1] >> 16) << 5) + 72`, auf 64
  aufgerundet — und zwar immer um einen vollen Schritt, auch bei glattem Rest.
  Passt das nicht mehr (`&zanp[n] >= zan_memp + 0x200000`), versucht er es jeden
  Pass erneut; `objP[lkono].model == NULL` bleibt still. Die Eckpunktzahl liest
  `model_object_vertex_count` (`native/scene.cpp`) aus dem ersten Eckpunktchunk
  des Gelenkmodells. Danach legt er je Pass ein 136 an (Ringplatz 0..5, Lage
  `pp->px`, Link auf dasselbe Werk) und löscht dessen `flg 0x80` gleich wieder.
- **136** hält im ersten Pass `objP[lkono]` von `mdl[mdlver]` in der aktuellen
  Lage fest. Das Original kopiert dafür die gehäutete `vlist` und die Matrix
  `owP[lkono]`; nativ ergibt dieselbe Weltlage `entity_geometry` — die
  Geometrie, mit der der Zeichenpass das Werk zeichnet, jetzt als gemeinsame
  Funktion aus `RoomWorld::append` herausgelöst. Danach zeichnet 136 fünf
  Passes über `ef_Zanzo[ct0]` (effect.c:938: von 5 bis 0, nach der
  ef_fnc-Liste; ein späterer Satz überschreibt den Platz) bzw. in Raum 1-00
  Fall 2, 5-82 und 9-32 Fall 1 über die ef_fnc-Liste, und endet.
- **bhDraw136**: `njSetConstantAttr(0, 0xA00)` mit `njControl3D(0x800)` macht
  jeden Streifen zu IS|UA — beleuchtet und mit dem Register seines
  Materialchunks gemischt. `npSetAllMatAlphaColor` setzt das Alpha
  `(ct0 + 1) * 40` = 200..40 in die Diffusmaterialien (Chunk 17/21: erste Farbe,
  19/23: beide). Das Licht ist das Simple-Licht: Farbe 1, Intensität 1, Umgebung
  0,8, Richtung −(0, 0.3, 0.7) im Kameraraum, Normalen unter `cam.mtx`
  (`njCnkCvVn`). Das bleibende Register des letzten Materialchunks gilt für die
  nächste ef_fnc-Funktion.
- **Dauerhafte Nebenwirkung:** `npSetAllMatAlphaColor` schreibt in das geteilte
  Modell des Werks. Gegner zeichnen nach `bhDrawEffect` und sehen das Alpha im
  selben Bild, Objekte/Items davor und sehen es ein Bild später (außer in den
  Albinoid-Räumen 7-10/3-06, system.c:1647). Knoten mit `evalflags 0x8`
  erreicht das Original nicht. Sichtbar ist das nur an Streifen mit UA; in den
  fünf Räumen hat das Quellgelenk keine.
- `sys->ef_flg 0x4` lässt `bhDrawEffect` `bhSetHalfLight` aufrufen. Das betrifft
  nur Chunkmodelle innerhalb des Effektpasses; `bhDraw136` setzt das Simple-Licht
  danach selbst vollständig, und `bhAllDrawModel` ruft `bhSetHalfLight` nach
  `bhDrawEffect` ohnehin (game.c:351).
- Historischer Stand dieser Runde: Spieler und `mdl[n > 0]` waren ungebunden.
  Die Aussage, die Lichtrechnung sei nicht aus Quellen belegbar, war falsch:
  `ps2_Vu1Strip.c:1654` enthält den Algorithmus. Aktueller Abgleich und Bindungen
  stehen im Abschnitt „Nachprüfung der Nachbildlücken“ unten.

**Prüfung Gruppe 10:** feste Tests `group10_afterimages` (Puffer, Reservierung
samt Rundung bei 0 und 100 Eckpunkten, 2-MB-Grenze mit `>=`, Modell NULL still,
unbekanntes Modell als Lücke, ohne `flg 0x80` nichts, sieben Aufträge mit
Ringplatz und Linkdaten, Schnappschuss, Alpha 200..40, Ende nach fünf Passes,
ef_fnc-Raum und Fall 1, fehlende Geometrie als Lücke) und `afterimage_room` an
**RM_7070** (ef_Zanzo) und **RM_1002** (ef_fnc): bhSetEffectLink verlinkt die
135er, je erzeugendem 135 sechs lebende Nachbilder, fünf Alphastufen 40..200,
Licht 0,8..1. RM_1002 Eintrag 17 hängt an `obwp[17]` Gelenk 0 ohne Modell und
bleibt wie im Original still. Numerischer Abgleich: die Schnappschussdreiecke
sind bei ruhendem Gegner lagegleich mit dem gezeichneten Gelenk
(`probe_afterimage.cpp`). Dauerlauf über alle fünf Räume
(`soak_afterimage.log`): Einschalten, nach 300 Bildern Ausschalten (nach 20
Passes kein Nachbild mehr), Wiedereinschalten, keine Ausnahme. Dazu die
Linkräume RM_4010/6100/9000/9050/9060/9120/9370 600 Bilder ohne Ausnahme; die
238-Lücken dort kommen von Werken ohne native Gelenkmatrix im Prüfprogramm.

**Gesamtprüfung nach Gruppe 10:** `tools/check_no_original_core.py` PASS,
vollständiger Build fehlerfrei, ctest **86/86** (auch `script_motion` ist mit dem
aktuellen Arbeitsbaum grün), RM_0050 unverändert stabil.

## Nachprüfung der Nachbildlücken (2026-10-14)

Der erneute Quellen-/Besitzerabgleich erfolgte ausdrücklich für den Effektkern,
unter Erhalt der parallelen Gegner-/Eventänderungen. Die erste Policyprüfung
meldete Stringliterale aus laufender EN13-Arbeit; diese wurden von der anderen
Arbeit zwischenzeitlich geändert. Kein Prüfer wurde abgeschwächt. Die Nachricht
zur Dateiabstimmung wurde wegen unterschiedlicher Sitzungsberechtigungen nicht
zugestellt; die Änderungen hier beschränken sich auf gezielte Nachbildstellen.

### Was von den drei alten Aussagen noch stimmt

1. **Material × Licht:** jetzt quellbelegt. `bhDraw136` setzt konstante Attribute
   `0xA00`, also Byteflags `0x0A`. Untexturierte Streifen nehmen
   `usCnkCsPolColorCalcFunc[2] = 4`, texturierte Simple-Streifen
   `usCnkCsTexColorCalcFunc[2][2] = 4` (`ps2_NinjaCnk.c:141/142/1759/1918`).
   Beide wählen `vu1GetVertexColorDifAmb` (`ps2_Vu1Strip.c:1654`):
   `RGB = Diffusmaterial * clamp(Licht + Ambient, 0, 1)`. Diffus-RGB wird für die
   ursprüngliche Texturmodulation mit 128 skaliert; der native dekodierte RGB-Pfad
   nutzt die entsprechende 255-Domäne. Alpha ist unabhängig davon
   `128 * Materialalpha` (`vu1SetAlphaRatio`, :197). Dafür gibt es jetzt den
   begrenzten nativen Algorithmus `cnk_afterimage_color` und feste Werteprüfungen.
   Keine VU-Ausführung oder Hardware-Nachbildung. Register-/Paketquantisierung
   des allgemeinen Renderers bleibt eine separate offene Abhängigkeit.
2. **Spieler/Zusatzmodelle:** kein pauschaler Ausschluss mehr. `bhEff135` liest
   jetzt die Eckpunktzahl des ausgewählten `mdl[n]`. Die Aufnahme zeichnet genau
   `objP[lkono].model`, auch bei verborgenem Knoten oder verborgenem Vorfahren;
   der vorherige Hierarchiedraw verlor solche Modelle. `sample_object_work`
   nutzt veröffentlichte physische `objP`-/`owP`-Daten, nicht Motion-Resampling.
   Der Spieler-Kopf-/Haarpass veröffentlicht Körper und `mdl[4]`-Anhang mit ihren
   eigenen Matrizen. Zusatzmodelle der Gegner können den bereits vorhandenen,
   berechneten Kindmodellbesitzer verwenden; ein Asset ohne veröffentlichten
   Baum bleibt ausdrücklich eine Lücke. Die Texturen kommen vom gewählten Modell.
   Veränderte Vertexströme (`live_model`, parallele EN13-Arbeit) bleiben Quelle.
3. **Sichtprüfung:** verbessert, aber weiterhin kein Durchspielnachweis.
   `Start.command` wurde mit isolierten Diagnose-INIs in RM_1002 ausgeführt,
   420 Bilder und Screenshot. Der Frame enthält Raum und Spieler; die 135er
   waren dort ohne Auftrag, also belegt er keine natürlich aktivierte Nachbildfolge.
   Zusätzlich erzeugt `visual_probe.cpp` einen A/B-Draw desselben Zustands mit
   Originalraumgeometrie: Diagnosekamera und eingeschaltete 135er, Quellkörper
   nur für diesen Vergleich ausgeblendet. Aus der Nordansicht ändern Nachbilder
   **33.017 Pixel**; beide PNGs wurden angesehen. Aus der ersten Gegenansicht
   waren sie vollständig verdeckt (0 Pixel), was als negativer Lauf erhalten ist.

### Weitere behobene Abweichungen

- Direkte Nachbildaufnahme erhält die Länge interpolierter Hautnormalen; das
  Original normiert diese im `npCalcSkin`-/Nachbildpfad nicht nachträglich.
- `npCalcSkinFM` (`njplus.c:2049/2140`) überspringt logische Knoten 11..14 und
  addiert bei Einflussnummern >10 vier. Nativ läuft die Übersetzung über die
  vorhandene Script-/Physical-Node-Tabelle, nicht über einen geratenen Arrayindex.
  Das war relevant für RM_7070: die dortige Figur setzt `mdflg 0x100`.
- Die Alpha-Schreibbedingung liest die veröffentlichten `evalflags 0x8` des
  gewählten Modells, nicht ausschließlich `WorldEntity::hidden_nodes`.
- Spieler-Neubindung löscht veröffentlichte Modellhandles. Ein nicht
  veröffentlichtes Zusatzmodell bekommt weder Einheits- noch Bindpose als Ersatz.

### Prüfstand und Grenzen

- Vorherstand gemessen: `./build.sh --test`, **86/86**. Die parallele Gegnerarbeit
  ergänzt währenddessen zwei Suiten; aktueller Lauf **88/88**, einschließlich
  `script_motion`, Effekt-, Actor-, Haar-/Kopf- und EN13-Suiten.
- Feste Tests: Farbrechnung/Reihenfolge, direkter versteckter Modelldraw,
  unnormierte Halbnormale, Skin-FM-Interpolation, veröffentlichter Spieler und
  `mdl[4]`, unveränderte Aufnahme nach Änderung des nächsten Motionframes und
  der Workposition, fehlender Zusatzmodellbaum und Neubindung.
- Dauerläufe aller fünf ursprünglichen Nachbildräume, je 600 Bilder mit ein/aus/
  wieder ein: gleicher stabiler Bestand, nach Abschalten 0 Nachbilder,
  keine Ausnahme. Fremde Gegnerlücken bleiben im Log sichtbar.
- Build ausschließlich `./build.sh`, `build/macos-arm64`, Start.command-Dev-App
  aktualisiert/signiert. arm64 und Signatur geprüft. Kein Paket, Commit oder Push.
- Offen bleiben nicht veröffentlichte Zusatzmodellbäume, die modifierartigen
  Chunk-56..58-Modelle im allgemeinen Modelldecoder, vollständige natürliche
  Aktivierung/Sichtabnahme und einige allgemeine Zeichennebenwirkungen (z. B.
  letzter Materialzustand ohne zeichnenden Streifen). Die Nachbilder sind deshalb
  im Ledger **partial**, nicht vollständig erledigt.

Belege: `build/validation/effproc-afterimage-recheck-20261014/`,
`tests/native/actor_test.cpp` und `tests/native/effect_room_sprites_test.cpp`.

## Verbleibende nie portierte Prozeduren (Stand nach Gruppe 10)

18 in effsub0/1/1b/5/6, dazu die Familien in effsub2/3/4. Der Scan der
Effektlisten aller 205 RDT (siehe Korrektur oben) sagt, welche davon überhaupt
statisch vorkommen; das ist die Reihenfolge, in der sie es wert sind.

- **Statisch in Räumen, offen:** 110 (3×) und 111 (2×, Rendertexturen
  `ren_*`/`ef_tex`/`memp`), 112 (2×, Spiegel `mr_pl`, `st_flg 0x100`), 085 (2×,
  tauscht `textures[3].texaddr` eines Gegnermodells), 021 (1×, 2D-Balken),
  133/134/137/138 (`ond_*`, `pad_ps`, `ufo_*`, `fade_pbk`: Ereignissteuerung;
  bhEff134 bearbeitet inzwischen eine parallele Sitzung in `native/world.cpp`).
- **Zeichenzustand, den der Host nicht führt:** 001 (`ef_mdf`, Schatten;
  Besitzer `tick_shadow` partial), 006 (`ef_opq` mit eigener Matrix und pn 3,
  liest die vom Zeichenpass geschriebene `op->mtx`), 021 (`njDrawPolygon2D` und
  `cb_flg 0x40` — vier Bildschirmrechtecke in 640×480, der Host hat keinen
  2D-Kanal in `Scene`), 026 (2D-Liste `ef_trs2d`, `bhGetGidx`).
- **Fehlender Erzeugerkontext:** 229 wird nur von bhEff228 ohne Linkzeiger
  erzeugt, seine Zeichenfunktion liest aber
  `op->lkwkp->mlwP->owP[lkono].mtx`.
- **Raum-/Systemzustand ohne nativen Besitzer:** 007 (`ef_pol`, `obwp`,
  Waffen-SE), 013 und 130 (Bogen/Armbrust: `ahtp`/`apos`, `cam`/`ene`/`plp`,
  Trefferdienste), 108 (Wasseroberfläche `wt_*`), 120 (`npSetOffsetUV` am
  Linkmodell).
- **effsub2/3/4:** Partikel- und Modellfamilien (bhEff300..30c/349,
  bhEff_Sub350..373/398, bhEff_E00_*) mit eigenen Besitzern; 300/302 liegen
  bereits in `native/effect_particles.cpp`.
- `bhEff003` bleibt als Pooleintrag offen; der Kampfpfad zeichnet dieselbe
  Waffenmündung bereits über `native/weapon_flash.cpp` (`spawn_weapon_flash`).
