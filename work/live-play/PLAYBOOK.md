# PLAYBOOK — Agent spielt RECVX live (Stand 2026-10-26)

Fuer das Modell, nicht fuer Skripte. Ziel: fluessig spielen, wenig Kontext, keine Bilder.
Nutzerregeln: Metal (`--renderer gpu`); Dialoge lesen; Snapshots nur Slot 2; keine
festen Ablaufskripte je Raum; Screenshots nur bei Verdacht auf Darstellungsfehler.

## 1. Schleife (eine Entscheidung = ein Werkzeugaufruf)

```
LESEN    python3 tools/agent_play/nav.py build/play/auto.json          (10–20 Zeilen)
PLANEN   Ziel aus Zonen/Gegnern/Dialog waehlen; Weg mit `path`, nie raten
HANDELN  eine Zeile route/fire anhaengen, dann `wait` bis frei/Gefahr/Raumwechsel
```

Ein Aufruf soll Lesen + Handeln + Warten verbinden, z. B.:
```
A=build/play/auto.json; L=build/play/live.txt; N=tools/agent_play/nav.py
python3 $N $A path 61 25 run face 75 25 then confirm >> $L && python3 $N $A wait $A $L 60 stop room danger | tail -12
```
Kein `sleep` ausser beim Start. Keine Dumps mit Namen in der Schleife, nur `auto.json`.

## 2. Start

```
rm -f build/play/live.txt build/play/live_state.json build/play/auto.json
printf '10 new_game\n70 select\n110 confirm\n150 select\n190 confirm\n230 select\n270 confirm\n330 cancel\n350 cancel\n370 inventory\n400 sdl_key down F2\n402 sdl_key up F2\n410 debug_select snapshot_load 1\n414 confirm\n440 autodump 10 auto\n441 run auto 18\n' > build/play/live.txt
```
Runtime im Terminal-Tab (Host hat Metal, Sandbox nicht):
```
cd /Users/patrickhoffmann/Projects/recvx-decomp && "build/start/macos-arm64/RECVX Native.app/Contents/MacOS/recvx-runtime" --data data --mods mods --controls controls.ini --debug-config debug.ini --video-config video.ini --renderer gpu --enemy-ai on --enemy-grab on --snapshots build/play/snap --control build/play/live.txt --state build/play/live_state.json
```
Nach ~20 s steht `auto.json`; Zeile 1 muss `FREI` zeigen. `Renderer: Metal ...` im Terminal pruefen.

## 3. Runtime-Schnittstelle (`--control <datei>`, Zeilen `<takt> <aktion>`, Takt relativ, 30 Hz)

| Aktion | Wirkung |
|---|---|
| `route x z [x z …] [run] [stop d] [face x z] [then <zeile>]` | laeuft Wegpunkte selbst ab (dreht, geht, naechster Punkt bei < 3), dann `face`, dann `then` (z. B. `confirm`). Stillstand 45 Takte → `control.result = stalled`, `then` verworfen |
| `goto x z [run] [stop d]` / `face x z` | Einzelpunkt / nur drehen |
| `keys forward\|back\|left\|right\|run\|aim\|attack\|none … [dauer]` | rohe Pads; bricht laufende route ab |
| `confirm` `cancel` `up` `down` `left` `right` `inventory` `select` `skip` | Menue/Dialog; `select` = einziger Kinoskip |
| `run on\|off\|auto [d]` | `auto`: Echtzeit, haelt aber an, wenn nichts ansteht UND ein aktiver Gegner < d ist (`control.held`) |
| `autodump <takte> <name>` | Live-Rohdaten, atomar; Standard `autodump 10 auto` |
| `dump <name> [shot]` | Einzeldump, `shot` = BMP (nur fuer Darstellungspruefung) |
| `sdl_key down\|up F2` | echtes Debugmenue-Toggle (zuverlaessiger als `debug_key`) |
| `debug_select <id> <wert>` + `confirm` | z. B. `snapshot_save 1` / `snapshot_load 1` (Slot 2) |

Ohne `--control` ist alles inaktiv. Fehler erscheinen nur als `Control: …` im Terminal.

## 4. Werkzeuge (`tools/agent_play/`)

