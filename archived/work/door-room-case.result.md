# Friedhof → Rodrigo: Raumvariante beim Türwechsel

## Ursache und Originalbeleg

Der Absturz ist mit unveränderten Raumdaten reproduziert: RM_0021, diagnostische
Platzierung `(108, 0, 150)`, yaw 16384, Confirm an Interaktion 1.
Die installierte alte Runtime bricht mit `door room missing: RM_0011.RDX` ab.

Die Interaktionstabelle fordert stage 0 / room 1 / spawn 1 / door type 1 an.
Der laufende Quellfall ist dabei noch 1. Das originale RM_0021-Ausgangsskript
setzt anschließend mit `bhRoomCaseNo 0` (scd1 relativ zum EVT_WORK @0x448) den
Fall auf 0 und wiederholt `DoorCall` (@0x44c). RM_0011 ist keine fehlende
Exportdatei: Die Originalraumtabelle enthält nur RM_0010 für diesen Raum.

Originalmechanismus:

- `room.c:883-918`, `bhSetDoorDemo`: speichert attr/stage/room/spawn/type,
  **keinen** rcase-Schnappschuss in DOOR_WORK.
- `event.c:379-384`, `bhControlEvent`: Szenario vor Scheduler; der Aufruf beendet
  nicht pauschal den übrigen Szenariodurchlauf.
- `event.c:3221-3233`, `bhRoomCaseNo`: schreibt die globale sys->rcase.
- `system.c:1623-1634`: erst der spätere Raumladetask formatiert den Namen
  aus stage/room und dem **dann aktuellen** sys->rcase.

Die native `request_door`-Vorprüfung war davor angeordnet und warf bereits beim
Interaktionsauftrag eine Ausnahme. Der Ausgangsdurchlauf konnte den Ziel-Fall
daher nicht mehr setzen. Außerdem verwendete der spätere Lader den beim
Auftrag kopierten Fall statt des noch veränderlichen Eventzustands.

## Änderung

`native/runtime.cpp`:

- `request_door` nimmt den Auftrag an, ohne vorzeitig einen Dateinamen aufzulösen.
- `door_task_tick` löst bei abgeschlossenem Türablauf den Dateinamen mit dem
  aktuellen `events->values[21]` auf. Kamera-/Spawnprüfung bleiben erhalten,
  jetzt am tatsächlichen Ladepunkt.
- Fehlende echte Ziele bleiben Fehler. Kein Fallback auf Variante 0 und kein
  Raumsonderfall. Ein nicht gefundenes Ziel lässt auch nicht unbemerkt den
  alten room_index stehen.
- Tür-Skip, Türtonbehandlung, Blenden und reguläre Zielübergabe unverändert.

Regression `tests/native/door_room_case_check.py`, CTest `door_room_case`:
Original-Friedhofsausgang regulär und mit Host-Skip; isolierte diagnostische
Szenarien mit nach DoorCall gesetzter Variante 1 (im selben Skriptdurchlauf
sowie erst im Ausgangsszenario), jeweils regulär und direkt/type 255; tatsächlich
fehlende Variante 9 bleibt `RM_0029`-Fehler. Die Zusatzszenarien ersetzen nur
EVT_WORK in temporären Mods, keine produktiven Daten.

## Prüfung und Auslieferung

Belege: `build/validation/door-room-case-20261021/`.

- `before.log` und `before-test.log`: altes installiertes Binary reproduziert
  exakt den gemeldeten RM_0011-Abbruch; neuer Regressionstest ist davor rot.
- Erster Prüflauf: Original-Friedhofsausgang bereits grün; isolierte Testfixture
  hatte ihren Scheduler nicht aktiviert. Fixture um das originale sp_flg-0x10-
  Tor ergänzt; keine Produktionsänderung dafür und keine abgeschwächte Erwartung.
- `./build.sh --platform macos --test-regex
  '^(door_room_case|door_transition|core_continuity|title_and_room_entry_camera)$'`:
  4/4 bestanden (`build-final.log`). arm64-Build, keine Compilerfehler.
- `tools/verify_doors.py`: bestanden (`doors.log`), einschließlich regulärer
  Blenden, Soundfolge, Skip mit gezieltem Stimmstopp und direktem Übergang.
- `installed-case-test.log`: vollständiger neuer Regressionstest gegen die
  installierte Dev-App bestanden.
- `Start.command` wählt weiterhin zuerst `build/start/macos-arm64/RECVX Native.app`.
  Deren Vorgänger wurde vom Buildwrapper unter `.previous-*` gesichert; neue
  App frisch signiert, `codesign --verify --deep --strict` bestanden,
  `lipo -archs` meldet ausschließlich arm64.
- `starter.log` / `starter-state.json`: derselbe Rückweg tatsächlich über
  `./Start.command` geprüft: RM_0010, 1 Raumwechsel, Tür beendet,
  0 blockierte Eventbefehle.
- `title.log` / `title-state.json`: Start ohne `--in-game` über den Starter
  ebenfalls erfolgreich, Titel aktiv.
- Policy-Prüfer vor Arbeit/Build sowie nach Link/Installation bestanden;
  `tools/port_check.py` ausgeführt (`port-check.log`).

Kein vollständiger Late-Game-Durchspielnachweis: Die Reproduktion beginnt im
originalen RM_0021 mit diagnostischer Platzierung, nicht mit einem durchgespielten
Benutzerspielstand. Windows hier nicht gebaut/ausgeführt; Änderung ist im
plattformgemeinsamen Runtime-Pfad. Standalone-Release-Paket nicht ersetzt.
Benutzer-INIs, Mods, Spielstände und Snapshots nicht verändert. Parallel entstandene
Frontend-/Dokumentationsänderungen erhalten. Kein Commit/Push/PR.
