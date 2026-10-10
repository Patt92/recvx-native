# 08 — Ergebnis: Echte Fledermäuse in der nativen Runtime

## Startkommandos

Automatisierter Regressionstest über den produktiven Pfad:

```
ctest --test-dir build/native -R en07_runtime
./build/native/recvx-en07-runtime-tests data
```

Reproduzierbarer Runtime-Lauf mit Bild:

```
./build/native/recvx-runtime --data data --room RM_0130.RDX --in-game \
  --headless --spawn 1 --replay tests/native/replays/rm0130-bats.txt \
  --frames 130 --screenshot bats.bmp --state bats.json
```

Raum: `RM_0130` (Aufgabe 01). Keine künstliche Spawn-Injektion, kein direkter
Enemy-Dispatch, kein Flagtest.

## Die sechs Pflichtnachweise

### 1. Entstehung aus der originalen RDT-/Ladekette

Die fünf `ETTY_WORK`-Einträge (`0x1a2c`…`0x1abc`, alle `flg 1`, `id 7`,
`type 0`) werden beim normalen Raumladen durch das originale `bhSetEnemy`
übernommen. Vor dem ersten `bhControlEnemy`-Pass existiert **keine** Pose — die
originale Reihenfolge Laden → Steuern → Zeichnen; der Test prüft das.
`bhEne07_Init` liefert danach die Originalwerte:

| Prüfung | Wert | Herkunft |
| --- | --- | --- |
| `hp` | 1 | `ENE07_HITPOINT[16]`, alle Einträge 1 |
| `mtn_no` | 3 | `bhEne07_Init`, Zweig `type != 2` |
| `flr_no` | 6 | `bhSetFloorNum` aus `grand[8] == 26`, nicht der RDT-Rohwert 3 |
| `y` | 27,0 | Deckentreffer aus `bhCollisionCheckLine` (RDT: 26,0) |
| Knoten | 14 | echtes EN07-Skelett |

### 2. Sichtbares vollständiges Modell aus echten Ressourcen

Über `RoomWorld::append` → `PlayerAsset::sample_work` mit den originalen
`O_WORK`-Matrizen. Gegenprobe mit `bhEneSetCk` (`stflg |= 0x1000000`), das genau
diese Works entfernt:

```
cut 1 renders 2898 triangles of which 2490 are the bats
```

Bild: `build/validation/en07-runtime-20261008/bats.png` — vier der fünf
Fledermäuse hängen texturiert an der Gewölbedecke.

### 3. Fortschreitende Originalpose

Nach 30 produktiven `tick()` haben sich die Gelenkmatrizen gegenüber dem ersten
Pass verändert, alle Werte bleiben endlich, und die Motionuhr steht nicht mehr
auf 0. Clipwrap und Übergangsinterpolation sind in Aufgabe 03 gegen die
Originalrückgaben geprüft.

### 4. Originale KI auf echte Raum-/Spielerbedingung

`bhEne07_BR00` löst die hängenden Fledermäuse, wenn das Feuerzeug
(`plp->wpnr_no == 1`) näher als 15 Einheiten kommt und `flg 0x400000` gesetzt
ist. Der Test stellt die Spielerin unter die erste Fledermaus und setzt die
Waffe; es folgt die originale Kette:

`MV00` (Ruhe, `mtn_no 3`) → `MV01` Abflug (`mtn_no 1`, SE `0x12300`) →
`MV02` Flug (`mtn_no 0`), Work verlässt die Deckenhöhe 27,0.

Der Test fordert beide Motionnummern und die Höhenänderung.

### 5. Originaler SFX über den nativen Audiopfad

`bhEne07_MV00` ruft `bhEne_CallSE(epw, &epw->px, 0x2303)`. Kette:
`bhEne_CallSE` → `RequestEnemySe` → `RequestEnemySeBasic` →
`core_enemy_services` → `AudioMixer::request_enemy_se` →
`ExecEnemySeManager`/`exec_enemy_se` → echtes PCM aus der Raum-Soundbank.

Bank `(0x2303>>8)&0xF = 3`, Liste `0x2303 & 255 = 3`. Im Mixer protokolliert als
`MULTSPQ1/0017/3/0003`. Der Test fordert genau diesen Eintrag und verlangt, dass
`audio.unavailable` leer bleibt.

Im Runtime-Lauf mischen sich Schritt-SE (`MULTSPQ1/0017/2/0000`) und
Fledermaus-Squeaks; dass nur zwei bis drei der fünf gleichzeitig zu hören sind,
ist die originale `CheckPlaySameSe`-Grenze `(se>>12)&0xF = 2`.

### 5b. Waffentreffer und Blut (Nachtrag 2026-10-09)

Der Abnahmetest schießt jetzt auch auf eine Fledermaus:

- alle fünf Works stehen als `combat_targets` mit ihrer **eigenen** originalen
  Körpergröße (`ar`/`ah`) und `hp` 1;
- `RoomWorld::damage` schreibt die Felder, die `weapon.c bhCheckEnemies`
  schreibt; `bhEne07_Damage` verbraucht sie im nächsten Pass, die Fledermaus
  stirbt, und die gezeichneten Dreiecke fallen von 2490 auf 1992 (genau eine
  von fünf weg);
- der Treffer erreicht über `bhEne_SetBloodEffect4` den originalen
  Partikel-Emitter; dessen Zeichenausgabe (`bhEff_PtclSpriteDrawB`) geht jetzt
  in die Szene, deshalb meldet der Lauf **keine Lücke** mehr.

### 6. Erreichte Kollisions-/Kampfpfade sind echt gebunden

Siehe `work/04_collision_floor_wall.result.md`: Deckensuche, Wandrückschiebung
und Etagenberechnung stammen aus `hitchkl.c`, `hitchk.c`, `en03.c` und
`eneset.c`. `RoomWorld::unbound_enemy_routines()` ist am Ende des Abnahmelaufs
**leer** — keine protokollierte Lücke lässt den Test bestehen.

## Testlauf

```
48/48 Tests bestanden (ctest, build/native, macOS arm64)
```

einschließlich `en07_runtime`, `core_actor_assets`, `core_enemy_arena`,
`core_enemy_interaction`, `core_model_matrices`, `native-gameplay-bindings`,
`event_lifecycle_control_release`, `enemy_damage_original_en01`.

Headless-Lauf RM_0130, 400 Frames: `0 pending world bindings, 0 blocked commands`.

## Nicht erreichte EN07-Zweige

Diese Routinen sind unverändert gelinkt, aber von diesem Ablauf **nicht**
angelaufen und damit nicht durch einen Lauf belegt:

- `bhEne07_BR01` (Typ-1-Brain), `MV03`…`MV08`;
- `NG00`/`NG01` (Griff), `bhEne07_PlayerControl`, `bhEne07_PlayerBloodEffect`;
- `bhEne07_Damage`, `DD00`…`DD06` (Treffer und Tod);
- `bhEne07_CheckHangingWall`, `bhEne07_AvoidWall`, `bhEne07_ObjEdge`.

Ebenfalls offen: der von `bhEne07_Init` angelegte Schattenwork wird nicht
gezeichnet, `StartVibrationEx` und die Partikelzeichenpfade aus `effsub3.c` sind
gemeldete Lücken, und die übrigen Gegner-IDs laufen weiter über ihren
bisherigen nativen Besitzer. Kein Durchspielnachweis.
