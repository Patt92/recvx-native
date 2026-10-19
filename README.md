# RECVX Native

Native Portierung von *Resident Evil CODE: Veronica X* (PS2) auf **macOS arm64**
und **Windows x64** – aufgebaut auf der Decompilation von
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).

Das Ziel ist ein eigenständiges natives Game mit originalem Verhalten:
Mechanismen werden eigenständig auf den vorhandenen nativen Besitzern
implementiert. Originalquellen sind nur Semantikreferenz; Originalkern,
generierte Funktionskörper und PS2-/VU-/GS-Nachbildung sind verboten.

> **Status (14. Oktober 2026): aktiver Port, nicht vollständig spielbar.**
> Der Schutzprüfer besteht für den aktuellen Quellbaum. Neue Teilstände umfassen
> Gegner-KI (unter anderem EN09), viele handportierte Effektprozeduren,
> Kartenrenderer und Eingabe-/Ablaufkorrekturen. Paketberichte enthalten
> gezielte Runtime- und Testnachweise; sie stammen aus unterschiedlichen Builds.
> Ein gemeinsamer Abnahmelauf des aktuellen Arbeitsbaums und eine neue
> Starter-/Paketersetzung sind nicht belegt. Windows-Runtime und mehrere
> Gegnerfamilien bleiben offen. Die Tabelle unten misst nur dokumentierte
> Mechanismen, keinen Fertigstellungsgrad des Spiels.

## Vorhandene native Teile

- **Originaldaten nativ:** Räume, Modelle, Texturen, Animationen, Nachrichten,
  ADX-Musik und -Stimmen, Sony-Soundbanken und alle 18 Filme – ohne ISO- oder
  AFS-Zugriff zur Laufzeit.
- **Event-VM:** originaler Szenario-Interpreter mit Tasks, Bedingungen, Kameras,
  Dialogen und Türanforderungen. Alle sieben D06-Inventarbefehle sind im
  produktiven Eventpfad angeschlossen, einschließlich Skriptgeschenk und
  Schlüsselverbrauch. Weitere Eventwirkungen bleiben unvollständig.
- **Inventar und Status:** Aufnahmefrage, Sortierung, Ausrüstung,
  inventarseitiges Nachladen/Munitionszählen, Kombinationen, Itemgebrauch,
  Kistentransfer, Mengenanzeige, Befehlsfenster und Heil-/Gift-EKG. Das ist
  kein Nachweis eines vollständigen Spielerdurchlaufs.
- **Untersuchung und Dateien:** originale Aktions-/Umwandlungsabläufe,
  Sichtbarkeitsmasken, Versatz, Kameradistanzen und Skalierung der Itemansicht;
  Dateiaufnahme, Leser und Ordner mit **24 Dateien / 167 Seiten**.
- **Actor/Rendering/Audio:** native Modell-, Textur-, Skin-/Pose- und
  Zeichengrundlagen, eigenständiger Face/Lip-Port, Math und Waffenblitze;
  BGM-Discregel an beiden Raumladewegen. Viele Effektprozeduren sind jetzt
  eigenständig handportiert; weitere Effekt-, Actor-, Shadow- und Soundpfade
  bleiben offen.

## Portierungsstand

Fortschritt nach tatsächlichen Paketen und Prüfgrenzen, nicht nach historischen
Generatorausgaben. **Portiert** bezeichnet den genannten Bereich, nicht das
gesamte Spiel oder sämtliche abhängigen Abläufe.

Die Prozentzahlen unten werden aus dem [Portstatus](docs/PORT_STATUS.md)
übernommen. Dieser Bericht berechnet sie mit `tools/port_check.py` aus dem
[Funktionsledger](docs/port_ledger.tsv) und den im nativen Eventpfad gebundenen
Event-Befehlen. Sie beschreiben nur den explizit belegten Belegumfang, keinen
Gesamtfortschritt des Spiels. **Gewichtet** zählt teilweise belegte
Mechanismen mit 0,5; **voll belegt** nur vollständig abgeschlossene.

