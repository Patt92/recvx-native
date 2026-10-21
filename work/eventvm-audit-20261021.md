# EventVm-Audit und fehlende Funktionen (2026-10-21)

Stand: Commit f86b985 plus Arbeitsbaum ohne Codeänderung. Nur Prüfung, kein Fix.

## Messung

- **Opcodetabelle** (event.c:59, 256 Einträge): Alle 223 echten Befehle haben im
  Dispatcher `EventVm::instruction` einen Fall. Ohne Fall sind nur die 33 `dm0`
  (event.c:437, `return 0` ohne Vorschub).
- **Raumscan** über 205 Räume, 600 Takte, ohne Eingabe:
  - Mit `--enemy-ai on`: 0 blockierte Befehle. Offen ist nur `0x23 bhItmSetCk`
    in RM_3100, RM_3140 und RM_7030; dort steht ein ATR-Index außerhalb der
    Tabelle, und das Original schreibt in fremden Speicher.
  - Ohne Gegner-KI (Standard von `tools/scan_event_rooms.py`) erscheinen
    zusätzlich Common 24/25/34 in 25 Räumen und Sub 148 in 2 Räumen. Das ist ein
    Messartefakt: Die Gegnerwerke sind dann nicht initialisiert.
  - Restliche Weltmeldungen betreffen alle undefinierte Originallesungen
    (Nachbarzelle, `ang`, Wegezone, ein Bild hinter `frm_num`).
  - Belege: `build/validation/eventvm-audit-20261021/`.

## Echte Lücken

1. **Player_controll 132, 140, 141, 143, 145** (event.c:9507/9425/9358/9544/9563)
   haben nativ keine Länge. Läuft einer davon, wirft die VM „unimplemented
   Player_controll command“ und hält das Ereignis an. Es sind Ereignismodi des
   Spielers (mode0 7, mode2 5/8, Motionbank plmthp/rmthp). Der Scan hat keinen
   davon erreicht. Ob ein Raumskript sie nutzt, ist **nicht belegt**; dafür
   fehlt ein statischer Skript-Walker.
2. **Ohne Weltwirkung**, Länge korrekt, als offene Bindung gemeldet:
   - `0x82 bhSetDebugLoopEx`: Debug-Ereigniskamera.
   - `0xCF bhExGameItemInit`: ExtraGameItemInit, Battle-Game-Ausrüstung.
   - `0xD3 bhRankingCall`: Rangliste.
3. **Tor vor bhCheckExmAtari** (player.c:1678): Das Original verlangt
   `plp->mode0 == 1`, den Spielerpass (sp_flg 0x1, stflg 0x1000000 frei), die
   drei Flagmasken und `pad_ps 0x200`. Nativ prüfen `exm_flags_clear` die Masken
   und `EventVm::interact` stattdessen st_flg 0x4, Dialog und Menü. Das ist eine
   Host-Näherung:
   - Es fehlt die Prüfung `mode0 == 1`.
   - st_flg 0x4 sperrt zusätzlich. Im Original löst erneutes OK während der
     Hockaufnahme bhCheckExmAtari wieder aus; nativ nicht.
   - Bei `bhMessageSet` mit v2 != 0 bleibt sp_flg 0x1 stehen, der Spielerpass
     läuft also weiter. Nativ sperrt der offene Dialog.
4. **`dm0`** wirft nativ einen Fehler. Im Original gibt es 0 ohne Vorschub
   zurück, die Aufgabe steht dann dauerhaft. Wirkung nur bei kaputten Skripten.
5. **Hostschutz bewusst:** das Budget von 10000 Befehlen je Lauf, die
   Debugmenü-Absicherung `cancel_item_request` und die
   StatusCancel-Übergabe über `status_use_pending_` (die animierte
   Schließphase fehlt).
6. **Unvollständig laut Ledger** (event.c, partial): die
   WeaponSet-/Monitor-Zustandsmaschine hinter bhPlItemLost/Ex,
   bhArmsItemSet und bhItemGetGet/Ex; der vollständige plp/ply_stflg-Pfad von
   bhPlayerPoisonCk; der Loop-/Sleep-Stack von bhNext; Common 24/25/34
   Selektor 0 (plmthp); `gm_flg 0x40` (Spieleraugen-Schnitt) in `control_exm`.

## Veraltete Buchführung

- `tools/port_check.py` zählt 12 Befehle als „consumed only“. Davon sind 8
  falsch eingestuft: Die ObjLink-Varianten 0x32/0x34/0x51-0x54/0x59/0xA3 bindet
  `world.cpp` über eine Tabelle, die der Klassifizierer nicht sieht. Bei 0x89
  (Spielerwechsel) hat `runtime.cpp` einen Verbraucher, nur der Kommentar
  „consumer pending“ ist veraltet. Tatsächlich offen sind 0x82, 0xCF und 0xD3.
- Ledgerzeile `event.c:bhScenarioJmpT:offen` (Stand 2026-10-19) nennt noch
  0x6F/0x97/0xAA/0xAF als offen; die sind inzwischen gebunden.
- `tools/scan_event_rooms.py` läuft ohne `--enemy-ai on` und meldet deshalb
  Scheinlücken.

---

# Nachtrag: Umsetzung (2026-10-21, Arbeitsbaum)

## Erledigt

