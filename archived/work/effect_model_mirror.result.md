# bhEff_ModelDraw – Spiegelzeichnung

## Ergebnis und Arbeitsgrenzen

Die Spiegel-Lücke war aktuell, kein veralteter Ledgervermerk: `native/world.cpp` übersprang den zweiten Aufruf für Effekt 364 ausdrücklich und meldete `bhEff_ModelDraw-Modell ohne Hostpfad`. Die Steuerung von `bhEff_Sub364` war dagegen bereits richtig und wurde nicht doppelt implementiert.

Eigene produktive Änderungen ausschließlich:

- `native/effect_sprite_procedures.cpp`: Kommentar vor `step_eff364` präzisiert; keine Verhaltensänderung an der Steuerprozedur.
- `native/effect_model_mirror.h`: eigener fokussierter Helfer für Sichttor, lokale Zeichenmatrix, SimpleMulti-Metadaten und das verbleibende Material-Blendpaar.
- `tests/native/effect_model_mirror_test.cpp`: fokussierte Mechanismus-/Raumbesitzertests.
- `archived/work/effect_model_mirror.integration.patch`: präziser Unified Patch für die fremd besessenen Dateien `native/world.cpp`, `native/world.h`, `CMakeLists.txt`.

Der Hauptprozess hat diesen Patch inzwischen integriert; `git apply --reverse --check archived/work/effect_model_mirror.integration.patch` war danach erfolgreich. Nicht erneut anwenden. Ich habe die fremden Besitzerdateien nicht direkt editiert. Andere Änderungen des Hauptprozesses/anderer Arbeit, INIs, Mods, saves und snapshots blieben erhalten. Keine Agents, Worktrees, Commits, Pushes, PRs oder App-/Paketänderungen.

## Originalbelege

Alle folgenden Dateien liegen unter `src/ps2/veronica/prog/`, ausschließlich als gelesene Semantikreferenz verwendet.