| Aufruf | Ausgabe |
|---|---|
| `nav.py A` | Raum, Pos, Blick, HP, Besitzer der Eingabe (FREI/KINO/DIALOG/INVENTAR/TUER/DEBUGMENUE/GAMEOVER), Dialogtext+Wahl, Zonen (TUER/TEXT/ITEM mit Mitte, Abstand), Gegner mit `SICHT`/`wand`, Items, Inventar |
| `nav.py A path x z [via x z …] [run] [face x z] [then …]` | Wegsuche (Gitter 1.0, Radius 4, gleiche Wandauswahl wie `move_walls`) → genau eine `route`-Zeile auf stdout; Fehler auf stderr, Exit 2 |
| `nav.py A wait A L s [stop] [room] [danger]` | blockiert bis Kino/Dialog/Tuer/route vorbei; quittiert fertige Dialogseiten selbst und gibt Text aus; haelt bei Ja/Nein; `room` = erst nach Raumwechsel; `danger` = frueher bei Gegner < 14; `stop` = danach `run off` |
| `nav.py A fire n [low] [face x z]` | Zeilen fuer n Handgun-Schuesse; `low` = Ziel senken (liegende Zombies) |
| `nav.py A map [zelle [x0 z0 x1 z1]]` | ASCII-Draufsicht (einmal je Raum, nur wenn `path` scheitert) |
| `waitfor.py feld wert [s]` | z. B. `box_open true` |
| `scdump.py RM_xxxx event7` | Raumskript lesbar (Ereignis einer Zone = `prm1` der TEXT-Zone) |

## 4a. Debug-Stream statt Bild (Hintergrund oder Unteragent)

```
python3 tools/agent_play/stream.py build/play/auto.json      # run_in_background; Lock, endet nach 120 s ohne Daten
```
Braucht `autodump 5 auto` (oder 10). Schreibt nur Aenderungen nach `build/play/stream.log`
(RAUM, BESITZ, DIALOG, HP, GRIFF, GEGNER Zustand/Haltung, NAH, SICHT, INVENTAR, ZONE/WAND an/aus,
ITEM sichtbar, ROUTE angekommen/steckengeblieben, GAMEOVER) und haelt `build/play/radar.txt`
aktuell (~15 Zeilen: Lage, 5 naechste Gegner mit Richtung links/rechts/vorn/hinten, Haltung,
Bodenkontakt, Sicht; 5 naechste Zonen; Inventar; letzte 4 Ereignisse). Der Agent liest `radar.txt`
(oder `tail stream.log`) statt eines Screenshots. Ein Unteragent kann den Stream halten und nur
bei NAH/GRIFF/GAMEOVER/DIALOG [JA/NEIN] melden.

Gegnerhaltung kommt aus den Knotenmatrizen der Engine relativ zum Boden unter dem Gegner:
`body.posture` standing/low/lying (top < 11 / < 6), `body.contact` ground/air/under (Fuss > 3 ueber
bzw. < −3 unter dem Boden). `lying` → `fire n low`. `air`/`under` ohne Skriptgrund = Auffaelligkeit
fuer den Nutzer (z. B. RM_0030 id62 bei 60 Hoehe = Hubschrauber/Skriptobjekt, kein Fehler).

## 4b. Hinweise, wenn der naechste Schritt unklar ist (`tools/agent_play/hint.py`)

Eskalation, billig zuerst: Zonentext lesen → `hint.py room` → `hint.py zone i` → `hint.py blocked x z`
→ `hint.py event n` / `op` / `grep` → erst dann Screenshot.

| Aufruf | Liefert |
|---|---|
| `hint.py room` | je Zone: Meldungstext, Item-Name (genommen/versteckt), Tuerziel, Skriptstelle |
| `hint.py zone i` | Sperrrichtungen, direkte Meldung, Skriptblock der Zone mit Bedingungen (Flags jetzt), wo sie an/aus geht |
| `hint.py blocked x z` | welche Wand den Weg sperrt; fest → Umweg; per Skript → welches Ereignis sie oeffnet und was es startet |
| `hint.py event n` | Ereignis dekodiert mit Originalnamen (bhScenarioJmpT), Texten, Items, Flags |
| `hint.py op b7` / `op bhItemPlToSBox` | Implementierung im Port und Originalstelle in event.c |
| `hint.py grep <regex>` | begrenzte Suche in native/ und src/ps2/veronica/prog/ (Semantikreferenz, nur lesen) |

Code-Peeking ist erlaubt, aber nur lesend und nur in diesen Pfaden; niemals archived/.

