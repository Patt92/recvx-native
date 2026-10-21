# Forbidden-Quarantäne, Schutzprüfer und native Roadmap

Stand: 2026-10-09. Cleanup-Auftrag durchgeführt; kein Gameplay-Fertigstellungsnachweis.

## Wiederherstellbare Verschiebung

490 Datei-/Ordner-Einträge nach `archived/FORBIDDEN_DONTUSE` verschoben bzw.
als recoverable Snapshot erhalten. Alle Manifestziele existieren. Verzeichnis-
einträge enthalten teils viele Dateien; die Zahl ist keine Einzeldateizählung.
Die genauen ursprünglichen und neuen Pfade stehen in
`work/forbidden_cleanup_manifest.json` außerhalb der gesperrten Quarantäne.

Erfasst sind:

- Originalkern/core32, der spätere generierte Enemy-/Actor-Core, der EN07-
  Mock, entsprechende Generatoren, Tests und die alten Handoff-Snapshots.
- Unproduktive GS-/VU-/Original-Work-/Renderer-Komponenten sowie der
  deaktivierte core32-Codeblock aus `native/world.cpp`.
- Alte Kernarchive, generierte Module, Objekt-/Link-Caches, historische
  Release-Engines und Launcher; auch Windows-, Sanitizer-, Snapshot- und
  Zig-Caches mit alten Port-/Kernartefakten.
- Zusätzliche eingefrorene Test-/Vergleichsbinaries und generierte Build-
  Module aus Validation-/Diagnoseverzeichnissen; Logs und Bilder bleiben.
- Das alte macOS-App-Binary und anschließend auch das zuletzt gebaute
  macOS-Binary: die Prüfung aller Symbole fand dort weiterhin lokale
  `sys/plp/rom/ene/eff`. Eine frühere Prüfung nur globaler Symbole war
  unzureichend. Beide Starterpakete sind deaktiviert, nicht ersetzt.

Keine Löschung, Rücksetzung, kein Commit/Push. Originalreferenzen in `src`
und `include`, aktive Spieldaten/Mods, SDKs und lokale Einstellungen bleiben
erhalten. Quarantänisierte historische Buildordner bleiben als Ganzes
wiederherstellbar; ihre Inhalte sind nicht als Vorlage freigegeben.

Root-AGENTS.md und die Quarantäne-AGENTS.md verbieten jedem Task Lesen,
Suchen, Kopieren, Wiederherstellen und Wiederverwenden; `.ignore` reduziert
versehentliche Suchtreffer. Nur eine neue ausdrückliche Nutzerfreigabe
erlaubt Recovery. **Das ist Agent-Policy plus Buildschutz, kein Betriebssystem-
Zugriffsverbot gegen beliebige Prozesse.** Keine solche Garantie behauptet.

## Schutzprüfer

`tools/check_no_original_core.py` wurde neu aufgebaut, ohne Ausnahmeliste:

- CMake einschließlich Unterverzeichnissen, direkte Originalquellen,
  verbotene Targets, Quarantäneimporte und Funktionsgeneratoren.
- Native Funktionsincludes, Originaldispatcher/Hardwarepfade und bekannte
  getrennte Original-Work-Besitzer, auch in deaktivierten Codeblöcken.
- Kompilierdatenbank, transitive Linkeingaben, generierte Funktionskörper
  einschließlich umbenannter Header und Binary-Symbole einschließlich lokaler
  Symbole. Fehlende Prüfevidenz, leere/gestrippte Binaries und fehlendes
  Symbolwerkzeug sperren die Freigabe. Quarantäne-Symlinks werden nicht gelesen.

Pflichtprüfung bei CMake-Konfiguration, Default- und expliziten Ziel-Builds,
nach dem Runtime-Link, in Build-/Packaging-Skripten, beiden Root-Startern und
CI. Ein normales Build restauriert keinen quarantinierten Starter automatisch.

Verifikation:

- 20/20 synthetische Schutzverträge bestanden. Keine Quarantänequellen oder
  Spielbinaries für diese Tests ausgeführt.
- Frische CMake-Konfiguration in `/private/tmp/recvx-policy-configure.*`
  scheitert **wie vorgesehen** vor Erzeugung der Spieltargets an der Policy.
- Root-Start.command scheitert wie vorgesehen am Prüfer, ohne Spielstart.
- Python-Syntax, Shell-Syntax und `git diff --check` bestanden.
- Windows-Starter integriert, aber nicht auf Windows ausgeführt; kein
  Windows-Runtime-Nachweis und kein neuer nativer Spieltest behauptet.

## Offen / nächste Baustelle

Produktive alte Wrapper sind nicht stillschweigend aus dem Gameplay entfernt,
sondern **gesperrte Ablöseaufgaben**. Der Quellprüfer war beim ersten Lauf
mit 53 Befunden rot. Während paralleler Änderungen sank der letzte erfasste
Snapshot auf 44: 21 Generator-Aufrufe, zwei direkte Originalquellen,
17 Funktionsincludes und vier getrennte Work-Besitzer. Diese Momentaufnahme
ist kein Fortschrittsbeleg für 1:1-Semantik oder vollständige Bindungen.
Das Entfernen eines Includes macht einen Mechanismus nicht fertig.

`docs/NATIVE_REIMPLEMENTATION_ROADMAP.md` enthält kleine Aufgaben mit
Abhängigkeiten, Schreibbereichen und Fertigkriterien: P00 gemeinsame
Besitzer/Schnittstellen, danach sieben disjunkte Pakete Kampf, Bewegung,
Gegner, Inventar, Effekte, Actor/Assets und Raum/Türen. Gemeinsame Dateien
gehören einem Integrator. Erst vollständige Paketimplementierung, dann
gebündelte Tests und Ursachen-Fixes. Keine neuen Agents gestartet.

Generierte Funktionen müssen eigenständig auf denselben nativen Besitzern
mit Originalsemantik implementiert werden, nicht kopiert/umbenannt oder
durch Stubs ersetzt. Originalquellen, Daten und Disassembly sind Referenz;
der Originalkern wird nie kompiliert. RM_0130 mit tatsächlichem Spawn,
Modell, Pose, KI und SFX bleibt erstes produktives Abnahmeziel, danach alle
Gegner und Regressionen. Build, Packaging und Start bleiben bis zur
vollständigen sauberen Ablösung gesperrt.

README/Portstatus kennzeichnen alte Prozent-/Sweepzahlen als historisch.
Der Abdeckungsprüfer führt keine Funktionsgeneratoren mehr aus und zählt
direkt kompilierten Originalcode nicht als Port; alte Ledger-/Opcode-Evidenz
muss vor neuen Vollständigkeitszahlen weiterhin überprüft werden.