| Fundstelle | Entscheidend |
|---|---|
| `effsub4.c:1774–1809` | `bhEff_Sub364`: Init setzt 8/10, `flg |= 0x240000`, `ani_ct=0`, `ct0=1`; Fallthrough dekrementiert auf 0; nächster **Steuerpass** löscht den Satz. Lebende Sätze werden in `ef_fnc` eingereiht. |
| `effsub4.c:3306–3389` | `bhEff_ModelDraw`: kein `bhPutModel`, kein eigener zweiter Spiegelaufruf. |
| `effsub4.c:3319` | Ausschluss durch `flg/stflg & 0x1000000` oder **Effekt**-`mdflg & 0x40` bei `gm_flg & 0x4000`. Kein zusätzliches `flg == 0`-Tor innerhalb der Zeichenfunktion. |
| `effsub4.c:3325–3348` | Filter 0/1 nach `flg 0x20000000`; ohne `stflg 0x20` gemeinsame `ef_tlist` ab `tex_id + ani_ct`, sonst `txp[0]`; Texturindex zunächst 0. |
| `effsub4.c:3353–3374` | `flg 0x200000` hat Vorrang vor Linkbit 0x80; nicht verlinkt `T(px)*Rz*Ry*Rx`, sonst bereits berechnete `owP[lkono].mtx` mit lokalem Versatz und Drehung; anschließend Skalierung. |
| `ps2_NaMatrix.c:1759–1775` | `njRotateEx(...,0)` ruft Z, Y, X in dieser Reihenfolge auf. Kein Billboard-Reset wie bei manchen Spritezeichnern. |
| `effsub4.c:3376–3381` | Bei `flr_no != 0` erst `npSetAllMatColor` am echten Modellstrom, danach direkter `njCnkSimpleMultiDrawModel(objP[mdlver].model)`. |
| `njplus.c:1375–1468` | Materialfarbe bleibt am Modell stehen; RGB wird geschrieben, Alpha nur falls das vorhandene Alphabyte nicht 0 ist. Objekte mit evalflags 0x8 werden beim Farbschreiben übersprungen. |
| `ps2_NinjaCnk.c:179,656–661` | `njCnkSimpleMultiDrawModel` wählt Modus 3. Die vorherigen `njCnkSetSimpleLight*` in ModelDraw ändern nur Modus 2; 0.8 ist deshalb **nicht** das Umgebungslicht dieser Modelle. |
| `ps2_NinjaCnk.c:505–538,1213–1274` | Multi-Lichtmatrizen aus Kameramatrix; Normalen bleiben unnormalisiert, kein erfundenes Flächennormalen-Fallback. |
| `ps2_NinjaCnk.c:867–875` und Materialfunktionen | Blendzustand wird auch von einem Chunk ohne nachfolgenden Streifen geschrieben. Nur das letzte Dreieck zu lesen verliert diese Schreibzugriffe. |
| `game.c:77–89,402–427` | `st_flg 0x100`: `gm_flg |= 0x4000`, gespiegeltes `cam.mtx`, `bhSetLight`, `bhEtcMirrorDrawModel`; dessen Effektaufruf nur bei `pt_flg 0x10`. Danach Matrix/Bit wiederhergestellt. |
| `game.c:320–348,402–427`, `system.c:1647–1657` | Reguläre Effekte mit `amb_chr`; früher Albinoid-Effektpass in den original benannten Räumen 7/10 und 3/6 mit `amb_rom`; Spiegel-Effektpass ebenfalls `amb_rom`. Dies sind belegte Originalausnahmen, keine erfundenen Raumspezialfälle. |
| `effect.c:923–935` | `ef_flg 0x4` stellt vor der `ef_fnc`-Liste halbes Licht ein; Liste wird in Reihenfolge abgearbeitet. |
| `MdlPut.c:18–118` | `bhPutModel` hat einen **anderen**, internen zweiten Zeichengang unter `st_flg 0x100` und prüft den **Gegner**-Work `mdflg 0x40`; auf ModelDraw zu übertragen würde doppelt spiegeln/falsch filtern. |
| `en22.c:2658–2695` | Acht 364-Sätze, `mdlver=1`, zufälliges `epw->mdl[1..6]`, eigene Texturliste, `flr_no=0`, Winkel je +8192. |
| `en17.c:1689–1720` | Steve-Beilnachbild: `mdl[5]`, `mdlver=1`, eigene Texturliste, Winkel aus übergebener berechneter Matrix, `flr_no=1`, `tv[0].col=argb`. |

Die Suche nach `bhEff_ModelDraw` im Original ergab nur die private Registrierung durch Sub364. Die zwei benannten 364-Erzeuger sind die EN17-/EN22-Wege. Beide setzen Sub364s `0x200000` und benutzen daher nicht den offenen Linkzweig.

## Integrationsänderungen

1. Der existierende zweite `bhDrawEffect`-Durchlauf ruft `append_effect_model` erneut auf, statt eine Lücke zu melden. Es gibt keinen zusätzlichen Kopier-/bhPutModel-Pass. Geometrie bleibt in Weltlage; der bestehende Renderer benutzt `cam.mtx * scene.mirror_matrix` für `mirror=3`. Materialphasen 6/10 und Modifier-Reihenfolge bleiben an den vorhandenen Szenenbesitzern.
2. `gm_flg` wird ausdrücklich an ModelDraw übergeben. Das Sichttor liest Effekt-`mdflg`, nicht den Herkunftsgegner. Verborgene Sätze verändern weder Geometrie noch Material-/Blendzustand.
3. Matrix wird ausschließlich aus bestehenden Effektfeldern gebaut. Keine Motionabfrage, kein neuer Knotenbaum, kein Next-Frame-Sampling.
4. Materialfarbe wird am bestehenden `WorldEntity::live_model/live_models` geschrieben. Deren `mutable` kennzeichnet den bereits im const-Zeichenpfad original nötigen Materialschreibzugriff, keine zweite Modell-/Workwelt. Jedes Decode hält die Farbe seines eigenen Listenaufrufs fest; der letzte Wert bleibt für folgende Aufrufe stehen.
5. Gemeinsamer Texturlistenzweig und Point/Linear-Auswahl ergänzt. SimpleMulti-Streifentexturcache wird über die vorhandene native Route gewählt. Das Arbeits-Blendpaar 8/10 wird nicht pauschal über das Modellmaterial gestülpt. Ein expliziter Materialstrom-Walk erhält auch nachlaufende/material-only Blendänderungen.
6. Zusätzlich bestätigter Fehler: `lit_triangle_count` wurde **vor** der ef_fnc-Erzeugung gespeichert. Daher blieben 364-Modelle bislang trotz Lichtkategorie 4 ganz außerhalb der Beleuchtung. Nur ModelDraw markiert seine Modelle intern mit Kategorie 5 und nimmt sie in den späten Lichtlauf auf. Andere Sprites, bereits beleuchtete Nachbilder und andere ef_fnc-Ausgaben bleiben unverändert. Am Ende steht wieder die öffentliche Kategorie 4 (bestehende EN17-/EN22-Tests). Multi-Licht mit richtiger Ambientkategorie; Null-/nichtnormierte Modellnormalen bleiben unverändert.

