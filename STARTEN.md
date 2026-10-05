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

## Bedienung

W/S oder I/K: vor/zurück; A/D oder J/L: drehen; Shift: laufen.
Tab: Inventar; Enter: bestätigen/benutzen; Escape: zurück/beenden.
Controller: linker Stick/Steuerkreuz; Y: Inventar; A: bestätigen;
B: laufen/zurück. Space oder Start überspringt freigegebene Zwischensequenzen.

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
