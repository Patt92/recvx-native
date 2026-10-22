# Effektportierung — Audit (reiner Lesestand, 2026-10-22)

Kein Code, kein Test, kein Ledger geändert, kein Build. Rohdaten unter
`build/validation/effect-audit-20261022/` (Dateinamen unten kursiv genannt).

## 1. Methode und Datenstand

- Arbeitsbaum: `main` auf 5eab955, lokal `M third_person.ini`, `?? video.ini`,
  `?? work/rm0020-explosion-flicker.result.md` (unverändert gelassen).
  `python3 tools/check_no_original_core.py` → PASS.
- Binary: `build/macos-arm64/recvx-runtime` (mtime 2026-10-22 00:01), nicht neu gebaut.
- Original-Sprungtabellen aus `src/ps2/veronica/prog/effect.c:33-37`
  (`bhJumpEffect[150]`, `bhJumpEffect0[100]`, `bhJumpEffect2[50]`, `bhJumpEffect3[50]`,
  `bhJumpEffect4[50]`, Dispatch `effect.c:817-836`) → *orig_jump_table.tsv*:
  400 Plätze, 143 `bhEffDmy`, 257 Nicht-Dummy-Plätze (darunter `bhEffBG` 7 Plätze 93..99,
  `bhEffFil` 3 Plätze 90..92). `ef_info[21]` (`effect.c:38`) ist die Modellblock-/
  Texturzählung für `bhInitEffect`, keine Prozedurtabelle.
- Native Zuordnung: `native/effect_sprite_procedures.cpp:6085-6122`
  `hand_effect_procedures[]` (206 Kennungen) plus Schalterfälle
  `native/effect_sprites.cpp:1125-1133` `hand_ported_effect_sprite` (41 Kennungen) →
  *native_supported.json*; dazu Besitzerpfade ohne Sprite-Prozedur: 1 (`world_enemies.cpp
  tick_shadow`), 3 (`world.cpp:1944 spawn_weapon_flash`), 258 (`world_enemies.cpp
  tick_e03_shadow`), 300/302 (`effect_particles.cpp`), 304 (`world_enemies.cpp
  tick_ry_shadow`), Dispatch `world.cpp:3533-3545`.
- Lückenmeldung: `world.cpp:848/874/894/1925/2108` `unbound_enemy_.insert("bhEff"+id)`
  bei `!supported_effect_sprite(id)`; Ausgabe als `Enemy gaps:` (`runtime.cpp:1613`).
- Ledger: `docs/port_ledger.tsv`, 251 Zeilen `effsub*/effect.c` (220 done, 30 partial,
  1 missing) → *ledger_effect_rows.tsv*, Nicht-done-Texte *ledger_effect_notdone.txt*.
- Anforderer: Original `sys->ef.id = N`, `bhSetEffect*(N`, `setentry(N` (effsub0.c:10),
  `WpnEffTab[23][4]` (player.c:48) → *orig_requesters.tsv*; nativ analog →
  *native_requesters.tsv*, *native_requesters_wide.json*.
- Raumtabellen: 205 RDT, Basis u32@16, Tabelle u32(Basis+0x14), Anzahl u32(Basis+0x94),
  68-Byte-Einträge id u16@+4, type u16@+6, lkflg u32@+40 (wie `world.cpp:196-206`):
  3171 Einträge → *room_effect_tables.tsv*. Kein Eintrag mit lkflg 5.
- Skripte: `tools/walk_event_scripts.py --find 2c 43 91 92 97 98 6e a8 aa b0 c3 c8 96 ca`
  → *walk*, ausgewertet nach Kennung (0x43/0x91/0x92/0x98/0xa8/0xaa über den Raum-
  Tabellenindex Byte 1) → *script_effect_requests.tsv*.
