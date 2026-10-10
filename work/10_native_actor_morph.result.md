# Gemeinsamer nativer Actor-Morph-Pfad (Codex, 2026-10-09)

Fortsetzung außerhalb von Claudes BGM-Auftrag. Keine BGM-Datei verändert,
kein Commit/Push, keine Rücksetzung bestehender Änderungen.

## Portierter Originalmechanismus

- `bhCyoutenHenkeiSet`, `Start`, `SetEX`, `StartEX` aus `event.c:4541–4601`
  und `6040–6120` werden vom Generator vollständig übernommen. Sie wählen
  `mbp`, schreiben `mlwP->objP`, `obj_a/obj_b`, `mdflg & 2` und `shp_ct`
  unmittelbar auf der gemeinsamen nativen Work, statt nur Host-Felder zu setzen.
- Nur unaligned u16-Lesen und die bereits im nativen Eventpfad belegte
  saturierende FOR-Arithmetik sind an den Host angepasst. Ein leerer FOR darf
  kein IEEE-NaN durch `inf * 0` erzeugen. Keine neue Gameplay-Kurve.
- Der gemeinsame Gegner-Draw übernimmt die Sequenz aus `eneset.c:845–853`:
  `npPushMdlstr2` → vorhandenes natives `npCalcMorphing` → Zeichnen der
  ausgewählten `obj_a` → `npPopMdlstr2`. Auch ein von der KI gewechseltes
  `obj_a` bestimmt den tatsächlichen Assetbesitzer; keine ID-Sonderregel.
- Native temporäre Chunk-Streams werden vor dem Pop in einen eigenen
  Render-Snapshot serialisiert. MDL-relative Offsets zeigen auf die eingefügten
  Streams. Die Renderer-Pose erfasst denselben Draw-Zeitpunkt. Die ursprünglichen
  Vertexzeiger und Objektpositionen werden restauriert; wiederholtes Zeichnen
  akkumuliert keine Verformung. `NP_WORK` wird anschließend ebenfalls restauriert.
- Scratch-Speicher folgt dem tatsächlichen Streambedarf, nicht einer
  nachgebildeten Konsolenadresse. Kein PS2-/core32-Kern und kein Interpreter.
- Noch nicht angebundene Subplayer-/Object-Morph-Besitzer werden als offene
  Bindung zurückgegeben, statt durch gesetzte Host-Felder Erfolg vorzutäuschen.

## Prüfung und Grenzen

Neue `core_actor_assets`-Regression auf originalformatigen RM_0130-Assets:
Work-Auswahl, beide FOR-Formeln, leere FOR, echte interpolierte Vertexdaten
im Render-Snapshot, temporäre Nodepositionen, Restaurierung und wiederholter
Draw ohne Drift. Die künstlich veränderte Zielgeometrie ist ausdrücklich
eine Komponentenfixture, kein Originalscript-Abnahmenachweis.

Produktive native Runtime: RM_5800 (ID 15), RM_5830 (ID 2), RM_9280 (ID 17)
jeweils 600 Ticks mit `--validate-poses`, ohne Absturz/Posefehler. Zustände in
`build/validation/morph-20261009`. Kein künstlicher Spawn und kein Alternativkern.
RM_5830 meldet weiterhin `StartVibrationEx`; RM_9280 vier offene Eventbindungen.
Diese Läufe beweisen keine vollständigen Bossaktionen oder sämtliche Morph-
Eventfolgen. Die Player-Befehle schreiben zwar die gemeinsame Player-Work,
die normale Player-Darstellung verwendet außerhalb eines Gegnergriffs aber
noch ihren bisherigen Actor-Besitzer. Dessen vollständige Modellwahl-/Morph-
Anbindung ist damit nicht nachgewiesen. Face-/Shadow-Bindungen bleiben offen,
ebenso der Windows-Runtime-Link.

Erster Gesamttest: 48/48 CTest; Raumsweep 205/205 geladen, 200/205 beenden
600 Ticks. Dieselben fünf bekannten BGM-Abbrüche (RM_9091/9340/9350/9360/9370),
keine zusätzlichen Signalabstürze. Dieser Sweep wurde beim Buildstart gestartet.

Finaler Sweep mit dem abschließenden Binary: **205/205 laden und beenden
600 Ticks, keine blockierten Eventcommands und kein Prozessfehler**.
Beleg: `build/validation/morph-20261009/final-room-sweep-600.json`.
Währenddessen bearbeitete Claude unabhängig den BGM-Bereich; das Verschwinden
der BGM-Abbrüche ist kein von dieser Morph-Änderung beanspruchter Fix.
Offene/pending Bindungen werden weiterhin separat gemeldet; 205 erfolgreiche
Raumstarts ersetzen keinen vollständigen Gameplay-/Durchspielnachweis.

Abschließender CTest: erneut 48/48 bestanden (72,35 Sekunden bei gleichzeitigem
Raumsweep). Windows x64: auch abschließender `recvx_game`-Compile erfolgreich.
Ein Bibliotheksbuild ist weiterhin kein Windows-Runtime-Nachweis.
Start.command-Paket mit dem neuen macOS-Binary aktualisiert und signiert;
`codesign --verify --deep --strict` erfolgreich. Gekochte Daten unverändert.
