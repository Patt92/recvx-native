# RM_7110: Türknauf / Hunter-Sequenz / Teleport nach RM_7160

## Korrektur des bisherigen Ursachenbefunds

Die unten behauptete falsche Initialisierungsreihenfolge ist widerlegt.
Original Event_init (event.c:347-354) ruft nach scd0 bhControlEnemy auf,
bevor der erste Schedulerpass startet. Genau diesen Callback besitzen
EventVm::initialize und RoomWorld::bind_player_collision bereits.
Die frueheren Headless-Laeufe hatten implizit --enemy-ai off; dadurch
unterblieb dieser Pass. Keine Aenderung der Initialisierungsreihenfolge.
Es ist NICHT belegt, dass der echte Nutzerstand Flag 356 bereits gesetzt
hatte. fx356 ist eine kuenstliche Gegenprobe, keine Diagnose des Nutzerstands.
Ein erneutes Laden eines unveraenderten Snapshots setzt nicht von selbst 356.

Die bereits im aktuellen HEAD enthaltene Frame-Abfrage liest den nativen Work
auch bei scripted=true (event.c:3276-3279). Neue Regression in
enemy_work_test.cpp prueft 38/39, Neustart und Freigabe gegen einen absichtlich
veralteten Entity-Spiegel. Arm64-Neubau und enemy_work_contract,
event_scheduler, event_opcode_lengths: 3/3 PASS.

Runtime mit --enemy-ai on --enemy-grab on, isolierte Diagnosefixture:
noskip Bild 850, early SELECT 30 Bild 600, second SELECT 250 Bild 600,
chain SELECT 30/200/400 Bild 450: jeweils RM_7110 nach zwei Raumwechseln,
cinematic=false, player_hidden=false, player_scripted=false, keine pending
commands. Sp_flg wieder 0xffffffff; HP 85/140/105/140. Laengere unbewegte
Laeufe werden von Huntern getoetet; deren Game-over ist kein Cinematic-Haenger.
Belege build/validation/hunter7110/final-*.json und ai-on-skip.trace.

NICHT vollstaendig abgenommen: natuerlicher Weg RM_7070/Pickup und Nutzerstand,
visuelle Vollstaendigkeit der RM_7160-Szene, Windows und installierte App.
Alle Laeufe melden noch njSetBackColor alpha >128 als Renderluecke.
Die aelteren Erfolgsaussagen zur dargestellten Szene waren zu weitgehend.
Installierte App unveraendert; keine Spielflags repariert/zurueckgesetzt.

## Belegte Kette

1. Snapshot (Batterie eingesetzt) lädt RM_7110. Aufheben des Türknaufs startet die Hunter-Sequenz (Event 2/9, Tasks 0/1/2/9/40).
2. Event 9 (scd0 @0xab8) läuft in einer WHILE-Schleife auf Opcode 0x38: `38 01 02 00 27 00` = warte, bis Animationsbild von Gegner-Work 2 ≥ 39. Trace: 3584 Ausführungen in 900 Bildern, Work 2 bleibt `mode [1,0,0,4] mtn 0 frm 0`.
3. Gegner 0 (Identität 11, Hunter) hat keinen Zustandsspeicher: `pending_commands` = `67 94 00 00` (Sub_controll 0x94 bhEne11_LightControl), `enemy_gaps` = `bhEne11_LightControl: exp0 fehlt`. Der Work hat bereits `mode0 = 5`; die native EN11-Routine initialisiert nur bei `mode0 == 0` (en11.c:418–422).
4. Skip beendet die hängende Cinematic; das Skript setzt fort und führt seinen eigenen DoorCall aus (`33 ... 07 10 01 ff` = stage 7, room 16 = RM_7160, dor_tp 255). RM_7160 Event 0 door't sofort zurück (`33 ... 07 0b 04 ff`). Ergebnis: `room_changes = 2`, sichtbar RM_7110 → RM_7160 → RM_7110.

Damit ist der Teleport **kein** Skip-Fehler allein, sondern Folge der hängenden Sequenz: ohne Skip bleibt das Spiel in der Cinematic stehen (3500 Bilder geprüft, `player_hidden`, Tasks 0/1/2/9/40).

## Geändert

`native/world.cpp` `RoomWorld::frame(kind,index)`: für gebundene Gegner-Works wird `enemy.frm_no` gelesen (Original event.c:3276–3279 `ene[rom->enep[v1].wrk_no].frm_no`) statt des gespiegelten `e.frame`, das `publish_enemy` nur für ungeskriptete Works schreibt (world_enemies.cpp:1697). Faithful, aber **nicht** ausreichend für diesen Fall, weil der Work selbst auf 0 steht.

Regression: `event_scheduler`, `event_opcode_lengths` grün. Skip-Lauf danach weiterhin `room_changes = 2`.

## Offen (nicht behoben)

Initialisierungsvertrag für skriptgesteuerte Gegner: Im Original durchläuft `bhControlEnemy` jedes Work mit `bhJumpEnemy[id]`; die Identitätsroutine initialisiert bei `mode0 == 0` (eneset.c:337–383, en11.c:58/133). Der Port setzt `mode0 = 5`, bevor diese Initialisierung je lief, wodurch `exp0`/`initialized` fehlen und `bhEne_Event` (mode0 5) nie ausgeführt wird. Nächster konkreter Schritt: klären, ob das Raumskript den Modus vor dem ersten Gegnerpass schreibt (Port-Reihenfolge) oder ob die Initialisierung wie `bhSetEnemy` beim Raumanlegen erfolgen muss; danach EN11/alle Identitäten über den bestehenden Besitzer korrigieren. Kein Host-Workaround, keine Raumsonderlösung.

