# Dritter Agent — vollständiges Gegnerpaket C01–C08

## Umfangskorrektur durch Nutzer (2026-10-10)

Der Auftrag umfasst **alle originalen Gegner**, nicht nur Hunde/Zombies
und nicht nur deren Fallenaktivierung. C01 bleibt ein Teilpaket dieses
Gesamtauftrags. Die bereits begonnene Aktivierungsanalyse erhalten und
einordnen; anschließend nicht bei `activate_enemy_spawn` aufhören.

Ziel ist das eigenständig laufende native Game auf macOS arm64 und
Windows x64 mit originalem Spielverhalten: alle Gegner im produktiven
World-Pfad, mit Spawn/Eventsteuerung, Initialisierung, vollständigem
Animationsset, KI, Angriff/Griff, Schaden/Tod, Child-Lebenszyklus,
Darstellung und SFX, soweit beim jeweiligen Originalgegner vorhanden.
Keine PS2-Nachbildung, kein Originalkern und keine Ersatz-Spiellogik.

Zuerst die vollständige ID-/Varianten-/Dispatcherliste anhand zulässiger
Originalreferenzen und Raumdaten bestimmen. Für jede ID festhalten:
Originalmechanismen, vorhandener nativer Besitzer, fehlende Bindungen,
Asset-/Motionquelle, Abhängigkeiten und produktiver Nachweis. Nicht nach
vermuteten Gegnernamen gruppieren; Bosse, verknüpfte Kinder, Sonderformen
und EN00 nicht vergessen. Subplayer korrekt nach Originaltabelle abgrenzen
und ihren gemeinsamen Besitzervertrag in C05 berücksichtigen.

Gemeinsame Feld-/Lebenszyklus-/Asset-/Motion-/Kampfverträge (C02/F/E/G)
zuerst mit dem Integrator abstimmen. Anschließend C03/EN01, C04/EN04,
C06/EN07 und sämtliche übrigen IDs (C07+) sowie C08 eigenständig auf
derselben nativen Zustandsquelle portieren. Keine bloße generierte oder
gelinkte Gegnerliste als Implementierung ausgeben. Fehlende gemeinsame
Dienste konkret als Anschlussaufgabe melden, nicht durch No-ops ersetzen.

**Reihenfolge des Nutzers:** zuerst alle Gegner/Assets zusammen portieren,
danach gesammelt testen und gemeinsame Ursachen global beheben. Kleine
Teilaufgaben dienen der Arbeitsteilung, nicht einem reduzierten Gesamtziel.
Pflichtprüfer und erforderliche Build-/Sicherheitsprüfungen bleiben bestehen.
Das erste verbindliche produktive Gegner-Abnahmeziel bleibt EN07 in
RM_0130 über den originalen Spawn-/Eventablauf: korrekter Zeitpunkt,
sichtbares vollständiges Modell, fortschreitende originale Pose, KI und
zugehörige SFX. Danach alle übrigen Gegner gesammelt abnehmen. Weder ein
205-Raum-Ladescan noch künstliches Injizieren oder Direktdispatch ersetzt
diese Abnahme. Angriffe, Treffer/HP, Tod und Eventfolgen zusätzlich über
echte native Abläufe nachweisen; offene Abhängigkeiten bleiben offen.

Eigene zusätzliche Gegnerdateien, Tests und Berichte vor Bearbeitung mit
dem Integrator abgrenzen. Gemeinsame Anschlussdateien bleiben bei ihm;
kein anderer Agent darf allein wegen der Umfangserweiterung deren Besitz
übernehmen. Gesamtübersicht und Teilberichte müssen zwischen implementiert,
produktiv angeschlossen, Runtime-geprüft und noch blockiert unterscheiden.

## C01-Teilauftrag — Fallen-/Gegneraktivierung

B04 (Haar) läuft bei einem anderen Agenten. Codex besitzt derzeit den
Kamera-/Clip-Anschluss, Monitor-Aufrufketten und die Untersuchung der
Eisraum-Darstellung. Diese Bereiche nicht bearbeiten.

## Ziel und Besitz

