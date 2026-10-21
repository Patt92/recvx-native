# Gegner: verknuepfter Oberkoerper (T-Pose) und Rodrigo in der Zelle — 2026-10-12

Umfang: nativer Gegnerpfad (`native/world_enemies.cpp`, `native/world.cpp/.h`,
`native/actor.cpp/.h`) und ein neuer Abschnitt in `tests/native/enemy_work_test.cpp`.
Dazu die von einer Parallelsitzung portierte Kindfamilie `native/enemies/en01_sub.c`
(en01sub.c) samt `tests/native/en01_sub_test.c`. Kein Originalkoerper, kein
Generator, keine Raummregel. Kein Commit/Push.
Pflichtpruefer vor jedem Build: `python3 tools/check_no_original_core.py` → PASS.

## 1. T-Pose der Arme — jetzt der Originalmechanismus

Der Zombie ist im Original **zwei** Works: der Hauptwork traegt den unteren
Koerper, ein verknuepfter Work ueber `bhEne01_SetLinkEnemy` (`en01.c:2815`) den
oberen. Beweiskette:

- `epp->mlwP = epw->mdl` (`en01.c:2827`) — beide zeigen auf **denselben**
  Objektbaum; `epp->mnwP = epw->mnwP` (`:2829`) — dieselbe Motionquelle.
- `bhEne01_SetMtn` (`en01.c:10347-10353`) und `bhEne01_CalcEnemy`
  (`:3145-3160`) verschieben fuer den Oberkoerperwork `objP += 8`, `owP += 8`,
  `obj_num -= 8`, fuehren `bhSetMotion`/`bhCalcModel` aus und nehmen die
  Verschiebung zurueck.
- Die Clips liegen als Paar vor (`en01_walk_mtn`, `en01.c:1118`): `mnwP[mtn]`
  8 Kanaele, `mnwP[mtn + 0xc8]` 10. Gemessen in RM_0020 (emtp der Identitaet 1,
  476 Eintraege): `0→8/200→10`, `4→8/204→10`, `40→8/240→10`, `41→8/241→10`.
- Kanalordnung: `mtn_tp = en01_flipTree` (`en01.c:2400`) mit 18 Eintraegen,
  `flipTree[8..17] == 8 + flipTree2[0..9]` (`:2407`).

**Hostseite (fertig):** `WorldEntity::model_owner` (geteilter Baum),
`model_window` (`objP/owP += 8`), `motion_library` (`mnwP` des Elternworks);
`create_enemy_child` legt Kindworks ueber `recvx_enemy_set` im ersten freien
`ene[]`-Platz an; der Kinddispatch (`0x1f..0x26`) wird ueber
`recvx_enemy_set_call_func` in die Tabelle eingetragen; alle baumlesenden
Dienste (`obj_num`, `set_motion`, `joint_matrix/-point`, `node_local*`,
`node_angle_*`, `model_node_hidden`, `model_root_position`, `weapon_atr`)
rechnen mit Fenster/Baumbesitzer; `PlayerAsset::tree_window` ist das
gefensterte `bhCalcTree` (`MdlPut.c:262`, Knoten ab `first`, nur deren owP).
Die frueher hier benutzte Clip-Zusammenfuehrung im nativen Pfad ist **entfernt**;
`enemy_mn_work` liefert wieder genau `mnwP[mtn]`.

**Beleg:** RM_0060 rendert jetzt einen gehenden Zombie mit artikulierten Armen
statt T-Pose; `enemy_work_test` prueft in RM_0020, dass die Oberkoerperknoten
`{8,9,10,11,15,16,17,12,13,14}` ihre Ruhelage verlassen — die erreicht der
Hauptwork allein nicht (sein Clip hat 8 Kanaele, `flipTree[0..7] ⊂ 0..7`), sie
kommen also aus dem Kindwork.

## 2. Rodrigo „nur noch Haende" — Ursache und Fix

Build-Bisektion mit demselben Original-Eroeffnungsreplay (RM_0000, Frame 2000):

