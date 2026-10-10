# 07 — Ergebnis: EN07 und die übrigen Gegner im produktiven Dispatcher

## Nachtrag 2026-10-09: alle Gegner über `bhControlEnemy`

`core_enemy_world_control` ruft nicht mehr eine eigene Schleife für ID 7, sondern
**`bhControlEnemy()` selbst** (eneset.c:344) mit seiner `bhJumpEnemy[id]`-Tabelle,
`bhControlMask` für `id > 90` und der `sys->en_obj`-Sammlung. Nichts an der
Reihenfolge oder den Gates ist nachgebaut.

`RoomWorld::init_core_enemies` übergibt jede Identität außer:

| Nicht übergeben | Grund |
| --- | --- |
| 1 (Zombie), 4 (Hund) | laufen weiter über den bestehenden `en01_damage.c`-Port; ein Gegner darf nie zwei Besitzer haben |
| 91–99 (Subplayer) | laufen über `subpl.c` wie bisher |

`core_enemy_world_create` nimmt eine `dispatch[100]`-Maske: nur die übergebenen
Identitäten bekommen hier ihre Work, die übrigen bleiben beim alten Besitzer.

**Rückgaben statt Doppelbesitz.** Drei Fälle geben eine Work sichtbar zurück und
melden den Grund über `RoomWorld::unbound_enemy_routines()`:

1. Modellblock nicht nativ realisierbar (Basic- statt Chunk-Modell) — die Work
   wird über das originale `flg`-Bit 0 plus `bhCheckEneWorkNum` freigegeben.
2. Skelett der nativen Work und des Hostmodells unterschiedlich — der Renderer
   speist die originalen `O_WORK`-Matrizen in das Hostmodell, also muss es
   dasselbe Skelett sein.
3. Ein Raumskript weist der Work eine Motion zu (`bind` 0x50/0x19). Im Original
   schreibt das Skript dieselbe Work; der Host-Clip-Pfad und die native Work
   dürfen den Actor nicht gleichzeitig besitzen, also verlässt die Work
   `bhControlEnemy` (`core_enemy_world_release`).

**Gemessener Stand über alle 205 Räume** (120 Ticks je Raum, `--in-game`):
183 Räume laden — identisch zum Stand vor dieser Arbeit, dieselben 22 Ausfälle
(vorbestehende `nonfinite asset coordinate`, `unterminated lip stream`,
`vector`-Fälle). Gemeldete Gegner-Lücken insgesamt: 5 × Basic-Modell,
7 × Skript-Motion-Rückgabe (IDs 5, 11, 19 ×2, 20, 21), 9 × `bhEff017`,
2 × `bhEne01_LinkFireEffect`, 1 × `pdGetPeripheral`, 1 × `StartVibrationEx`.
Kein einziger Skelett-Mismatch.

Stichprobe, dass die originale KI wirklich läuft (Position Tick 2 → Tick 200):
ID 9 in RM_1060 (2 von 2 Works bewegt), ID 11 in RM_7081 (2/2), ID 19 in
RM_1122 (1/1). IDs 43 und 67 stehen still — das sind `id > 40`-Works, die
`bhSetEnemy` mit `mdflg 0x20` anlegt.

**Neue Plattformgrenzen** (`native/core_enemy_platform.c`), weil der Dispatcher
jetzt mehr Originalcode erreicht. Nativ portiert, weil es Algorithmen sind:
`njRotTransPers` und `njRotTrans` (`ps2_NaMatrix.c:1811/1833`). Gemeldete, nicht
ersetzte Transportgrenzen: `Ps2AddPrim3D`, `Ps2SetFogColor`, `Send_Tim2_dataEx`,
`njDrawPolygon3D`, `njDrawPolygon3DEx`, `njDrawLine3DEx`, die vier `fm*`-Gesichts-
routinen und `pdGetPeripheral`. Sie melden sich über `core_enemy_gap_report`.

Zusätzlich behoben: `core_actor_model_realize` kopiert den Modellblock jetzt in
eigenen Speicher. `npSkinConvert` (room.c:597) schreibt die Vertexliste um — im
Original in seiner eigenen RDT-Kopie, im Port vorher in die unveränderlichen
Raumbytes des Aufrufers. Dasselbe gilt für SKIN- und MASK-Blöcke.

Und: `core_enemy_world_activate` zeigte die gemeinsamen Globals `sys`/`plp`/`rom`
dauerhaft auf die Gegnerwelt. Die Status-/Inventar-/Message-Schicht ist aus
denselben Originalquellen erzeugt und liest dieselben Symbole. Jeder
Einstiegspunkt sichert und restauriert sie jetzt (`core_enter`/`core_leave`).

## Was jetzt läuft

`recvx_game` und `recvx-runtime` linken `recvx_core_enemy_world` und darüber die
nativ kompilierte Originallogik (`recvx_original_engine`). EN07 wird **nicht**
nachgebaut: `bhEne07` samt `bhEne07_Init`, `Brain`, `MV00..MV08`, `NG00/NG01`,
`Damage`, `DD00..DD06` kommt unverändert aus `en07.c`.

## Eine Workquelle