<!-- port-status:begin -->
<!-- Fortschrittstabelle und Kennzahlen direkt aus docs/PORT_STATUS.md synchronisiert. -->

Expliziter Belegumfang: **86.8 %** gewichtet, **74.1 %** voll belegt (438/591 Mechanismen; kein Gesamtfortschritt).

Event-Befehle (bhScenarioJmpT): 195/223 gebunden, 17 nur konsumiert, 11 fehlen.

| Bereich | Mechanismen | Belegumfang | voll belegt | teilweise | offen |
|---|---:|---:|---:|---:|---:|
| Gegner/KI | 61/93 | 82.8 % | 65.6 % | 32 | 0 |
| Effekte | 197/218 | 95.2 % | 90.4 % | 21 | 0 |
| Events/Skript | 50/65 | 88.5 % | 76.9 % | 15 | 0 |
| Spieler/Kampf | 40/58 | 84.5 % | 69.0 % | 18 | 0 |
| Raum/Kamera/Tür | 11/32 | 67.2 % | 34.4 % | 21 | 0 |
| Menü/Items/Text | 43/54 | 87.0 % | 79.6 % | 8 | 3 |
| Modelle/Animation | 9/14 | 82.1 % | 64.3 % | 5 | 0 |
| Audio | 5/21 | 61.9 % | 23.8 % | 16 | 0 |
| Ablauf/Speichern | 22/36 | 80.6 % | 61.1 % | 14 | 0 |
| Plattform (Host-ersetzt) | 14/29 | 74.1 % | 48.3 % | 15 | 0 |

Nur dokumentierter Belegumfang, kein Gesamtfortschritt des Spiels.
<!-- port-status:end -->

<!-- native-progress:begin -->

| Bereich | Stand | Nachweis / verbleibende Grenze |
|---|---|---|
| Schutz / Build | Quellpolicy PASS; gemeinsamer Stand offen | Der aktuelle Quellbaum besteht `check_no_original_core.py`. Die jüngsten Änderungen sind nicht gemeinsam gebaut; kein aktuelles Starter-/Paketrelease belegt. |
| Inventar / Kiste / Status (D01/D02/D06–D09) | Portiert, Teilabläufe geprüft | Aufnahme, Gebrauch, Kombination, Transfer, Itembefehle und EKG besitzen positive Teilnachweise. Ein vollständiger Spieldurchlauf ist damit nicht belegt. |
| Untersuchung / Dateien / Karte (D03/D10) | Portiert, Runtime-Teile geprüft | Itemansicht, Dateiabläufe sowie MAP-Geometrie, Marker und Sprites sind in eigenen Build-Runtimes geprüft; noch keine aktuelle Starter-/Paketersetzung. |
| Spieler / Eingabe / Kampf (A/B) | Teilweise portiert | Bewegung, Kollision, Push, Treppen und Zielwahl haben eigene Nachweise. M93R-Aufschaltung/Treffer sind diagnostisch geprüft; allgemeine Spielerzustände und weitere Waffen fehlen. |
| Gegner / Actor (C/F) | Mehrere Familien aktiv; Gesamtport offen | EN01/EN04-Angriffe und EN09/Bandersnatcher sind in nativen Runtime-Teilabläufen geprüft. EN21 ist angeschlossen, seine Tests wurden zurückgestellt. EN05, EN12, EN13/13B, EN23, EN56 und weitere Dienste bleiben offen. |
| Effekte / Licht / Audio (E) | Viele native Prozeduren, Gesamtpfade offen | Der Regex-Übersetzer wurde entfernt; viele Effektprozeduren sind handportiert. Weitere IDs, Partikel-/Modellpfade, Licht-/Sounddienste und Renderinggrenzen bleiben offen. |
| Shadow / Kamera / Türen (F/G/P) | Teilweise portiert | Türsequenzen und Clipaufrufe besitzen gezielte Nachweise. Shadow-Renderer, vollständiger Kamerapfad und originale Eisraumdarstellung bleiben offen. |
| System / Speichern | Unvollständig | Eingabe- und Ablaufkorrekturen sind teilweise geprüft; originales Speichern/Laden und vollständige Systemzustände fehlen. |
| Windows x64 | Kein aktueller Nachweis | Vorhandene Windows-Binaries stammen aus älteren Builds; der aktuelle Arbeitsbaum ist nicht auf Windows ausgeführt. |

