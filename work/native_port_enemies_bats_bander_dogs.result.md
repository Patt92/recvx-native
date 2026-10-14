# Fledermäuse / Bandersnatch / Hunde — Ergebnis (Subagent enemies, 2026-10-14)

Belege: `build/validation/enemies_bats_bander_dogs-20261014/` (`before/` = Binary vor
den Änderungen, gesichert aus dem Arbeitsbaum-Build 18:26; `after/` = Build nach den
Änderungen). Alle Runtime-Läufe headless, isolierte Einstellungen in `$TMPDIR`,
`--enemy-ai on`. Spielerlagen per Replay-`position` sind **Diagnoseaufstellungen**,
kein natürliches Durchspielen. Build mit `./build.sh --test`: 91/91 Tests grün,
Start.command-Dev-App aktualisiert (kein Paket ersetzt).

## 1. Fledermäuse ignorieren „Gegner-Angriff aus“ (RM_2010)

**Befund (gemessen):** Die zentrale Sperre in `RoomWorld::enemy_player_set`
(`player_damage_allowed`) unterdrückt nur den HP-Abzug. Mit Schalter aus griffen
die Fledermäuse weiterhin an: `mode0 2` (NG00/NG01), Spieler `plp->mode0 4`
(gehalten), Stimme/Vibration, nur ohne HP-Verlust. Sweep 50 Proben 30..1500 Bilder,
Spieler (55,0,65): vorher 9 Proben mit angreifender Fledermaus, 8 mit gehaltenem
Spieler (`before/rm2010_grab_off_sweep.txt`, Bild `before/shot_bat_before.png`:
Fledermaus am Kopf).

**Ursache:** en07 ist der einzige Gegner ohne Tor am Angriffsbeginn. Das Original
startet den Angriff nur in `bhEne07_BR00` (en07.c:341 Tor, :352/:367 `mode0 = 2`);
BR01/andere Zweige setzen kein `mode0 2`.

**Änderung:** `native/enemies/en07.c` BR00: `|| pl.debug_attack_disabled` im
Originaltor en07.c:341, wie en01/en04/en09/en10/en21. Suche, Hängen, Flug laufen weiter.

**Zentrale Sperre:** nicht fachlich falsch für Heilung (Zunahme wird übernommen) und
Gift (eigener Weg `player_poison_request_`), aber allein unzureichend (Griff/Motion
laufen ohne Schaden weiter — ein Zustand, den das Original nicht kennt). Sie stammt
von einem anderen Agenten und bleibt nach Regel „fremde Änderungen nie zurücknehmen“
als Rückhalt stehen; Kommentar in `native/world_enemies.cpp` korrigiert (Fledermäuse
jetzt am Tor, Sperre nur für Module ohne eigenes Tor).

**Nachher:** Schalter aus: 50/50 Proben ohne Angriff, HP 160 konstant, Fledermäuse
fliegen/kreisen (`after/rm2010_grab_off_sweep.txt`, `after/shot_bat_after.png`).
Schalter an: unverändert zum Vorher (gleiche Verteilung 6×1/3×2 Angreifer, HP 160→116,
`after/rm2010_grab_on_sweep.txt`). Festtest `en07_test.cpp br00_gates` Fall 4:
kein `mode0 2`, Sicht weiter ausgewertet, keine Kette/Spielerflags.

## 2. Bandersnatch RM_2000 außerhalb der Karte, kein Angriff

Zwei Ursachen, beide im Skript-/Work-Abgleich, nicht in EN09:

a) **Etage:** Common_controll 7 (event.c:12034-12062) setzt `work->flr_no =
bhCheckFloorNum(py)` (:12055). Nativ schrieb Befehl 7 den Work, aber nicht
`WorldEntity::object_floor`; der folgende Sub_controll 0x8f (`adopt_enemy_script`)
sah die geänderte Lage und übernahm die alte Etage der Raumtabelle zurück. RM_2000
EN09 #1 hat in der Raumtabelle py −36/`flr_no −1`; das Skript legt ihn auf Etage 0
bzw. 2 → nativ blieb `flr_no −1` (gemessen `EN09_ATK_DBG`: `flr=-1/0`). Damit nie
dieselbe Etage wie der Spieler → keine Route, kein AttackCheck (en09.c:3488 verlangt
`flr_no == plp->flr_no`), nur Herumlaufen.
**Änderung:** `native/world.cpp` Befehl 7: `e.object_floor = w.flr_no`.

