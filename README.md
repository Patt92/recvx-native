# RECVX Native

Native Portierung von *Resident Evil CODE: Veronica X* (PS2) auf **macOS arm64**
und **Windows x64** – aufgebaut auf der Decompilation von
[AshfordFamily/recvx-decomp](https://github.com/AshfordFamily/recvx-decomp).

Das Ziel ist ein eigenständiges natives Game mit originalem Verhalten:
Mechanismen werden eigenständig auf den vorhandenen nativen Besitzern
implementiert. Originalquellen sind nur Semantikreferenz; Originalkern,
generierte Funktionskörper und PS2-/VU-/GS-Nachbildung sind verboten.

> **Status (9. Oktober 2026): stabil baubar, aber noch nicht vollständig spielbar.**
> Inventar, Itembefehle, EKG, Untersuchung und Dateiordner sind eigenständig
> nativ umgesetzt und mit positiven Teilabläufen geprüft. macOS arm64:
> **44/44 CTest**, **205/205 Räume laden**, **0 blockierte Eventbefehle**.
> Windows x64: Binary gebaut und laut Prüfbericht auditiert; ein tatsächlicher
> Windows-Lauf fehlt. Kampf, Kollision, Treppen, Effekte und vollständige
> Gegnerabläufe bleiben offen. Nächster Gameplay-Auftrag: **D04 Computerterminal**.
> Aufgaben und Zuständigkeiten: [Roadmap](docs/NATIVE_REIMPLEMENTATION_ROADMAP.md).

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
  kein Nachweis des noch fehlenden Spielerkampfs oder Itembox-Deckelablaufs.
- **Untersuchung und Dateien:** originale Aktions-/Umwandlungsabläufe,
  Sichtbarkeitsmasken, Versatz, Kameradistanzen und Skalierung der Itemansicht;
  Dateiaufnahme, Leser und Ordner mit **24 Dateien / 167 Seiten**.
- **Actor/Rendering/Audio:** native Modell-, Textur-, Skin-/Pose- und
  Zeichengrundlagen, eigenständiger Face/Lip-Port, Math und Waffenblitze;
  BGM-Discregel an beiden Raumladewegen. Vollständige Actor-/Gegnerbindungen,
  Face-Korrekturen, Morph, Shadow und weitere Effekt-/Soundpfade bleiben offen.

## Portierungsstand

Fortschritt nach tatsächlichen Paketen und Prüfgrenzen, nicht nach historischen
Generatorausgaben. **Portiert** bezeichnet den genannten Bereich, nicht das
gesamte Spiel oder sämtliche abhängigen Abläufe.

<!-- native-progress:begin -->

| Bereich | Stand | Nachweis / verbleibende Grenze |
|---|---|---|
| Schutz / Build | PASS | macOS-Quellen, Build und Runtime-Binary geprüft. Alle 22 gelisteten Funktionskörper-Generatoren quarantiniert; Build verwendet native Datenmodule. |
| Inventar / Kiste / Inventar-Events (D01/D02/D06/D07) | Portiert, positive Teilabläufe geprüft | Aufnahme, Gebrauch, Kombination, Transfer und sieben Eventmutationen wirken auf derselben Inventory. Vollständige Aufnahme-Replays hängen teilweise an Kollision/Kampf; Itembox-Objektablauf B05 bleibt offen. |
| EKG / Mengen / Befehlsfenster (D08/D09) | Portiert, Ausgabe geprüft | Heil-/Giftverlauf, sichtbare EKG-Ausgabe, Mengen/Farbbanken und Itembefehle. |
| Untersuchung / Dateien (D03) | Portiert, Runtime-Abläufe geprüft | 156 Untersuchungsfälle mit 11 Umwandlungen; Itemansicht mit originalen Größen. Dateiaufnahme, Katalog und Ordner-Replays bestanden. |
| Computerterminal (D04) | In Arbeit, produktiv noch Lücke | Plan vorhanden; Fensterautomat, Interpreter, Tastatur, Darstellung und Scanner-/Passwortabläufe noch nicht integriert. |
| Spielerbewegung / Kampf (A/B) | Wesentliche Mechanismen offen | Wand-/Bodenauflösung, Push, Treppen/Leitern, Zielwahl und vollständige Angriffs-/Trefferabläufe fehlen. Inventar-Nachladen ist kein Kampfport. |
| Gegner / Actor (C/F) | Teilweise portiert | Native-Portierungs-Kern vorhanden: Dispatcher `recvx_enemy_control` (Port von `bhControlEnemy`/`bhJumpEnemy`, `eneset.c:344`) liegt vor und meldet jede noch offene Id namentlich als Portierungslücke statt zu verschweigen. Subplayer-Pfad (T5, 45 Id./195 Works) portiert; Gegnerschaden `en01` über CTest belegt; gemeinsamer Actor-/Morph-Pfad portiert; EN07-Abnahme RM_0130 im erlaubten nativen Pfad erledigt. Gegner-Lücken über alle 205 Räume gemessen (120 Ticks je Raum): 5× Basic-Modell, 7× Skript-Motion-Rückgabe (IDs 5, 11×2, 20, 21), 9× `bhEff017`, je 1× `pdGetPeripheral`/`StartVibrationEx`, `bhEne01_LinkFireEffect`×2; Skelett-Mismatch 0. Offen: `en04` (Hund, Dispatch-Index bewusst NULL), `en02` ff. und vollständige Hunde-KI, Face/Shadow/Subplayer-Object-Morph, Restzustände der KI. |
| Raum / Kamera / Türen (G/P04) | Raumladen geprüft, Sequenzen offen | 205/205 Räume laden; originale Türdemos und kamerabezogene Clipbereiche fehlen. |
| Effekte / Audio (E) | Teilweise portiert | Native Audio-/Zeichengrundlagen und BGM-Discregel vorhanden; Effektzustände, gemeinsame Effekttexturen, Kollisionsabfragen und weitere SFX-Bindungen offen. |
| Gameover / System / Speichern (D05) | Unvollständig | Snapshot-Replay geprüft; Tod/Retry und vollständige originale System-/Saveabläufe nicht abgeschlossen. |
| Windows x64 | Gebaut, laut Bericht auditiert | Linkeingänge und PE-Vergleichslink geprüft. Tatsächlicher Windows-Lauf und unabhängige Prüfung des neuen Auditwegs stehen aus. |

<!-- native-progress:end -->

**Prüfstand:** Im vorhandenen macOS-Testprotokoll bestehen **44/44 CTest**;
die Suite enthält auch Lückenverträge, deren Erfolg keine Spielfunktion
beweist. Die Berichte dokumentieren **24/24 Policy-Tests**, neun Windows-
Auditprüfungen und **205/205 ladende Räume mit 0 blockierten Befehlen**.
Bestandene native Runtime-Teilabläufe: `verify_events`, `verify_pickup_slide`,
`verify_progression`, `verify_snapshot`, `verify_item_catalog`,
`verify_file_pickup` und `verify_file_folder`.

**Noch blockiert:** vollständige `verify_pickups` (Kollision),
`verify_ammunition`/`verify_combat` (Spielerkampf), `verify_detector`
(Effekte) und `verify_continuity` (Treppenmotion). Direkt gestartete
Aufnahmen sind nur Teilnachweise. Der statische Besitzer-Skriptscan ist
unvollständig; ein Raumsweep deckt nicht alle Skriptpfade ab. Es gibt keinen
Durchspielnachweis und keinen tatsächlichen Windows-Runtime-Nachweis.

Eine belastbare Gesamt-Prozentzahl ist derzeit nicht ausgewiesen:
`tools/port_check.py` und das [historische Funktionsledger](docs/port_ledger.tsv)
enthalten noch alte Portbelege beziehungsweise Kommentarverweise. Sie müssen
vor einer neuen Prozentbewertung an den aktuellen nativen Wirkungen geprüft
werden. Die früheren Prozentbalken und Erfolge über den verbotenen Kern sind
keine aktuelle Portabnahme. Diese Statusübersicht wird anhand von Code,
Testprotokollen und Berichten gepflegt, nicht vom alten README-Prozentexport.

Belege: [Inventar](work/native_port_inventory_tests.result.md),
[Untersuchung/Dateien](work/native_port_D03_files_inspection.result.md),
[Windows-Audit](work/native_port_windows_audit.md) und aktuelle Ergänzungen in
[AGENTS.md](AGENTS.md). Nächster Auftrag: [D04-Plan](work/native_port_D04_computer.plan.md).

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
