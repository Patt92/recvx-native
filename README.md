# RECVX Native

Native Portierung von *Resident Evil CODE: Veronica X* (PS2) auf **macOS arm64**
und **Windows x64** – aufgebaut auf der Decompilation von
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).

Das Ziel ist ein eigenständiges natives Game mit originalem Verhalten:
Mechanismen werden eigenständig auf den vorhandenen nativen Besitzern
implementiert. Originalquellen sind nur Semantikreferenz; Originalkern,
generierte Funktionskörper und PS2-/VU-/GS-Nachbildung sind verboten.

> **Status (9. Oktober 2026):** Cleanup und eigenständige native Ablösung.
> Verbotene unproduktive Kern-/Hardwarepfade sind quarantiniert; Build,
> Packaging und Start sind bis zur Ablösung der verbliebenen produktiven
> Alt-Teilports durch den Schutzprüfer gesperrt. Die untenstehenden alten
> Abdeckungs-/Sweepzahlen sind keine Freigabe des erlaubten nativen Ports.
> Aufgaben und Parallelaufteilung: [Roadmap](docs/NATIVE_REIMPLEMENTATION_ROADMAP.md).

## Vorhandene Funktionen (noch keine aktuelle Freigabe)

- **Originaldaten nativ:** Räume, Modelle, Texturen, Animationen, Nachrichten,
  ADX-Musik und -Stimmen, Sony-Soundbanken und alle 18 Filme – ohne ISO- oder
  AFS-Zugriff zur Laufzeit.
- **Event-VM:** originaler Szenario-Interpreter mit Tasks, Bedingungen, Kameras,
  Dialogen und Türanforderungen; alle 205 Räume laden. Die native
  Actor-Control bindet zusätzlich originale Motion-/Morph-Fälle.
- **Spieler:** Bewegung, Treppen, Kisten und Kollision über die originalen
  Routinen aus `player.c` und `hitchk.c`; Nahkampf- und Schusswaffen mit
  originaler Zielwahl.
- **Inventar:** Statusbildschirm mit originalen Sprites, Kombinationen,
  Untersuchen, Dateiordner, Item-Box und Aufnahme-Ablauf.
- **Gegner:** Der verbotene Original-Dispatcher ist entfernt. Vollständige
  eigenständige native Gegnerabläufe einschließlich EN07 sind offen;
  verbleibende Alt-Teilports müssen auf die gemeinsamen Besitzer übertragen werden.
- **Türen und Musik:** originale Türsequenzen, BGM-Wechsel und Raumklänge.

## Portierungsstand

Abdeckung der Originalfunktionen nach Kategorie, gemessen mit
`python3 tools/port_check.py`. *Gewichtet* zählt teilweise portierte
Funktionen anteilig, *streng* nur vollständig belegte. Die Zahlen messen
Codeabdeckung, nicht semantische Vollständigkeit oder erfolgreiches Durchspielen.

Historische Prüfung vor dem verbindlichen Cleanup: 205/205 Räume mit je 600 Ticks, ohne Abbruch
oder blockierte Eventbefehle. macOS arm64: 48/48 CTest. Windows x64:
`recvx_game` kompiliert; Runtime-Link und Laufzeitprüfung stehen noch aus.

<!-- port-status:begin -->
<!-- Automatisch erzeugt: python3 tools/port_check.py --readme README.md -->

| Kategorie | Fortschritt | Gewichtet | Streng | Funktionen | Stand |
|---|---|---:|---:|---:|---|
| Spieler/Kampf | `██████████` | 95,2 % | 90,8 % | 159/166 | Wandkollision, Kistenschieben, Waffen/Zielwahl; allgemeine Spieler-Work und weitere Zustandsroutinen offen. |
| Events/Skript | `█████████░` | 91,7 % | 90,7 % | 251/272 | Originale Event-VM, gemeinsame Actor-Control-Fälle und 150/223 Befehle gebunden; weitere Befehle/Abläufe offen. |
| Menü/Items/Text | `███████░░░` | 70,4 % | 63,4 % | 130/241 | Itemregeln, Dateiordner, Statusansichten; Statusmenü-Zustandsmaschine und Map offen. |
| Effekte | `█████░░░░░` | 52,0 % | 51,7 % | 175/361 | Einzelne Effekt-IDs; allgemeiner Effektlebenszyklus, Licht und viele IDs offen. |
| Raum/Kamera/Tür | `█████░░░░░` | 51,2 % | 39,5 % | 64/188 | Raumwechsel, Eventkameras, Türsequenzen; weitere Originaldienste offen. |
| Modelle/Animation | `█████░░░░░` | 48,7 % | 16,9 % | 11/88 | Gemeinsame Actor-Assets, Skinning und Gegner-Morph-Draw; Face/Lip, Shadow und Player-Morph offen. |
| Gegner/KI | `██░░░░░░░░` | 23,4 % | 22,9 % | 305/1357 | Original-Dispatcher entfernt; vollständige eigenständige Gegnerabläufe und Bindungen noch offen. |
| Audio | `██░░░░░░░░` | 16,8 % | 0,0 % | 0/164 | Host-Mixer mit einzelnen Originalbindungen; Original-Audiofunktionen nicht vollständig belegt. |
| Ablauf/Speichern | `█░░░░░░░░░` | 11,9 % | 0,1 % | 3/331 | Einzelne Ablaufpfade; Save-/Systemzustandsmaschinen größtenteils offen. |
| **Engine gesamt** | `█████░░░░░` | **46,6 %** | **42,7 %** | **1098/3168** | 150/223 Event-Befehle gebunden. |

<!-- port-status:end -->

Die Zahlen sind ein historischer Stand, der noch generierte Funktionskörper
anrechnete. Der Prüfer zählt diese nicht mehr als nativen Port; eine neue
Abdeckung ist erst nach Prüfung der verbleibenden Evidenz sinnvoll. Die
Tabelle wird mit `python3 tools/port_check.py --readme README.md` erzeugt
und ersetzt kein Semantik-Audit.

Einzelheiten und die größten offenen Funktionen: [docs/PORT_STATUS.md](docs/PORT_STATUS.md).

## Schnellstart

Start und Build sind derzeit durch die verbindliche Port-Policy gesperrt.
Nach vollständiger Ablösung der verbotenen Altpfade:

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

- **Originalsemantik, eigene native Implementierung.** Originalcode,
  Originaldaten und Disassembly sind die Referenz. Keine generierten
  Funktionskörper, kein Originalkern und keine zweite Spielwelt.
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
| [docs/PS2_HARDWARE.md](docs/PS2_HARDWARE.md) | Hardwaregrenze: VU-Mikrocode, GS-Register, Disassembly |
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
