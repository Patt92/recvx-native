# Übergabe: Orchestrierung des nativen Game-Ports

Stand: 2026-10-10, Europe/Berlin. Repo:
`/Users/patrickhoffmann/Projects/recvx-decomp`.
Dies ist eine Arbeitsanweisung und Bestandsaufnahme, keine Gameplay-Freigabe.

## Auftrag des Nachfolgers

Übernimm die Integration und Orchestrierung der offenen Pakete. Erhalte
bestehende Portierungen, koordiniere klare Dateibesitzer und bringe die
Mechanismen in den produktiven nativen Ablauf. Nicht nur Tasks verteilen
oder Tests grün machen. Das Gesamtziel bleibt:

Das decoded/dekompilierte Game eigenständig nativ auf macOS arm64 und
Windows x64 portieren, mit originalem Spielverhalten und **allen Gegnern**
in der produktiven World. Keine PS2-Nachbildung, kein Originalkern und
keine erfundene Ersatz-Spiellogik. Originale Reihenfolge, Flags, Tabellen,
Zustände und Seiteneffekte erhalten; Fehler an gemeinsamen Ursachen beheben.

## Nicht verhandelbare Regeln

- Aktuelles AGENTS.md vollständig lesen. Neuere Nutzerklarstellungen haben
  Vorrang vor historischen Arbeitsständen und Erfolgsmeldungen.
- Vor Arbeit und jedem Build `python3 tools/check_no_original_core.py`.
  Verstoß zuerst melden/klären; kein Bypass, keine neue Ausnahme/Whitelist.
- `archived/FORBIDDEN_DONTUSE` niemals lesen, durchsuchen, kopieren,
  zurückholen oder verwenden; auch nicht indirekt über Historie, andere
  Tasks, Archive oder Caches. Nur neue ausdrückliche Nutzerautorisierung
  könnte Wiederherstellung erlauben.
- Originalquellen und Disassembly außerhalb der Quarantäne sind
  Semantikreferenz. Kein Kompilieren/Linken/Aufrufen der Originalengine,
  keine kopierten oder generierten Funktionskörper unter anderem Namen.
  ELF/MMI-Befunde dürfen Verhalten klären, sind kein Laufzeitbaustein.
- Vorhandene native Player-/RoomWorld-/WorldEntity-/Inventory-/Effect-
  Besitzer bleiben die Autorität. Keine zweite sys/plp/rom/ene/eff-Welt,
  BH_PWORK-Spiegeltabelle oder Copy-in/Copy-out-Originalengine.
- Keine Heuristik, geratenen Konstanten, Raum-/Gegner-Sonderfälle oder
  stillen No-ops als Ersatz. Unklare Originalsemantik belegt offen lassen.
- Wirkungsprüfungen nicht auf neutrale Lückenantworten abschwächen.
- Lokale Änderungen und ungetrackte Dateien gehören dem Nutzer/Peers.
  Kein Reset, fremdes Stash/Entfernen, Commit, Push oder Startpaket ohne
  ausdrücklichen Auftrag. Dateien mit apply_patch bearbeiten.

## Erstes Vorgehen

1. `git status --short`, aktuelles AGENTS.md und Pflichtprüfer. Bei dieser
   Übergabe: Prüfer PASS; HEAD `8b951f5` (savepoint). Zahlreiche wichtige
   native Dateien, Tests, Berichte und Auftragsdokumente sind ungetrackt.
   Der Stand kann durch aktive Peers jederzeit weiterlaufen.
2. Aktive Agents und ihre tatsächlich beanspruchten Dateien feststellen.
   Die letzten Nutzermeldungen ersetzen keine aktuelle Besitzerabfrage.
   Andere Chats nur nach Nutzerautorisierung anschreiben; für ausdrücklich
   delegierte Subtasks die Agent-Werkzeuge verwenden, keine neuen Chats
   ohne Auftrag erzeugen. Keine bereits laufende Arbeit doppelt starten.
