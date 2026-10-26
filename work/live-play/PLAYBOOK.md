# PLAYBOOK — Agent Playmode

Nur allgemeine Spieltechniken, Steuerungsmechanik und Verarbeitung. Keine Story,
Raumlösungen, Itempositionen, Fortschrittsnotizen oder Snapshotinhalte. Sitzungsdaten
bleiben unter `build/play/`, Prüfergebnisse unter `build/validation/`.

## Verbindlicher Ablauf: zwei Stufen

1. **Denken:** Aus der letzten kompakten Beobachtung genau eine nächste Aktion wählen.
2. **Ausführen:** Eine Funktion aufrufen. Sie führt die gewählte Eingabe aus und liefert
   bereits die kompakte Beobachtung für die nächste Entscheidung zurück.

Keine zusätzlichen Desktop-Abfragen, Wartewerkzeuge, Gesamtberichte oder Planungsrunden
zwischen diesen Stufen. Bei `running`/`busy` direkt `wait` wählen; Befehle nicht erneut senden.
Keine festen Raumsequenzen und keine im Werkzeug versteckte Spielstrategie.

## Schnellster Weg: Host-Treiber mit eigenem LLM

Für flüssiges Spiel fragt `tools/agent_play/driver.py` ein Modell hinter einem
OpenAI-kompatiblen Endpoint (`/v1/chat/completions`) direkt, eine Aktion pro Entscheidung.
Ein Agent mit Shell-Werkzeug braucht pro Runde 10–30 s; der Treiber braucht Modellzeit plus
0,1–0,6 s Spielzeit für Tasten, wenige Sekunden für Wege.

```sh
RECVX_LLM_BASE_URL=http://localhost:8080/v1 RECVX_LLM_MODEL=<name> python3 tools/agent_play/driver.py
```

- System-Prompt ist `tools/agent_play/system_prompt.md` (Regeln, Aktionsschema, Beispiele); er
  bleibt unverändert und wird vom Endpoint per Prefix-Cache nur einmal vorgerechnet.
- Pro Entscheidung gehen nur Zustand (`play.py`-Beobachtung), letztes Ergebnis und `note` (das
  Gedächtnis des Modells) als neue Tokens hinein; der Verlauf wächst bis `--window`
  Entscheidungen und wird dann blockweise halbiert, damit der Cache-Präfix gültig bleibt.
- `zone` und `hint` sind Function-Calls; ohne Function-Calling `--no-tools` (Modell antwortet
  dann `{"op":"zone","i":4}`).
- Szenen, Türen, Videos, laufende Wege und gesperrte Menüs wartet der Treiber ohne Modellaufruf ab.
- Jede Entscheidung steht in `build/play/driver.log` (llm_ms, act_ms, Tokens, Aktion, Ergebnis).
- `--dry-run` zeigt die erste Anfrage; `--steps n` begrenzt den Lauf.
- Endpoint, Modell und API-Key stehen in der ungetrackten `.env` im Repo-Wurzelverzeichnis
  (`RECVX_LLM_BASE_URL`, `RECVX_LLM_MODEL`, `RECVX_LLM_API_KEY`; `.gitignore`). Gesetzte
  Umgebungsvariablen haben Vorrang; der Key geht nur als `Authorization: Bearer` hinaus und
  erscheint in keinem Log. Andere Datei: `--env PFAD`.

Der Treiber startet das Spiel nicht; die Runtime läuft wie unten mit `--control` und
`autodump 5 auto`. Ein Agent-Playmode mit `play.py` bleibt für Diagnose und Werkzeugentwicklung.

## Nutzerregeln

- Das Spiel läuft sichtbar mit Metal (`--renderer gpu`). **Keine Vision, Screenshots,
  Accessibility-/Fensterabfragen oder wiederholtes Vordergrundholen im Spielloop.**
  Daten kommen ausschließlich aus Runtime-Metriken und Stream. Sichtbarkeit beim Start
  herstellen; während des Spielens das Fenster nicht mit Desktop-Werkzeugen anfassen.
- Im Agent Playmode **keine Git-Befehle und kein `tools/check_no_original_core.py`**, auch
  nicht beim Einstieg oder je Heartbeat. Diese ausdrückliche Nutzerentscheidung hat für
  den Playmode Vorrang vor den allgemeinen Vorprüfungen in AGENTS.md. Sie erlaubt keine
  Engineänderungen, Builds, Installation oder Änderungen am Policy-Prüfer.