- **dm0 wie im Original.** Die 33 unbelegten Tabellenplätze (0xD5-0xF2, 0xF5-0xF7)
  liefern 0 ohne Vorschub (event.c:437); die Aufgabe bleibt auf dem Byte stehen
  und meldet keinen Hostfehler mehr. Einmal je Fundstelle erscheint auf der
  Konsole `EVENT dm0: Ereignis N, Ebene L, Offset 0x…, Opcode 0x…`. Kein
  Raumskript benutzt einen dieser Plätze (Walker, s. u.).
- **0x82 bhSetDebugLoopEx** (event.c:2421): setzt die laufende Ereigniskamera auf
  cam.ct0 neu auf (`RoomWorld::bind`). Ohne laufende Ereigniskamera gemeldet,
  keine Aktion. Kein Raumskript nutzt ihn.
- **0xCF bhExGameItemInit** → `Inventory::extra_game_item_init`
  (sub1.c:8553 ExtraGameItemInit, Battle-Game-Ausrüstung in itm[cng_pid*16];
  der ssd_flg-Zweig hat im Original keinen Schreiber, ist aber mitgeführt).
  Nur RM_5500 Ereignis 24.
- **0xD3 bhRankingCall** (event.c:9049): ts_flg = 0 und die Taskanforderung
  (tk_flg 0x702040 = Pad, Ending, Monitor, SndMonitor, ScreenSaver). Der Host
  verbraucht sie; **ranking.c ControlRanking** (Task 13, 1171 Zeilen, hängt am
  Systemspeicher) bleibt eine gemeldete Lücke. Vorkommen: RM_5800–5840, RM_9360
  (Battle Game, nativ nicht erreichbar).
- **Buchführung:** `port_check` sieht jetzt tabellengebundene Befehle (ObjLink);
  Stand 223/223 gebunden, 0 nur konsumiert. Ledgerzeile `bhScenarioJmpT:offen`
  und der 0x89-Kommentar aktualisiert. `tools/scan_event_rooms.py` läuft mit
  `--enemy-ai on`.
- **Punkt 5 neu belegt** (jeweils Zeile für Zeile am Original): bhItemGetGet/Ex,
  bhPlItemLost/Ex, bhArmsItemSet (keine dieser Routinen ruft WeaponSet, der
  frühere Ledgertext stammte aus der Generatorzeit), bhPlayerPoisonCk (Bit
  0x200000 beim Inventarbesitzer), bhNext/bhFor. Alle jetzt `done`.
- Neues Werkzeug **`tools/walk_event_scripts.py`**: statischer Lauf über alle
  Skripte der 205 Räume (324 080 Befehle, 0 unbekannte Längen, jedes Skript endet
  genau am nächsten Skriptoffset). `--find 64:84 82 cf d3` listet Fundstellen.

## Player_controll 132, 140, 141, 143, 145 – Befund

Der Walker findet **keinen einzigen Aufruf** in den 205 Raumskripten. Die fünf
Fälle sind in der ausgelieferten Fassung toter Code. Zur Entscheidung, was sie
täten (event.c:9358-9600, alle am `bhCetask->work`, also dem Werk der laufenden
Ereignisaufgabe):

| Fall | Länge | Wirkung |
| --- | --- | --- |
| 132 | 6 | `mnwP = plmthp` (Spielerbibliothek), mode0 7, mode2 5, mode3 0; mode1/ct0/ct1/ct2 aus den Operanden. Ereignismodus 5 des Spielers = `bhCPM0_event`-Zweig 5 (pl_evt.c). |
| 140 | 8 | mode3 = (mode2 == v0), mode0 7, mode2 = v0 + 4, ct0 = ±u16 << 16 \| ±u16 (Vorzeichen aus bpx/bpz der Aufgabe). |
| 141 | 6 | `exp1[0] \|= 0x1C`, `exp1[16] = v0`, `exp1[34..35] = v1, v2` (Kopfsteuerung: Blickziel/Flags). |
| 143 | 4 | `mnwP = plmthp`, mode0 7, mode2 5, mode3 = v0. |
| 145 | 6 | `mtn_add = 32768` (halbe Geschwindigkeit), `mnwP = rmthp` (Raummotion), mode0 7, mode2 8, mode3 0; mode1/ct0/ct1/ct2 aus den Operanden. |

Nativ hätten 132/143/145 den Ereignismodus des Spielers (`player_event.c`) um die
Zweige mode2 5 und 8 zu erweitern; 140 braucht den ct0-Zielpunkt dieser Zweige;
141 schreibt direkt in die Kopfsteuerung (`RECVX_HEAD_STATE`). Ohne Aufrufer gibt
es keinen Beleg, was sie im Spiel bewirken; ein Port wäre nur gegen die
Quelle prüfbar. Mein Vorschlag: nicht portieren, als „unbenutzt“ im Ledger führen.

## Weiterhin offen

- Tor vor bhCheckExmAtari (Punkt 3 des Audits): unverändert.
- Common 24/25/34 Selektor 0 (`mnwP = plmthp`) **auf Gegnerwerken**: 294
  Fundstellen mit Selektor 0, der Laufzeit-Scan erreicht keine davon auf einem
  Gegner; auf dem Spieler ist der Selektor gebunden.
- `bhCheckCut(1)` unter gm_flg 0x40 (Augenkamera aktiv, cut.c:24-64): das
  Original setzt Nebel/pt_flg zurück und ruft bhInitPlEyeCamera erneut; nativ
  lässt `control_exm` gm_flg 0x800 stehen. Kein beobachteter Auslöser.
- ranking.c ControlRanking (s. o.).
