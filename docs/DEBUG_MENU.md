# Debugmenue-Mod

F2 oeffnet/schliesst das Debugmenue. Der Debug-Hotkey hat Vorrang vor
Spielsteuerung und der bisherigen F2-Wireframe-Belegung. Neue Steuerungsdefaults
legen Wireframe auf F3; vorhandene Benutzerbelegungen werden erhalten.

## Konfiguration und Bedienung

`debug.ini` ist unabhaengig von `controls.ini`:

```ini
[debug]
hotkey = F2
```

SDL-Tastennamen wie `F6` sind moeglich. Nach Aenderung neu starten.
Die Runtime legt eine fehlende Datei mit F2 an und ueberschreibt vorhandene
Dateien nicht. Ungueltige oder doppelte Einstellungen werden als Fehler gemeldet.

- macOS-Paket: Datei neben `RECVX Native.app`, ausserhalb des signierten Bundles.
- Windows-Paket: Datei neben `recvx-runtime.exe`.
- Direkte Runtime: Datei neben dem ausfuehrbaren Programm, unabhaengig vom CWD.
- Projekt-Starter ohne Paket: `debug.ini` im Projektordner.
- `--debug-config <Pfad>` waehlt ausdruecklich eine andere Konfigurationsdatei.

Mit den konfigurierten Menueaktionen navigieren: Hoch/Runter waehlt die Funktion,
Links/Rechts den Wert, OK wendet an. Tastatur und Controller verwenden denselben
Pfad. PageUp/PageDown bzw. LB/RB springen in langen Listen um 20 Werte.
Escape/Controller-B oder der Debug-Hotkey schliesst das Menue. Gedrueckthalten
wiederholt das Oeffnen oder Anwenden nicht.

Waehrend der Auswahl pausieren Event-VM, Welt, Animationen, Kampf, Video und
Audio. Die Ansicht bleibt sichtbar. Beim Schliessen werden gehaltene
Spielaktionen bis zum Loslassen unterdrueckt, damit OK keine Aufnahme oder
Attacke im Hintergrund ausloest.

## Funktionen

| Aktion | Verhalten |
|---|---|
| Charakter | Originale SYSTEM-Koerper 0010–0014, einschliesslich Claire/Chris. Modell laden und aktuellen Raum neu initialisieren. Die bereits vorhandene Zuordnung der Inventare bleibt erhalten: 0010/0013/0014 verwenden Claire, 0011 Chris, 0012 Spieler 2. Dies implementiert keine neuen Charakterkampagnen. |
| Equip / Waffe | Vorhandenes Item ausruesten; fehlende Waffe mit Originalmenge ins Inventar geben und ausruesten. „Keine“ legt Ausruestung ab. Waffen, deren Kampf noch fehlt, erhalten durch diese Mod keinen neuen Kampfcode. |
| Raum | Alle vorhandenen RDX-Raumnamen zur Auswahl. Zielressourcen werden vorab dekodiert/geprueft, dann Spawn 0 geladen. Inventar und globale Flags bleiben erhalten; aktive raumlokale Tasks starten neu. |
| Spawn | Spieler an einen originalen Spawn des aktuellen Raums versetzen. |
| Itemmenge / Item geben | Menge 1–999 und Originalitem 1–145. Originale Inventarkapazitaet/Stapellogik gelten; fehlgeschlagene Aufnahme veraendert die Items nicht. |
| Gesundheit | HP innerhalb des derzeitigen Maximums setzen. |
| Tasche | 8/10 Slots, einschliesslich originalem Taschenflag. Entfernen wird bei mehr als acht belegten Slots abgelehnt. |
| Wireframe | Drahtgitterdarstellung umschalten. |
| Weiter spielen | Debugmenue schliessen. |

Fehler werden im Debugmenue angezeigt. Ein abgelehnter Raum wird nicht einfach
als geladen markiert. Raumspruenge sind Diagnosefunktionen; sie ergaenzen keine
fehlenden Event- oder Weltbindungen des noch unvollstaendigen Ports.
Aenderungen betreffen die laufende Sitzung. Save/Load ist noch nicht vollstaendig
implementiert.

## Erweiterung

`native/debug_menu.*` implementiert Konfiguration, Hotkey, Navigation und Overlay.
`DebugMenu::add(DebugAction)` registriert Funktionen ohne Anpassung der UI:

```cpp
debug.add({
    "example", "Neue Funktion", "OK: ausfuehren.",
    [] { return std::vector<recvx::DebugChoice>{{0, "Aus"}, {1, "An"}}; },
    [&] { return unsigned(enabled); },
    [&](unsigned value) { enabled = bool(value); return std::string("Gesetzt"); }
});
```

Jede Aktion hat eine stabile ID, dynamische Werte, einen aktuellen Wert und einen
validierenden Ausfuehrungs-Callback. Die Spielbindungen sind in
`native/runtime.cpp` registriert. Erst im Callback wird der Spielzustand
veraendert. Ressourcen vorher pruefen; globale Referenzen/Actorassets bei
Raum- oder Charakterwechsel sauber aktualisieren. Inventaraenderungen werden mit
einer Kopie vorab geprueft; erst bei Erfolg werden Inventar und Statusmenue
gemeinsam uebernommen. Laufende Itemfragen werden dann verworfen, damit
Slotverschiebungen keine alten Kombinationen oder Untersuchungen ausloesen.

## Pruefung

```sh
build/native/recvx-debug-tests data build/validation/debug-menu.bmp
python3 tools/verify_debug.py --runtime build/native/recvx-runtime --data data
```

Der CTest-Test prueft Standard/remappten Hotkey, Wiederholungen, Softwarepfade,
Registry-Erweiterungen, Fehler und Eingabeunterdrueckung. Die Runtime-Pruefung
pausiert echte Originalraeume und prueft alle fuenf Koerper, Ausruestung, Items,
HP/Tasche/Wireframe sowie Raum/Spawnwechsel. Die Replaybefehle `debug_key` und
`debug_select` verwenden die gleichen Hotkey-/Auswahl-/Anwenden-Routinen wie die UI.
