# RM_0150: Eisenstatue nach Schwertaufnahme nicht greifbar

## Ursache

Die freie Laufsteuerung führte `mode2/mode3` in `RECVX_PLAYER_ACTION`, gab sie
aber nicht an das bestehende Spielerwerk (`RoomWorld::collision_mode_`) weiter.
Nur der Ereignisbewegungszweig hatte diese Rückbindung. Die Figur zeigte deshalb
eine Gehmotion, während Objekt-/Kollisionsleser weiter `mode2 = 0` sahen.

Originalbelege:

- `player.c:1838` bhControlPlayerPad und `player.c:1957` bhCPM1_act_bas
  schreiben die Aktionszustände am einen plp-Werk.
- `game.c:49-51`: Spielerpass vor Effekt-/Objektpass.
- `objitm.c:1736-1749` bhObj007: sechs passende Kontaktpässe mit Vorwärts-Pad,
  `mode2` 3/4/5 und ohne stflg 0x80 führen zur Drehaktion (`mode2` 22).
- `player.c:4324` bhCPM2_act_rpsh; `objitm.c:1763-1845`: Griff, Drehung,
  Zielwinkel und ev_flg 2 lösen die originalen Raumereignisse aus.

Gemessener Vorherstand aus Nutzersnapshot Slot 1: Gehmotion 2 läuft, aber
`player_work.mode = [1,0,0,0]`. Keine Drehaktion und kein ev_flg 2.
Der bestehende Komponententest `turn_object` übersah die Runtime-Bindung,
weil er den Gehmodus selbst mit `set_player_mode` einrichtet.

## Zweite Ursache: Host-Latch überlebte den Skript-Reset

`bhCPM2_act_rpsh` wird im Original über `plp->mode2 == 22` dispatcht
(`player.c:1957`). `bhLoadWork` Spieler setzt `mode2 = 0`
(`event.c:12722-12737`), `Player_controll 128` setzt mode0..3 = 1/0/0/0
(`event.c:9640`). Der native `RoomWorld::turn_active_` war ein zweiter,
unabhängiger Besitzer und blieb nach diesen Resets stehen. Dadurch lief die
Schiebeaktion noch während der Stop-Gas-Ereignisse 15–19 weiter: Claire blieb
in Schiebepose, wurde über `tick_turn` weiter gedreht und die Hände lagen nicht
mehr am Objekt. Der Fehler ist damit derselbe Zustandsbesitz-Fehler wie der
Gehmodus, nicht eine Animations- oder Kameraeigenheit.

Fix: `turning()` verlangt jetzt `turn_active_ && mode2 == 22`; `tick_turn`
beendet den Latch, sobald das Werkfeld nicht mehr 22 ist. `bhLoadWork` Spieler
und `Player_controll 128` löschen ihn ebenfalls. Keine neuen Timer,
Sonderfälle oder Ersatzanimationen.

## Fix

`native/runtime.cpp`: Nach dem normalen Lauf-/Drehpass mode2/mode3 in den
vorhandenen Spielerbesitzer übertragen, unter Erhalt von mode0/mode1, vor
Wandpass und bhControlObjItm. `RoomWorld` bindet `turn_active_` an das originale
mode2-22-Feld. Keine geänderten Greifradien, Timer, Original-Bedingungen oder
Raumsonderfälle. Kein neuer Work-/Zustandsbesitzer.

## Regression und Prüfungen

`tests/native/rm0150_statue_turn_check.py`, CTest `rm0150_statue_turn`:

- Ohne Benutzersnapshot: frischer RM_0150, originales Schwert/GetItem (Item 103),
  originale Gasereignisse 5..8, gewöhnliche Vorwärtseingabe am Statuenkontakt.
- Keine injizierten Events, Objekttransformationen oder erzwungenen Spielermodes.
  Nur Spielerplatzierungen verkürzen die Laufwege (Diagnose, kein Durchspielbeleg).
- Greifen erreicht mode2 22; frühes Loslassen beendet die Aktion ohne Rätsellösung;
  erneuter vollständiger Lauf dreht bis ev_flg 2 und startet die originalen
  Gasabschlussereignisse 15..19.
