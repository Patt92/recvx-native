# Effekt-Skriptbefehle: Blutlache und geteilter eff[512]-Satz

Stand 2026-10-13/14. Dev-App `build/start/macos-arm64` ersetzt und signiert, Hash `70cf7f60…`
(Backup des Punkt-1-Stands: `archived/releases/blend-state-before-20261013-233239`,
`a55478ed…`). Kein Commit, kein Paket.

**Hinweis zur Parallelitaet:** ein Dev-Build um 23:57 enthielt den halbfertigen Stand der
parallelen Box-Sitzung (`box_menu.*`, `runtime.cpp`, `scene.cpp`, alle 23:55/23:56) und liess
`verify_progression` rot werden. Gegenprobe: derselbe Baum mit und ohne meine Befehle besteht
die Suite, sobald die Nachbardateien ihren naechsten Stand haben — die Ursache lag nicht an
dieser Aenderung. Der Dev-Build nimmt immer den gesamten Arbeitsbaum mit.

## Ausgangsmessung

`tools/scan_event_rooms.py` gegen die neue Dev-Runtime, 205/205 Raeume, 600 Ticks,
kein Input. Die Tabelle ist gemessen, nicht geschaetzt:

- **Gar nicht implementiert** (Skriptabbruch ueber den `default:`-Zweig der VM):
  `0x2c bhEffectSet`, `0x4e bhEffBloodSet`, `0x56 bhEffBloodPoolSet`, `0x70 bhEffPush`,
  `0x71 bhEffPop`, `0x96/0x97/0x98 bhEffectSensya/Kokuen/SandSet`, `0xaa bhEffUVSet2`,
  `0xb0 bhHEffectSet2`, `0xc3 bhEffectSandSetMatsumoto`, `0xc8 bhEffBakuDrmSet`.
- **Laenge bekannt, Bindung fehlt:** `0x58 bhEffBloodPoolSet2` (eine Bindung offen in
  RM_3140).

Damit ist die im Bericht `work/native_port_blend_state.result.md` genannte Annahme
widerlegt, die sichtbaren **Fraeß-/Laser-Effekte in RM_0090** kaemen aus dem Effektsystem:
im Scanner-Replay (`verify_scanner`) laeuft **kein** Effekt-Skriptbefehl — der Trace
(`RECVX_EVENT_TRACE=1`) kennt dort nur 0x69/0x65/0x43/0x24/0x25 usw. Was dort feuert,
gehoert zum Terminaltask und zu den Raumobjekten, nicht zu `sys->ef_*`.

Ebenfalls gemessen: der **Blut-Pool ist mechanisch schon portiert** —
`bhSetBloodPoolLnk` (effsub3.c:145) und `bhEff300` (effsub3.c:234) liegen vollstaendig
in `native/effect_particles.cpp` (Pool-Klasse, `spawn_pool`, `step_pool`, `append_pools`),
und `world_enemies.cpp:1001` bindet den Gegnerdienst darauf. Offen war allein die
**Skriptoberflaeche**.

## Was umgesetzt wurde

`native/events.cpp`:

- `0x4e` neu, Laenge 14 (aus dem `bhScePtr`-Vorschub von event.c:4302).
- `0x56` neu, Laenge 6 (aus dem `bhScePtr`-Vorschub von event.c:4384).
- `0x58` Laenge 14 war schon richtig, der Kommentar „effect spawn pending“ entfaellt.

`native/world.cpp` (in `RoomWorld::bind`):

- `0x2c`: `bhSetEffect(v1, &{0,0,0}, NULL, 0)` — erster freier Platz des geteilten
  `eff[512]`-Satzes (`RoomWorld::allocate_effect`, dieselbe Zuweisung wie `spawn_effect`).
  Der Befehl legt den Arbeitssatz **auch bei noch nicht portierter Prozedur** an und meldet
  sie namentlich (`unbound_enemy_` → `bhEff<id>`), statt sie zu verdecken; vorher brach der
  `default:`-Zweig der VM die ganze Eventaufgabe ab.
- `0x70/0x71`: `bhPushEffectWork`/`bhPopEffectWork` sichern bzw. ersetzen den **ganzen**
  Satz. Der Host sichert `sprites_` samt Partikelnutzlast (`EffectParticles::save/restore`,
  `native/effect_particles.cpp`) in genau einen Schnappschuss — wie das Original in genau
  einen Speicherblock (`sys->ef_psp`); ohne vorangehendes Push tut Pop nichts.
- `0x4e`: `v0` = Raumindex, `v1` zugleich `epw->djnt_no` und der Gelenkpunkt, `v2` die
  Vorzeichenbits des Trefferversatzes, danach Typ, Flags (Bit 1 negiert den Winkel) und
  Gradwinkel. Die Ausfuehrung liegt in `RoomWorld::script_blood_set`
  (`native/world_enemies.cpp`), das `epw->djnt_no` schreibt und den schon portierten Kern
  `bhEne_SetBlood2` (`recvx_t3_set_blood2`, zonzon.c:466) aufruft. Fuer genau diesen Aufruf
  werden Weltzeiger und T3-Ergaenzung gebunden (`EnemyWorldScope`/`EnemyExtScope`, wie in
  `control_enemy_works`); ohne die Bindung meldet die T3-Bibliothek den Dienst als Luecke
  (gemessen: `[zonzon.c:493 effect.c bhSetEffectTb]`).