b) **Rücksprung auf die Raumtabellenlage:** Das Skript ruft nach der Platzierung
Common_controll 47 = `bhCalcModel(bhCetask->work)` (event.c:10053). Nativ war
Befehl 47 ein No-op („Matrices are rebuilt when sampling geometry“). Der native
Gegnerwork besitzt owP aber selbst (`enemy_owp_mtx`); der Baum blieb auf der
Raumtabellenlage (room.c:444), und `bhCalcFixOffset` (Motion.c:868) zog den Work
im ersten aktiven Pass dorthin zurück: RM_2000 Eintritt 0 #1 von (40,0,45) auf
(25.9,0,68.7) hinter die Wand; Eintritt 1 #1 von (145,36,60) auf (23,36,69) und
lief dann bis x −133 (außerhalb der Karte, `before/rm2000_spawn1_AB.txt`,
`before/shot_rm2000_spawn1_f300.png`).
**Änderung:** `native/world.cpp` Befehl 47 für gebundene native Gegnerworks:
Skriptschreibzugriffe wie vor jedem Pass übernehmen (`adopt_enemy_script`), dann
`calc_enemy_model`. Andere Besitzer unverändert.

**Nachher:** Eintritt 1: #1 bleibt bei (144,36,60), greift an (MV12-Griff mtn 57,
Spieler mode0 4, HP 160→10 bis Bild 900), #0 springt auf Etage 2 und greift mit an
(`after/rm2000_spawn1_AB.txt`, `after/shot_rm2000_spawn1_f300.png`). Eintritt 0
(Spieler diagnostisch bei 40,0,52): #1 bleibt an der Skriptlage, Armangriff MV07,
HP 160→−20 bis Bild 600 (`after/rm2000_spawn0_near_AB.txt`,
`after/shot_rm2000_spawn0_near_f100.png`). Schalter aus: kein Angriff, HP 160.
Abschießbar waren sie vorher schon (unverändert).
Festtest `en09_world_test.cpp`: beide Eintritte `flr_no == bhCheckFloorNum(py)`,
#1 Etage 0 bzw. 2; nach Aktivierung (Befehl 0x24, Diagnose) kein Rücksprung.

**Nebenwirkung, belegt als Korrektur:** Derselbe Rücksprung traf alle per Skript
platzierten nativen Gegner. RM_0080 Spawn 2: Zombies stehen jetzt an der Skriptlage
(−3,0,21)/(14,0,14)/(33,0,4) (ev 0 off 1292-1520) statt an der Raumtabellenlage
(4.57,0,−37.14)… . `tools/verify_enemy_ai.py` war auf die falsche Lage kalibriert
(`FRONT 4.76 0 -34`, Kommentar nannte sie „Skriptlage“). Geändert auf die im
Docstring schon genannte `position -3 0 24 0` (knapp 3 vor Zombie 0, Blickrichtung
belegt), `death` von 1500 auf 1800 Bilder (Phase 2 jetzt gemessen bei 1600, Phase 1
bei 1500; Prüfinhalt Tod+Phase 2 unverändert). front/lever/death grün.

## 3. Hunde RM_1000 unbegrenzte HP

**Befund (gemessen, Handfeuerwaffe, `--enemy-grab off`):** Hund 1 HP 90→73→…→−7,
danach `mode [1,0,7,…]`, nie tot; Hund 2 ebenso (−4). Treffer im Bild 907 (`flg 0x4`),
HP −7 im Bild 908, Modus bleibt 1 (`before/rm1000_dogs.txt`).

**Ursache:** `RECVX_EN04_WPNDAMAGE` hatte die Feldfolge `{flg, nm_act, cb_act,
nm_blood, cb_blood}`, die Tabelle `en04_tables.c` aber die Originalfolge aus
types.h:1931 `{flg, nm_act, nm_blood, cb_act, cb_blood}`. Der tödliche Treffer geht
über den Kombozweig (en04.c:696 `hp < 0`), liest `cb_act` (:716) — nativ war das
`nm_blood = 7` → `act >= 4` (:719), kein Wechsel nach mode0 3, DG00 (:3226 `hp < 0 →
mode0 4`) wird nie erreicht; `bhEne04_DamageAdd` zieht bei `hp < 0` nichts mehr ab
(:741). Nebenbei waren Blutart normal/Kombo vertauscht.
**Änderung:** `native/enemies/en04.h` Feldfolge wie types.h:1931. Der bestehende
Tabellentest kodierte die falsche Folge (Zeile 15 `cb_act==1`, `nm_blood==3`) und ist
auf die Originalfolge korrigiert (nm_blood 1, cb_act 3, en04.c:285); neu Zeile 4 und
ein Fall „tödlicher Treffer Waffe 4 → mode0 3“ (Vorher-Folge: act 7, schlägt fehl).

