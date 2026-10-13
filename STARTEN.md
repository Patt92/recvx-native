# Starten und Bedienung

RECVX Native ist ein Entwicklungsbuild. Er benötigt lokal exportierte
Originaldaten und ist noch kein vollständiger, spielbarer Port.

## Starten

Im Projektordner:

- **macOS (Apple Silicon):** `Start.command` öffnen.
- **Windows x64:** `Start.cmd` öffnen.

Die Paketstarter verwenden die jeweils mitgelieferten Laufzeitdaten. Beim
Spielen wird keine ISO eingelegt oder gelesen. Ein Quellcode-Clone ohne den
lokalen Ordner `data/` kann nicht gestartet werden.

`./build.sh` erzeugt auf macOS die Entwicklungs-App unter
`build/start/macos-arm64/RECVX Native.app`. `Start.command` bevorzugt diese
App; sie nutzt die aktuellen Daten, Mods und Einstellungen im Projektordner.
Auch ohne vorhandenes Paket wird die App erstellt. Ein fehlgeschlagener Build
aktualisiert den Starter nicht.

Eigenständige Pakete mit kopierten Laufzeitdaten erzeugt zusätzlich
`./build.sh --platform macos --package --output <neuer-Paketordner>`.
Die Standardausgaben für Pakete liegen unter `build/packages/`; vorhandene
Pakete werden nicht überschrieben.
Details zu Build, Datenprüfung und Plattformen stehen in
[docs/NATIVE_PORT.md](docs/NATIVE_PORT.md).

## Standardbedienung

| Aktion | Tastatur | Controller |
|---|---|---|
| Bewegen | W/S oder I/K | Linker Stick oder Steuerkreuz |
| Drehen | A/D oder J/L | Linker Stick oder Steuerkreuz |
| Laufen | Shift | B |
| Bestätigen / Interagieren | Enter | A |
| Zurück | Escape | B |
| Pause (ein/aus) | Escape oder P | Back/Select |
| Inventar | Tab | Y |
| Zwischensequenz überspringen | Space | Start |
| Debugmenü | F2 | – |

Tastaturbelegung, Controllerbelegung, Stick-Totzone und Menütasten lassen sich
im Hauptmenü unter **Einstellungen** ändern. Die vollständige Beschreibung
steht in [docs/CONTROLS.md](docs/CONTROLS.md).

## Diagnose

Das Debugmenü ist ein Entwicklungswerkzeug, kein normaler Spielfortschritt.
Es kann Räume, Positionen und Zustände gezielt inspizieren; solche Sprünge sind
kein Nachweis eines authentischen Spielverlaufs. Konfiguration und Funktionen
sind in [docs/DEBUG_MENU.md](docs/DEBUG_MENU.md) dokumentiert.

Den aktuellen Portstand und bekannte Lücken beschreibt
[docs/NATIVE_GAMEPLAY.md](docs/NATIVE_GAMEPLAY.md). Prüfergebnisse und ihre
jeweilige Aussagegrenze stehen in [docs/VALIDATION.md](docs/VALIDATION.md).