- `0x56`: `v0` = Raumindex in `rom->enep`, `v1` zugleich `epw->djnt_no` und die
  Tabellenauswahl (0/1 `evnt_BldTbl2`, 2 `evnt_BldTbl`), `v2` die Palettenbank. Richtung
  `ay + mlwP->objP[1].ang[1]`, im Fall 0 zusaetzlich `+32768`; Ursprung `owP[0]`, danach
  `x -= njSin`, `z -= njCos`. Fehlt der native Gegnerwork oder der Objektbaum, bleibt der
  Befehl offen (Rueckgabe `false`), statt einen Ersatzort zu erfinden.
- `0x58`: eigene Lage in Zentimetern mit den Vorzeichenbits 0/1/2 von `v0`, dann `v1`
  (Tabellenauswahl **und** Winkelvorzeichen in Bit 1), `v2` als Gradwinkel, `v3` als
  Palettenbank; Fall 0 `+32768`, Fall 2 nimmt `evnt_BldTbl`.
- Beide Tabellen `evnt_BldTbl`/`evnt_BldTbl2` (event.c:43/51) stehen feldgleich als
  `RECVX_ENEMY_BP_WORK` — derselbe Typ, den `EffectParticles::spawn_pool` und
  `world_enemies.cpp` schon benutzen (BP_WORK, types.h:2210).

## Nachweis

- `tests/native/gameplay_bindings_test.cpp`: neuer Block auf dem freigegebenen
  RM_0080-Zombie (Work gebunden und gerechnet). `0x56` liefert `true`, erhoeht
  `blood_particles()` um genau eins und schreibt `epw->djnt_no`; `0x58` erhoeht um ein
  weiteres; `0x4e` startet den Originaleffekt 5 am Work und lehnt einen fehlenden Gegnerwork
  ab (`false`). Lauf: PASS.
- Gemessene Gegenprobe an echten Daten: `RM_3140` hatte mit dem Vorher-Binary
  `pending_world_bindings` 3 **mit** Opcode 0x58, mit dem neuen Stand 2 ohne.
- CTest `build/alphadomain-claude-20261013`: **80/82**, rot nur `script_motion`
  (fremde Sitzung, vorbestehend) und `original_map_render` (parallele Kartenarbeit;
  `verify_map` scheitert mit dem Vorher-Binary identisch).
- Dev-App gegen die installierte Runtime: lighter, detector, events, scanner, combat,
  progression, doors, fog, continuity, snapshot, stairs, pickups — alle PASS;
  arm64-only, `codesign --verify --deep --strict` gueltig.

## Offen (naechste Schritte, in dieser Reihenfolge sinnvoll)

1. `0x2c bhEffectSet` und die uebrigen fehlenden Effektbefehle: sie starten Arbeiten ueber
   `bhSetEffect`, deren **Prozeduren** erst mit dem Effektgenerator kommen; ohne die waere
   die Bindung nur ein Platzhalter.
3. Punkt 3 des Plans (Nicht-CNK-Primitiven `njDrawTexture3DEx`/`njDrawPolygon3D`/
   `njDrawLine3D`): gemessen ist die Vertexfarbenhalbierung `(c+1)>>1` (0..128-Domaene) mit
   der Alpha-Domaene aus Punkt 1 deckungsgleich; die echten Unterschiede sind das
   `trans`-Flag als ABE, der feste Nebelwert 128 samt `Ps2SetFogColorSys` im
   `njDrawTexture3DHEx`-Pfad und der `_Clip_Screen`-Abbruch.

## Gemessene Effektprioritaet (205 Raeume, 3171 Plaetze, 170 IDs)

Aus den RDT-Raumeffekttabellen dekodiert (Header +0x14 Zeiger, +0x94 Anzahl, 68-Byte-Satz, ID bei +4).
Bereits portiert: 0, 2, 5, 9, 17, 122, 123, 125, 126, 181, 182, 218, 230, 300, 302.

| ID | Plaetze | Raeume |
|---|---|---|
| 41 | 56 | 55 |
| 35 | 48 | 46 |
| 92 | 43 | 42 |
| 414 | 20 | 20 |
| 8 | 63 | 15 |
| 401 | 15 | 15 |
| 70 | 15 | 15 |
| 400 | 14 | 14 |
| 404 | 13 | 13 |
| 71 | 13 | 13 |
| 102 | 12 | 12 |
| 33 | 12 | 12 |
| 34 | 11 | 11 |
| 77 | 10 | 10 |
| 75 | 10 | 10 |
| 54 | 10 | 10 |
| 407 | 9 | 9 |
| 78 | 9 | 9 |
| 72 | 9 | 9 |
| 62 | 9 | 9 |
| 43 | 9 | 9 |
| 206 | 65 | 8 |
| 165 | 40 | 8 |
| 164 | 22 | 8 |
| 402 | 8 | 8 |

Das ist die Arbeitsliste fuer die Effektprozeduren; ohne sie bleibt `0x2c` fuer diese IDs
eine gemeldete Luecke (der Arbeitssatz entsteht, gezeichnet wird nichts).