**Nachher:** alle drei Hunde sterben: `mode [4,0,0,1]`, `flg 0x953` (Bit 2, DD00
en04.c:3376), Blutlache (`after/rm1000_dogs_final.txt`,
`after/shot_dog_dead_after.png`). Hund 2 liegt hinter einer Bordsteinkante; aus der
ersten Diagnoselage (48,0,80) trifft kein Schuss (Geometrie), aus (62,0,94) ja.

## Prüfungen

- `python3 tools/check_no_original_core.py` PASS vor Arbeit und jedem Build.
- `./build.sh --test`: 91/91 grün (darin `enemy_en07_component`, `enemy_en04_component`,
  `enemy_en09_world`). Ein Zwischenbuild hatte 6 rote Inventartests aus fremder,
  laufender Arbeit (ui.cpp/Inventar); im Endbuild grün.
- Runtime-Nachbarn (Endbinary): `verify_enemy_entry`, `verify_event_freeze`,
  `verify_gameover`, `verify_events`, `verify_continuity` grün (Logs im Belegordner).
- `verify_enemy_ai`: bis einschließlich RM_3070 grün; rot bleibt `dogs`
  „Todesbiss NG01 (plp->mode0 6) fehlt“ — **Vorbestand**: identische Assertion und
  identischer `player_work` mit dem Vorher-Binary (`verify_enemy_ai_before.log`).

## Offen

- Weitere Module setzen Spieler-HP ohne eigenes Tor am Angriffsbeginn (en05, en12,
  en13, en14, en16, en23, en24, en25, en27, en30 — statische Suche); dort greift nur
  die zentrale HP-Sperre, Griff/Motion laufen weiter. EN04: das Tor steht nur am
  Griff (en04.c:536), Sprungangriff MV07/Biss MV09 (`attack_mode 7/9`) starten mit
  Schalter aus weiter (gemessen: Hund 1 in MV07 bei Schalter aus). Nicht geändert,
  weil außerhalb des Auftrags; jeweils am Originalangriffsbeginn zu lösen.
- Befehl 47 für Spieler (`bhCalcModel(plp)`) und Objekte bleibt beim bisherigen
  Abtastpfad; nur native Gegnerworks rechnen jetzt sofort.
- Kein Fenster-/Hardwarelauf, kein Windows-Build; Bilder sind Headless-Screenshots.

## Geänderte Dateien

- `native/enemies/en07.c` (BR00-Tor), `native/enemies/en04.h` (Feldfolge),
  `native/world.cpp` (Common 7 `object_floor`, Common 47 `bhCalcModel`),
  `native/world_enemies.cpp` (nur Kommentar der zentralen Sperre)
- Tests: `tests/native/en07_test.cpp`, `tests/native/enemy_en04_test.cpp`,
  `tests/native/en09_world_test.cpp` (Fixture mit Raum/Eintritt, RM_2000-Fälle)
- Werkzeug: `tools/verify_enemy_ai.py` (FRONT-Lage/Death-Länge, siehe 2.)

## Ledger-Vorschläge

```
en07.c:bhEne07_BR00	done	native/enemies/en07.c en07_br00: Originaltor :341 plus Host-Schalter debug_attack_disabled (einziger Angriffsbeginn :352/:367); tests/native/en07_test.cpp br00_gates Fall 4; RM_2010 Sweep build/validation/enemies_bats_bander_dogs-20261014
en04.c:bhEne04_ChgDmgMode	done	RECVX_EN04_WPNDAMAGE Feldfolge types.h:1931 {flg,nm_act,nm_blood,cb_act,cb_blood}; toetender Treffer cb_act 3 -> mode0 3 -> DG00 :3226 mode0 4; enemy_en04_test damage(); RM_1000 alle drei Hunde tot (enemies_bats_bander_dogs-20261014)
event.c:Common_controll	partial	Kommando 7 schreibt flr_no = bhCheckFloorNum(py) (:12055) auch in die Entitaetssicht (object_floor), sonst setzte adopt_enemy_script die Raumtabellenetage zurueck; Kommando 47 (:10053) bhCalcModel fuer gebundene native Gegnerworks (adopt + calc_enemy_model), Spieler/Objekte weiter ueber Abtastpfad; en09_world_test RM_2000
```