## Prüfstand

Anfangsauftrag: **kein Build**, nur Syntaxprüfung. Entsprechend waren Header/Test/Integrationspatch zunächst ausschließlich mit `-fsyntax-only` geprüft und bis zum zentralen Hauptbuild **ungetestet**. Anschließend autorisierte der Hauptprozess ausdrücklich exklusiv das Ziel `recvx-effect-model-mirror-tests` einschließlich Ausführung und Gegenproben. Nur dieses Ziel wurde von mir gebaut; vor **jedem** Build lief der unveränderte Policychecker.

- Anfang und Ende: `python3 tools/check_no_original_core.py` **PASS**.
- `python3 tools/port_check.py --domain Effekte --top 0`: Exit 0; keine Ledgerdatei beschrieben. Zahlen sind kein Semantik-/Vollspielnachweis.
- Eigene Prozedurdatei, Test und vorgeschlagener world-Patch mit echten arm64-Flags aus `build/macos-arm64/compile_commands.json`: **Syntax grün**. Der erste Scratchlauf fand doppelte `world.h`-Definitionen aus gemischten Original-/Scratchincludes; konsistente lokale Headerkopien für `combat.h/targeting.h` beseitigten ausschließlich dieses Prüflayoutproblem.
- Integrierter fokussierter Test: **grün**, `build/validation/effect-model-mirror/restored.log`.
- `git diff --check`: grün. Reverse-Apply-Check bestätigt bereits erfolgte Integration.

### Testfälle

- Sichttore `flg`, `stflg`, effektlokales `mdflg` mit/ohne `gm_flg 0x4000`; explizit kein `flg==0`-Tor innerhalb des Zeichners.
- Unabhängige komponentenweise Matrixerwartung für nichtkommutierende Drehungen und nichtuniforme Skalierung; Billboardbit darf keine Drehung entfernen; fehlende Linkmatrix darf keine Ersatzpose erzeugen; `0x200000` übersteuert 0x80.
- Normal-/Spiegelphasen und Filterwahl, keine pauschale Material-/Alpha-/Blendüberschreibung.
- Nachlaufendes NJD_CB_BA ohne Dreieck bestimmt den bleibenden Blendzustand.
- Steve in Originalraum RM_9280 über bestehenden EN17-Nachbilderzeuger, echter `tick_after_player` publiziert die Effektliste; sein Herkunftsgegner trägt absichtlich `mdflg 0x40`, der Effekt nicht. Zweiter Aufruf bleibt sichtbar. Effekt-`mdflg` schaltet nur den zweiten Aufruf ab. Beide liefern identische ungespiegelte Weltvertices/-normalen an den Renderer, Phasen 6/10 und Spiegelart 0/3.
- Persistenter Steve-Materialstrom; RGB vor Decode gesetzt; Wechsel `flr_no 1 -> 0` behält die Farbe.
- owP aller Herkunftsknoten vor/nach Draw byteweise floatgleich; ct0 wird durch mehrfaches Draw nicht weitergezählt; nächster Steuerpass gibt 364 frei.
- Albinoid RM_7100 über echten `Brain00 -> MV04 -> SparkEffect -> SetDengekiEffect2` mit acht Blitzmodellen. Nur Freigabe-/Spielerbodenbedingungen sind Diagnosevorgaben.
- Numerisches MODE-3-Licht: dynamische Raum-/Eventlichter per echtem Opcode 0x35 abgeschaltet; vier verschiedene nichtweiße Ambientpaletten per 0x44. Erwartung ist Ambient des richtigen Slots multipliziert mit jeweiligem Chunk-Ambientmaterial. Getrennte Prüfung Steve normal/spiegel und Albinoid früher/spiegelnder Effektpass; nicht nur world_lit/gouraud-Metadaten.

