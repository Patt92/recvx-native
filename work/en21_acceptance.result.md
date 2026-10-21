# EN21 (Albinoid-Jungtier) — Laufzeit-/Weltabnahme

Auftrag `en21_acceptance` (Subagent, 2026-10-20). Kein Build in `build/macos-arm64`,
kein `./build.sh`, kein Commit, keine App-/Paketänderung. Pflichtprüfer
`tools/check_no_original_core.py`: PASS vor und nach der Arbeit.

## Räume und Headless-Lauf

Raumscan-Angabe bestätigt: EN21 steht nur in **RM_3120** (1 Jungtier, Typ 0, Satz 0)
und **RM_3140** (7 Jungtiere, Typ 1, alle unter Ereignissteuerung `mode0 5`).
`recvx-runtime --headless --in-game --enemy-ai on --frames 600` (eigenes `--user-dir`
unter `$TMPDIR`, frische `--state`-Dateien):

- RM_3120: Jungtier initialisiert (mode 1/1/1/0, hp 80, frm_no 65536+roh) und vom
  Raumskript über 0x24 inaktiv gehalten (stflg 0x1000000, flg 0x8000). Einzige
  Gegnerlücke ist **fremd**: `bhSetMotion:frm_no 9226 ausserhalb von frm_num 2 (id 1, mtn 1)`
  (vom Skript gehaltener Zombie, Typ 10).
- RM_3140: keine Gegnerlücke; eine offene Weltbindung ist 0x23 (Item-Interaktion
  außerhalb der etcp-Tabelle), nicht EN21.
- Die natürliche Freigabe des RM_3120-Jungtiers durch das Raumskript ist **nicht
  verfolgt**; der Welttest gibt per 0x24 frei (als Diagnose gekennzeichnet).

## Originalbefund gegen nativen Code

Zeilenweiser Abgleich `native/enemies/en21.c` gegen `src/ps2/veronica/prog/en21.c`
(:88–1392) und `zonzon1.c:391–394`, plus Bindung in `world_enemies.cpp:1895–1907`
(`effect_request` → `sys->ef`-Vorlage → `bhSetEffectTb`-Felder wie `effect.c:436–534`,
Nachtrag stflg 0x20/txp[0]/tex_id) und `effect_sprites.cpp:780` (Feldsicht des Slots).
**Kein Fehler in `en21.c` gefunden**, daher keine Codeänderung an EN21. Alle
Abweichungsverdachte haben sich am echten Raum als originalgetreu erwiesen:

- Reihenfolge Init → `bhSetMotion` im selben Pass (:95): frm_no = rand%19 roh + 65536,
  daher MV01 im ersten Move mit `En21SpdAddTbl[1]` = −0.25 (spd 0.25).
- Spieler-hokan_count 5 (:955) wird im selben Bild vom nachfolgenden `bhControlPlayer`
  weitergezählt (game.c:48/49 Gegner vor Spieler) — keine Abweichung.
- Ereignisgesteuerte Jungtiere spielen Raummotionen (0x30, `mtn_room`); dort gibt es
  keinen Eintrag 6, eine erzwungene Sterbemotion meldet korrekt `mnwP[mtn_no].frm_num`.

## Änderungen

- `tests/native/en21_world_test.cpp` (neu; **Achtung:** ein Zwischenstand davon ist durch
  den Hauptagenten in Commit 9a7a501 gelandet, der Arbeitsbaum enthält die Endfassung).
- `work/en21_acceptance.integration.patch`: CMake-Registrierung `recvx-en21-world-tests` /
  CTest `enemy_en21_world` (hinter `data/rooms`), Abschnitt EN21 in
  `docs/NATIVE_GAMEPLAY.md`. `git apply --check` gegen den aktuellen Arbeitsbaum: OK.

## Testfälle (echte RoomWorld, Erwartungen aus den Originalzeilen)

1. **Init Typ 0** (RM_3120, :196–252): mode, hp 80, ar/ah/aox/aoy/aoz/car/cah,
   flg 0x178|0x800, flg2 0x40, lok_jno 4, mtn 7, hokan_rate/mtn_add 65536, CapColTab,
   clp_jno, mdflg, EXP0 999/0/…/4/0; Skriptsperre ohne Move.
2. **Bewegung** (:256–335, :380–551): SearchPlayer-Abstand/Ziel, Spieler hinten → MV02/03
   mit Return vor dem ct0-Abzug, ay ∓728.
