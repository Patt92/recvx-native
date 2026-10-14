# EN09 Bandersnatcher — Stillstand behoben, KI/Motion portiert; Gegner-Audit

Auftrag 2026-10-14: Bandersnatcher bewegt sich nicht; KI oder Motion prüfen,
danach bestehende Gegner auf saubere Anbindung/Spekulatives prüfen.
Keine Starter-/Paketersetzung, kein Commit.

## Ursache (gemessen)

Vorher-Binary `build/validation/en09-20261014/recvx-runtime-before`, RM_1060,
`--enemy-ai on`, 300/600 Bilder: beide Works `mode [0,0,0,0]`, `mtn_no 0`, HP 0,
Lage unverändert. Zwei Ursachen, beide KI-seitig:

1. Identität 9 war weder in `recvx_game` gebaut noch in `bhJumpEnemy[9]`
   eingetragen (`enemy_dispatch.c`). Der Pass meldete nur `bhEne09`; `Init`
   lief nie, der Renderer zeigte die Bindepose.
2. `native/enemies/en09.c` enthielt nur Rahmen/Motionschicht; Brain, MV00..14,
   NG/PlyDG, DmgCheck/DG/DD waren benannte Lücken. Init/SetMtn meldeten fehlende
   Knoten-/Fußfixierungsdienste und setzten dennoch Ersatzwerte (HP-Zeile 0,
   Armlänge 0) — beides entfernt.

Motion selbst (Bibliothek, `bhSetMotion`, Fußfixierung `bhCalcFixOffset`) war
intakt, wurde aber ohne Zustandsmaschine nie mit Bewegung angesteuert.

## Portiert (eigenständig, Referenz `src/ps2/veronica/prog/en09.c`)

Neue Datei `native/enemies/en09_behavior.inc` (in `en09.c` eingebunden, derselbe
Work, keine zweite Dienstinstanz) plus Änderungen in `en09.c/.h`:

- Init :2332 mit echtem `sys->gm_mode` (HP-Tabelle), Armlängen aus objP[7..10],
  zweiter Init (Block nullen, Längen zurück, Spielerknotenflags), Modell-/
  Morphwahl mbp[0], Materialfarbe 0xFFB2B2B2.
- SetMtn :7396: Armsegmente, Wurzelversatz-Varianten, Kopfspiegelung/-drehung,
  objP[0].pos[1]=0; MtnTblPlay :7558 mit Fußfixierung über den vorhandenen
  `calc_fix_offset` (Baumtausch 0x10000000, Typen 7/8 mit Versatz −1.5).
- Suche :3002 (Bytezähler; `bgez` bei 0x1c89b4 belegt: Sichtbit fällt so nie
  ab, unverändert), Brain :3048 mit Route, Etagenwechsel (`GetCloseEnemyAtari`
  :3245, `JumpCheck` :3309), Wandüberstieg/Armreise, Brain00 :3426,
  AttackCheck :3488 (Kurzschluss-RNG), AramLineCheck :3607, OtherEnemyCheck.
- MV00/01/03/04/05/06/07/08/09/10/11/12/13/14 (MV02 im Original leer),
  ChkArmLen/ChkArmLen2 (temporäres SetMotion auf demselben objP, exakte
  Wiederherstellung), CollChkArm/CollChkArm2.
- NG00/NG01, PlyDG00/PlyDG01 inkl. Spielermotion aus der Gegnerbibliothek,
  PlayerLink :2920 (Handkette des Spielers über vorhandenen Spielerbesitzer).
- DmgCheck :2484 (früher Ausstieg Nahkampf: ELF 0x1c7a98 belegt), ChgDmgMode,
  ExDmgCheck (gepackte Modebytes), DamageAdd (Blut/Feuer/Säure über T3-Helfer),
  DG00..DG10, DD00.
- Effekt 369 (Rauch, effsub4.c:2149/3142/3177) auf dem vorhandenen eff-Pool mit
  Modelltexturliste (`txp[0]`, tex_id 10), Vorlage `sys->ef` geteilt.
- `world_enemies.cpp`: passgebundener `RECVX_EN09_SERVICES`-Satz aus den
  vorhandenen Anbietern (Knoten, Fußfixierung, Suche, Route, Wand-ATR,
  Bodenflächen, WpnTab, Materialfarbe, Spielerknotenflags, Handkette, Rauch).
- `runtime.cpp --state`: zusätzlich frm_no/mtn_add/hokan_count/flg2 des Works
  (nur Diagnose).

## Prüfung

- Pflichtprüfer PASS vor jedem Build; eigene arm64-Build
  `build/en09-claude-20261014`.