### Gezielt ausgeführte Gegenproben

Alle Änderungen ausschließlich temporär im eigenen Header, anschließend bytegenau restauriert und erneut gebaut/getestet.

| Gegenprobe | Resultat/diagnostischer Testabbruch | Log unter `build/validation/effect-model-mirror/` |
|---|---|---|
| Effekt-mdflg-Tor entfernt | ROT: `gm_flg 0x4000 must test effect mdflg 0x40` | `drop-effect-mdflg.log` |
| Rz/Ry/Rx durch Rx/Ry/Rz ersetzt | ROT: `T * Rz * Ry * Rx * S order; no billboard rotation reset` | `wrong-matrix-order.log` |
| Kategorie-5-Lichtmarker entfernt | ROT: `late MODE-3 models must enter lighting` | `omit-late-model-light.log` |
| Spiegelart stets 0 statt 3 | ROT: `second effect phase metadata` | `omit-mirror-routing.log` |
| NJD_CB_BA-Schreibzugriff ignoriert | ROT: `trailing NJD_CB_BA must survive without a triangle` | `omit-trailing-material-write.log` |
| Unveränderter Endstand | GRÜN: `effect model mirror passed` | `restored.log` |

Wichtiger Prüfhinweis: Der erste schnelle Mutationslauf hatte wegen Make-mtime-Granularität die Test-TU bei zwei aufeinanderfolgenden Headeränderungen nicht neu kompiliert. Diese Ergebnisse wurden **nicht** als Beleg verwendet. Matrix-/Blendprobe wurden mit explizitem Touch und frischer Test-TU erneut ausgeführt; die oben genannten Logs sind die gültigen Wiederholungen.

## Grenzen / weiter offen

- Kein App-/Start.command-Paket ausgeliefert und keine reale Bild-/Durchspielabnahme. Die künstlich aktivierte Spiegelebene in RM_9280/RM_7100 ist ein Diagnoseaufbau, nicht ein im Original natürlich gespielter Spiegelraum.
- Der generische Linkzweig `flg 0x80 ohne 0x200000` bleibt in RoomWorld benannt offen. Der Helfer kann eine bereits berechnete Matrix verketten, aber die beiden überprüften Originalerzeuger erreichen diesen Zweig nicht. Keine neue Pose und keine Ersatzmatrix dafür eingeführt.
- Echte Bildschirm-/Modellkugel-Verwerfung vor nachlaufenden Materialschreibzugriffen ist vom bestehenden allgemeinen Modelldecoder nicht vollständig erfasst; der neue Walk behauptet keinen vollständigen Clip-/Materialregisterautomaten. Die Blendprobe belegt den nicht verworfenen material-only-Fall.
- Die globalen Simple-Einzellichtschreibzugriffe vor dem Sichttor von ModelDraw können nachfolgende Modus-2-Zeichner beeinflussen. Für diese Modus-3-Modelle selbst sind sie wirkungslos; die komplette globale Lichtregisterfolge über sämtliche ef_fnc-Zeichner ist hier nicht nachgebildet.
- Fehlende Ressourcen, offene Modellbesitzer/Linkfälle und die bereits benannten undefinierten `njMirror`-Zweige bleiben Diagnosen. Keine Hardware-/CPU-/VU-/GS-Nachbildung, kein Originalkern, keine generierten/kopierten Funktionskörper kompiliert oder eingezogen.
- Ledger/STATUS gehören dem Hauptprozess. Dort nach zentraler Regression den bislang als offen benannten Spiegelpfad aktualisieren, die genannten generischen Grenzen erhalten. Die originalen Fehlzweige `eff[-1]` bei vollem Pool der Erzeuger wurden nicht geändert.
