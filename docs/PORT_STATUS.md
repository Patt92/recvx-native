# Nativer Portstatus

Dieser Bericht wertet nur explizit belegte native Mechanismen. Die vollständige Originaldecompilation ist absichtlich weder Nenner noch Fehlermenge.

Expliziter Belegumfang: **84.0 %** gewichtet, **69.0 %** voll belegt (263/381 Mechanismen; kein Gesamtfortschritt).

Event-Befehle (bhScenarioJmpT): 176/223 gebunden, 29 nur konsumiert, 18 fehlen.

| Bereich | Mechanismen | Belegumfang | voll belegt | teilweise | offen |
|---|---:|---:|---:|---:|---:|
| Gegner/KI | 21/45 | 73.3 % | 46.7 % | 24 | 0 |
| Effekte | 158/172 | 95.9 % | 91.9 % | 14 | 0 |
| Events/Skript | 32/41 | 89.0 % | 78.0 % | 9 | 0 |
| Spieler/Kampf | 9/23 | 69.6 % | 39.1 % | 14 | 0 |
| Raum/Kamera/Tür | 3/22 | 54.5 % | 13.6 % | 18 | 1 |
| Menü/Items/Text | 30/41 | 82.9 % | 73.2 % | 8 | 3 |
| Modelle/Animation | 5/8 | 81.2 % | 62.5 % | 3 | 0 |
| Audio | 4/20 | 60.0 % | 20.0 % | 16 | 0 |
| Ablauf/Speichern | 1/9 | 55.6 % | 11.1 % | 8 | 0 |
| Plattform (Host-ersetzt) | 8/17 | 73.5 % | 47.1 % | 9 | 0 |

Nur Ledger-Einträge mit eigenständiger nativer Evidenz zählen. Namensgleichheit, Originalquellen und quarantänierte Generatoren zählen nie als Portbeleg.

## Größte offene Funktionen

