# RECVX Native 0.5

Native Portierungsarbeit für Resident Evil CODE: Veronica X (PS2), ausgehend von
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).
Der aktuelle Entwicklungsbuild läuft auf Windows x64 und macOS (Intel/Apple Silicon).
Er ist noch kein vollständig spielbares Spiel.

Der Start verwendet die originalen RM_0000-Ereignisse: Intro/Film, dunkle Zelle,
Feuerzeughinweis, Inventar/Ausrüsten, Rodrigo-Sequenz und Rückgabe der Steuerung.
Originale Modelle, Texturen, Animationen, Inventarsymbole, Nachrichten,
ADX-Musik/Stimmen und die 18 Filme mit ihrem PCM-Ton werden nativ verarbeitet.
Dialoge verwenden weiße Schrift mit schwarzer Kontur ohne Hintergrundfläche.
Leere Inventarfelder verwenden den Original-Atlas; die zwei Taschenplätze und
der längere Rahmen erscheinen erst mit der Tasche. Effekte, Gesichtssystem,
Soundbanken/Schritte, Kampf, Raumwechsel, Speicherstände und weitere Menüseiten
sind noch unvollständig. Ausführliche Grenzen stehen in der Gameplay-Dokumentation.

## Starten und bauen

Voraussetzungen: Python 3.12+, CMake, C/C++-Compiler (Apple Command Line Tools oder
Visual Studio mit C++); für Windows-Crossbuilds zusätzlich Ninja. SDL2 ist über
Version und SHA256 festgelegt und wird beim ersten Build geladen. Der Spielstart
benötigt keine Netzwerkverbindung.

```sh
# macOS: Universal-Build, Tests und vollständiges lokales App-Paket
./build.sh --platform macos --verify-data --package
# Windows-Crossbuild auf macOS/Linux
./build.sh --platform windows --verify-data --package
```

```powershell
# Windows: nativer Release-Build, Tests und Paket
./build.ps1 --platform windows --verify-data --package
```

Die Pakete liegen in `build/packages/macos-universal-v0.5` und
`build/packages/windows-x64-v0.5`. macOS startet über `RECVX Native.app`, Windows
über `Start.cmd`. Jeder Build enthält den vollständigen lokalen Datenexport;
während des Spielstarts wird weder ISO noch AFS gelesen.

Tastatur und SDL-Gamepads werden unterstützt; Windows nutzt auch XInput.
Die Belegung und Stick-Totzone sind in `controls.ini` konfigurierbar
([Steuerungsdokumentation](docs/CONTROLS.md)).
Tab öffnet das Inventar, Pfeiltasten wählen, Enter bestätigt/interagiert, Escape
geht zurück. W/S oder I/K bewegt Claire vor/zurück, A/D oder J/L dreht, Shift läuft schnell.
Space verwendet den originalen Cinematic-Skip. Diagnosefunktionen sind dokumentiert.

## Ordner

| Ordner | Zweck |
|---|---|
| `native/`, `src/`, `include/` | Native Implementierung und erhaltene Decompilation |
| `tools/`, `tests/`, `docs/` | Build-/Asset-Werkzeuge, Prüfungen, Dokumentation |
| `data/` | Vollständige lose Originaldaten und konvertierte Laufzeit-Assets, lokal ignoriert |
| `mods/` | Optionale austauschbare Texturen, Audio-/Filmdaten und unterstützte Modellpakete |
| `build/` | Erzeugte Binärdateien, Abhängigkeiten und Pakete, lokal ignoriert |
| `archived/` | ISO, alter PS2-Build, alte Exporte/Versionen und frühere Git-Historie; ignoriert, keine produktive Abhängigkeit |

Das Repository wurde als neues Git initialisiert. Die vorherige Historie und deren
HEAD sind unter `archived/upstream-git` und `archived/relocations.json` erhalten.
Spieldaten werden weder von Git erfasst noch mit dem Quellcode veröffentlicht.

- [Build, Architektur und Validierung](docs/NATIVE_PORT.md)
- [Gameplay, Startsequenz und offene Funktionen](docs/NATIVE_GAMEPLAY.md)
- [Assets, HD-Texturen und Modellgrenzen](docs/NATIVE_ASSETS.md)
- [Originalmodelle und Bewegung](docs/NATIVE_PLAYER.md)
- [Audio und Filme](docs/AUDIO.md)

Die ursprüngliche Projektlizenz bleibt in `LICENSE` erhalten. Drittanbieter-Lizenzen
liegen bei den jeweiligen Abhängigkeiten und werden den Paketen beigelegt.