- Laufzeit: `tools/scan_event_rooms.py --frames 600 --jobs 3` → *scan600/* (205/205 Räume
  berichtet, 0 blocked, 0 failed; *scan600/summary.txt*), Aggregat der `Enemy gaps:`/
  `Combat gaps:` → *scan_gaps_by_pattern.tsv*.
- Gesamttabelle je Kennung → *effect_master.tsv* (306 Zeilen: alle Nicht-Dummy-Plätze,
  Dmy 139..142 und alle Dummy-Plätze, die in Raumtabellen vorkommen).

Walker-Vorbehalt: RM_0120, RM_5520 und RM_1030 haben Ereigniskopf 8 (0 Ereignisse);
der Walker läuft dort in scd1 bis Dateiende weiter. Alle 2334 „0x2c“-Treffer (Kennungen
514..21578), die 12 „0x6e“-Treffer in RM_0120 (547, 1026, ...) und die 880 „0x43 idx -1“
-Treffer sind Artefakte dieser drei Räume plus Indizes außerhalb der Raumtabelle
(RM_0150/1060/3190/6030/6080/6090/7090/9020/9030/9200/9220/9270, nicht einzeln geprüft).
Legitime Skriptanforderungen per Kennung: 0x6e → 113 (RM_0030/0070/1090/3100/3140),
398 (RM_3130); 0xb0 → 113 (RM_9110), 210 (RM_5001), 211 (RM_1050, RM_5001);
0x97 → 2 (9×); 0x96 → 120 (2×); 0xc8 → 23/24 (12×). Kein legitimes 0x2c gefunden.

## 2. Status aller originalen Kennungen

Vollständig in *effect_master.tsv* (Spalten: Original-Prozedur/-Fundstelle, nativer
Pfad, Ledgerstatus/-zeile, Original-/native Anforderer, Raumtabellen, Skripte,
Scan sichtbar/unsichtbar). Zusammenfassung:

| Klasse | Zahl | Kennungen |
|---|---|---|
| nativ vorhanden und im Dispatch (Tabelle/Schalter/Besitzer) | 253 Plätze | alle Nicht-Dummy-Plätze außer unten; dazu Dmy 139..142 |
| **fehlt** (keine native Prozedur, kein Besitzer) | 8 Plätze / 2 Prozeduren | `bhEffBG` 93..99 (effsub1.c:145), `bhEff_Sub367` 367 (effsub4.c:2007) |
| Raum-Texturhalter ohne Prozedur (Original `bhEffDmy`) | 49 Kennungen | 30..36, 39, 41..46, 50..52, 54..57, 59..62, 64..83 (Raumtabellen, *room_effect_tables.tsv*) |
| Ledger **partial** | 30 Zeilen | 0, 1, 9, 111, 130, 134, 136, 199, 260, 261, 270, 360, 361, 362, 365, 368, 370, 371/372/373, 398, Dmy; dazu effect.c `bhDrawTrsEffect3D`, `bhInitEffect`, `bhSetShadow`, `bhControlEffect`, `bhDrawThlEffect3D`, `bhEff_ModelDraw:Spiegel`, `bhDraw136`, `bhDraw138`, `bhEff30cDrw`, `bhEff_E11_CheckCollisionPlayer` |
| Ledger **missing** | 1 | 367 |
| nativ vorhanden, aber **ohne Ledgerzeile** | 38 | 2, 3, 4, 6, 20, 21, 28, 122, 123, 125, 126, 129, 174, 181, 182, 197, 218, 229, 230, 262, 264, 265, 274, 275, 276, 296, 297, 300, 302, 304, 307, 309, 311, 312, 352, 396, 397, 399 |

### 2a. Fehlende Prozeduren

- **bhEffBG (93..99)**, `effsub1.c:145-176`: mode0 0 setzt flg 0x4100001, `tex_id = id`,
  weiße Ecken, UV (0..0.625, 0..0.46875), Blend 8/6; mode0 1 reiht jeden Pass in
  `sys->ef_opq` (opake Liste) ein. Vorkommen: Raumtabellen 93 RM_5001; 94 RM_1000/1001/1002;
  95 RM_7240; 96 RM_3240/RM_9090; 97 RM_9090 (*room_effect_tables.tsv*). Skripte: 0x43
  bhEffDispSet auf 93 (RM_5001 1×), 94 (RM_1000/1001/1002 6×), 96 (RM_3240/RM_9090 15×),
  97 (RM_9090 10×); 0x92 auf 94 (RM_1000), 95 (RM_7240) (*script_effect_requests.tsv*).
  Nativ: kein Eintrag in `hand_effect_procedures[]`, kein Schalterfall, keine Ledgerzeile,
  und `ported_effect_atlas` (`effect_sprites.cpp:1138-1158`) nennt 93..99 nicht, so dass
  `world.cpp:206` den Atlas dieser Kennungen gar nicht dekodiert. Der Satz bleibt mit
  flg 1 ohne Prozedur im Pool; eine Lückenmeldung entsteht nur beim Skript-/Code-Spawn
  (`world.cpp:848 ff.`), nicht für Raumtabelleneinträge.
- **bhEff_Sub367**, `effsub4.c:2007`: Ledger missing („exp0/op->func ohne Erzeuger im
  Original“). Kein Anforderer in prog/*.c, keine Raumtabelle, kein Skript.

### 2b. Vorhanden, aber nicht angebunden

- **bhEff026 aus bhCheckEvtTimer** (`player.c:1268-1290`): bei abgelaufenem `evt_tim`
  und ev_flg 69 ruft das Original `bhSetEffect(26, &pnt, NULL, 0)` (player.c:1284) vor
  `plp->hp = -1`. Nativ `native/events.cpp:555-557` setzt `inventory_.hp=-1` und löscht
  Flag 69, fordert aber **keinen** Effekt 26 an. Die Kette 26→26 innerhalb der Prozedur
  (effsub1.c:5676) ist nativ gebunden (`effect_sprite_procedures.cpp:4308`). Betroffen:
  RM_3140 (0xcb Modus 0 in Ereignis 2, *walk_cb*); Prozedur selbst ist registriert
  (`{26,step_eff026}`).
- **Raum-Texturhalter 30..83**: Original führt `bhEffDmy` aus (`effsub1.c:22 op->flg = 0`),
  der Platz wird nach dem ersten Pass frei. Nativ erhalten diese Sätze keine Prozedur
  (`supported_effect_sprite` false, `world.cpp:3575`) und werden nicht freigegeben
  (`world.cpp:3611` blendet nur `dynamic`/`player_effect` aus). Folge: dauerhaft belegte
  Pool-Plätze je Halter (z. B. RM_3010/3011 mit 70..79, 400..407: >15 Plätze). Sichtbare
  Wirkung nicht belegt (eff[512] im Scan nie voll); als Abweichung zum Original offen.
- **0x92 bhEffTypeSet auf Halter-/unportierte Kennungen**: `world.cpp:811` gibt bei
  `!supported_effect_sprite` `false` zurück (Befehl bleibt ungebunden), während 0x91 (Zeile
  810) unbedingt schreibt. Skriptstellen: 41 (RM_1020/1021/3120/3130/3140/5001/5010/6050/
  7130/8010, 14×), 34 RM_7100, 35 RM_3020, 52 RM_6000, 54 RM_9370, 56 RM_4030/8000,
  59 RM_1090, 70 RM_1120, 71 RM_3010/3011, 80 RM_9370, 94 RM_1000, 95 RM_7240, 409 RM_9000,
  441 RM_8001. Originalverhalten von bhEffTypeSet in diesem Audit nicht nachgelesen
  (Funktion unter diesem Namen in event.c nicht gefunden) → offen, nicht als Fehler gewertet.
- **0x98 bhEffectSandSet** über Index auf Kennung 66 (Halter) in RM_3091 (11×): native
  Behandlung `world.cpp:932` nicht gegen den Halter geprüft → offen.
- Keine weiteren: alle anderen Original-Code-Anforderer haben einen nativen Gegenpart
  (z. B. 13/130 über die WpnEffTab-Kopie `combat.cpp:105`, 258 `en03.c:677`/`en23.c:339`,
  260 `en06.c:1134`, 365/366 `world_enemies.cpp:2864/2887`, effsub0-Kinder 151..195 über
  `setentry`/`plain_request` in `effect_sprite_procedures.cpp:231-648/1414`).

### 2c. Nativ ohne jeden Erzeuger (weder Original-Code noch Raum noch Skript)

104, 118, 121, 129, 144, 151 (nur Kind), 156, 172, 174, 175, 183, 185, 186, 194, 197,
198, 199, 200, 205, 208, 228, 251, 255, 266, 268, 271, 272, 299, 306, 357, 360, 361,
368, 371, 373 — Erzeuger laufen über `op->func`, Daten oder Kindketten, die diese
Textsuche nicht erfasst; keine Aussage „unerreichbar“, nur „kein direkter Treffer“.

## 3. Laufzeitlücken aus dem Scan (600 Takte, Spawn 0, Gegner-KI an)

*scan_gaps_by_pattern.tsv*, 7 Muster gesamt, davon effektbezogen:

| Muster | Räume | Erzeuger |
|---|---|---|
| `en02.c:2452 ang uninitialisiert (bhEne02_SetSandEffectMain Fall 8)` | RM_5830, RM_7230, RM_7231 | EN02 Sandstaub 253/254 (benanntes Original-UB; 253/254 dort trotzdem sichtbar) |
| `bhDrawThlEffect3D: ef_tn[tex_id] ohne gezeichnetes Bild (Inhalt vor dem ersten Rendercut undefiniert)` | RM_7030 | bhEff111 Monitor über 0xaa (RM_7030 `script_effect_requests.tsv`) |

Nicht effektbezogen: bhObj005 Nachbarzelle (12 Räume), njSetBackColor Alpha (5),
bhCheckRoute (2), bhSetMotion (4). **Keine einzige `bhEffNNN`-Meldung** im Scan: kein
unportierter Effekt wurde im eingabefreien Lauf angefordert. `pending_commands` nur 0x23
(RM_3100/3140/7030). `audio_unavailable` nur Gegner-SE Bank 3 (RM_2011/7071/9302),
bhEasySESet (RM_7160), Stimme (RM_7250) — keine Effekt-SE.

Effektzustand (`effects[]`, nur portierte Kennungen): sichtbar u. a. 122 (52 Räume),
218 (16), 230 (15), 182 (13), 8 (12); allokiert aber unsichtbar u. a. 125 (46 Räume,
Steuerwerk ohne eigene Fläche), 123 (40), 0 (33), 181 (30), 122 (29), 17 (12). Unsichtbar
heißt hier „kein Zeicheneintrag im Endbild“, kein Fehlerbeleg (vgl. Memory „Scan sieht nur
das Endbild“).

## 4. Widersprüche Ledger ↔ Code

1. **bhEffBG**: 7 Sprungplätze, 9 Räume, Skriptbezug — weder Ledgerzeile noch native
   Prozedur; `port_check` kann die Lücke nicht zählen (einzige Lücke ohne Statuszeile).
2. **38 native Kennungen ohne Ledgerzeile** (Liste oben) — Abdeckung wird unterschätzt;
   darunter zentrale Raumeffekte 122/123/125/181/218/230 (zusammen 1 500+ Skriptstellen).
3. **Ledger `bhEffDmy` partial** („30..84, 86..89, 106 ungeprüft“) gegen `world.cpp:3611`:
   die Halter werden nativ nie freigegeben (Original: flg 0 nach erstem Pass). Der
   Ledgertext nennt das nicht.
4. **`effsub1.c:bhEff106` done**, obwohl 106 im Sprungtisch `bhEffDmy` ist; 106 läuft über
   `op->func` (effsub1.c:5942). Kein Fehler, aber die Zeile belegt keinen Tabellenplatz.
5. **bhEff026/bhCheckEvtTimer**: keine Ledgerzeile für player.c:bhCheckEvtTimer mit
   Effektbezug; `events.cpp:550-558` kommentiert die Gating-Flags, nicht den fehlenden
   bhSetEffect(26).
6. Ledger-`done`-Einträge, die im Scan eine Gap melden: **keine**. Ledger-`partial`
   mit „Build/Gegenprobe zentral ausstehend“ (362, 370, 371/372/373): im Scan tauchen
   350/351/352/355 sichtbar auf, 362/370 nicht (kein Kopfplatzen ohne Eingabe) → Status
   bleibt unbestätigt, kein Widerspruch.
7. `effsub0.c:bhEff199` partial „kein Erzeuger“ deckt sich mit dieser Suche (keine
   Anforderung in prog/*.c, Raum, Skript).

## 5. Priorisierte Portierungslücken mit Spielwirkung

1. **bhEffBG 93..97 fehlt** — RM_1000/1001/1002, RM_9090, RM_3240, RM_5001, RM_7240:
   opake Hintergrund-Sprites, per 0x43 ein-/ausgeschaltet; Atlas nicht dekodiert.
2. **bhEff026 beim Ablauf des Ereignis-Timers (Gas) unangebunden** — RM_3140: Spieler
   stirbt (hp -1) ohne den Originaleffekt 26.
3. **bhEff001/bhSetShadow partial** (Zeichnen F05: Renderer-Verbrauch/Fog/Receiver-Tiefe
   offen, Ledger) — alle Gegnerräume mit bhSetShadow.
4. **bhEff_Sub365 ohne Fläche** (njPtclDrawSprite n=1, Ledger) — Partikel aus
   bhEff_SetPtcl (zonzon.c Nikuhen 362, en01) in allen Zombieräumen beim Kopfplatzen.
5. **bhEff111/110 Monitor**: PS2_Render_Tex_Sub-Kopie/Stufe 7 offen (Ledger), Scan-Gap
   erstes Bild RM_7030 — RM_1050, RM_7030, RM_4030, RM_8050.
6. **bhEff_E16_LaserSight**: bhSetGunSplash liest nie beschriebene GA_WORK (benannte Lücke,
   Ledger) — RM_1020, RM_3010 (270 im Scan allokiert).
7. **bhEff130 type 4/6 ohne Raumlauf** (Ledger) — Waffen 18/20, alle Kampfräume.
8. **bhEff134 Spiegel-Zweitlauf / bhDraw138 OT-Einsortierung / bhEff136-bhDraw136
   Nachbild** partial — RM_4020/8020, RM_8001, Nachbildräume.
9. **bhEff_E06_Rinpun Spielerberührung** (Original uninitialisiert, Ledger) — Mottenräume
   (EN06), im Scan nicht erreicht.
10. **Raum-Texturhalter 30..83 nie freigegeben** und **0x92 auf Halterkennungen
    abgewiesen** (`world.cpp:811`) — RM_3010/3011, RM_9370, RM_1020/1021/3120/3130/3140/
    5001/5010/6050/7130/8010 u. a.; Originalverhalten von bhEffTypeSet noch nachzulesen.
Nachrang: bhEff_Sub360/361/368 ohne Erzeuger (Ledger), bhEff_Sub367 missing ohne
Erzeuger, bhEff199 Ps2_ptcl_spr_flag, bhEff30cDrw Ninja-Modus 1, bhDrawThlEffect3D
verlinkte Sätze/flg 0x2000000.

## 6. Grenzen des Audits

- Statische Textsuche nach Anforderern (Regex auf `id = N`, `bhSetEffect*(N`, `setentry(N`,
  WpnEffTab); Erzeuger über `op->func`, Datenfelder, berechnete Kennungen oder
  Kindketten werden nicht erfasst (Abschnitt 2c).
- Scan ist eingabefrei, 600 Takte, Spawn 0: Ereignisse mit Spielereingabe, Kampf,
  Kopfplatzen, Gegner-Sterbeeffekte und Türfolgen sind nur statisch (Walker, Raumtabellen)
  erfasst; Zeichenabbrüche vor dem Endbild bleiben unsichtbar.
- Walker-Artefakte in RM_0120/RM_5520/RM_1030 (Abschnitt 1); 0x43/0x91/0x92-Ziele
  wurden über den Raumtabellenindex aufgelöst, nicht über die Laufzeit-`efid`.
- Ledgertexte wurden gelesen, aber keine Prozedur semantisch gegen das Original geprüft;
  „done“ bedeutet hier nur: Ledger sagt done und der Scan widerspricht nicht.
- bhEffTypeSet-Original (0x92) und bhEffectSandSet (0x98) auf Halterkennungen nicht
  nachgelesen; bleiben als Prüfpunkte offen.
- Kein Build, kein Test; das geprüfte Binary ist der Stand vom 2026-10-22 00:01.
