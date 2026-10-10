# 08 — End-to-End-Abnahme: echte Fledermäuse in nativer Runtime

## Auftrag

Baue den verbindlichen Regressionstest und den reproduzierbaren Runtime-Lauf
für den in Aufgabe 01 belegten Raum. Der Test startet den normalen nativen
Raum-/Eventablauf, ohne künstliches Spawn-Injizieren und ohne direkten
Enemy-Dispatch.

## Gemeinsame Pflichtnachweise

1. EN07 entsteht aus der originalen RDT-/Eventkette zum belegten Zeitpunkt.
2. Das vollständige Modell ist sichtbar und verwendet die echten Ressourcen.
3. Die originale Motionnummer und Pose schreiten über mehrere Frames fort.
4. Die Original-KI wechselt aufgrund realer Raum-/Spielerbedingungen einen
   nachweisbaren Zustand oder bewegt den Gegner entsprechend.
5. Mindestens ein zugehöriger Original-SFX wird über den nativen Audiopfad
   ausgelöst.
6. Kollision/Kampfpfade, die der gewählte Ablauf erreicht, sind echt gebunden;
   keine protokollierte Lücke darf den Test als bestanden gelten lassen.

## Ergebnis

Lege `work/08_runtime_acceptance.result.md` mit Startkommando, Raum, Frames,
Belegen und verbleibenden nicht erreichten EN07-Zweigen an. Ergänze einen
automatisierten Regressionstest, der den produktiven Runtimepfad benutzt. Ein
synthetischer Unit-Test kann zusätzlich existieren, ersetzt diese Abnahme aber
nicht.