- AGENTS.md vor der Arbeitsphase lesen; Regeln behalten, keine Historie je Aktion nachladen.
- Dialogtext lesen und bewusst bestätigen. Kein automatisches ACK und kein blindes SELECT.
  SELECT ist der einzige Cinematic-Skip; Video und Tür haben eigene Mechanismen.
- Snapshots ausschließlich Slot 2. Laden nur auf Auftrag oder zur autorisierten Erholung
  nach Game Over. Nicht automatisch einen alten Snapshot als Startpunkt voraussetzen.
- Bei Abbruch sämtliche automatische Fortsetzung beenden. Kein Neustart nach geschlossenem
  Spiel. Keine periodischen Heartbeats, die laufende Steuerentscheidungen unterbrechen.
- Keine Unteragenten/neuen Chats ohne ausdrücklichen Auftrag. Ein Steuerprozess führt nur
  die vom LLM gewählte Aktion aus; er entscheidet nicht selbst über Ziele oder Rätsel.

## Funktionsschnittstelle

Einstieg einmalig:

```sh
python3 tools/agent_play/play.py state
```

Danach jeweils **ein** Aufruf, mit ausreichendem Tool-Zeitfenster für das Ergebnis:

```sh
python3 tools/agent_play/play.py act '{"op":"move","to":[10,12],"run":true,"face":[10,20]}'
python3 tools/agent_play/play.py act '{"op":"interact","zone":2,"to":[10,12],"face":[10,20]}'
python3 tools/agent_play/play.py act '{"op":"press","key":"confirm"}'
python3 tools/agent_play/play.py act '{"op":"keys","keys":["left"],"ticks":1}'
python3 tools/agent_play/play.py act '{"op":"fire","shots":2,"low":true}'
python3 tools/agent_play/play.py act '{"op":"shake","presses":8,"then":{"op":"move","to":[10,12]}}'
python3 tools/agent_play/play.py act '{"op":"wait"}'
```

Die Zahlen sind Syntaxbeispiele, keine Raumroute. `to` und `face` aus dem aktuellen Zustand
wählen. `interact` benötigt eine explizite freie Annäherung und hängt Bestätigen an den
Ankunftspunkt; eine Zone allein rechtfertigt keine erfundene Frontsonde.
`press`: `confirm/cancel/up/down/left/right/inventory/select/skip`.
`keys` hält bekannte Padaktionen für 1–90 Takte und lässt anschließend los.
`fire`: nur mit ausgerüsteter Handgun (Item 9 im Platz `items[0]`; `weapon` = `wpnr_no` ist
dafür nicht maßgeblich), 1–8 Schüsse, `low` zielt tief. Die Runtime-Steuerzeile `fire n [low]`
hält Zielen 10 Takte und setzt dann alle 20 Takte eine Feuerflanke; zuvor dreht `face` zum
nächsten aktiven/greifenden Gegner (oder `"target": index`). Mehrere `keys`-Aufrufe ersetzen
`fire` nicht (Zielen wird zwischen Aufrufen losgelassen). Ergebnis zusätzlich `fire`:
`shots`/`hits` dieses Aufrufs, `target` (Aufschaltung `combat_target`) und `target_hp`.
`shake` nur unter Eigentümer `GRAB`: 1–30 neu gedrückte Richtungen abwechselnd links/rechts
(alle 3 Takte; `bhEne_LeverCheck` liest `sys->pad_ps`). `keys` ist unter `GRAB` ebenfalls erlaubt.
`"then": {"op":"move",...}` hängt die Fluchtroute ohne Entscheidungslücke an; die Runtime wartet
die beim Start laufende Greifszene ab und steuert erst danach (ohne `danger`-Abbruch).
`move`/`interact` mit `"danger": false` laufen ohne Gefahrabbruch.

Standardbudget pro Aufruf: 4 Sekunden, mit `--seconds` höchstens 8. Das ist ein begrenztes
Warten auf Metriken, kein Denkbudget. Der Aufruf kehrt bei einem Entscheidungspunkt früher
zurück. Nicht mit Einsekunden-Polls und weiteren LLM-Runden auf denselben Aufruf warten.
Das Ergebnis enthält Eigentümer, Position, HP, Inventar, höchstens fünf nahe Zonen, vier
nahe Gegner und drei neue Stream-Ereignisse. Aktueller Dialogtext bleibt vollständig; ein
Itemname im Text (Meldungscode FF03, z. B. Aufnahmefrage) ist eingesetzt.

