# Build, Pakete und Architektur

RECVX Native führt Host-C/C++-Code aus und liest vorbereitete lose
Spieldateien. Es gibt keine PS2-CPU-Emulation; ISO und AFS-Container werden
zur Laufzeit nicht geöffnet. Die Originalquellen unter `src/` bleiben die
Referenz, aus der Generatoren Routinen in den nativen Code übernehmen.

## Voraussetzungen

- Python 3.12+, CMake, C/C++-Compiler
- macOS: Apple Command Line Tools
- Windows: Visual Studio mit C++-Workload
- Windows-Crossbuild von macOS/Linux: zusätzlich Ninja

Der erste Build lädt festgelegte Abhängigkeiten (SDL2 2.32.10, für den
Crossbuild Zig 0.16.0; beide per Prüfsumme fixiert). Danach braucht weder der
Build noch das Spiel eine Netzwerkverbindung.

## Bauen und Pakete erzeugen

```sh
./build.sh --platform macos --verify-data --package
```

```sh
./build.sh --platform windows --verify-data --package
```

```powershell
./build.ps1 --platform windows --verify-data --package
```

| Option | Wirkung |
|---|---|
| *(keine)* | Nur Laufzeit-Binary bauen |
| `--test` | Testziele bauen und CTest ausführen |
| `--verify-runtime` | Längere Replay-Suite mit Originaldaten |
| `--verify-data` | Manifest der Spieldaten prüfen |
| `--data <pfad>` | Anderen Datenbaum verwenden |
| `--package` | Startfähiges Paket unter `build/packages/` erzeugen |
| `--output <neu>` | Anderes Paketziel; vorhandene Ziele werden nie überschrieben |

macOS-Builds sind ausschließlich arm64 (Deployment Target 11.0, SDL2 arm64,
kein Rosetta) und lokal ad hoc signiert, nicht notarisiert. Windows-Builds
sind x64; Crossbuilds müssen auf Windows oder unter Wine getestet werden.

Direkte Entwicklungsbefehle:

```sh
cmake -S . -B build/native -DRECVX_FETCH_SDL2=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
```

```sh
cmake --build build/native --parallel 8
```

```sh
ctest --test-dir build/native --output-on-failure
```

```sh
build/native/recvx-runtime --data data
```

## Spieldaten

`data/` ist der vollständige lokale Export: alle Disc-Dateien, aufgeteilte
AFS-Einträge, 205 expandierte Räume, dekodierte Texturen und native
Filmpakete. `asset-manifest.json` enthält Größen und SHA-256 jeder Datei.

```sh
python3 tools/prepare_native_data.py --output data --verify
```

`archived/` enthält ISO, Quellenextraktion, alte Toolchains, historische
Arbeitsstände und Belege. Kein normaler Build oder Start hängt davon ab.
`data/`, `build/` und `archived/` sind nicht im Git-Repository.

## Architektur

| Bibliothek / Programm | Aufgabe |
|---|---|
| `recvx_data` | Begrenztes Binärlesen, Raumexpansion, Archiv- und Exportwerkzeuge |
| `recvx_math` | Ninja-Mathematik und Matrizen |
| `recvx_scene` | Modell-/Texturdekodierung, Motion, Skinning, Software-Renderer |
| `recvx_game` | Event-VM, Inventar, Nachrichten, Welt, Kampf, Gegner, Audio, Filme |
| `recvx_input` | Tastatur/Controller, Menü- und Cinematic-Weiterleitung |
| `recvx-runtime` | SDL-Fenster, Eingabe, Audioausgabe, Spielschleife |

### Originalcode im Port

Der Großteil der Spiellogik stammt nicht aus Nachbauten, sondern aus
Generatoren unter `tools/generate_*.py`. Sie extrahieren Funktionen,
Tabellen und Strukturen aus `src/ps2/veronica/prog/*.c` und erzeugen
`.inc`-Dateien, die in dünne C-Brücken (`native/*_core.c`) eingebunden werden.
Die Brücken liefern nur die Dienste, die im Original die Engine bereitstellt
(Arbeitsspeicher, Ninja-Aufrufe, Zeichnen, Sound). Welche Originalfunktionen
so gebunden sind, erfasst `docs/port_ledger.tsv`; `tools/port_check.py`
wertet das aus.

### Takt

Das Original wartet pro Spielschritt zwei VSyncs. Der Port läuft mit einem
festen 30-Hz-Spieltakt (`native/timing.h`); Filme, ADX-Fades und der
48-kHz-Mixer (1600 Stereo-Frames pro Tick) folgen derselben Uhr. Gezeichnet
wird nur bei neuem Tick. Die getrennte 60-Hz-VSync-Zeit des Originals
(`sys->time`) wird für Ereigniszeitgeber in VSync-Einheiten mitgeführt.

### Darstellung

Software-Renderer mit interner Auflösung 640×480, ganzzahlig skaliert (Fenster
standardmäßig 1280×960). Optional Supersampling und Texturfilter
(`video.ini`). Material- und Blendmodi folgen den originalen Chunk-/GS-Tabellen.

## Diagnose

| Option | Zweck |
|---|---|
| `--in-game` | Hauptmenü überspringen |
| `--preview` | Raum-/Modellinspektor mit freier Kamera |
| `--room`, `--player`, `--motion`, `--frame`, `--inventory` | Inspektion |
| `--headless --replay <datei> --frames <n>` | Deterministischer Lauf ohne Fenster |
| `--state <neu.json>`, `--screenshot <neu.bmp>` | Zustand bzw. Bild ausgeben |
| `--snapshots <ordner>` | Snapshot-Ordner (F5/F9) |
| `--enemy-ai on\|off`, `--enemy-grab on` | Gegner-Schalter |

Headless-Läufe führen jeden Simulationsschritt aus und zeichnen nur das
angeforderte letzte Bild. Ausgabedateien werden nie überschrieben.
Diagnostische Positionierung ist kein Nachweis eines echten Spielverlaufs.
Replays beginnen mit `0 new_game`.