- `adv.c` Adv_GameOptionScreen (missing, Optionsbildschirm (DisplayOptionPlateLevel0-3, Sound/Vibration/Key-Config) nicht portiert; OPTION oeffnet den Host-Einstellungsbildschirm (frontend.h, dokumentierte Abweichung))
- `adv.c` CheckMcSelectPortInfoState (missing, Originale Memory-Card-Portabfrage nicht portiert; die Host-Slotauswahl ermittelt belegte Spielstaende aus dem Benutzerdatenverzeichnis)
- `adv.c` FindFirstVmDrive (missing, Originale Memory-Card-Suche nicht portiert; der Host ersetzt sie durch native Spielstaende im Plattform-Benutzerdatenverzeichnis und aktiviert LOAD GAME bei belegtem Slot)
- `objitm.c` bhDrawSpObject (missing, Die gemeinsame Schnitt-Beschraenkung (objitm.c:688) ist gebunden; die Spezialobjektliste sys->ob_spc mit ihren Typzweigen (Licht 5 usw.) hat im Host keinen Besitzer.)
- `adv.c` AdvGetCancelButton (partial, Host-Cancel statt keytype-Tabelle {32,128,64})
- `adv.c` AdvGetOkButton (partial, Host-Confirm steht fuer 0x800 (Start) und den keytype-OK-Button zugleich; keytype-Tabellen {192,96,160} nicht gebunden)
- `adv.c` ResetAdvSystem (partial, Nur die Titelfelder NextReturnCode/ExtraFlag/NextMode/GenFlag und StopAdvScreenSaver(1) (Frontend::reset_adv_system); Texturlisten, Optionsfelder und VM-Felder fehlen)
- `camera.c` bhControlCamera (partial, cam.ofx/ofy/ofz: Besitzer Runtime (ueberdauert den Raumwechsel), RoomWorld::camera addiert ihn auf cam.px (Sicht, Effektkamera, Schallposition). Schreiber gebunden: EN13. Offen: en02/en17/event.c/effsub3.c-Schreiber.)
- `cut.c` bhControlEventCamera (partial, Vollstaendige mode0-Maschine in RoomWorld::control_event_camera (native/world.cpp): 0 Initpass mit st_flg 0x10/0x800000 und bhInitEventCamera (:1998-2020), 1 Fahrt, 2 Endhalten unter !(gm_flg 0x1) (:2140), 3 aus bhCamPauseSet ohne case-Zweig (Pose, cam.ct0 und cam.keyf_no halten). cam.ct0 zaehlt nur bei !(sys->st_flg & 0x80) (:2101). Alle vier Splines laufen jetzt ueber die 20 Kontrollslots cam.ips/ian/ipf/icr statt ueber die Rohkeys; Keyframe-Lock lkflg/lkno/lkono nur bei Gleichheit mit kfp[1] bzw. keyf_n==1 (:2049-2056, Fall 2 ohne den keyf_n-Zweig :2159-2166). Gemessen RM_0040 ev5 (bhCamSet 0,10,1 + bhCamPauseSet 0): vorher drehte ax 65338->54060 ueber 30 Bilder, jetzt Standbild exakt auf Key 1 (7.5,11.5,4.5 / -198,-17736,0 / pers 9102) wie im RDT. Offen: bhSetEventHideObjLgt wird nicht als Aufruf nachgebildet (die Masken werden je Bild aus dem aktuellen Key gelesen), sys->pt_flg 0x1 des 0x800000-Zweigs hat keinen nativen Besitzer, und cam.pers wird ohne belegte CAM_WORK-Breite nicht genarrowt.)
- `cut.c` bhSetCut (partial, cam.fog_* vom Schnitt, fog_ct = fog_cngct-3 (ef_flg 0x10 einmal weniger) bei st_flg 0x2 && flg 0x8, sonst 0, gm_flg 0x10 (:349-377) in RoomWorld::cut_fog_handover ueber initialize_camera_clip; sys->fog_cngct ohne ss_flg-Bit-1-Besitzer fest 2 (fog_ct ist bei 2 und 3 gleich 0). Uebrige bhSetCut-Teile (Clip/Hide) wie zuvor.)
- `cut.c` bhSetEventCamera (partial, Opcode 0x13 Modus 0 setzt jetzt cam.px/py/pz, cam.ax/ay/az (als vorzeichenbehaftete CAM_KEYF_WORK-shorts) und cam.pers aus dem Key und bei lkflg != 0 sofort die Zielwinkel (:1660-1684), danach *(int*)&cam.mode0 = 0 (:1686) als Initpass von bhControlEventCamera. cam.fog_* nehmen den Key in BEIDEN Zweigen; nur sys->fog_ct unterscheidet sich (fog_cngct - 4, geklemmt, bei st_flg 0x2 && kfp->flg 0x8, sonst 0, :1694-1716) - vorher uebernahm der Host die Fogwerte nur im ersten Zweig und setzte fog_ct nie. gm_flg 0x10 (:1718). Splines cam.ipf/cam.icr je Bild in control_event_camera. Offen: cam.pxp/axp-Vorgaengerfelder, cam.ln, cam.flg 0x2/0x38 und bhSetEventHideObjLgt als Aufruf.)
- `cut.c` bhSetFixedCut (partial, Fog-Uebergabe :463-484 (ohne ef_flg-Zweig) bei Opcode 0x2a Modus 0.)
- `door.c` bhControlDoor (partial, native/doors.cpp tick: recvx_door_step, zwei CallDoorSe-Gruppen (0xF0000 sofort, 0xF00000 erst bei status 0x20), ControlSoundTiming-Kommandos, Ende nur ueber status 0x40 aus FadeProc1 (door.c:641). spd_mde-Schleife nur DMO_SPD_1; Zeichnung Host-Renderer.)
- `effect.c` bhControlEffect (partial, Spielerlink liest jetzt wie effect.c:803-811 pp->mlwP->owP[op->lkono].mtx aus dem zuletzt von bhCalcModel(plp) festgeschriebenen Baum (native/world.cpp RoomWorld::link_matrix, player_joint_matrices_) statt plp->mnwP[plp->mtn_no] neu abzutasten. Behebt den Absturz actor motion unavailable im Freigabebild eines Griffs. Tests: tests/native/lighter_test.cpp grab_release_links, tools/verify_enemy_ai.py rm0060-lighter-release/-death. Uebrige Effektzweige unveraendert offen.)
- `effect.c` bhDrawTrsEffect3D (partial, Rueckseitentor flg 0x20000 portiert: njCalcPoint + njProjectScreen der ersten drei Eckpunkte, njGetPlaneNormal2, njUnitVector, vec.z < 0 -> stflg 0x10 und nicht zeichnen. Das Bit merkt der const-Zeichenpass und bhControlEffect traegt es im naechsten Bild ein; einziger Originalleser ist bhDrawSmallScreenRenderTexture (screen.c:943, offen). Texturwahl/Blendpaar/Billboard waren schon gebunden. native/effect_sprites.cpp append_effect_sprite; Tests group7_backface_and_eyes und eye_lights (RM_1002, Tor schliesst aus Kamera 2))