3. Eine kompakte Besitz-/Abhängigkeitstabelle und Gegner-ID-Matrix führen.
   Neue Parallelaufgaben nur mit disjunkten Dateien und minimalem Vertrag.
4. Dokumentstatus mit aktuellem Code und neuesten Paketberichten abgleichen.
   `CLAUDE_NEXT_TASKS.md` und große Teile der Roadmap sind historisch:
   dortige Aussagen „Inventar/Computer/EKG fehlen“ oder „R02 offen“ nicht
   ungeprüft als neue Aufgaben ausgeben. Konflikte sichtbar bereinigen.
5. Aktuellen Buildstand in eigenem Buildverzeichnis feststellen, ohne
   laufende Peer-Builds zu stören. Kein frischer Gesamtlauf behauptet:
   bei dieser Übergabe wurden nur Status/Policy/Dokumente geprüft.

## Zuständigkeiten und aktueller Auftrag

Ein Integrator besitzt `CMakeLists.txt`, `native/world.*`, `runtime.cpp`,
`events.cpp`, gemeinsame Zustands-/Assetverträge und Dokumentintegration.
Renderer-/Scene-Schnittstellen ebenfalls exklusiv abstimmen. Peers liefern
eigene Module sowie kleine Anschluss-Patches; keine konkurrierenden Änderungen
an diesen Dateien. Eine explizit vereinbarte Übergabe ist möglich, muss aber
vorher dokumentiert werden.

**Gegner-Agent:** Der Nutzer hat den zunächst engen C01-Auftrag ausdrücklich
auf **C01–C08 einschließlich sämtlicher C07+-IDs** erweitert. Maßgeblich:
`docs/THIRD_AGENT_C01_TASK.md`. Zuletzt analysierte der Agent noch Zombie-/
Hunde-Fallen in `enemy_spawn.*`; das ist nützliche C01-Teilarbeit, nicht der
Gesamtauftrag. Sicherstellen, dass er die aktualisierte Datei gelesen hat.
Nicht behaupten, die Änderung sei bereits bei ihm angekommen.

Vollständige originale ID-/Varianten-/Child-/Dispatcherzuordnung herleiten,
C02-Verträge am vorhandenen Besitzer abstimmen, dann alle Gegnermechanismen
und Assets portieren: Init, Eventspawn, Motion/Interpolation, KI,
Attack/Grab/Damage/Death, Child-Lebenszyklus, Draw und SFX, soweit vorhanden.
C05/Subplayer nach echter ID-Tabelle abgrenzen, EN00 nicht vergessen.
Erst alle Gegner/Assets zusammen implementieren und integrieren, danach
gesammelt testen/fixen. Pflichtprüfer und nötige Build-/Sicherheitschecks
bleiben währenddessen verpflichtend. Erstes verbindliches produktives
Abnahmeziel: EN07 in RM_0130 über Originalspawn/-events, vollständiges
sichtbares Modell, fortschreitendes Animationsset, KI und SFX. Danach alle
übrigen Gegner; Fledermäuse ersetzen das Gesamtziel nicht.

**Kleineres Modell, falls delegiert:** Nur belegte Gegner-ID-/Asset-
Bestandsaufnahme in `work/enemy_id_asset_inventory.md`: pro ID Original-
Routine, native Implementierungsstelle, Modell/Motion, Lücken und Datei-/
Zeilenbelege. Keine Codeänderung, keine Semantik erfinden, keine Generatoren,
keine Quarantäne. „Datei vorhanden“ ist kein Funktionsnachweis. Dieser
Teilauftrag wurde vorgeschlagen, noch nicht gestartet.

**Weitere Peers:** B04/Haar und B01/Treppen wurden zuletzt separat bearbeitet.
Berichte melden Fortschritt/Nacharbeit, aber laufende Besitzer neu ermitteln.
`native/player_head.c/.h` liegen inzwischen ungetrackt im Baum: nicht ohne
Abgleich einen zweiten Headwork-Port starten. Kampf A01–A05, restliche
Effekte, Pose und Renderer haben eigene Verträge; nicht vom Gegner-Agenten
unbemerkt durch Ersatzlogik nachbauen lassen.

