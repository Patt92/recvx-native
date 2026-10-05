# Spiel starten

Die Starter liegen direkt hier im Projektordner:

- **macOS:** `Start.command` doppelklicken. Alternativ die App
  `build/packages/macos-universal-v0.5/RECVX Native.app` öffnen.
- **Windows:** `Start.cmd` doppelklicken. Der gesamte Windows-Paketordner
  `build/packages/windows-x64-v0.5` kann auf den Windows-Rechner kopiert werden;
  darin ebenfalls `Start.cmd` öffnen.

Die vorhandenen Pakete enthalten alle lokal exportierten Daten und SDL2.
Beim Spielen wird keine ISO benötigt. Das aktuelle Spiel ist ein Entwicklungsbuild;
Kampf, Speichern und zahlreiche spätere Ereignisse sind noch nicht fertig.

## Hauptmenü und Einstellungen

Das Standardfenster ist jetzt **1280×960**, mit scharfer ganzzahliger Skalierung.
Der interne Renderer und die Original-Fontatlanten bleiben 640×480; HD-UI ist
noch nicht umgesetzt. Der gemeinsame Spieltakt wurde auf originale 30 Hz umgestellt.

Der Start öffnet jetzt das Hauptmenü mit **Neues Spiel**, **Einstellungen** und
**Beenden**. Das Intro beginnt erst nach Neues Spiel. Eine bereits laufende alte
Version schließen und über den Starter neu öffnen.

In Einstellungen mit Links/Rechts zwischen Tastatur und Controller wechseln.
Aktion auswählen, OK drücken und die gewünschte Taste bzw. Stickrichtung eingeben.
Escape bricht die Aufnahme ab. Stick-Totzone und Menüschwelle lassen sich ebenfalls
ändern; die Belegung wird sofort angewendet und in der Paketdatei `controls.ini`
gespeichert. Die Standardbelegung kann hier wiederhergestellt werden.

Laufende Videos mit **Enter oder Space**, am Controller **A oder Start/Options**,
überspringen. Die Anzeige im Video zeigt die aktuell konfigurierten Tasten.

## Debugmenue

**F2** oeffnet das Debugmenue, auch waehrend eines Videos oder Events.
Spiel, Events, Animationen und Audio pausieren, solange es offen ist.
Hoch/Runter waehlt eine Funktion, Links/Rechts einen Wert, **Enter/Controller-A**
wendet ihn an. **F2 oder Escape/Controller-B** schliesst das Menue.
PageUp/PageDown oder LB/RB springen bei langen Listen um 20 Werte.

Verfuegbar: Charaktermodell, Equip/Waffe, Raum, Spawn, Itemmenge/Items,
HP, Tasche und Wireframe. Charakterwechsel laedt den aktuellen Raum neu.
Ein Raumsprung erhaelt Inventar und globale Eventflags.

Der Hotkey steht in `debug.ini` unter `[debug]`, `hotkey = F2`.
Im macOS-Paket liegt die Datei neben **RECVX Native.app**, im Windows-Paket
neben **recvx-runtime.exe**. Aendern und neu starten. Die Datei im Projektordner
ist die Vorlage und wird beim direkten Build-Fallback des Starters verwendet.
Details und Erweiterungsschnittstelle: [DEBUG_MENU.md](docs/DEBUG_MENU.md).

## Bedienung

W/S oder I/K: vor/zurück; A/D oder J/L: drehen; Shift: laufen.
Tab: Inventar; Enter: bestätigen/benutzen; Escape: zurück/beenden.
Controller: linker Stick/Steuerkreuz; Y: Inventar; A: bestätigen;
B: laufen/zurück.
Messer oder Pistole im Inventar ausrüsten: **R halten + Enter/Left Ctrl** zum
Angriff; am Controller **RB halten + A**. Ohne Zielen hebt Enter/A Raumitems
über die Statusansicht mit originalem Modell und Aufnahmefrage auf. Zielen und Angriff sind in Einstellungen änderbar. Space oder Start überspringt freigegebene Zwischensequenzen.