3. **Verfolgung → Griff** (:288–322, :744–837, :931–1018): Griff genau bei Abstand < 5.5 und
   |Winkel| < 910; NG00/PlayerControl case 0/1/2, hp −9 genau bei ct0 4, mtn 73/74 und
   ct0/ct1 aus `frm_num`, ay-Annäherung, Vibration (1,10), Freigabe-Flags, Sperrzeit 35
   (−1 im Pass), 33 Bilder ohne erneuten Griff; MV06-Zweig aus unabhängiger
   AllWayWallCheck-Probe über `collision_contact`.
4. **Funke 399 Typ 1** (:779–782, :1085–1164, :1168–1296): genau 8 Funken (ct0 0,2,…,14),
   Gelenk `En21SparkTbl[ct0]` ± Zufallsraster, type 1, flg 0x180000, stflg 0x20,
   tex_id 5 (zeichenbar über die Modelltexturliste), Blend 8/10, ct1 7, drei Bilder
   Lebensdauer, py +0.2 je Pass, UV uvinfo1, sx/sy 2.5, Farbe 0xFFA0A0FF; `sys->ef.ax/ay`
   bleiben unverändert (Vorlage diagnostisch vorbelegt).
5. **Funke Typ 3** aus MV00 (Diagnose-Einstieg mode2 0): nur bei ct0 8, owP[1]+0.7,
   ay+180°, ein 0.7-Schritt, uvinfo3, ct1 8.
6. **MV04/MV05** und **Griffgrenze**: Winkel ±26° → MV05/MV04 mit ay ±1274; 4.7° greift.
7. **Spur 396** (:410/:493/:537, :1299–1321, :1324–1392): type 0, flg 0x80000, stflg 0x20,
   tex_id 6, Blend 11/3, ax 16384, ay je MV-Zweig, Lage owP[0]+0.1; volle Folge ct1 32..0,
   ct2-Takt, Endeintrag {−1,0,0,0} (u −1/256, sx 0), danach frei.
8. **Schaden/Tod** (:840–928, zonzon1.c:391): Trefferfelder wie weapon.c (Diagnose):
   hp −30, Blutpartikel, dam/flg 0x4 gelöscht; hp < dam → mode 4; DD00 mtn 4, nächste 5,
   Pfütze genau bei ct0 0 (eff 300, flg 0x200001, mtn_add 0), dann hp −1/flg 0x2.
   Pfütze zeichnet mit Atlas‑17‑Bild `BloodType[21].color` = 0 (Inhaltsvergleich).
9. **Echter Waffentreffer**: CombatController, Waffe 4 (Item 9), Zielen nach unten:
   hp‑Abzug im Move, Tod mit Pfütze (Magazin diagnostisch nachgefüllt).
10. **Tödlicher Griff** (:787–791, :829–835): hp 5 → −4 bleibt stehen, mode3 3, danach
    mode 1/0/0/0 und MV00 ohne Brain.
11. **RM_3140 Typ 1** (:162–195): sechs Typ‑1‑Works mit Typ‑1‑Init-Feldern; Verteilung
    Sperrzeit 35 (frühere Works 35, spätere 34) und Sterbemotion 6→4 an alle sieben
    (Diagnose-Zustandsschreibung).

Lauf: privat unter `$TMPDIR` gegen Kopien der `librecvx_*.a` (22:15 und erneut 22:36,
nach Commit 9a7a501): `EN21 world PASS`; Komponententest `recvx-en21-tests` PASS.

## Gegenproben (Sabotage an einer Kopie von en21.c bzw. world_enemies.cpp, je rot, zurück grün)

| Sabotage | rote Prüfung |
|---|---|
| Init `aoz 0.8 → 0.7` | Init: ar/ah/aox/aoy/aoz/car/cah |
| `flm_tbl {10,3,2,3} → {10,4,2,3}` | Funke 399 … ct2 flm 3 − 1 |
| Spur `bl_dst 3 → 6` | Spur 396 … Blend 11/3 |
| Typ‑1‑Funke schreibt ax | SetSparkEff Typ 1 schreibt sys->ef.ax/ay nicht |
| BloodPool‑Aufruf entfernt | bhEne_BloodPool: kein eff 300 |
| Griffschaden 9 → 8 | NG00 ct0 4: plp->hp −= 9 |
| Spur‑Endeintrag ersetzt | bhEff_E21_Mark: ct1/ct2/UV‑Folge |
| Sperrzeit nur eigenes Work | NG00 case 2: EXP0_UC(0x15) = 35 an alle |
| Sterbemotion‑Wrap > 7 | DD00: 6 + 1 > 6 → 4 an alle |
| MV05 7° → 6° | MV04/05: ay ±1274 |
| Damage im Move entfernt | Damage: hp − 30 |
| Griffwinkel 5° → 4° | Brain: \|Winkel\| < NJM_DEG_ANG(5) greift nicht |
| Pfützenpalette `color+1` (world_enemies.cpp‑Kopie) | Blutpfütze zeichnet mit fremder Palette |

