# Qualität der Original-Engine-Portierung

Verbindlich für alle Portierungsarbeiten gemäß `AGENTS.md` (Nutzer, 2026-10-07).
Das Ziel ist die vollständige Original-Engine. Annäherungen, Ersatzlogik und
nachträglich erfundene Sonderfall-Fixes sind verboten.

## Abdeckung ist kein Qualitätsnachweis

`python3 tools/port_check.py` erfasst die nachgewiesene Port-Abdeckung nach
Bereichen und Originalfunktionen. Auch extrahierter Originalcode kann durch
Generatoränderungen, falsche Eingaben oder fehlende Dienste andere Semantik
haben. Prozentwerte sind deshalb kein Nachweis originalgetreuen Verhaltens.

## Prüfmethode bei jeder Portierung

1. Vor Änderungen `git status --short` lesen und laufende fremde Arbeit erhalten.
2. Den zuständigen Originalmechanismus finden: Funktion, Aufrufer, Dispatch,
   Tabellen und Daten. Bei unklarer Semantik die Disassembly heranziehen.
3. Original und Port einschließlich Generator und Host-Bindung vergleichen.
   Nicht beim Namen einer Originalfunktion oder einem kopierten Funktionskörper
   aufhören. Den tatsächlich ausgeführten Pfad bis zu seinen Diensten prüfen.
4. Zustände, Flags, Aufrufreihenfolge, Framefortschritt, Interpolation, Timer,
   Zufallsaufrufe, Ressourcenwahl und Seiteneffekte vergleichen. Auch Init,
   Freigabe, Abbruch, Raumwechsel und Wiedereintritt berücksichtigen.
5. Besonders nach festen Rückgaben, Leerfunktionen, Heuristiken, Host-Phasen,
   festen Zeitgrenzen, synthetischen Posen, pauschalem Blending und Filtern nach
   Raum-/Objekt-ID suchen. Treffer sind Hinweise; erst der Originalvergleich
   belegt eine Abweichung. Originale Sonderfälle mit Fundstelle übernehmen.
6. Plattformanpassungen auf ABI, Datenrepräsentation und technische Schnittstellen
   begrenzen. Eine andere Entscheidung oder ein anderer Zustandsübergang ist
   keine Plattformanpassung. Debug-Overrides müssen am Debug-Zustand ihren
   Ursprung haben; Originalroutinen dürfen keine eigenen Debug-Defaults besitzen.
7. Abweichungen mit beiden Fundstellen und der konkreten Verhaltensfolge
   dokumentieren. Belegte Fehler, offene Bindungen und ungeklärte Semantik
   unterscheiden. Keine Abweichung aus einem veralteten Kommentar allein ableiten.
8. Den gemeinsamen Originalmechanismus und seine Abhängigkeiten portieren.
   Keine neue Ersatzlösung hinzufügen. Offene Originaldienste ausdrücklich als
   Lücken behandeln und nicht mit einem vermeintlich harmlosen Standardwert
   überdecken.
9. Gezielt gegen originale Zustandsübergänge und Daten prüfen. Prüfungen müssen
   falsche Bindungen aufdecken können, nicht nur den neuen Code nacherzählen.
   Testaufstellungen und Debug-Overrides als solche kennzeichnen. Keine feste
   Host-Tickzahl als Erwartung übernehmen, wenn sie gerade die Abweichung ist.
10. Im Ergebnis Originalfundstellen, Änderungen, Prüfungen und verbleibende
    Grenzen angeben. Ein Build, Screenshot oder Einzel-Replay belegt weder
    vollständige Portierung noch vollständiges Durchspielen.

## Befunde vom 2026-10-07

Belegte Beispiele für diese Prüfung: HP-Auswahl nach Gegnerindex statt
`rand() % 16`; eigene Kampfphasen statt originaler Spielerzustände; pauschales
additives Effekt-Blending statt `bl_src`/`bl_dst`; feste Rückgaben und leere
Gegnerdienste; synthetisches Umkippen als Todesdarstellung. Diese Liste ist
eine Ausgangsbasis, kein vollständiges Audit und keine Erledigtmeldung.
Kistenschieben wird separat bearbeitet.