Felder der kompakten Beobachtung (nur wenn der jeweilige Besitzer aktiv ist):

- `inv`: `[platz, id, anzahl, name]`; Name wie die Statusliste (`item_names` im Dump).
- `equipped`: `[name, anzahl]` des ausgerüsteten Platzes (`items[0]`).
- `enemies[].reach` / `in_reach`: Gegner näher als 7 (beobachtete Greifstarts bei 5.8–6.3);
  Vorbeigehen ist dort nicht frei.
- `gameover`: `retry_room`/`retry_play_time` des Retry-Punkts (`bhPopGameData`). Ohne späteren
  Retry-Punkt ist das der Spielstart; dann ersetzt Retry den Fortschritt (Snapshot 2 erwägen).
- `title`: `stage` (`fade_in`/`press_start`/`game_mode`/`fade_out`/`settings`), `cursor`,
  `entry` = Eintrag unter dem Cursor, `entries` = Zeilen der Tafel, `load_available`.
- `menu`: `cursor`/`navigation` (Leiste EXIT/FILE/MAP/ITEM), `slot` + `item` = Platz und Name
  unter dem Cursor; bei offener Befehlsliste `commands` und `command` (gewählter Eintrag);
  `examining`; `combine_from` = `[platz, name]` der Ausgangsseite beim Kombinieren;
  `equipped: true`, wenn der Platz unter dem Cursor ausgerüstet ist. Equip schaltet um:
  Equip auf dem ausgerüsteten Platz rüstet ab. `locked: true`, solange Bildschirm-/Befehlsfenster
  gleiten oder der Schirm schließt (Pad wird ignoriert); `act` meldet `menu` erst danach, und der
  Entscheidungshalt greift in dieser Zeit nicht.
- `dialog.voice` + `dialog.index`: Seite ohne druckbaren Text (Sprachseite einer Szene); Bestätigen
  blättert sie regulär weiter.
- `enemies[].mode`: Arbeitsmodus `[mode0..3]` (z. B. zur Unterscheidung geskripteter Gegner).
- `typewriter`: `mode` (`load`/`save`), `phase_name` (`files`/`confirm`/`no_save`/`io`/...),
  `text` = angezeigte Meldung, `cursor`, `selection` (Ja/Nein-Feld, 0 = erstes), `used_slots`,
  `slot` = Datei unter dem Cursor.
- `zones[].item` und `far_items`: Itemzonen tragen den Zustand des verknüpften Raumitems
  (`present`/`hidden`/`inactive`/`none`/`object`). `far_items` listet alle Itemzonen jenseits
  der fünf nächsten als `[zone, at, d, zustand]`.

Nur wenn wirklich benötigt, Details genau einer Zone lesen:

```sh
python3 tools/agent_play/play.py zone 2
```

`zone` liefert zusätzlich `approach`: bis zu drei erreichbare Standpunkte (`to`, `face`, `path`),
frei beim Körperradius 3.5, von denen die Sonde stehend in der Zone liegt (Reichweiten aus
`EventVm::interact`: Item 2.0, Tür 4.5, Text 6.0 vor dem Spieler; gesperrte Blickquadranten
der Zone ausgeschlossen). Reine Geometrie; ob die Zone reagiert, entscheidet das Raumskript.
Rangfolge: ohne Überlappung, dann Sondentiefe in der Zone (`depth`), dann Nähe zur Mittellinie
quer zur langen Seite, dann Weglänge. Punkte, an denen die Sonde einer Zone mit kleinerem Index
trifft, entfallen (sie antwortet zuerst); trifft eine Zone mit größerem Index mit, steht sie in
`overlap`. `interact` fährt mit `near s`; `s` ist die Strecke, um die der Spieler vor dem Ziel
stehen bleiben darf, ohne dass die Sonde die Zone verlässt. Bleibt die Route innerhalb `s`
stehen (45 Takte ohne Fortschritt oder 8 Takte Gehen auf der Stelle, z. B. vor Tür/Wand), werden
Blickrichtung und Bestätigen trotzdem ausgeführt (Ergebnis `arrived_near`). Das Endstück unter
3 Einheiten wird gegangen, nicht gerannt.
Treppen/Leitern (Typ 1) und Stufen (Typ 2, nur prm0 0) haben ebenfalls `approach`: Sonde 0.75
bzw. 4.5 vor dem Spieler; bei Typ 1 nur Blickrichtungen innerhalb 45° zur Partnerzone (anderes
Stockwerk), bei Typ 2 zusätzlich der 6-Einheiten-Einrückpunkt. Die Stufensuche läuft vor den
Zonen 0/3/4 und hat deshalb Vorrang bei Überlappung. Leeres `approach` trägt `approach_note`
(z. B. alle vier Blickrichtungen gesperrt, Attribut 0x3c00).

