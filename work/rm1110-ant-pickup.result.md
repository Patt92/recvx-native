# RM_1110: Glanz nach dem Aufheben bleibt bis zum Snapshot-Laden

## Snapshot und Reproduktion

Slot 1 ist `RM_1110.RDX`, stage 1 / room 11, Fall 0, Spawn 0, Cut 2.
Die Inventarliste enthält Item **61 King Ant Object**; der Automat liefert
danach „There is nothing left“. Der Snapshot wurde also nach dem Aufheben
gespeichert.

Für den Vorherstand wurde eine **Kopie** von Slot 1 verwendet: Item 61 aus dem
Inventar entfernt und das Fortschrittsbit **Itemflag 7/13** gelöscht. Damit
bietet der Automat wieder die Originalfrage 157 an; Ja (rechts + OK) fügt Item 61
ein und startet Ereignis 1. Der echte Snapshot bleibt unverändert.

## Ursache (globaler Besitzerfehler)

`bhItmCk`/`bhItmSetCk` setzen im Original `itwp[prm0].stflg |= 0x1000000`
(event.c:2090-2172 und :2180 ff.). Das ist das **Inactive-Bit** des Itemmodells;
`bhControlObjItm` (objitm.c:405) und der Itemglanz prüfen genau dieses Bit.
`EventVm::hide_item` benutzte dafür aber den `bhSetDispObj`-Weg
(Opcode 0x20, nur `mdflg 0x1`/hidden). Folge: Das aufgenommene Itemmodell war
zwar nicht gezeichnet, blieb aber aktiv; der Item-/Glanz-Zustand der
Raum-Entity lief weiter. Ein Raumladen baut die Entitys neu auf und setzt
Inactive aus den Skript-/Fortschrittsflags; deshalb war es nach dem Laden weg.

Das ist derselbe Zustandsbesitz-Fehler wie beim Statuendrehen: ein Hostfeld
(`hidden`) ersetzte ein Originalfeld (`stflg 0x1000000`).

## Fix (bereinigt 2026-10-21)

Der erste Fix lief über einen erfundenen `0x20`-Modus (Byte 3 = 2), der
zusätzlich `hidden` setzte und den Glanzzähler zurücksetzte. Das war ein
Host-Umweg und ist entfernt. Jetzt gilt:

- Eigene Bindung `EventVm::item_disable` → `RoomWorld::deactivate_item`: setzt
  nur `inactive` (= `itwp[n].stflg 0x1000000`). `mdflg 0x1`/`hidden` bleibt
  unberührt, der Glanz friert wie im Original ein (objitm.c:407/:559).
  Ungebunden wird der Originalbefehl einmal als offene Bindung gemeldet, statt
  einen Kunstbefehl einzureihen.
- Die Host-Verstecke in `pickup_result` (GetItem) und `begin_file` (GetFile)
  sind entfernt. Weder `bhCheckExmAtari` (hitchk.c:4847-4890) noch
  GetItem/GetFile verstecken ein Modell; das macht allein das Raumskript mit
  `bhItmCk` auf cb_flg 0x800. Der alte Hostpfad versteckte zudem bei
  Sofortgebrauch (testmode 4) nicht, das Skript schon: der Zustand hing vom Weg ab.
- Masken wie im Original: `bhItmSetCk` prüft `it_flg` mit 0x1FF,
  `bhItmCk` löscht `ic_flg` mit 0x1E0.

Damit gilt der `bhItmCk`/`bhItmSetCk`-Pfad für alle Aufhebungen und Dateien,
nicht nur RM_1110.

## Prüfungen

- Kopie-Snapshot `bit13` (Vorherstand), Replay `pickyes.txt` mit der
  installierten Dev-App: Frage 157, Ja, Item 61. Danach `room_items[2]`
  inactive true, hidden false, keine offenen oder blockierten Befehle.
- Echter Slot 1 nur lesend geladen: `room_items` identisch (Item 61 inactive,
  nicht hidden). Damit ist der Zustand nach Aufheben im Spiel gleich dem nach
  Snapshot-Laden. Slot 1 vorher/nachher SHA-256
  `76a14a1ff8bdc2bc06754e57562571d4c1bb0023ec5eb886396e7f5d7b2b99bc`.
- CTest `rm0150_statue_turn` prüft jetzt End-to-End: Schwert (Item 103) nach
  GetItem durch das Raumskript inactive, nicht hidden. `turn_object` prüft
  `deactivate_item` (inactive, nicht hidden).
- 26 gezielte CTests grün (u. a. item_box_object, script_effect_opcodes,
  player_event, core_continuity, turn_object, rm0150_statue_turn,
  pickup_status_reset, pickup_gap_pad, ammunition_pickup_zero,
  native-gameplay-bindings, inventory_events_assets, event_opcode_lengths,
  event_lifecycle_control_release, rm0110_event_zombie, item_draw_light).
- Build über `./build.sh`, Dev-App arm64 und signiert; Titelstart ohne
  `--in-game` 400 Takte ohne Fehler. Policy-Prüfer und `tools/port_check.py`
  ausgeführt, Ledgereintrag `event.c:bhItmCk/bhItmSetCk` aktualisiert (dabei
  eine verklebte Ledgerzeile `sdMidiPlay` getrennt).

## Grenze und offene Lücken

Der sichtbare Glanz ist im Headless-Zustand nicht als Pixelmerkmal belegt:
Effekt 123 (`bhEff123`, Raumglanz am Automaten) meldet im reproduzierten Ablauf
`visible=false`; der Test belegt die Item-Entity-Inactive-Semantik, nicht jede
Renderer-/Skriptglanzvariante. Eine weiterhin sichtbare **Effekt**-Spur wäre
ein separater Skript-/Effektpfad und müsste mit einer realen Bildabnahme
nachgezogen werden. Kein vollständiger RM_1110-Durchspielnachweis.

Weitere bei der Prüfung gefundene, **nicht** behobene Lücken:

- Hockaufnahme `bhCPM2_act_cro` und Item-ATR-Zweig von `bhCheckExmAtari`:
  inzwischen portiert, siehe `work/crouch-pickup.result.md`.
- bhItmCk-Flagindizes >= 512 überlaufen im Original in ic_flg/ed_flg; nativ
  getrennte Matrizen (nur relevant, falls ein Skript solche Indizes nutzt).