- Nach dem Skriptreset (`after-event`, 1150) verlangt der Test `turning == false`
  und `player mode2 == 0`; damit wird der weiterlaufende Schiebe-Latch direkt
  geprüft. `--state` führt dafür `turning` (Diagnose des Spielerbesitzers).
- Optionaler `--snapshot` lädt Slot 1 nur lesend. Hash vor/nach identisch:
  `28f1e8fcae9164dde28539299b62a5712a27e39d42e1df0467c43e7ec33ce563`.
- Gegenprobe mit gesichertem altem App-Binary: derselbe vollständige Lauf bei
  1200 endet dort noch in `mode3 4`, `motion 0`, `yaw -49152` (Schiebe-Latch);
  die korrigierte Runtime ist zu diesem Zeitpunkt bereits frei gehend
  (`mode2 3`, `motion 2`, `yaw 49008`). Belege `end/old-1200.*` und
  `end/fixed-1200.*`.
- Der frühere Gegenprobe-Lauf `counter-before.log` belegt zusätzlich den
  fehlenden mode2-Sync vor dem ersten Fix (Gehpose, mode2 0).
- Installierte Dev-App über **Start.command**, Slot 1: Snapshot-Lauf und
  SDL-Lauf mit F9 + gehaltenem W über InputManager grün
  (`end/final-snapshot.log`, `end/fixed-sdl.log`); Snapshot danach bytegleich.
- Finaler Build über `./build.sh --platform macos --test-regex ...`: 6/6 grün (`end/build-final-latch.log`):
  rm0150_statue_turn, turn_object, player_action, player_event, player_push,
  core_continuity.
- arm64-only und `codesign --verify --deep --strict` geprüft; Buildwrapper hat
  Vorgänger-App gesichert und die Dev-App aktualisiert. Standalone-Paket unverändert.
- Policy-Prüfer vor Arbeit/jedem Build sowie nach Link bestanden;
  tools/port_check.py ausgeführt, Ledger-Evidenz ergänzt.

Belege unter `build/validation/rm0150-statue-20261021/`.

## Offene Grenze

Der Test belegt den Abschluss der Schiebeaktion und den freien Spieler nach den
Stop-Gas-Ereignissen. Er belegt keine vollständige visuelle Hand-/Objektabnahme
der Cutscene 15–17 und keinen durchgespielten Benutzerstand. Die Stange selbst
wird von Ereignis 17 über die Original-Common-Befehle bewegt; eine weitere
Spielerkopplung ist in den Raumskripten nicht vorhanden. Sollte die Handpose
während des eigentlichen Schiebens weiterhin abweichen, ist das eine separate
Motion-/Pose-Frage, nicht der hier behobene Zustands-Latch.

Diagnosehinweise: Headless-Replay `keys` schreibt das Objekt-Padbit nur in dem
Takt mit einer `keys`-Zeile; gehaltene Bewegung braucht deshalb hier je Takt eine
Zeile. Dieser Replay-Diagnosepfad wurde nicht als weiterer Produktionsfix
verändert. Im SDL-Pfad zählt `--frames` Host-Schleifendurchläufe, nicht exakt
Simulationstakte; eine Prüfung auf eine Zwischenpose nach gleicher Zahl war
ungültig (Logs sdl-snapshot). Der optionale SDL-Lauf prüft daher nur den
abgeschlossenen Vorgang nach ausreichendem Budget, F9 und gehaltenes W gehen
durch echte SDL-Ereignisse/InputManager, mit Dummy-Ausgabegeräten.

Benutzer-INIs, Mods und Snapshots unverändert. Windows/physische Controller
hier nicht ausgeführt. In dieser Sitzung kein Commit/Push ausgeführt. Der
Runtime-Fix, CTest-Anschluss und erste Regressionsteststand wurden während der
Prüfung von paralleler Arbeit committed (7d11698, anschließend 76d1d9b für den
Spielerpass); die letzte SDL-Testanpassung und dieser Bericht sind noch lokal. Zwischenzeitliche Fensterdarstellungs- und
Dokumentationsänderungen (9f19dd7/6eeced9) nicht zurückgesetzt.