Nicht regelmäßig `nav.py`-Gesamtlisten, vollständige JSON-Dumps oder `hint.py room` ausgeben.
`hint.py zone`, danach `blocked` oder ein gezieltes Event bleiben Werkzeuge für Unklarheiten.
Keine Bild-/Desktop-Ausweichroute. Fehlende Metriken als Werkzeuglücke benennen.

## Ergebnis korrekt behandeln

- `choice` / `dialog`: Text lesen, nächste Eingabe bewusst wählen. Druckende Seiten abwarten.
- `menu`: Status, Datei, Box oder Debug besitzt die Eingabe; nicht als freie Bewegung behandeln.
- `room`: neuer Raum beobachtet; dessen aktuellen Zustand verwenden. Das beweist kein
  Durchspielen. Ein Diagnose-/Snapshotstart bleibt als solcher gekennzeichnet.
- `stalled` / `cancelled`: Folgebefehl der Route entfällt. Keine Wiederholung ohne neue Lage.
- `danger`: nur Gegner im Arbeitsmodus aktiv/greifend (nicht geskriptet), nur unter `FREE`/`GRAB`.
  `wait`/`press`/`keys` melden ihn, sobald einer neu näher als 15 kommt. Routen (`move`/
  `interact`, Option `danger 15`) bricht die Runtime nur ab, wenn einer neu in 15 UND näher als 7
  am Restweg steht; Routen unter 8 Einheiten und `"danger": false` werden nie abgebrochen.
  `fire`/`shake` melden keinen `danger`.
- `held`: Runtime steht im Entscheidungshalt (`run auto`); nächste Aktion wählen.
- `interrupted`: Kino (auch Greifen), Video oder Raumwechsel hat die laufende Route beendet;
  sie läuft danach nicht weiter. Neue Lage lesen.
- `grab` / Eigentümer `GRAB`: Spielermodus 4; das Kinobit ist dabei gesetzt, Eingabe bleibt beim
  Spieler (`shake`/`keys`). Bewegung ist gesperrt.
- `no_interaction_observed`: Ankunft ohne beobachteten Dialog-/Menü-/Raumwechsel; kein Erfolg.
- `running` / `no_update`: nichts erneut auslösen. `wait` liest weiter und schreibt keine Eingabe.
- `error` mit veralteten/nicht fortschreitenden Metriken: kein Befehl gesendet. Laufzeitstatus
  klären, keine Kameraabfrage, kein Wiederstart und keine Eingabe auf Verdacht.
- Laufende Route/ausstehende Befehle sperren weitere Aktionen; mehrere Clients sind verriegelt.
  Alte `nav.py`-/manuelle Dateischreiber nicht parallel mit der Funktionsschnittstelle nutzen.

Die Schnittstelle prüft vor einer Eingabe zusätzlich einen fortschreitenden Metrik-Sample.
Damit genügt eine frisch aussehende Abschlussdatei eines beendeten Spiels nicht als Freigabe.
Sie verändert keine Enginezustände und trifft keine Spielentscheidungen.

## Navigation und Eingaben

Die Pfadsuche plant über die aktuellen Kollisionsdaten; blockierte Ziele werden abgelehnt,
nicht still auf irgendeine freie Zelle verschoben. Der geprüfte exakte Zielpunkt wird als
letzter Wegpunkt mit `stop 0.35` erhalten. Der native Controller verwendet für Zwischenpunkte
weiterhin 3 Einheiten. Ankunft und erfolgreiche Interaktion sind getrennte Ergebnisse.

Die neue Annäherungsplanung verwendet den vorhandenen Geh-Radius 3.5 (native/runtime.cpp,
Stufeneinstieg; native/world.cpp, Kollisionsschnittstelle). Das ist Navigation des Agenten,
keine neue Engine-Kollision. Zwischenpunkte werden mit 0.5 Abstand zusätzlich geplant, wenn
der Raum es zulässt, sonst mit 3.5; das letzte Stück zum exakten Ziel prüft die Wandgeometrie
direkt (nicht das Raster). Ist die gerade Linie zum Ziel frei (mit 0.5 Abstand, unter 3
Einheiten mit dem Körperradius), wird ohne Rasterumweg gerade gesteuert. Frontpunkt/Frontversatz im Gehen sind nicht im Dump: `approach`
rechnet nur den stehenden Fall (`fpx == px`, `arn` 0). Die Itemsonde reicht 2 Einheiten vor
`actor`, die Stufensonde 0.75; gültige Blickrichtung und Zone aus aktuellen Metriken prüfen.