- `tests/native/en09_world_test.cpp` (neu, CTest `enemy_en09_world`, echte
  RoomWorld/RM_1060): Init/Dispatch, echte Armlängen, Gehen bewegt den Work,
  angehaltener Pass friert Uhr/Lage/Pose, Typ 10 bleibt original untätig,
  DD00-Leichenstart, Schaden dam[]→HP, Rauch-UV/Freigabe. **PASS.**
  Negativkontrolle ohne `jump[9]`: schlägt fehl.
  (Befund unterwegs: Testprozesse müssen `_Make_SinTable()` aufrufen wie die
  Runtime, sonst ist jede Rotation null.)
- Runtime A/B RM_1060 600 Bilder, `--enemy-ai on --enemy-grab on`:
  vorher Stillstand; nachher Verfolgung, Armangriff MV07, Griff MV12,
  Spieler-HP 160→50; 1500 Bilder: Tod und natives Game Over. Keine Lücken.
- RM_1090: Bandersnatcher `stflg 0x1000000` (bhEneSetCk-Gate) — korrekt untätig.
- Nachbarn grün: enemy_work, en21, en07, c07 (Komponenten); verify_event_freeze,
  events, doors, continuity, gameover, push, stairs, combat (runtime-only).
- Rot ist Vorbestand: `verify_enemy_ai` scheitert an `natural` (Zombie beißt
  nicht, HP 160). Gleich mit Vorher-Binary, ohne meine Griffpass-Änderung, mit
  den Commits db8ec03 und 1c2934d, und bereits 2026-10-13 in
  `build/validation/dupclaire3-20261013` belegt. Nicht durch EN09 verursacht.
- Keine echte Sichtprüfung (Fenster) durchgeführt; Headless ist keine Bildabnahme.

## Offen bei EN09

- PlyDG00 Bild 70: drei `bhSetEffParticle` mit `pd` (sp+0x50), das der Code
  vorher nie schreibt (Disassembly 0x1cd848). Gemeldet statt geraten.
- ChkArmLen Fehltreffer: Original speichert uninitialisiertes f20 (0x1d18ac);
  nativ nur bei Treffer genutzt, MV05 meldet sonst.
- Bildabnahme von Arm/Morph/Rauch im Fenster; Windows nicht gebaut.

## Gegner-Audit (Originalräume, 400 Bilder, KI an; `build/validation/en09-20261014/audit/`)

| ID | Raum | Befund |
|---|---|---|
| 0 Fass | RM_0080 | läuft; nur Spieler-owP vor erstem CalcModel |
| 4 Hund | RM_1000 | läuft, Griff erreicht; `plp->mlwP->owP[n]` |
| 7 Fledermaus | RM_0130 | läuft; Schatten-`mdlver` gemeldet |
| 8 | RM_4000 | läuft; `bhEne03_Collision` gemeldet |
| 9 Bandersnatcher | RM_1060 | **läuft, keine Lücken** |
| 10 | RM_9170 | läuft; **`bhCheckWall` ungebunden** |
| 11 Spotter | RM_7081 | hängt in Init: EN11-Effekt-/Knotendienste fehlen; `bhEne05` fehlt |
| 12 | RM_5820 | `bhCheckWall(en12)`, `bhControlMask`, `bhSetEffectTb` |
| 14/30 | RM_5810 | `bhEne13` fehlt → EN30 ohne `lkwkp` |
| 16 | RM_1020 | Effekt 270 fehlt, `bhControlMask` |
| 21 Albinoid | RM_3120 | läuft; **`bhCheckWall` ungebunden**, Motion-frm_no-Meldung |
| 24 | RM_9050 | `bhEne23` fehlt; kein Modell |
| 25 | RM_9290 | kein Modell (`mdl[slot]`) |
| 56 | RM_4020 | kein Modell, `sys->ufo_*` ungebunden |

Gemeinsame Lücke mit größter Wirkung: `hitchk.c:12 bhCheckWall` (Gegner gegen
rom->walp/mwalp, ~630 Zeilen inkl. bhCheckBox/bhCheckInnerTriangle) ist kein
Dienst — EN10/EN12/EN21 laufen ohne Wandkollision. `bhCheckWallEx` ist ein
anderer Algorithmus und darf nicht ersatzweise gebunden werden.

Statische Suche nach Ersatzwerten nach Lückenmeldung in den produktiven
Gegnermodulen: kein weiterer Fall wie die alten EN09-Ersatzwerte gefunden.
Auffällig, aber unkritisch: `en00.c` setzt `atr.py=0` ohne `grand` (Anbieter
ist gebunden, Zweig tot); `en07.c` klemmt Index 16→15 (alle 16 Einträge 1,
Werteidentität belegt).