Die Sabotagen liefen nur an Kopien unter `$TMPDIR`; Repository‑Dateien blieben unverändert.

## Ledger-Vorschlag (Text, nicht eingetragen)

```
en21.c:bhEne21	done	Natives en21.c im WorldEntity-Dispatch; tests/native/en21_world_test.cpp am Originalraum RM_3120 (Typ 0) / RM_3140 (Typ 1): Init, Skriptsperre, SearchPlayer/Brain-Grenzen (5.5, 5/100 Grad), MV00-06, Griff NG00/PlayerControl inkl. hp-9/Vibration/Freigabe/Sperrzeit, Damage (Trefferfelder und echter CombatController-Treffer), DD00 mit Pfuetze, toedlicher Griff, Verteilung Sperrzeit/Sterbemotion; 13 Gegenproben rot. Offen: natuerliche Skriptfreigabe RM_3120, Typ-2-Spur im Ereignismodus (:97), MV06 case 1/2 Drehbetrag, Stimme/SE/plp->dpx/way nur im Komponententest. work/en21_acceptance.result.md
en21.c:bhEne21_SetSparkEff	done	Typ 1 (NG00) und Typ 3 (MV00) am echten Raum: Gelenk En21SparkTbl[ct0]/owP[1]+0.7, Zufallsraster, ay+180 nur Typ 3, sys->ef.ax/ay bei Typ 1 unveraendert, stflg 0x20/txp[0]=Modelltexturliste/tex_id 5 zeichenbar (en21.c:1085-1164). Typ 0/2 ruft en21.c nicht auf. work/en21_acceptance.result.md
en21.c:bhEne21_SetMarkEff	done	Am echten Raum: type 0, ax 16384, ay je MV-Zweig, owP[0]+0.1, stflg 0x20/tex_id 6 zeichenbar (en21.c:1299-1321); Gegenproben rot. work/en21_acceptance.result.md
en21.c:bhEff_E21_Spark	done	Slot 399 am echten Raum: Blend 8/10, ct1 pat, Lebensdauer flm 3, py+0.2 (Typ 1), 0.7-Schritt (Typ 3), UV uvinfo1/3, sx/sy, Farbe (en21.c:1168-1296). work/en21_acceptance.result.md
en21.c:bhEff_E21_Mark	done	Slot 396 am echten Raum: Blend 11/3, ct1 33..0, ct2-Takt, Endeintrag {-1,0,0,0}, Farbe 0x40808080, Freigabe (en21.c:1324-1392). work/en21_acceptance.result.md
zonzon1.c:bhEne_BloodPool	done	sv->blood_pool -> EffectParticles::spawn_pool mit BloodType[id].color; EN21-Tod am echten Raum: eff 300 flg 0x200001 genau bei ct0 0, Zeichnung mit Atlas-17-Bild BloodType[21].color (zonzon1.c:391-394); Palette+1-Gegenprobe rot. work/en21_acceptance.result.md
```

## Grenzen

- Natürliche Freigabe des RM_3120-Jungtiers und Typ‑2‑Pfad (`mode0 5 && type 2`, :97)
  nicht am Raum belegt; RM_3140-Index 0 (Typ 1 mit flg 0x841/flg2 0x40, mtn 4 bei y 14.5)
  ist ereignisgesteuert und wurde nicht weiter untersucht.
- `plp->djnt_no/dpx..dpz/way`, Spielerstimme 0x402 und SE‑Nummern haben keinen öffentlichen
  Leser am Raumbesitzer; nur die Komponentenfixture `en21_test.cpp` prüft sie.
- Kein Bild-/Audio-Sichtvergleich, kein Windows-Lauf, kein Start.command-Paket; CTest
  wird erst nach Einspielen des Patches durch den Hauptagenten gebaut.
- Der echte Waffentest hängt vom Zufallsstrom und vom Kampfkern ab (Magazin nachgefüllt).