Ein Pickup kann Statusbesitz melden, bevor die Aufnahmefrage existiert. Erst die Frage lesen,
dann bestätigen; danach kann die Menüausfahrt länger dauern als der Dialog. Dateien können
geöffnet bleiben. Das Werkzeug meldet solche Eigentümer; es bestätigt sie nicht selbst.

Legacy-Werkzeuge bleiben für gezielte Diagnose erhalten. `nav.py wait` bestätigt keinen Text
mehr automatisch und meldet einen offenen Statusschirm nicht mehr als frei. Aktive Produzenten
nicht durch `head` abbrechen; bevorzugt direkt das kompakte Funktionsergebnis verwenden.

## Laufzeit und Cache

Nur eine Runtime und ein Stream-Reader. Bestehende Steuerdateien nicht abschneiden. Bei einem
neuen, beauftragten Lauf alte Daten recoverable sichern; die Runtime liest Steuerzeilen beim
Start ab Offset 0. Keine fest getaktete New-Game-/Skip-Sequenz.

Für die Funktionsschnittstelle laufende Metriken verwenden (`autodump 5 auto`). `run off`
setzt Updates aus; die Schnittstelle hebt so einen Halt nicht auf Verdacht auf.

Laufmodus wählt der Agent mit `act '{"op":"run","mode":"auto","dist":18}'` bzw. `"mode":"on"`:

- `on`: Echtzeit; das Spiel läuft auch während der LLM-Entscheidung weiter.
- `auto` (Entscheidungshalt, empfohlen bei Gegnern): Echtzeit, aber ohne ausstehende Zeilen und
  ohne laufende Route hält die Runtime, sobald ein aktiver Gegner näher als `dist` ist und
  ein Entscheidungspunkt vorliegt (kein Video, keine Tür, keine Szene außer Greifen, keine
  noch druckende Meldung). Jede Aktion läuft dann genau für ihre eigenen Takte (Route bis
  Ankunft, `keys`/`shake`/`fire` bis zur Freigabe) und hält danach erneut. Im Halt schreibt die
  Runtime alle 200 ms einen Herzschlag-Dump (`control.held`, `control.heartbeat`,
  `control.tick` steht); die Metriken bleiben frisch. Kompakt: `held: true`, Ergebnis `held`.
  `play.py` setzt bei `auto` kein `run on` mehr vor die Aktion.
- Zeit im Halt bewusst verstreichen lassen: `act '{"op":"wait","ticks":30}'` (1–300 Takte,
  Steuerzeile `tick` ohne Eingabe). `wait` ohne `ticks` schreibt nichts.
- Der Halt zählt nur gebundene Gegner im Modus aktiv/greifend. Gefahrabbruch von Routen
  siehe `danger` unter "Ergebnis korrekt behandeln".

Optional kann ein **rein anfragegetriebener** Prozess JSON und Geometrie zwischen Aufrufen
behalten. Er spielt nicht selbst und startet weder Spiel noch LLM. Lokale Unix-Sockets
benötigen einen Host-Prozess, wenn die Sandbox deren Bind/Connect sperrt. Der direkte
`play.py`-Aufruf funktioniert ohne Server oder Socket; er ist der Standard.

```sh
python3 tools/agent_play/play.py serve
python3 tools/agent_play/play.py --socket build/play/play.sock state
python3 tools/agent_play/play.py --socket build/play/play.sock act '{"op":"wait"}'
```

Der Cache gilt nur bei identischer Raum-/Wandgeometrie, Etage, Höhe und Rasterparametern.
Auch Wandpositionen und Ausdehnungen invalidieren ihn, nicht nur Aktivierungsflags. Gegner
und Eigentümer werden aus aktuellen Metriken gelesen. Keine zweite Workwelt.
Nebenräume vorzurechnen bleibt eine offene Optimierung: nur lose Daten lesend vorbereiten,
beim Eintritt gegen echte Runtime-Metriken validieren und Kosten/Nutzen messen, bevor ein
Geschwindigkeitsgewinn behauptet wird. Keine Raumkenntnisse in diesem Playbook speichern.
