# Folgeauftrag für Claude — stabil erhalten, fehlendes Gameplay portieren

Stand: 2026-10-09. Zugehörige Task-IDs und andere Pakete stehen in
`NATIVE_REIMPLEMENTATION_ROADMAP.md`. Dies ist ein Auftrag zur Implementierung,
keine weitere Stilllegung und kein Auftrag, Tests lediglich grün zu machen.

## Verbindliches Ziel und Ausgangslage

Das decoded/dekompilierte Game eigenständig nativ auf macOS arm64 und Windows
x64 portieren, mit originalem Spielverhalten und allen Gegnern vollständig in
der produktiven World. Keine PS2-Nachbildung, kein separater Originalkern,
keine erfundene Ersatz-Spiellogik. Alle Agents halten AGENTS.md ein.

Dein Bericht belegt wieder Configure/Build/Runtime-Link und 205/205 ladende
Räume ohne blockierte Eventbefehle. Der Quellen-/Build-/Binary-Prüfer für
`build/gapcheck-claude/recvx-runtime` besteht auch beim erneuten Abgleich.
Das Spiel ist dennoch nicht vollständig spielbar. 34/41 CTest war dein
Zwischenstand; liefere das tatsächliche Endergebnis mit getrennten Kategorien
für Lückenverträge und positive Gameplay-Wirkung.

Vor Beginn `git status --short`, AGENTS.md und Schutzprüfung lesen/ausführen.
Bestehende Änderungen erhalten; kein Reset, Commit oder Push. Quarantäne
`archived/FORBIDDEN_DONTUSE` nicht lesen, durchsuchen, zurückholen oder benutzen.
Originalquellen, Originaldaten und Disassembly außerhalb der Quarantäne sind
nur Semantikreferenz: nicht kompilieren/linken und keine Funktionskörper
kopieren, extrahieren oder generieren, auch nicht unter neuen Dateinamen.

## 1. Kleinen Erzeugerrest schließen — R02

Für ADX ruft CMake noch `generate_native_tables.py` auf, erzeugt damit drei
gesperrte Funktionsdateien und löscht sie anschließend. Ersetze ausschließlich
diesen Pfad durch einen Daten-only-Erzeuger oder ein natives Datenmodul mit
identischen ADX-Werten. Keine vorübergehende Funktionsausgabe und kein Löschen
als Schutzkonzept. Prüfer um diesen bekannten Mischgeneratorfall ergänzen;
Negativfall „erzeugen, danach löschen“ und erlaubte reine Daten testen.

Schreibbereich für diesen abgegrenzten Auftrag vorab mit dem Integrator
abstimmen: ADX-Datenbereitstellung, betroffene CMake-Regel, Schutzprüfer und
dessen Tests. Keine sonstige Build-Umbau- oder neue Rückbauwelle. Daten aus
`item_tables.c/.h`, `room_table.c/.h`, `ui_tables_data.inc` erhalten.

## 2. Inventar vollständig auf seinem nativen Besitzer — D02/D07/D01/D06

Ein Besitzer: die bestehende `Inventory`. Bevor du implementierst, belegte
Feld-/Flag-/Slotverträge für Player, Status, Message und Event mit dem
Integrator festlegen. Keine zweite SYS-/Player-/Inventory-Work, kein
Copy-in/Copy-out-Kern und keine neue globale Schattenzustandsverwaltung.

Bearbeite in dieser Reihenfolge alle Originalfälle, nicht nur einen Happy Path:

1. **D02a/D02b:** GetItem-Aufnahmefrage und Modi, Kapazität/breite Items,
   Stack-Maxima, Geschenke/Sofortgebrauch; gemeinsame Sortierung/Suche/Löschen
   und korrekte Nachführung von Ausrüstung und ausgewählten Slots.
2. **D02c/D02d:** inventarseitiges Reload/CountBullet, gepackte Flags/signed
   Mengen; alle Gebrauchsfälle mit Heilung, Gift, Spray, Bodentrigger,
   Meldung, Schließen und Pulseanforderung. Kampf-State-Machines bleiben beim
   Kampfbesitzer; liefere dafür einen kleinen Inventory-Vertrag.