Portiere die bisher stillgelegte Fallenaktivierung eigenständig in
`native/enemies/enemy_spawn.cpp/.h`. `activate_enemy_spawn` liefert derzeit immer
false. Normale Raumplatzierung ist vorhanden, aber kein Ersatz für die
originale spätere Aktivierung. Laufende KI nach Aktivierung gehört zu den
weiteren Teilpaketen des obigen Gesamtauftrags und darf durch den Abschluss
dieses Spawn-Teils nicht als fertig behauptet werden.

Eigene Dateien: `native/enemies/enemy_spawn.*`, neue gezielte Spawn-Tests,
`tools/verify_enemy_spawn.py` und `work/native_port_C01_spawn.result.md`.
Keine direkten Änderungen an `world.*`, `runtime.cpp`, `events.cpp`,
`effect_sprites.*`, `renderer.*`, `player_hair.*` oder CMake. Nötige
Anschluss-/CTest-Änderungen als kleinen Patch mit Schnittstellenvertrag
an Codex übergeben; er integriert sie. Fremde Änderungen erhalten.

## Reihenfolge

1. `git status --short` und `python3 tools/check_no_original_core.py`
   lesen/ausführen. Den Prüfer vor jedem Build erneut ausführen.
2. Zulässige Semantikreferenzen vollständig nachvollziehen:
   `bhEne_EnemyAtariCheck` in `zonzon.c` und
   `bhEne04_AtariCheck`/`bhEne04_AtariCheck2` in `en04.c`, einschließlich
   ihrer tatsächlichen Aufrufer, ATR-Daten, Flags und Zufallsdienste.
3. Zuerst alle zugehörigen Aktivierungszweige portieren: ATR-Typ-4-
   Parameterfolge, Etagen-/Richtungswahl, Zombie-Typ 1, Hunde-Typ 2 und
   beide EN04-Kontaktfälle. Kontaktgeometrie braucht einen belegten
   vorhandenen nativen Dienst; fehlende Dienste als Abhängigkeit melden.
4. Danach gesammelt testen und gemeinsame Ursachen beheben. Nicht nach
   jedem Einzelfall eine Raum-/Gegner-Sonderlösung einbauen.
5. Einen geeigneten Raum und den Trigger aus Originalraumdaten bestimmen.
   Natürlicher Spawn plus tatsächliche Runtime-Eingabe muss die originale
   Aktivierung, Platzierung, Richtung und Flagfolge nachweisen. Kein
   künstliches Gegner-Injizieren oder Direktdispatch als Abnahme.

## Verbindliche Schnittstellen

Die bestehende WorldEntity/RoomWorld bleibt einziger Gegnerbesitzer.
`EnemySpawn`/`EnemyArea` sind Ansichten/Ein- und Ausgabeverträge, keine
zweite Workwelt. Bestehende Player-/Raumflags und der gemeinsame RNG sind
die Autorität. Ein eigener Zufallsstrom im Kontext ist kein Ersatz:
benötigte RNG-Aufrufe samt Reihenfolge als Anschlussvertrag festhalten.
Nichttreffer dürfen keine vom Original unbelegten Mutationen auslösen.
Feldänderungen, Bereichsverbrauch, Kontaktbits, Raumflags und RNG-
Fortschritt positiv prüfen, einschließlich Grenz-/Nichttrefferfällen.

## Grenzen und Abnahme

Kein Originalkern, Generator-Funktionskörper, zweites sys/plp/ene-Werk,
PS2-/MMI-/GS-/VU-Nachbau oder erfundener Radius. Quarantäne
`archived/FORBIDDEN_DONTUSE` weder lesen noch durchsuchen oder verwenden.
Originalquellen außerhalb der Quarantäne sind nur Semantikreferenz.

Berichte Komponenten-, echte Runtime- und Plattformnachweise getrennt.
Mac-Build plus Policy und positive Tests erforderlich; Windows nur als
kompiliert/gelinkt nennen, wenn wirklich geprüft, nicht als ausgeführt.
Offene KI-/Kampf-/Kontakt-Abhängigkeiten ausdrücklich nennen. Keine
Testabschwächung, kein Paket/Commit/Push ohne Nutzerauftrag.