<!-- native-progress:end -->

**Prüfstand:** Paketläufe unterscheiden sich je Build. Der Effekt-Handport meldet
82/83 CTest, EN09 86/86 und der Gegner-Audit 85/86; die roten Fälle und ihre
Vorherstände sind in den jeweiligen Berichten dokumentiert. Diese Zahlen sind
keine gemeinsame Abnahme des aktuellen Arbeitsbaums. Der Policy-Quellcheck
besteht; aktuelle Runtime-/Paket- und Windows-Abnahme fehlen.

Ausgewählte Nachweise: [EN09/Bandersnatcher](work/native_port_EN09_world.result.md),
[Gegner-Audit](work/native_port_enemy_audit_fixes.result.md),
[Effektprozeduren](work/native_port_effsub0_handport.result.md),
[MAP-Renderer](work/native_port_D10_map_render.result.md),
[Eingabesperre RM_1020](work/native_port_discard_question.result.md).
Die automatisch berechnete Ledger-Tabelle oben beschreibt dokumentierte
Mechanismen, nicht Spielvollständigkeit.

## Schnellstart

Der lokale Build ist wieder möglich, solange die verbindliche Schutzprüfung
besteht. Die Startskripte prüfen ebenfalls die Policy und benötigen ein
passendes natives Binary sowie lokale Spieldaten. Ein aktualisierter
Paketstand ist für die jüngsten Änderungen noch nicht belegt; alte Pakete
nicht mit dem aktuellen Quell-/Buildstand verwechseln.

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
| [docs/PS2_HARDWARE.md](docs/PS2_HARDWARE.md) | Historische Hardware-/Disassembly-Referenz, keine Runtime-Architektur |
| [docs/AUDIO.md](docs/AUDIO.md) | Musik, Soundbanken, Filme |
| [docs/NATIVE_ASSETS.md](docs/NATIVE_ASSETS.md) | Ressourcen, Mods, HD-Texturen |
| [docs/CONTROLS.md](docs/CONTROLS.md) | Tastatur- und Controllerbelegung |
| [docs/DEBUG_MENU.md](docs/DEBUG_MENU.md) | Debugmenü |
| [docs/VALIDATION.md](docs/VALIDATION.md) | Testgruppen und Prüfwerkzeuge |
| [docs/INVENTORY-COVERAGE.md](docs/INVENTORY-COVERAGE.md) | Historische Item-/Rezeptabdeckung, kein aktueller Portnachweis |

## Projektstruktur

| Pfad | Inhalt |
|---|---|
| `native/` | Native Engine und Host-Schicht |
| `src/`, `include/` | Erhaltene Original-Decompilation |
| `tools/` | Datenwerkzeuge, Extraktion, Build und Schutz-/Prüfwerkzeuge |
| `tests/` | Regressionstests und Replays |
| `docs/` | Dokumentation |
| `mods/` | Optionale austauschbare Ressourcen |
| `data/`, `build/` | Lokale Spieldaten und Buildartefakte |
| `archived/FORBIDDEN_DONTUSE/` | Gesperrte, recoverable Quarantäne; nicht lesen oder verwenden |

## Lizenz

Die ursprüngliche Projektlizenz liegt in [LICENSE](LICENSE). Lizenzen der
Abhängigkeiten werden den Paketen beigelegt.
