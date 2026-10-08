# RECVX Native

Native Portierung von *Resident Evil CODE: Veronica X* (PS2) auf **macOS arm64**
und **Windows x64** – aufgebaut auf der Decompilation von
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).

Das Ziel ist die originale Engine, nicht ein Remake: Spiellogik, Zustands-
maschinen, Tabellen und Berechnungen werden aus dem Originalcode übernommen,
meist direkt per Generator aus den C-Quellen extrahiert. Der Host ersetzt nur
das, was die PS2-Hardware bereitgestellt hat – Fenster, Eingabe, Audioausgabe,
Rendering und Dateizugriff.

> **Status:** Entwicklungsbuild. Der Prolog und die ersten Spielbereiche laufen
> über die originalen Raumskripte; viele Systeme (Effekte, Gegnerfamilien,
> Speichern, Karte) sind noch offen. Ein vollständiges Durchspielen ist nicht
> möglich.

## Was bereits läuft

- **Originaldaten nativ:** Räume, Modelle, Texturen, Animationen, Nachrichten,
  ADX-Musik und -Stimmen, Sony-Soundbanken und alle 18 Filme – ohne ISO- oder
  AFS-Zugriff zur Laufzeit.
- **Event-VM:** originaler Szenario-Interpreter mit Tasks, Bedingungen, Kameras,
  Dialogen und Türanforderungen; alle 205 Räume laden.
- **Spieler:** Bewegung, Treppen, Kisten und Kollision über die originalen
  Routinen aus `player.c` und `hitchk.c`; Nahkampf- und Schusswaffen mit
  originaler Zielwahl.
- **Inventar:** Statusbildschirm mit originalen Sprites, Kombinationen,
  Untersuchen, Dateiordner, Item-Box und Aufnahme-Ablauf.
- **Gegner:** Zombie-KI, Trefferreaktionen, Tod und Greifen nach `en01.c`.
- **Türen und Musik:** originale Türsequenzen, BGM-Wechsel und Raumklänge.

## Portierungsstand

Abdeckung der Originalfunktionen nach Kategorie, gemessen mit
`python3 tools/port_check.py`. *Gewichtet* zählt teilweise portierte
Funktionen anteilig, *streng* nur vollständig belegte. Die Zahlen messen
Abdeckung, nicht semantische Vollständigkeit.

<!-- port-status:begin -->
<!-- Automatisch erzeugt: python3 tools/port_check.py --readme README.md -->

| Kategorie | Fortschritt | Gewichtet | Streng | Funktionen | Stand |
|---|---|---:|---:|---:|---|
| Spieler/Kampf | `██████████` | 95,2 % | 90,8 % | 159/166 | Wandkollision, Kistenschieben, Waffen/Zielwahl; allgemeine Spieler-Work und weitere Zustandsroutinen offen. |
| Events/Skript | `█████████░` | 91,7 % | 90,7 % | 251/272 | Event-VM und Befehle portiert; Common-Control und restliche Befehle offen. |
| Menü/Items/Text | `███████░░░` | 70,3 % | 63,4 % | 130/241 | Itemregeln, Dateiordner, Statusansichten; Statusmenü-Zustandsmaschine und Map offen. |
| Effekte | `█████░░░░░` | 52,0 % | 51,7 % | 175/361 | Einzelne Effekt-IDs; allgemeiner Effektlebenszyklus, Licht und viele IDs offen. |
| Raum/Kamera/Tür | `█████░░░░░` | 50,3 % | 39,5 % | 64/188 | Raumwechsel, Eventkameras, Türsequenzen; weitere Originaldienste offen. |
| Modelle/Animation | `████░░░░░░` | 42,8 % | 16,9 % | 11/88 | Ressourcen und ausgewählte Animationspfade; Skin/Morph/Chunk-Renderer offen. |
| Gegner/KI | `███░░░░░░░` | 26,2 % | 25,9 % | 342/1357 | Zombie/Hund teilweise; die meisten enXX-Zustandsroutinen fehlen. |
| Audio | `██░░░░░░░░` | 16,4 % | 0,0 % | 0/164 | Host-Mixer mit einzelnen Originalbindungen; Original-Audiofunktionen nicht vollständig belegt. |
| Ablauf/Speichern | `█░░░░░░░░░` | 11,9 % | 0,1 % | 3/331 | Einzelne Ablaufpfade; Save-/Systemzustandsmaschinen größtenteils offen. |
| **Engine gesamt** | `█████░░░░░` | **47,5 %** | **43,9 %** | **1135/3168** | 148/223 Event-Befehle gebunden. |