## Effektprozeduren: Übersetzer abgelöst (Nutzeranweisung 2026-10-14)

Die am 2026-10-14 zunächst freigegebenen, regex-übersetzten Originalprozeduren
(`native/effect_procedures.cpp`, Shim `native/effect_core.{h,cpp}`, Übersetzer
`tools/generate_effect_procedures.py`) sind auf spätere Nutzeranweisung vom
selben Tag („weg vom Generatorkram“) vollständig durch eigenständig
geschriebene Implementierungen ersetzt und entfernt worden:
`native/effect_sprite_procedures.cpp` (Prozeduren), `native/effect_sprite_procedure_tables.inc`
(reine UV-/Bild-/Pausentabellen mit Originalzeilen), `native/effect_sprites_internal.h`
(gemeinsame Dienste `effect_detail::{effinit, effset, setentry, request_template,
submit_template, latched_uv, quad_uv, quad_color, effect_random}`).

- `effect_detail::request_template()` ist `sys->ef`: eine prozessweite Vorlage,
  deren nicht beschriebene Felder vom letzten Schreiber stehen bleiben
  (bhEff101/103/104/015 u. a. schreiben nur einen Teil). Keine zweite Vorlage anlegen.
- Zufallsreihenfolge innerhalb eines Ausdrucks (`rand() - rand()`) ist aus der
  Disassembly belegt (bhEff152, 184, 246, 248: erster Aufruf ist der Minuend).
  `(short)(float)` der EE wird als `short_of()` (erst `int`, dann 16 Bit) nachgebildet.
- Nachträge der Originalerzeuger am neuen `eff[n]` (xn/yn/zn, tex_id, az, lkono,
  `exp0 = op`) laufen als Felder von `EffectSpawn` (`velocity`, `tex_id_override`,
  `az`, `lkono`, `parent_slot`); der Raumbesitzer trägt `slot` ein und löst
  `parent_slot` vor jedem Pass in `EffectSprite::parent` auf. `sys->ef_opq` ist
  `opaque_queued` (Paar 8/6, Tiefenschreiben). `cam.ax/ay/az` des Vorbildes stehen
  in `EffectContext::camera_angles` (bhControlEffect läuft vor bhControlCamera);
  `stage_no/room_no` kommen aus dem Raumnamen. `sys->winds/windr/thunder` haben mit
  `effect_weather()` einen Besitzer im Effektmodul (nur Effektprozeduren schreiben sie;
  Leser bhEff106/bhDrawThunder sind noch offen). CallBackGroundSe und bhCheckBombAtari
  laufen als `EffectOutput.background_sounds`/`bombs` zum Raumbesitzer.
- Prozeduren mit eigener Zeichenfunktion (`sys->ef_fnc`, `op->func`) reihen sich mit
  `queue_custom` ein und legen ihre Vierecke/Linien in `EffectSprite::quads`/`lines`;
  der Raumbesitzer zeichnet sie und überspringt für sie den `bhDrawTrsEffect3D`-Pfad.
  `bhSetExtraEffectWork` ist derselbe `eff[512]`-Pool (`set_effect_extra_allocator`,
  Platz mit `flg 2`), die Hilfsarbeitssätze liegen in `EffectSprite::extra`.
  Zeichenfunktionen, die auch Zustand fortschreiben (bhEff022/024/025/027/100/107),
  laufen hinter demselben Tor wie der Originalzeichenpass (`pt_flg 0x10`,
  game.c:338/426); Mehrfachdurchläufe je Bild (Spiegelpass) hat der Host nicht.
- Was eine sonst portierte Prozedur nicht erreicht, meldet sie benannt über
  `EffectOutput.gaps` an den Raumbesitzer, statt es zu überdecken (bhEff019 und
  das Raumlicht `rom->lgtp`). Der Ledger führt solche Prozeduren als `partial`.