## Erreichte Teilstände und offene Grenzen

Die folgenden Aussagen stammen aus Paketberichten/AGENTS oder Nutzer-
meldungen. Sie wurden bei dieser Übergabe nicht alle erneut ausgeführt.

| Bereich | Erhalten / Stand | Offen / nächster Abgleich |
|---|---|---|
| R02/Policy/Daten | Laut Bericht Build-Generatoren entfernt, reine Daten eingefroren, Funktionsgeneratoren quarantiniert. Aktueller Pflichtprüfer PASS. | Historische R02-Offenmeldungen bereinigen; PASS nicht als Gameplay-Abnahme zählen. |
| D01–D09 | Inventar, Kombinationen, Eventmutationen, EKG/UI, Untersuchung/Dateien, Computer und Gameover wurden nativ implementiert und positiv geprüft gemeldet. | Nicht neu stilllegen/portieren. Paketgrenzen und offene Details prüfen; natürlicher Kampftod und volle visuelle Gameover-Abnahme sind nicht belegt. |
| B00/P03/E01 | Native ATR-Boden-/Wand-/Linien-/Reflexionsmechanismen und freier Player-Wandanschluss mit RM_0050-Nachweisen. Fehlender EN01-State darf HP nicht auf 1 klemmen. | Keine Behauptung vollständiger Kollision; echte bhCheckL2Wall, weitere Capsule-/Push-/Gegnerdienste und Zylinderdeckel-Semantik abgleichen. |
| B01 | Neuer Bericht meldet sechs Treppen-/Stufen-/Leiterroutinen samt produktivem Anschluss. | Leiterpfad laut Bericht in Originaldaten nicht erreichbar, nur Komponentenprüfung. Headwork/Effektdienste und neueste Runtime-Abnahme prüfen. |
| B04 | Nacharbeitsbericht meldet korrigierte Reihenfolge hinter locomote und gemeinsame pitch/roll-Bindung. | Früher 49/50 mit rückwärts scheiterndem player_hair; keinen alten Fehler als aktuellen Endstand übernehmen. Nacharbeit unabhängig prüfen, Headwork-Besitzer abstimmen. |
| B05 | Native Itembox-Deckelsteuerung, Sound, Menüpass, Rückkehr; fünf Originalraumtests und RM_1080-Diagnoselauf. | Allgemeiner Objekt-/Playerdispatcher, reservierte/verknüpfte Besitzer bleiben offen; kein Storynachweis. |
| E03/E04 | Eff009 mit UV-Banken, 40 gemeinsamen Texturen, Feuerzeug Handknoten 9/Objektflamme, RGB-Lichttypen 1–6. | Originale Armhaltung/IK bhArmIkMdk, allgemeine pt_flg-/Drawgates, andere Sprite-/Blut-/Explosions-/Partikeleffekte und SFX-/Lichtdienste fehlen teilweise. |
| P04/Renderer | Cliptabellen und belegte Installationszeitpunkte nativ geprüft; Flagwechsel allein installieren keinen neuen Clip. Gameover 2/20000 erhalten. | Eff137, voller Kameraautomat, originale Materialidentität/-reihenfolge und sichtbare Eisraumgleichheit offen. Kein ICE/OT/GS-Kern als Ersatz. |
| G01 | Bericht meldet native Türpaket-Sequenzen, Kamera/Flügel/Blende/Sound/Licht und weitere Verfahren. | Aktuelle Endabnahme/Grenzen aus Bericht prüfen; nicht mehr bloß sofortiger Türabschluss, aber kein vollständiges Gameplay daraus ableiten. |
| F01–F05 | Bestehende native Decoder/Actor/Face/Lip/Math erhalten. | Face-Knotenwinkelübernahme und eyemat-Blickrichtung, Morph-Besitzer, Shadow und gemeinsame Pose-/Motiondienste abgleichen; alte Kern-Ergebnisse sind keine Abnahme. |
| Windows | Runtime-Binary laut Bericht gebaut, Linkeingangs-/Vergleichsaudit implementiert. | Unabhängige Prüfung des Auditmechanismus und tatsächlicher Windows-Lauf offen. Mac-Erfolg und PE-Link ersetzen diese nicht. |

