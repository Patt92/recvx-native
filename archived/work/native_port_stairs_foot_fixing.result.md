# Treppenaufstieg: Fußfixierung der Spielerarbeit (RM_0050 und alle Räume)

## Nutzermeldung

Im Raum der ersten Hunde (RM_0050) wirft die kleine Treppe die Figur beim
Hochsteigen zurück und setzt sie in die Luft: die Figur läuft unten weiter,
die Kamera bleibt oben verankert.

## Ursache, am Originalcode belegt

Der Aufstieg `bhCPM2_act_kdu` (player.c:2872) bewegt die Figurenlage selbst
fast nicht. Die gesamte Vorwärts-/Hubbewegung steckt in der Wurzelbewegung der
Steigmotion: Motion 35 von `SYSTEM/0010..0014` führt Objekt 0 über ihre 27
Bilder von (-0.01, -1.59, 0.00) nach (-0.01, 6.41, -11.99), also acht Einheiten
hoch und zwölf nach vorn (gemessen aus den Originaldaten, nicht geschätzt).

Diese Wurzelbewegung wandert im Original über die **Fußfixierung** in die
Arbeitslage: `plp->flg |= 0x8040000` am Übergang in die Abschlussphase
(player.c:3033), danach ruft `bhControlPlayer` für jedes Bild dieser Phase
`bhFixPositionXYZ(plp, PlyLegRoute[0])` (player.c:1538-1558, Motion.c:857/868).
`bhCalcFixOffset` zieht die Verschiebung des Kettenendes zwischen der jetzt
abgetasteten Pose und der Pose, mit der `owP` zuletzt berechnet wurde, von
`plp->px/py/pz` ab — der aufgesetzte Fuß bleibt stehen, die Figur rückt vor.

Der native Treppentreiber hatte diesen Zweig nicht: `native/player_stairs.c`
setzte das Bit, aber niemand verbrauchte es. Damit endete der Aufstieg 1.06
Einheiten vor dem Start (nur `bhAddSpeed` aus kdu case 3), also mitten im
Typ-1-ATR-Riegel der Treppe (RM_0050 ATR 47: x -185.3..-183.31, y 0..10). Der
nächste gewöhnliche `bhCheckWallEx`-Pass löste diese Überdeckung nach Westen
auf, während `plp->py` die obere Etagenhöhe 8 behielt: Rückstoß über die
untere Fläche, Kamera auf dem Schnitt der oberen Etage. Genau die Meldung.

Zweiter, kleinerer Befund an derselben Stelle: `bhSetMotion` tastet `objP` am
aktuellen `plp->frm_no` ab und schiebt **danach** vor (Motion.c:70-108); das
anschließende `bhCalcModel` zeichnet also die abgetastete Pose. Die Laufzeit
übergab dagegen den schon vorgeschobenen Zähler als Zeichenbild, womit auch
`owP` ein Bild zu weit lag. Davon hängen die Routinen ab, die ihr Ergebnis aus
`owP[0]` lesen (kdd/dnu/dnd-Abschluss).

## Umsetzung

Eigenständiger nativer Code, kein Originalkörper und kein Generator.

- `native/player_stairs.c`: Originaldatentabelle `PlyLegRoute` (player.c:214),
  `stair_chain_point` (Kettenprodukt wie `bhCalcTree`, MdlPut.c:267) und
  `stair_fix_position` (bhFixPosition/XYZ mit `offP == NULL`). Aufruf an der
  Originalstelle: nach dem Bildvorschub und der `flg 0x400000`-Berechnung,
  Bein über `flg 0x80000`, XYZ-Zweig über `flg 0x8000000`.
- Die Pose, mit der `owP` zuletzt berechnet wurde, hält der Zustand selbst
  (`draw_*`). `bhCalcTree` baut `owP[n].mtx` aus genau diesen Werten entlang
  der Knotenkette auf, deshalb braucht es keine zweite Knotenkopie des Hosts.
- `sample_frame` gibt die abgetastete Bildnummer heraus; `native/runtime.cpp`
  zeichnet damit dieselbe Pose wie `bhCalcModel` und liefert sie im nächsten
  Bild als `owP` an die Aktionsroutine.
- Neuer Hostdienst `local_node`: `objP[node].pos/ang` der abgetasteten Pose.
  Der Skriptknoten der Originalroute wird auf seinen Modellknoten abgebildet,
  weil die MASK-Töchter die Modellindizes verschieben. Fehlt ein Knoten, meldet
  der Zustand `RECVX_STAIR_UNBOUND_FIXPOS` statt zu raten.

Die ungebundene HOKAN-Mischung (`SetMtnNormalHokan`) ändert das Ergebnis des
Aufstiegs **nicht**: die Summe der Bildversätze hängt nur von Anfangs- und
Endpose ab, und beide sind reine Clipposen. Sichtbar bleibt nur, dass die
Arbeitslage den Rückstand in einem Bild statt über die vier Mischbilder
aufholt; die Figur selbst steht dabei still, weil ihr Modell an
`px + Wurzelbewegung` hängt. Die Kamera zieht entsprechend in einem Schritt
nach. Das bleibt als Lücke gemeldet (`RECVX_STAIR_UNBOUND_HOKAN`).

## Nachweise (macOS arm64, build/stairs-claude-20261010)

Gemessener Vorzustand mit dem unveränderten Binary von 10:04 derselben
Arbeitskopie, nicht erschlossen.