## 4c. Screenshot = letztes Mittel

Nur `dump x shot`, wenn Stream/Hint widersprechen oder der Nutzer nach dem Bild fragt
(Darstellungsfehler). Nie zur Navigation, nie fuer Menues (Box/Inventar: `box_open`,
`security_items`, `items` im Dump lesen).

## 5. Entscheidungsregeln

- **Hindernis/Raetsel**: erst Zonen-Texte lesen (TEXT-Zone + `confirm`), Text sagt die Loesung
  (z. B. "release the metal detector" → Security Box). Dann `scdump.py` des Zonen-Ereignisses.
  Nicht laufen, um zu suchen. `path` = "KEIN WEG" heisst: Tor/Wand per Skript, nicht Navigationsfehler.
- **Gegner**: `SICHT` + Abstand 15–35 → `fire`; < 10 → erst `path` weg, dann schiessen; liegend → `fire n low`.
  Griff (`pw[0]==4`): 20x `keys left`/`keys right` im Wechsel. Munition im Blick behalten (`inv 9,x` Handgun, `12,x` Patronen).
- **Heilen** unter 60 HP: `inventory`, Cursor auf Kraut, `confirm`, `confirm`, 3x `cancel`.
- **Snapshot Slot 2** nach jedem Raumwechsel ohne Gegnerkontakt und vor Kaempfen. Game Over → laden, nicht Retry.
- **Kein Zurueckrennen ohne Plan**: Ziel vorher aus Zonen/Text bestimmen. Abkuerzungen (Vorhaengeschloss
  RM_0050) sind spaeter; der Raum hat einen Umweg (`path` findet ihn).
- **Budget**: Ziel ≤ 1 Aufruf je Aktion, Raum mit 2 Items und Tuer in ≤ 6 Aufrufen. Screenshots: 0.

## 6. Messwerte

Drehen 1310 Yaw/Takt; Gehen 0.49, Laufen 1.0 Einheiten/Takt; yaw 0 = Blick −z, 16384 = −x.
Typ-4-Item-Sonde 2.0 vor dem Spieler, muss in der Zone liegen; attr 0x400/0x800/0x1000/0x2000 sperren
Blick +z/−x/−z/+x. Treppen (Typ 1): Sonde 0.75 vor dem Spieler, an der Zonenkante OK druecken.
Tuer 150–600 Takte, Kinos 1200–1700. Leerlauf 4–7 % eines Kerns, `auto.json` 30–60 KB.

## 7. Fortschritt / Fakten (Claire, Disc 1)

RM_0000–RM_0070 erledigt (Feuerzeug, Messer, Handgun, Karte, Kraeuter, Munition).
RM_0050 → RM_0080: Umweg im Westen statt Vorhaengeschloss (`path -40 52.5`, Tuer mit Blick +x).
RM_0080: 5 Zombies, Ostkorridor; Ausgang Tuer i1 (60,−12) Blick −z → RM_0090.
RM_0090: Metalldetektor. Security Box i11 (Konsole 69,106 Blick +x): **alles Metallische inkl.
Feuerzeug und Ink Ribbon einlagern**, Schalter i7 betrifft nur die Freigabe. Zweite Box i12 (44,4).
**Offen/ungeprueft:** die Nordbox oeffnet `itm[224]` (leer), die Gegenstaende liegen in `itm[192]`
(`security_items`). Ob das Original zwischen beiden umkopiert (sub1.c:1606-1617, Ereignis 0xb7/0xb8)
oder der Port hier eine Luecke hat, ist nicht geklaert — erst pruefen, dann weiterspielen.
**Snapshot 2** = RM_0090 nordseitig (41,12) mit leerem Inventar; bei Problemen neu von RM_0080.

## 8. Fallen (schon passiert)

- `path` ohne `2>/dev/null`-Trennung haengte frueher Fehlertext an live.txt (jetzt stderr).
- Nach `snapshot_load` bleibt das Debugmenue teils offen → Zeile 1 von `nav.py` zeigt DEBUGMENUE.
- Menuezeilen mit festen Takten nach einer `route` laufen ins Leere: erst `waitfor`, dann Menue.
- `wait` vor dem ersten frischen `auto.json` liefert alten Stand; es wartet auf neue Dateizeit.
- Schuesse in die Luft bei liegenden Zombies: `fire n low`.