<!-- port-status:end -->

Die Tabelle wird von `python3 tools/port_check.py --readme README.md`
erzeugt (nicht von Hand ändern) und ersetzt kein Semantik-Audit. Sie muss
vor jedem Commit neu erzeugt werden.

Einzelheiten und die größten offenen Funktionen: [docs/PORT_STATUS.md](docs/PORT_STATUS.md).

## Schnellstart

Spielen (vorhandene Pakete mit lokalen Spieldaten):

- **macOS:** `Start.command`
- **Windows:** `Start.cmd`

Bedienung, Tasten und Spieldaten: [STARTEN.md](STARTEN.md).

Selbst bauen (Python 3.12+, CMake, C/C++-Compiler):

```sh
./build.sh --platform macos --verify-data --package
```

```sh
./build.sh --platform windows --verify-data --package
```

`--test` baut und startet zusätzlich die Regressionstests. Details:
[docs/NATIVE_PORT.md](docs/NATIVE_PORT.md).

Spieldaten werden nicht mit dem Quellcode veröffentlicht; ein reiner
Quellcode-Clone ist ohne lokalen `data/`-Ordner nicht lauffähig.

## Prinzipien

- **Original vor Nachbau.** Originalcode, Originaldaten und Disassembly sind
  die Referenz. Wo möglich wird Originalcode per Generator übernommen statt
  nachprogrammiert.
- **Keine Ersatzlogik.** Keine Heuristiken, keine Raum-Sonderfälle, keine
  „ungefähr passenden“ Platzhalter. Fehlt ein Mechanismus, bleibt er als Lücke
  sichtbar.
- **Plattformcode bleibt Plattformcode.** Host-Anpassungen betreffen nur
  Schnittstellen und Datenformate, nie Spielentscheidungen.

Die verbindliche Prüfmethode steht in [QUALITY.md](QUALITY.md).

## Dokumentation

| Seite | Inhalt |
|---|---|
| [STARTEN.md](STARTEN.md) | Start, Bedienung, Diagnose |
| [docs/NATIVE_PORT.md](docs/NATIVE_PORT.md) | Build, Pakete, Architektur |
| [docs/NATIVE_GAMEPLAY.md](docs/NATIVE_GAMEPLAY.md) | Portstand der Spielsysteme und offene Lücken |
| [docs/NATIVE_PLAYER.md](docs/NATIVE_PLAYER.md) | Spielermodell, Skinning, Animation |
| [docs/AUDIO.md](docs/AUDIO.md) | Musik, Soundbanken, Filme |
| [docs/NATIVE_ASSETS.md](docs/NATIVE_ASSETS.md) | Ressourcen, Mods, HD-Texturen |
| [docs/CONTROLS.md](docs/CONTROLS.md) | Tastatur- und Controllerbelegung |
| [docs/DEBUG_MENU.md](docs/DEBUG_MENU.md) | Debugmenü |
| [docs/VALIDATION.md](docs/VALIDATION.md) | Testgruppen und Prüfwerkzeuge |
| [docs/INVENTORY-COVERAGE.md](docs/INVENTORY-COVERAGE.md) | Generierte Item-/Rezeptabdeckung |

## Projektstruktur

| Pfad | Inhalt |
|---|---|
| `native/` | Native Engine und Host-Schicht |
| `src/`, `include/` | Erhaltene Original-Decompilation |
| `tools/` | Generatoren, Extraktion, Build und Prüfwerkzeuge |
| `tests/` | Regressionstests und Replays |
| `docs/` | Dokumentation |
| `mods/` | Optionale austauschbare Ressourcen |
| `data/`, `build/`, `archived/` | Lokal, nicht im Repository |

## Lizenz

Die ursprüngliche Projektlizenz liegt in [LICENSE](LICENSE). Lizenzen der
Abhängigkeiten werden den Paketen beigelegt.