| Build | Zeit | Befund |
|---|---|---|
| `push-claude-20261011` | 10-11 01:21 | Claire und Rodrigo vollstaendig |
| `sub-rodrigo` | 10-11 18:47 | vollstaendig |
| `events-claude-20261011` | 10-11 20:59 | vollstaendig |
| `eventgaps-claude-20261011` | 10-11 23:05 | **nur Handteile** |
| `macos-arm64` | 10-11 23:15 | **nur Handteile** |

Der Sprung liegt zwischen 20:59 und 23:05. In diesem Fenster wurde
Common_controll 8 (`event.c:12099`, `work->mdl->objP[model_cno].pos = …`) als
**dauerhafter** `node_overrides`-Eintrag gebunden. Das Original schreibt den
Knoten einmal; der naechste `bhSetMotion` desselben Works ueberschreibt ihn
wieder. Die Freigabe in `RoomWorld::tick` uebersprang aber native Works —
Rodrigo (Identitaet 98) und die Film-Claire (Identitaet 91) sind native
Subplayer-Works, ihre Wurzel blieb deshalb am Ursprung kleben, waehrend die
starren Handteile an ihrer alten Stelle gezeichnet wurden.

Fix: (a) die Freigabe gilt jetzt auch fuer skriptgefuehrte native Works — mit
`enemy_motion_available` als Wache, weil Szenenfiguren wie Identitaet 91 in
RM_9301 gar keinen emtp-Eintrag haben (dort laeuft kein `bhSetMotion`, also
darf auch nichts freigegeben werden); (b) ein **frei laufender** nativer Work
schreibt den Skriptzugriff direkt in sein eigenes `objP` (`enemy_obj`) statt in
einen Override, den der native Zeichenweg nie liest.

Beleg: dasselbe Replay rendert Frame 2000 wieder mit vollstaendiger Claire und
vollstaendigem Rodrigo (Bild `rod2/after.png` gegen `rod2/macos-arm64.png`).

## 3. Kindfamilie en01sub.c (Parallelsitzung) und ihre Anbindung

`native/enemies/en01_sub.c` portiert Parent/Arm/Leg/Head/Cap/Worm/Bom/Scope mit
Init/Move/Damage/DG*/MV*, alle Tabellen und die exp0-Blocklayouts
(`RECVX_EN01_SUB_EXP0`, `_MTX`). `tests/native/en01_sub_test.c` laeuft mit
170 Pruefungen fehlerfrei.

Angebunden in `RoomWorld::bind_enemy_services`:
`child_create`, `register_child_dispatch`, `child_model`, `shift_object_window`,
`node_local`, `node_local_clear_xz`, `node_angle_get/set`, `joint_matrix_set`,
`joint_flags_or`, `work_matrix`, `player_matrix`, `player_node_clear_xz` und der
Kontextsatz `stg_no`/`rom_no`/`stage_valid` aus `values[0]/[1]` (dieselben
Felder, die `bhSv` `event.c:813/816` und der Spielstand schreiben; Spielstand:
`native/snapshot.cpp:71`).

Ergebnis: `en01:child-create` und `bhEne_SetCallFunc` sind aus allen drei
Pruefraeumen verschwunden; `sys->stg_no / sys->rom_no (en01sub.c:177)` ebenfalls.

## Nachweis (arm64 Release, `build/enemy-ai-claude-20261011`)