An einer Tür **Enter oder Controller-A** drücken. Während der Türsequenz
überspringt ein **erneuter Druck derselben Bestätigungstaste** die Animation
und lädt den Zielraum. Gedrückthalten überspringt sie nicht automatisch.
Die Bestätigungstaste ist über `controls.ini` konfigurierbar, einschließlich
abweichender Controller-Belegung. Bei Paketstart die Datei neben dem Paket-Starter
bearbeiten; beim Start eines direkten Builds die Datei im Projektordner.

Am Anfang den Hinweis in der Zelle mit Enter bzw. Controller-A bestätigen. Das
Inventar öffnet sich automatisch; Claire bleibt bis dahin gesperrt. Dann zweimal
Enter bzw. A: Feuerzeug auswählen und **Equip** bestätigen.
Ein schneller Inventardruck direkt nach dem Hinweis wird kurz vorgemerkt, bis
das Originalereignis die Steuerung freigibt. Inventar, Ausrüstung, HP und Tasche bleiben bei Raumwechseln
im Spielzustand erhalten.

## Ressourcen und Mods

Das untersuchbare Feuerzeug ist das originale 3D-Modell aus `ITEM1/0054.bin`
(Item-ID 55, Inventarplatz 1). Inventarsymbole stammen aus dem originalen Atlas. Die TGA-Dateien
sind exportierte Originaltexturen für Austausch/HD-Mods; BMP/PNG-Prüfbilder
unter `build/` sind keine Spielressourcen.

Türmodelle, Texturen und Bewegungsparameter kommen aus den originalen
`MRY/0071.bin` und folgenden Paketen. Es werden originale Tür-, Kamera- und
Fade-Routinen nativ kompiliert. Tür-Soundbanken und spezielle Türbeleuchtung
sind noch nicht vollständig portiert.

Siehe [Ressourcen und Mods](docs/NATIVE_ASSETS.md),
[Steuerung](docs/CONTROLS.md) und [Build-Anleitung](docs/NATIVE_PORT.md).

## Items kombinieren und Koffer öffnen

Ein Kraut auswählen → **Combine** → zweites Kraut auswählen → OK. Die originalen
Mischrezepte funktionieren in beiden Reihenfolgen. Bei einer ungültigen Mischung
bleiben beide Items erhalten; Abbrechen kehrt zum Ausgangsitem zurück.

Verschlossene Koffer: Dietrich im Inventar behalten, Koffer → **Examine** → zur
Schlossseite drehen → OK → „Use the lockpick?“ mit **Yes** bestätigen. Mehrseitige
Dialoge mit einzelnen OK-Drücken weiterblättern. Der Dietrich bleibt erhalten;
der Inhalt ersetzt den Koffer auch bei vollem Inventar. Die Aktentasche mit Knopf
öffnet sich über ihre originale Knopffrage ohne Dietrich. Pfeile beziehungsweise
D-pad/Stick drehen die 3D-Ansicht horizontal und vertikal; Escape/B bricht ab.

Details und noch offene Spezialfälle stehen in `docs/NATIVE_GAMEPLAY.md`.

## Aktueller Portstand

Die gemeinsame Event-Ausführung verwendet jetzt Raumclips für geskriptete
Spielerbewegungen, geteilte Gegner-Motionsbibliotheken und den originalen
FOR-Zähler für Positions-/Winkelinterpolation. Bodenabfragen erfassen auch
Rampen und flache Bodenflächen. Friedhof- und Steve-Tasks werden auf Rückgabe
der Steuerung geprüft; vollständige Gegner-KI, NPC-Modellteile und Effekte
fehlen noch. Das Spiel ist weiterhin kein vollständig spielbarer Port.

Inventarregeln werden aus dem Originalcode gebaut. Alle elf Untersuchungs-
umwandlungen, einschließlich der silbernen Libelle, sowie 117 gerichtete
Kombinationen sind geprüft. Details und Grenzen: `INVENTORY-COVERAGE.md` im
Paket beziehungsweise `docs/INVENTORY-COVERAGE.md` im Projekt.