3. **D07:** alle sechs Combi-Mechanismen, Mengenprüfung, Produktwechsel,
   Restmenge, Flags, Slots und Meldungen. Ungültige Kombination darf nach
   Originalregel scheitern; gültige muss wirklich wirken.
4. **D01:** Itembox-Auswahl/Transfer/Tausch/Stapeln und originale Restriktionen
   auf demselben Bestand. Deckel-/Objektablauf B05 bleibt separater Anschluss.
5. **D06:** native Verträge und Anschluss-Patch für die sieben Eventmutationen
   aus der Roadmap liefern. `events.cpp` nur durch den Integrator ändern;
   fehlende Wirkung nicht durch vorzeitigen Eventabschluss kaschieren.

Dein Modulbereich: `native/inventory.cpp/.h` und kleine zugehörige native
Module. Kein anderer Agent bearbeitet gleichzeitig diese Inventory. Bestehende
Host-Helper anhand Originalsemantik prüfen; ihre Existenz ist kein 1:1-Beleg.

## 3. Status und sichtbare Bedienung anschließen — D09/D08

- **D09, `native/ui.cpp`:** fehlende Mengen-/Ziffern-/Farbbankausgabe sowie
  Triger-/ItemCommand-Auswahl, Sichtbarkeit, Höhe und Beschriftung eigenständig
  nativ implementieren. Vorhandene reine UI-Daten behalten. Menüeingabe ohne
  gezeichnete Befehle gilt nicht als fertige UI.
- **D08, `native/status_pulse.cpp/.h`:** vollständiger Original-EKG-Automat
  mit Zustands-/Heil-/Gift-/Fadeverlauf und tatsächlicher Quad-/UV-/Farbausgabe.
  Er verarbeitet dieselben D02d-Wirkungen; keine erfundene Kurve und keine
  ständig neutrale Condition-/heal_mode-Rückgabe.

Face/Lip, Actor/Morph/Shadow, Kollision/Bewegung, Gegner und Türen sind andere
Pakete. `face.*` ist policy-konform und wird hier nicht zurückgebaut. Die zwei
belegten Face-Abweichungen sind F04a/F04b beim Actor-Besitzer. Bestehende
BGM-Regel, Math, WeaponFlash und aktiven Lip-Cursor erhalten. Den
`size_t(-1)`-Guard in `world.cpp` nicht entfernen; echte gemeinsame
Effekttexturen bleiben E03. Gemeinsame World-/Runtime-Dateien nicht parallel
editieren: konkrete fehlende Verträge an den Integrator übergeben.

## 4. Danach gebündelt prüfen und Ursachen global beheben

Erst das gesamte Inventar-/UI-/Statuspaket implementieren und integrieren,
dann gemeinsam testen und fixen. Schutz-/Buildchecks bleiben vor jedem Build
Pflicht; keinen großen Raumsweep nach jeder einzelnen Funktion wiederholen.
Die sieben noch gemeldeten Testfehler nach Ursache klassifizieren, keine
ursprüngliche Wirkungsprüfung zu „keine Mutation/keine Quads“ abschwächen.

Positive Abnahme: originale Aufnahme/Abbruch/Kapazitätsgrenzen, Slot-/Equip-
Folgen, alle Combi-Klassen und Restmengen, Reload/Munitionsflags, Heilung/Gift/
Trigger, Kistentransfer, Eventmutationen, sichtbare Mengen/Befehle und EKG.
Dann dieselben Abläufe im produktiven nativen Runtime-Pfad prüfen. Keine
Debug-Injektion oder isolierte Komponentenroutine als Runtime-Beweis ausgeben.
Fehler bis Daten-/Work-/Aufrufursache zurückverfolgen und gemeinsam beheben;
keine Raum-, Item- oder Gegner-Sonderlösungen erfinden.

Liefere Quellen-/Disassembly-Belege, zuständige native Besitzer, tatsächliche
Tests und Spielwirkung sowie offene Anschlüsse unter `work/native_port_<ID>.result.md`.
Windows x64 separat mit Build/Link und Runtime-Befund ausweisen; macOS-Erfolg
nicht als Windows-Nachweis zählen. Nach diesem Paket D03, D04 und D05 als
separate Folgeaufträge behandeln. Gesamtziel einschließlich aller Gegner und
Fledermausabnahme RM_0130 bleibt offen, bis produktiv nachgewiesen.