- `ctest -j 1`: **65/66**. Rot bleibt allein `native-gameplay-bindings`
  („enemy gap changed the original HP", Hund/Identitaet 4). Ursache ist nicht
  diese Arbeit: `recvx_en04_init_work` wuerfelt `hp` bei jedem Init neu
  (60 → 90 zwischen den beiden Ticks des Tests); der Identitaets-4-Pfad wird
  hier nicht beruehrt. Im C05-Bericht vom 2026-10-11 bereits als vorbestehend
  vermerkt.
- 205-Raum-Scan (600 Ticks, ohne Eingaben, `jobs 4`): **204/205** mit Report,
  0 blockierte Befehle — identisch zum Parallel-Scan
  `build/validation/morph-20261012`. Der eine Ausfall ist RM_2010 (vorbestehend,
  C05-Bericht). Ein zwischenzeitlicher Zusatzausfall RM_9301
  („built-in enemy motion unavailable: identity 91") kam aus meiner
  Freigabe-Aenderung und ist mit `enemy_motion_available` behoben — der Baum ist
  wieder auf dem Ausgangsstand.
- Gegnerluecken RM_0020 (nach allen Anbindungen, 12): `bhCheckEnemies`,
  `bhCheckPlayer`, `bhCheckRouteID/bhCheckRoute`,
  `bhEne01_DmgCheck: exp0`, `bhEne_EnemyAtariCheck`,
  `bhSetMotion:objP/owP-Index ausserhalb des Modells`, `bhSetShadow`,
  `en01:Nachbar ohne exp0`, `enemy-control:player-offset-clear`,
  `mlwP->owP[joint].mtx`, `npSetAllMatColor`. Die Treffer- und
  Schattendienste sind der naechste Anschluss.

## 4. Wandlauf und Dansa-Tritt fuer Gegner

- `bhCheckDansa` (`hitchk.c:5391`) ist als Dienst `check_dansa` im gemeinsamen
  Vertrag und wird an allen drei EN01-Aufrufstellen benutzt (`en01.c:4273`,
  `:4309` und `zonzon.c:1222`). Der Host laeuft dieselbe Reihenfolge wie
  `bhCheckExmAtari`: `EventVm::interactions` (rom->etcp), danach die beweglichen
  Saetze von bhObj001 (`RoomWorld::push_steps()`); geprueft wird der Fusspunkt,
  nur Typ 2 mit `prm0 != 0` bei freiem `stflg 0x30`.
- `bhCheckWallEx` (`hitchk.c:641`) laeuft fuer Gegnerwork ueber denselben
  geordneten Durchlauf wie fuer den Spieler
  (`geometry_motion.cpp resolve_motion_wall`), mit `flg`/`stflg`/`flg2`/Lage/
  Etage/Trefferfeldern des Gegnerworks. Der Zweig `attr 0x40` liest und schreibt
  im Original **immer** den Spielerwork (`hitchk.c:250-295`: `!(plp->flg & 6)`,
  `plp->flg |= 0x10004`, `*(int*)&plp->mode0`, `sys->st_flg |= 4`,
  `bhSetEffect(29, &plp->px, …)`) — auch bei einem Gegneraufruf
  (`zonzon.c:1222`). Dafuer hat `MotionActor` jetzt `player_flags` (plp->flg),
  waehrend `flags` der Work des Aufrufers bleibt (`solid_enabled` liest ihn).

Ergebnis: `bhCheckDansa (hitchk.c:5391)` und `bhCheckWallEx (hitchk.c)` sind aus
den Pruefraeumen verschwunden; RM_0060 zeigt den Zombie im Verfolgungsschritt
mit ausgestreckten Armen an der Spielerfigur.

## 6. Kontaktdienste (neu, Verifikation ausstehend)

- hitchk.c:5712 `bhCheckPlayer(pp)` als Dienst `check_player` angebunden: fruehe
  Ausgaenge ueber `flg 0x8`, `stflg 0x40000000`, `flg2 0x1`, `flg 0x40`; sonst
  Abstand/Kapselhoehen-Vergleich ueber `car`/`cah` und die beiden Schubzweige
  (Dreieckstoss bei `plp->flg2 0x1` oder `pp->flg2 0x40`, sonst halbierter
  Versatz in beide Richtungen), danach `pp->stflg |= 4`, `plp->stflg |= 2`.
  `plp->car/cah` sind `PlyInfo[ply_id][0/1] - 1.0f` (player.c:199/708) und
  werden im Spielerbild gefuellt; `plp->aox/aoy/aoz` sind im Original nur in
  player.c:713 genullt und werden nirgends geschrieben, also 0. Den Zweig
  `plp->flg & 8` (Nullage `EXP_WORK.nlxb/nlyb/nlzb`, player.c:1379-1387) fuehrt
  der native Spielerbesitzer nicht — er meldet dort eine Luecke, statt einen
  Wert zu erfinden.
- hitchk.c:5788 `bhCheckEnemies(pp)` als Dienst `check_enemies`: Durchlauf ueber
  die ganze ene-Liste mit dem Filter `flg&1`, `flg&8`, `!stflg 0x1000000`,
  `enep != pp`, `!(flg 0x80) || !(lkwkp->stflg 0x1000000)`; je Treffer die
  Kapselpruefung und der verschobene Punkt, danach Mittelung ueber alle Treffer
  (`hitchk.c:5840-5860`). Der Zweig `pp == plp` ist unerreichbar, weil der
  Aufrufer immer ein Gegnerwork ist (der Spielerwork gehoert dem
  Spielerbesitzer) — dokumentiert, nicht ersetzt.

**Verifikation dieser beiden Dienste steht aus:** der gemeinsame Baum laesst
sich zurzeit nicht linken. Ursache ist der laufende Audio-Umbau einer
Parallelsitzung (`AudioMixer::footstep` mit zusaetzlichem `SoundPoint`,
`AudioMixer::object_sound_flag`/`object_sound_slot` ohne Definition —
`ld: symbol(s) not found ... object_sound_slot`). Die beiden dadurch
entstandenen Aufrufer in `native/world.cpp:440` (FootSeCall, Lage = Fusspunkt)
und `native/combat.cpp:162` (Schrittstimme, Lage = Schrittpunkt) sind
nachgezogen; `clang -fsyntax-only` laeuft fuer `world_enemies.cpp` und
`world.cpp` fehlerfrei durch. Sobald der Link steht, folgen ctest, 205-Raum-Scan
und Bildprobe wie oben.

## 5. Materialtoenung `npSetAllMatColor`

`npSetAllMatColor(mlwP->objP, obj_num, color)` (`njplus.c:1375`) patcht in jedem
Objekt des Baums die Materialchunks 17..23: B/G/R werden immer geschrieben, A
nur, wenn der Modellwert ungleich 0 war (`njplus.c:1411-1450`); Objekte mit
`evalflags & 0x8` werden uebersprungen. Nativ liegt der Wert am Baumbesitzer
(`WorldEntity::enemy_material_set/-_color`, wie `model_owner`); angewendet wird
er beim Zeichnen des nativen Gegners in `RoomWorld::append` mit genau dieser
Alpha-Regel. Angebunden sind `RECVX_EN01_SERVICES.set_material_color`
(Init-Toenung `en01.c:2772/2776`, Die-Ausblendung `:2927`) und derselbe Dienst am
Kindbaum; der EN04-Eintrag ist noch offen (siehe unten).

## Offen

1. `bhCheckPlayer` (`hitchk.c`) und `bhCheckEnemies` (`weapon.c`) — die
   Trefferpruefungen der Kindfamilie (Arm/Bein/Kopf gegen Spieler und Gegner).
2. `bhSetShadow`/`bhEff001` (`effect.c`) und `npSetAllMatColor` (`njplus.c`) —
   Schatten und Materialtoenung des Gegners (auch fuer `bhEne01_Init`).
3. `mlwP->owP[joint].mtx` bleibt in manchen Paessen unaufgeloest (Kappe/Wurm
   haben einen eigenen Baum, der erst nach ihrem ersten `bhCalcTree` gueltig
   ist) — naechster Schritt: Herkunft pro Aufrufstelle bestimmen.
4. `link_player`, `player_exp0_float_set`, `pad_on_clear` (Wurm haengt sich an
   den Spielerwork) sind im Vertrag vorhanden, aber weiter NULL.
5. `stg_no`/`rom_no`-Startwerte fuer `gm_mode >= 3` (system.c:494-508 setzt dort
   5/50) fehlen, weil der Host diesen Spielmodus-Start nicht nachbildet; im
   normalen Spielzug (gm_mode < 3) sind 0/0 korrekt.
6. `bhSetDansaLimitAtari` (`hitchk.c` vor `bhCheckDansa`) ist im Host-Wandlauf
   nicht gebunden; der Zweig `stflg 0x20000` setzt dort weiterhin nur die
   Trittflaeche.

## Grenzen

Kein Durchspielnachweis, kein Windows-Build in diesem Schritt, keine Aussage
ueber die uebrigen Gegnerfamilien. Nur arm64 Release gebaut.
