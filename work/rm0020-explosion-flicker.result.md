# RM_0020: schwarzes Flackern der Explosionsszene (2026-10-22)

Messung, kein Codefix. Belege: `build/validation/rm0020-flicker/`.

## Messaufbau

- Frischer RM_0020-Start, Bodentrigger 0 der Kinoszene (Position 133/0/134),
  Ereignis 0..6, Ereigniskamera 12; 867 Einzelbilder (Takt 34–900), je Lauf
  `--frames N --screenshot` (der Renderer rastert nur das letzte Bild), Mittlere
  Helligkeit je Bild aus dem BMP.
- Zweite Messung ab dem Nutzer-Snapshot in RM_0020 (Slot 1, Stand 23:43).

## Befund

- **Bilder ~88–101, Ereigniskamera 12:** streng zweitaktig. Gerade Bilder um
  13–15, ungerade um 18–22 mittlere Helligkeit, also rund 30 % Unterschied im
  ganzen Bild (oben links +30 %, Mitte +25 %, unterer Rand unverändert).
- Weitere Einbrüche nur an Szenenübergängen (Bild 770/809, Ereigniskamera 20,
  Blende).
- Ursache sind ausschließlich die Rauchpuffs **Effekt 119 Typ 1** (5 Stück,
  alternierend Phase 1/3/5/7/9 gegen 2/4/6/8/10). Blende 11/3, volle Eckfarbe,
  Texturgruppe 39. Erzeuger `bhEff103` Typ 2 legt jeden zweiten Takt einen
  neuen Puff an (`ct0 & 1`, effsub1.c:6080); durch die 2-Takt-Anlage haben alle
  Puffs dieselbe Parität, also wechselt das ganze Feld gemeinsam zwischen zwei
  Atlaszellen-Sätzen.
- Die Geometrie folgt den Originalformeln: `py = gpy + sz·sin(ct0·1024)`
  (hier sz = 101, also eine 100 Einheiten hohe Rauchsäule) und
  `sx = 8·sxb·uv.xs` (sxb 22 → 33–44 Einheiten breite Puffs, effsub1.c:7890).
  Die Kamera der Szene steht rund 14,5 Einheiten vor der Säule, deshalb decken
  die Puffs weite Teile des Bildes.
- Damit deckt sich der Befund mit der Messung vom 2026-10-18 (STATUS.md): kein
  nachweisbarer Portfehler, die Helligkeitsmodulation kommt aus dem
  Originaleffekt selbst.

## Atlaszellen von Texturgruppe 39 (ausgelesen 2026-10-22)

Über `RoomWorld::effect_group_textures(39)` aus denselben Originaldaten
dekodiert (Werkzeug `build/validation/rm0020-flicker/atlas39.cpp`, Ausgabe
`atlas39.txt`): eine 256×256-Textur, alle UV-Rechtecke von `uvinfo_p0` liegen
innerhalb der Textur (maximal 248 px). Mittleres Alpha je Zelle:

| Zelle | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| meanA | 28,4 | 35,5 | 38,7 | 29,5 | 21,6 | 14,7 | 9,3 | 5,6 | 3,3 | 1,2 | -1 (Tod) |

Das ist ein **monotoner Ausblendverlauf** über das Puffleben. Die Puffs werden
im Zweitakt angelegt, ihre UV-Zelle rückt aber jeden Takt weiter; dadurch
wechselt die Menge der gezeichneten Zellen zwischen {6,8,10,12,14}
(Summe 86,5) und {7,9,11,13,15} (Summe 72,9, Zelle 15 beendet den Puff). Diese
Summendifferenz erklärt die gemessenen rund 30 % Helligkeitsunterschied genau.

**Entscheidung:** Die Zellen werden richtig gewählt (Tabelle, Gruppe, Bildindex
und Grenzen stimmen). Das Flackern ist eine Eigenschaft der Originaldaten
(Zweitakt-Anlage gegen einen 10-Bild-Ausblendverlauf), kein Auswahlfehler.

## Nebenbefund

`snapshots/snapshot-1.rcvxsnap` wurde während der Messung um 23:49:42 auf
RM_0060 geändert (Hash `3e6ba21c…`), offenbar durch einen F5 im Spiel; die
frühere RM_0020-Fassung (`e99e1dd8…`) wurde vorher nach
`build/validation/rm0020-flicker/slot/` kopiert. Meine Läufe haben nur gelesen.