Nicht geprüft: Windows/Hardware, natürlicher Aufhebe-Ablauf ohne Diagnose-Snapshot-Bearbeitung (Fixture setzt Event 37/Item-Flags), vollständige Raumabnahme. Installierte App unverändert.

## Nachtrag: Ist der Aufenthalt in RM_7160 korrekt?

Raumziel: **ja**, es steht im Original-Raumskript von RM_7110 (`33`, Stage 7 / Raum 16, dor_tp 255).
Zustand: **nein.**

RM_7160 scd1: `04 01 64 01 00 01` startet Event 0/1/8 nur, wenn `ev_flg 356` **frei** ist, und setzt 356 direkt danach (`05 01 64 01 00 00`). Es ist ein einmaliger Cutscene-Besuch.

Messung A (Flag frei): RM_7160, `cinematic=true`, Tasks 0/1/8/15, Chris auf `y=-100` (außerhalb des Spiels). Danach Rueckkehr nach RM_7110 (`room_changes=2`).
Messung B (Flag 356 gesetzt, `fx356/`): RM_7160, `cinematic=false`, `player_scripted=false`, Chris auf `[102, 0, 115.3]` mit normaler Steuerung, nur Task 15. Event 0 (der Rueckweg) startet nicht -> **Sackgasse**.
`f356-400.json`, `in7160-{300..460}.json`, `q{260..460}.json`.

Folge: Der erste (hängende, per Skip verlassene) Versuch setzt 356. Jeder weitere Versuch landet dadurch ohne Szene an der Doppeltür und kommt nicht mehr heraus. Das erklärt die Beobachtung "Position passt überhaupt nicht".

## Nachtrag: Herkunft von mode0 = 5

`native/world.cpp` 0x2d/0x30 (bhInitMotionPause/Ex, event.c:1851/1879) setzt `w.mode0=5; w.mode3=4` — genau der Zustand des Hunter-Works. Im Original laeuft `bhControlEnemy` (game.c:46) **vor** dem Eventpass, die Identitaetsroutine initialisiert dort bei `mode0==0` (eneset.c:377, en11.c:58/133). Der Port fuehrt das Raumskript (scd0/Event 2) vor dem ersten Gegnerpass aus, daher bleibt `exp0`/`initialized` leer.

## Endabnahme natuerlicher Weg (2026-10-25 abends)

Startzustand `build/validation/hunter7110/natural/pre/`: Diagnosefixture mit
geloeschtem it_flg 37 (Tuerknauf liegt wieder, Item 93 aktiv, keine Szene,
CRC neu). Sonst unveraendert; kein Nutzersnapshot taugt (alle haben ev_flg 356
bereits gesetzt bzw. liegen vor der Antarktis). Ablauf: Position am Knauf
(19.1/0/49.5), OK -> Dialog -> Aufnahme (Bild ~150) -> scd1 startet Event
0/1/2/9 (Bild ~260) -> RM_7160 Event 0/1/8 -> DoorCall `07 0b 04` zurueck nach
RM_7110 (nicht RM_7070; RM_7070 hat keinen DoorCall) -> Event 3/4/5 -> frei.
Mit --enemy-ai on --enemy-grab on, Build- und Start.command-App-Binary:
ohne Skip, SELECT 280/330 (erste Hunter-Szene, Nutzerfall), 500 (RM_7160),
280+470, 280+470+720, 4x SELECT 280..290: alle RM_7110, room_changes 2,
player_scripted/hidden false, keine pending/blocked commands, Bewegung per
keys verschiebt Chris. Hunter greifen danach an (stillstehend teils Tod).
Offen: njSetBackColor alpha >128 weiter gemeldet; Bildinhalt der Szenen
(Aufzugsschacht RM_7160 mit schwarzem Schachtgrund) ohne Originalvergleich;
Sichtpruefung im Fenster, Windows.

Fensterabnahme Nutzer (2026-10-25, Start.command-App-Binary, Fixture natural/pre):
Start RM_7070 -> RM_7110, Knauf aufgehoben, SELECT in Szene RM_7110, RM_7160
und Folgeszene RM_7110 -> frei, danach Tuerwechsel RM_7070/RM_7110 normal.
Drei Laeufe mit diesem Raumverlauf, Event VM ohne offene/blockierte Befehle.
Korrektur: Snapshotladen schreibt sehr wohl eine Raumzeile (headless belegt).
Keiner der vier Fensterlaeufe hat die Fixture geladen; alle starteten per
Debug-Teleport aus dem Neuspielstand RM_0000 (Laeufe 2-4 nach RM_7070, Lauf 1
direkt nach RM_7160). Lauf 1 ("wieder an der Tuer in RM_7160", P gespammt) ist
damit ein kuenstlicher Flagstand; headless Teleport RM_0000->RM_7160 bleibt in
RM_7160 ohne Szene (356 frei, nur Task 15). Nicht reproduziert, kein Befund.
P-Spam headless (alle 2/3/5/7 Bilder ab Aufnahme, je zwei Phasen) auf der
Fixture: 8/8 RM_7110 -> RM_7160 -> RM_7110, frei, keine offenen Befehle.
