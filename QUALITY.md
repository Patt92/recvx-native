# Qualität der Original-Engine-Portierung

Verbindlich für alle Portierungsarbeiten gemäß `AGENTS.md` (Nutzer, 2026-10-07).
Das Ziel ist die vollständige Original-Engine. Annäherungen, Ersatzlogik und
nachträglich erfundene Sonderfall-Fixes sind verboten.

## Abdeckung ist kein Qualitätsnachweis

`python3 tools/port_check.py` erfasst die nachgewiesene Port-Abdeckung nach
Bereichen und Originalfunktionen. Auch extrahierter Originalcode kann durch
Generatoränderungen, falsche Eingaben oder fehlende Dienste andere Semantik
haben. Prozentwerte sind deshalb kein Nachweis originalgetreuen Verhaltens.

## Prüfmethode bei jeder Portierung

1. Vor Änderungen `git status --short` lesen und laufende fremde Arbeit erhalten.
2. Den zuständigen Originalmechanismus finden: Funktion, Aufrufer, Dispatch,
   Tabellen und Daten. Bei unklarer Semantik die Disassembly heranziehen.
3. Original und Port einschließlich Generator und Host-Bindung vergleichen.
   Nicht beim Namen einer Originalfunktion oder einem kopierten Funktionskörper
   aufhören. Den tatsächlich ausgeführten Pfad bis zu seinen Diensten prüfen.
4. Zustände, Flags, Aufrufreihenfolge, Framefortschritt, Interpolation, Timer,
   Zufallsaufrufe, Ressourcenwahl und Seiteneffekte vergleichen. Auch Init,
   Freigabe, Abbruch, Raumwechsel und Wiedereintritt berücksichtigen.
5. Besonders nach festen Rückgaben, Leerfunktionen, Heuristiken, Host-Phasen,
   festen Zeitgrenzen, synthetischen Posen, pauschalem Blending und Filtern nach
   Raum-/Objekt-ID suchen. Treffer sind Hinweise; erst der Originalvergleich
   belegt eine Abweichung. Originale Sonderfälle mit Fundstelle übernehmen.
6. Plattformanpassungen auf ABI, Datenrepräsentation und technische Schnittstellen
   begrenzen. Eine andere Entscheidung oder ein anderer Zustandsübergang ist
   keine Plattformanpassung. Debug-Overrides müssen am Debug-Zustand ihren
   Ursprung haben; Originalroutinen dürfen keine eigenen Debug-Defaults besitzen.
7. Abweichungen mit beiden Fundstellen und der konkreten Verhaltensfolge
   dokumentieren. Belegte Fehler, offene Bindungen und ungeklärte Semantik
   unterscheiden. Keine Abweichung aus einem veralteten Kommentar allein ableiten.
8. Den gemeinsamen Originalmechanismus und seine Abhängigkeiten portieren.
   Keine neue Ersatzlösung hinzufügen. Offene Originaldienste ausdrücklich als
   Lücken behandeln und nicht mit einem vermeintlich harmlosen Standardwert
   überdecken.
9. Gezielt gegen originale Zustandsübergänge und Daten prüfen. Prüfungen müssen
   falsche Bindungen aufdecken können, nicht nur den neuen Code nacherzählen.
   Testaufstellungen und Debug-Overrides als solche kennzeichnen. Keine feste
   Host-Tickzahl als Erwartung übernehmen, wenn sie gerade die Abweichung ist.
10. Im Ergebnis Originalfundstellen, Änderungen, Prüfungen und verbleibende
    Grenzen angeben. Ein Build, Screenshot oder Einzel-Replay belegt weder
    vollständige Portierung noch vollständiges Durchspielen.

## Befunde vom 2026-10-07

Belegte Beispiele für diese Prüfung: HP-Auswahl nach Gegnerindex statt
`rand() % 16`; eigene Kampfphasen statt originaler Spielerzustände; pauschales
additives Effekt-Blending statt `bl_src`/`bl_dst`; feste Rückgaben und leere
Gegnerdienste; synthetisches Umkippen als Todesdarstellung. Diese Liste ist
eine Ausgangsbasis, kein vollständiges Audit und keine Erledigtmeldung.
Kistenschieben wird separat bearbeitet.