- Das Rückseitentor `flg 0x20000` der vier Effektzeichenpässe (effect.c:1241/1374/
  1515/1651) ist portiert, zusammen mit den Ninja-Funktionen, die es braucht:
  `njGetPlaneNormal`/`njGetPlaneNormal2` (ps2_NaColi.c:301/314), `njProjectScreen`
  (ps2_NaMatrix.c:1572) und der Bildschirmzustand aus ps2_NaView.c. Die
  Sichtbarkeitsrichtung ist belegt, nicht gewählt: nach `cmat` (njSetView,
  diag(1,-1,-1)) ist `z` vor der Kamera positiv und `y` zeigt nach unten, und der
  Einheitsquad aus bhSetEffectTb ergibt ein negatives Kreuzprodukt, wenn seine
  lokale +z-Seite zur Kamera zeigt. Ein Billboard (0x100000) dreht die Reihenfolge
  mit `njScale(1,-1,-1)` um und kommt deshalb immer durch das Tor. `stflg 0x10`
  schreibt der Zeichenpass über `RoomWorld::effect_backface_` zurück, weil er
  `const` ist; einziger Originalleser ist bhDrawSmallScreenRenderTexture
  (screen.c:943) und der bleibt offen.
- `njDrawTexture3DHEx` (ps2_NaDraw.c:464) zeichnet mit der Grundfarbe `bcol` als
  Eckfarbe und setzt aus `ocol` des ersten Eckpunkts die Nebelfarbe genau dieses
  Aufrufs (`SceneTriangle::fog_color`). Das ist aus der Routine belegt, nicht
  geraten: `njDrawTextureSub3DH` liest nur `ocol`, und `njDrawTexture3DHEx`
  schreibt `bcol` dorthin.
- `sys->ef_ntx` ist derselbe Zeichenpfad ohne Textur, `sys->ef_pnc` derselbe wie
  `sys->ef_trs`: `njControl3D(0x4000)` liest der Texturzeichner (ps2_NaDraw.c:280)
  nie, nur die Chunk-Modelle (ps2_NinjaCnk.c) tun das. Der Unterschied ist die
  Reihenfolge der Durchgänge, nicht der Zeichenzustand.
- Nachweis je Gruppe: Differenzlauf Generat gegen Handfassung über Typen,
  Aufträge, mdlver und Passes vor dem Entfernen (Logs und Prüfprogramm in
  `build/validation/effproc-handport-20261014/`), dazu feste Tests mit
  Originalfundstellen in `tests/native/effect_room_sprites_test.cpp`.

**Befund am entfernten Generat:** der Übersetzer ersetzte `rand()` durch das
bereits normierte `effect_random()`, ließ aber `/ -2.1474836E9f` stehen, sodass
jeder Zufallsausdruck ≈0 ergab (Streuung fehlte, `>= 0.5f` war nie wahr,
Pausentabellen nahmen Index 0). Außerdem las er den Versatz `&op->xn` als
`(xn, zn, sx)` statt des Originallayouts `(xn, yn, zn)` (bhEff132). Beides ist
in der Handfassung korrigiert und in den Tests festgehalten.

- Eigene Zeichenfunktionen (`ef_fnc`) laufen in Platzreihenfolge mit einem
  durchgereichten Blendzustand: `DrawEff5SnowRect` setzt keins und erbt, `bhDraw107`
  hinterlässt 8/10, alle übrigen portierten stellen 8/6 zurück (Scan aller
  Zeichenfunktionen in effsub*/effect.c). `bhSetExtraEffectWork`-Plätze ohne
  Satzinhalt belegt `reserve_extra_slot`; die Schneebox hält elf davon.
- `sys->gas_py` (bhEff127) hat mit `effect_gas_height()` einen Besitzer im
  Effektmodul; nur diese Prozedur schreibt und liest sie. `plp->hp = -1` geht als
  `EffectOutput::player_killed` denselben Weg wie ein Biss (world.cpp:1142).

Noch nicht portiert bleiben die Prozeduren, die der Übersetzer nie ausgab
(`sys->`-Dienste, `cam`/`rom`/`eff`-Zugriffe, Partikel-/Modellpfade aus
effsub2/3/4); Liste und Dienstbedarf in
`work/native_port_effsub0_handport.result.md`.