| Zustand | Besitzer |
| --- | --- |
| `sys`, `rom`, `plp` | `CORE_ENEMY_WORLD` des Raums, über `core_enemy_world_activate` |
| `ene[128]` | dieselbe Struktur, über `core_enemy_table` |
| `eff[512]` | dieselbe Struktur, über `core_effect_table` |
| Work-Speicher | originaler `bhEne_CallocWork` auf der `core_enemy_arena` |
| Modelle/Motions | `core_actor_assets` aus den echten RDT-Blöcken |

Es gibt keine zweite `BH_PWORK`-, Player- oder System-Instanz, keine festen
`exp0`/`exp1`-Blöcke und keine Null-Stubs auf dem erreichten Pfad. Das
gruppengeteilte `exp1` entsteht durch die originale Suche in `bhEne07_Init`
(`eneset.c`-Arena, erste aktive ID-7-Work); der Komponententest prüft, dass alle
fünf Works denselben Zeiger führen.

## Verdrahtung in `RoomWorld`

| Original | Nativ |
| --- | --- |
| `room.c:47` `bhSetRoom` → `bhSetEneMdl` → `bhSetEnemy` | `RoomWorld::init_core_enemies` am Ende des Konstruktors |
| `eneset.c:344` `bhControlEnemy` (`sp_flg & 2`) | `RoomWorld::tick_core_enemies` → `core_enemy_world_control` |
| `bhControlEffect` für die Partikelworks | `core_enemy_world_effects` (`bhEff302`) |
| `sdfunc.c:3024` `ExecSoundSystemMonitor` | `audio_.exec_enemy_se()` nach allen Gegnerpässen |
| `event.c:2715` `bhEneSetCk` | `RoomWorld::deactivate_enemy` → `core_enemy_world_deactivate` |
| `eneset.c:807` `bhDrawEnemy` | `core_enemy_world_visible` + `core_enemy_world_clipped` in `append` |

Die Identitäten, die der Dispatcher noch nicht führt, behalten ihren
bestehenden nativen Besitzer. Das ist eine schrittweise Übernahme und
ausdrücklich keine Behauptung, alle Gegner seien portiert.

## Spielerwork

Der EN07-Pfad liest und **schreibt** `plp`. `core_enemy_world_set_player`
überträgt die hostseitigen Felder (Position, Winkel, Etage, HP, Waffe,
`at_flg`, Pad, Charakter, Kollisionsbit `flg 0x2`) und die Lärmbits
`stflg 0x100/0x200`, die `player.c:1390` jeden Pass wieder löscht; `0x200`
setzt `RoomWorld::player_footstep` wie `player.c:1435`. Solange eine
Gegnerroutine den Spieler hält (`mode0 4/6`), bleiben Position, Winkel und
Modus beim Work — genau dort unterdrückt das Original auch die eigene
Spielersteuerung. `core_enemy_world_player_state` gibt zurück, was
`bhEne07_PlayerControl` geschrieben hat; `RoomWorld` setzt daraus
`player_scripted`, die Motion aus der **Gegner**-MTN-Bibliothek und den
HP-Verlust über denselben `blast_damage_`-Weg wie der en01-Griff.

## Plattformblätter

`native/core_enemy_services.c` bindet die erreichten Dienste mit vollständigen
Originalargumenten:

| Original | Bindung |
| --- | --- |
| `sdfunc.c:1708` `RequestEnemySeBasic` | `AudioMixer::request_enemy_se` |
| `sdfunc.c:1747/1753` `RequestEnemySe`/`Ex` | originale Einzeiler darüber |
| `sdfunc.c:1759` `ChechPlayEnemySe` | `AudioMixer::enemy_se_playing` |
| `CallPlayerVoice` | `AudioMixer::request_enemy_se(127, …)` |
| `StartVibrationEx` | gemeldete Lücke |
| `effsub3.c:1386/1432` Partikelzeichnen | gemeldete Lücke |

Gemeldete Lücken landen in `RoomWorld::unbound_enemy_routines()` und werden nie
still verschluckt. Im Abnahmelauf ist diese Liste **leer**.

## Linkeigenschaften

`eneset.c`'s `bhJumpEnemy[]` nennt jeden Gegner, deshalb lädt der Linker das
ganze Archiv; `-Wl,-dead_strip` (bzw. `--gc-sections`) entfernt die
unerreichten Routinen. Die beiden unportierten Plattformeinsprungpunkte
`Ps2AddPrim3D` (`ps2_NinjaPtcl.c`) und `Ps2SetFogColor` (`ps2_NaFog.c`) bleiben
**absichtlich undefiniert**: Würde ein erreichter Pfad sie anlaufen, bricht der
Link, statt still Ersatzlogik zu fahren. Im Runtime-Binary sind sie nicht
vorhanden.

Drei Symbolkollisionen zwischen dem historischen Treppenauszug und dem
vollständigen `player.c` (`PlFootSnd`, `PlKDU`, `PlMtnAct`) sind an der Wurzel
behoben: `tools/generate_player_stairs.py` gibt den wörtlichen Tabellen jetzt
interne Bindung, wie es die ebenfalls ausgezogenen `bhCPM2_act_*` schon hatten.

## Archiviert

- `archived/en07-bat-draft-20261008` — der abgelehnte Mock-Entwurf.
- `archived/core32-20261008` — der ILP32-/wasm2c-Originalkern.