## Unbestätigte historische Ledger-Einträge

- `event.c` bhArmsItemSet: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Ersttreffer/Taschenbereich/Equip-Slot; komplette WeaponSet/Monitor-Zustandsmaschine fehlt
- `event.c` bhItemGetGet: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Inventarbank/Flags/Menge/Equip-Slot geprüft; unabhängiges ply_wno/Monitorverhalten im Host nicht vollständig
- `event.c` bhItemGetGetEx: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; freie Slots/Shotgun/Equip geprüft; unabhängiges ply_wno/Monitorverhalten nicht vollständig
- `event.c` bhPlItemChangeM: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Ersttreffer, gepackte ID und originale Mengen/Flag-Löschung geprüft
- `event.c` bhPlItemLost: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; feste Slotbereiche/Ersttreffer/Kompaktierung; komplette WeaponSet/Monitor-Zustandsmaschine fehlt
- `event.c` bhPlItemLostEx: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Transfer in Bank 14 und feste Kompaktierung; komplette WeaponSet/Monitor-Zustandsmaschine fehlt
- `event.c` bhPlItemTamaSet: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Ersttreffer und u16 Little-Endian-Menge geprüft
- `hitchk.c` bhCheckWall2Box: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function and geometry helpers extracted, static/mobile ATR walls and per-object old position bound; wall/second crate tests
- `hitchk.c` bhGetGroundPosition: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Generated original query; room-owned htp retains room/mobile index and live ATR work; player.c:1743-1762 bound only where push/stair state exists; player shot caller creates linked bhEff000 with ground/water/wall/audio dispatch, ef_pol state and SYSTEM model/texture draw; EE libc rand/srand and bhRandTest/RandamSet are bound from executable disassembly and bhSetEffectTb first-free eff[512] allocation is mapped; other random consumers, exact EasyLight, full effect lifecycle and general player work remain open
- `objitm.c` bhObj001: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function extracted; mobile walls/side probes/work indices, original speed/box collision tested; auxiliary metc/mflr consumers for crate climbing not fully bound
- `player.c` bhCPM2_act_psh: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function extracted verbatim (packed mode-byte ABI write adapted); MTN49/50/reverse49, original Hokan and curve9 bound and tested; physical vibration callback not bound
- `ps2_NaSystem.c` njColorBlendingModeSys: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Zustandsautomat in native/chunk_blend.cpp (nj-Save und gs-Save: mode_bk[0..1]/set_last, ALPHA-Register; Index 0/1 schreiben 0x01/0x44 ohne Tabellenzugriff, ein Paar mit Index unter 2 laesst das Register stehen, ungueltige Paare werden als blend_unavailable gemeldet statt wie im Original abzubrechen); alpha_tbl[13][12] + Ninja-Indextabellen generiert, RGB=(A-B)*C+D in blend_gs. Nicht modelliert: FBA=0 (blend_gs schreibt das Zielalpha), PABE und GS-Paketquantisierung, dc_alpha (im Original nur genullt, kein Leser). Belege: chunk_blend_test (Sonderfaelle, Register-bleibt, Diagnose), effect_particles/weapon_flash/lighter-Tests, verify_lighter/combat/detector/scanner.
- `sub1.c` GetItem: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original GetItem extrahiert inkl. Zweig 153, danach originale ItemSort-Phase; StatusCancel/Sprite-Ein-/Ausfahren und ganzer StatusMain nicht vollständig gebunden
- `sub1.c` ItemSort: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalfunktion extrahiert, pip-ILP32-Zugriff hostgerecht; Standardinventar nach Pickup und Mutationen sortiert/geprüft; Sonder-flgchk des ganzen StatusMain noch unvollständig

Erzeugt mit `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