Bewährte Invarianten nicht regressieren: `common_effect_textures_offset ==
size_t(-1)` vor Zugriff prüfen; fehlender Player-State hat keine HP-/Tod-
Wirkung; aktive Lip-Streamquelle wird nicht durch bhSetLip umgelenkt;
vorhandene BGM-Discregel und nativen Math-/WeaponFlash-Port erhalten.

## Abnahme- und Berichtspflichten

Pro Paket eine belegte Liste aller Originalzweige, Mutationen, Flags,
Aufrufreihenfolge und Datenabhängigkeiten. Status sauber trennen:
fehlend / implementiert / produktiv angeschlossen / Komponenten-geprüft /
Runtime-geprüft / Windows-geprüft. Implementierung ohne Anschluss ist offen.

Zuerst gesamtes abgestimmtes Paket implementieren, dann gemeinsamen Testblock
und Ursachenanalyse. Keine großen 205-Raum-Sweeps nach jeder Funktion.
Ladeprüfung, diagnostische Runtime-Prüfung und natürlicher Spielablauf sind
verschiedene Nachweise. Debug-Positionierung ausdrücklich kennzeichnen;
Gegner-Injektion/Direktdispatch ist keine verpflichtende Spawn-Abnahme.

Nach Integration betroffene positive Komponenten- und Runtime-Abläufe,
Regressionsprüfungen sowie Policy gegen Quellen, Build und tatsächlich
genutztes Binary durchführen. Buildpfad/Binary, Plattform, Befehle, Ergebnisse
und Logs im Paketbericht festhalten. Windows: Compile, Link, Audit und
Ausführung getrennt ausweisen. Verbleibende rote Tests nach Ursache sortieren:
Regression, noch fehlender Mechanismus, belegte falsche Testerwartung.

README/Gameplay-Doku/Ledger erst aus diesen Nachweisen aktualisieren.
Keine aus Testanzahl geschätzte Prozentzahl oder Behauptung aller Gegner.
Start.command kann ein älteres Paket ausführen; tatsächlich gestartetes
Binary mit dem geprüften Build abgleichen, ohne eigenmächtig Packaging zu
ändern. Commit/Push ist ein separater Nutzerauftrag; ungetrackte Dateien
werden durch git push allein nicht hochgeladen.

## Relevante Unterlagen

- `AGENTS.md` und `docs/THIRD_AGENT_C01_TASK.md` — Regeln/aktueller Gegnerauftrag.
- `docs/NATIVE_REIMPLEMENTATION_ROADMAP.md`, `docs/port_ledger.tsv`,
  `docs/NATIVE_GAMEPLAY.md` — Planung/Status, teilweise historische Einträge.
- `work/native_port_B05_item_box_object.result.md`.
- `work/native_port_P04_camera_invocations.result.md`, `work/native_port_P04_clip.result.md`.
- `work/native_port_B04_hair_followup.result.md`, `work/native_port_B01_stairs.result.md`.
- `work/native_port_B00_motion.result.md`, `work/native_port_P03_E01_ground.result.md`,
  `work/native_port_E01_line_reflection.result.md`.
- `work/native_port_E03_lighter.result.md`, `work/native_port_G01_doors.result.md`.
- `work/native_port_inventory_tests.result.md`, `work/native_port_D03_files_inspection.result.md`,
  `work/native_port_D04_computer.result.md`, `work/native_port_D05_gameover.result.md`.
- `work/native_port_R02_D09.result.md`, `work/native_port_windows_audit.md`.

Historische Gegner-/Morphberichte mit Originalkernpfaden sind keine
Abnahme des heutigen kernfreien Ports. Nicht von alten Erfolgsmeldungen
zum Wiederaufbau verbotener Mechanismen verleiten lassen.