- RM_0050, natürlicher Ablauf: von der Tür an die Treppe laufen (Stopp am
  Originalriegel bei x = -188.8), OK, Aufstieg. Vorher: Ende bei x = -187.74,
  danach Rückstoß auf x = -188.8 bei beibehaltener Höhe 8, kein Weiterlaufen.
  Jetzt: Ende bei x = -173.43, Höhe 8, Etage 2, anschließend freies Laufen
  über den oberen Gang bis x = -146.96; die Folgekamera zieht mit.
- Sweep über alle 66 benutzbaren originalen Typ-1-Aufstiegsflächen der 205
  Räume (diagnostische Platzierung in der Flächenmitte, Richtung aus prm1):
  vorher **1 von 66** Landungen frei von den Typ-0/1-ATR-Riegeln, jetzt
  **57 von 66**. Die neun übrigen sind Platzierungsartefakte des Sweeps: mit
  der realistischen Nahkantenplatzierung landen sechs davon frei, und die
  restlichen drei (RM_1030 etc 0, RM_3000/RM_5690 etc 5) überdecken den
  **gegenüberliegenden** Riegel der Zielfläche um 0.9 bis 2 Einheiten, was der
  gewöhnliche Wandpass nach vorn auflöst: die Figur bleibt auf der oberen
  Etage und läuft normal weiter (nachgemessen). Sieben Flächen startet dieses
  Prüfgerüst nicht (Skriptzustand), sie sind nicht bewertet.
- `tools/verify_stairs.py`: alle Prüfungen bestanden, einschließlich des neuen
  Abschnitts „ascent lands clear of the stair guard walls (RM_0050)“. Dieser
  Abschnitt leitet die Riegel aus den Raumdaten ab (`wall_entries`,
  `blocking_walls` mit dem Originalfiltersatz hitchk.c:705) und schlägt mit dem
  Binary von 10:04 fehl (ATR 47 an der Landung, kein Weiterlaufen).
- `core_continuity` (tests/native/continuity_test.cpp) prüft den Zweig jetzt
  als Komponente: der Versatz des Kettenendes muss im ersten Bild der
  Abschlussphase vollständig in px/py/pz stehen. Gegenprobe gemacht: mit
  abgeschaltetem Aufruf schlägt der Test fehl.
- `tools/verify_rooms.py`: 205/205 Räume laden, 0 blockierte Eventbefehle.
- `verify_continuity` (enthält beide Treppenrichtungen und die Türen des
  Hundebereichs), `verify_events`, `verify_progression`, `verify_room_spawn`,
  `verify_gameover`, `verify_motion_collision`: bestanden.
- CTest 49/52 in `build/stairs-claude-20261010`.

## Angepasste Erwartungen

`tools/verify_stairs.py` trug die vom defekten Host erzeugten Endpunkte als
„original node“. Neu eingetragen sind die Werte der portierten Mechanik; die
Begründung steht im Skript. Betroffen: kdu (der eigentliche Fehler) sowie die
0.03–0.48 Einheiten, um die kdd/dnu/dnd und der Skriptabstieg in RM_1120/1121
wegen der korrigierten Abtastbildnummer früher landen.

## Nicht behauptet, offen

- HOKAN-Mischung des Motionwechsels (siehe oben).
- `bhCPM2_act_suw` ist die zweite Stelle mit `plp->flg 0x40000` (player.c:4641)
  und hat weiterhin keinen nativen Besitzer; die Fußfixierung ist hier nur im
  Treppentreiber gebunden, nicht im allgemeinen `bhControlPlayer`.
- Kein Durchspielnachweis, kein Windows-Lauf, kein aktualisiertes Startpaket.

## Fremde Befunde aus derselben Prüfung (nicht von dieser Änderung)

Mit dem Binary von 10:04 derselben Arbeitskopie identisch reproduziert:

- `tools/verify_push.py` (RM_2070) und `tools/verify_crate_edge.py` (RM_0100)
  schlagen fehl: Schiebekisten lassen sich nicht anfassen (MTN49/50 startet
  nie), und auf einer Kiste stehend läuft die Figur über die Kante hinaus in
  der Luft weiter, weil `plp->stflg 0x20000` nie gesetzt wird. Ursache ist die
  dokumentierte Portierungslücke in `native/player_push_core.c` (Rückbau
  2026-10-09): `bhObj001` liefert keine beweglichen ETC-/Boden-/Wandeinträge
  mehr, `bhCPM2_act_psh` fehlt, und `PlayerPush::contact` ist seit dem neuen
  freien Wandpfad gar nicht mehr aufgerufen. `collision_push_count_` wird in
  `geometry_motion.cpp` geführt, aber von niemandem verbraucht, sodass der
  originale Übergang `psh_ct > 5 → mode2 18` fehlt.
- CTest: `effect_pool_lifetime` („particle gap reports a live emitter“) und
  `native-gameplay-bindings` („combat gap entered an original weapon mode“)
  schlagen mit und ohne diese Änderung fehl (gemessen, indem nur
  `native/player_stairs.c/.h` auf den HEAD-Stand zurückgesetzt wurden).
- `recvx-progression-tests`, `recvx-subplayer-tests` und
  `recvx-enemy-spawn-tests` übersetzen in dieser Arbeitskopie nicht
  (`EnemySpawnContext::random` fehlt; `enemy_work.h`/`enemy_spawn.h` liegen
  nach der Umbenennung in `native/enemies/`, die Testziele binden den
  Suchpfad nicht ein). Beides liegt außerhalb dieser Änderung.
