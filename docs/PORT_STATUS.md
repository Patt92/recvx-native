# Nativer Portstatus

Dieser Bericht wertet nur explizit belegte native Mechanismen. Die vollständige Originaldecompilation ist absichtlich weder Nenner noch Fehlermenge.

Expliziter Belegumfang: **77.8 %** gewichtet, **55.7 %** voll belegt (54/97 Mechanismen; kein Gesamtfortschritt).

Event-Befehle (bhScenarioJmpT): 161/223 gebunden, 34 nur konsumiert, 28 fehlen.

| Bereich | Mechanismen | Belegumfang | voll belegt | teilweise | offen |
|---|---:|---:|---:|---:|---:|
| Gegner/KI | 9/12 | 87.5 % | 75.0 % | 3 | 0 |
| Effekte | 2/8 | 62.5 % | 25.0 % | 6 | 0 |
| Events/Skript | 21/24 | 93.8 % | 87.5 % | 3 | 0 |
| Spieler/Kampf | 6/16 | 68.8 % | 37.5 % | 10 | 0 |
| Raum/Kamera/Tür | 1/4 | 62.5 % | 25.0 % | 3 | 0 |
| Menü/Items/Text | 7/8 | 93.8 % | 87.5 % | 1 | 0 |
| Modelle/Animation | 4/5 | 90.0 % | 80.0 % | 1 | 0 |
| Audio | 4/18 | 61.1 % | 22.2 % | 14 | 0 |
| Ablauf/Speichern | 0/2 | 50.0 % | 0.0 % | 2 | 0 |
| Plattform (Host-ersetzt) | 0/4 | 50.0 % | 0.0 % | 4 | 0 |

Nur Ledger-Einträge mit eigenständiger nativer Evidenz zählen. Namensgleichheit, Originalquellen und quarantänierte Generatoren zählen nie als Portbeleg.

## Größte offene Funktionen

- `effect.c` bhDrawTrsEffect3D (partial, Eigenstaendiger nativer Eff009-Zeichenweg: Hand-/Objektanker, Billboard bzw. verknuepfte Matrix, unveraendertes Atlasalpha, Ninja-Blendpaar, Point/Linear je Draw und Tiefe positiv geprueft. Andere Effekte/Drawgates/Linktyp 5 bleiben offen. work/native_port_E03_lighter.result.md)
- `effect.c` bhInitEffect (partial, Eigenstaendige ef_info-Auswertung der vier Modellblöcke, PVP an Offset 8448 und 40 gemeinsame Texturen auf dem bestehenden nativen Assetbesitzer positiv geprueft; kein Generator. Vollstaendige Modellwork-Initialisierung fuer andere Effekte bleibt offen. work/native_port_E03_lighter.result.md)
- `effect.c` bhSetShadow (partial, Allokation 1:1 im gemeinsamen eff[512]-Pool (reserve_effect_slot, erster Slot ohne flg&3): flg 0x240001, id 1, type 0/1 nach Gelenkkette, tex_id -1, sx/sy/sz samt Kopie, lkwkp/lkono mit flg 0x80, jno[] bis -1. Das Zeichnen ist F05; id 1 wird weder gestept noch gezeichnet, der Slot belegt den Pool wie im Original. Ohne die Bindung brach bhEne04_Init (en04.c:939) vor mode0 = 1 ab und wuerfelte jeden Pass neue hp aus hp_tbl.)
- `effsub1.c` bhEff000 (partial, Patronenhuelse Typ0..3/10 mit Originalzufall, Boden-/Wandabprall, Wasseruebergang und CallYakkyouSe; Komponententest plus Runtime-Huelsenklang.)
- `effsub1.c` bhEff001 (partial, native/world_enemies.cpp tick_shadow: Besitzer ueber RECVX_ENEMY_WORK::owner (auch Kindworks); Scene-Submission vorhanden, Renderer-Verbrauch/Fog/Receiver-Tiefe offen (F05, work/native_port_F05_semantics.result.md))
- `effsub1.c` bhEff009 (partial, Eigenstaendige Flammensteuerung mit gelatchter UV-Bank, acht Schritten, Originaltypen/Flags und Queue-/Freigabegates; echte Eröffnung und normales Ausruesten/Ablegen mit unveraendertem HP/Fortschritt positiv geprueft. Vollstaendige Spieler-Armhaltung/pt_flg-Integration bleibt F01/P00. work/native_port_E03_lighter.result.md)
- `en04.c` bhEne04_Init (partial, Laeuft jetzt einmal bis mode0 = 1: RECVX_EN04_CONTEXT ist gebunden; npSetAllMatColor und objP pos/ang kommen vom selben Anbieter wie EN01 (en04.h: 'denselben Anbieter binden'), bhSetShadow vom eff[512]-Pool. native-gameplay-bindings belegt konstante Hunde-hp ueber Paesse. Uebrige EN04-Dienste (Raum-ATR, Route, Wand2, Blut, Spielerteile, Objekte) bleiben NULL und melden sich namentlich.)
- `en04.c` bhEne04_MV00 (partial, Originale Bereichs-/Trap-Routinen und Warte-/Lauf-MTN an RoomWorld gebunden; Verfolgung/Angriffe/gesamte MV-Zustandsmaschine fehlen)
- `en07.c` bhEne07_PlayerControl (partial, plp->mnwP = epw->mnwP wird gebunden; room.c:375 setzt epw->mnwP = sys->emtp[epw->id], die Bibliothek haengt also an der IDENTITAET des greifenden Gegners, nicht am Slotindex (sys->enow). en07/en12/en14 schreiben deshalb p.mtn_enemy_owner = w->id, und der Besitzer loest die Bibliothek ueber dieselbe Identitaet auf (WorldEntity::identity, mit der Raumform von bhSetEneMtn auf dem Work). Positiv in en07_test und enemy_morph_selection. Der Spielerclip nach dem Release kommt aus player.c:bhCPM2_act_std.)
- `event.c` bhNext (partial, Gemeinsamer WHILE/NEXT-Stack, signierter FOR-Zähler, Nullcountdown/Task-Reuse geprüft; ursprünglicher Loop/Sleep-Stack nicht vollständig übernommen)
- `event.c` bhPlayerPoisonCk (partial, Bedingung auf gemeinsamen Inventory.poisoned; alle Modi geprüft; vollständiger plp/ply_stflg-Pfad fehlt)
- `event.c` bhWalAtariSet2 (partial, Originale Attribute/Parameter/Typ im gemeinsamen CollisionWall gespeichert; Kollisionsalgorithmen bleiben native Nachbauten)
- `hitchk.c` bhCheckFloorP (partial, Eigenstaendige native Bodenfelder auf World-/Eventbesitzern: sicherer Vorfuehler/arn, Richtungsfilter, Floorindex, Wasser/footeff/Trittschall, fuenf Stufenkontakte; native_motion_collision und realer Worldtest. Vollstaendige Spielerzustandsmaschine und nachgelagerte Verbraucher offen. work/native_port_B00_motion.result.md)
- `hitchk.c` bhCheckPlayer (partial, native/world_enemies.cpp s.check_player: Bedingung plp->flg&8 war invertiert portiert (nlxb-Zweig gilt nur ohne flg 8, d.h. sterbender Spieler); korrigiert 2026-10-12. Der nlxb/nlyb/nlzb-Zweig (EXP_WORK, player.c:1379) bleibt gemeldete Luecke.)
- `hitchk.c` bhCheckWall (partial, Eigenstaendige native direkte ATR-Aufloesung in geometry_motion.cpp: Y-Rundung, Typzweige, Filter, Rundungen, Rampenprojektion und grand-Seiteneffekte positiv geprueft; der freie Spielerweg verwendet stattdessen den belegten Ex-Doppelpass. Effekt29/Hurt-State und weitere Aufrufer bleiben offen. work/native_port_B00_motion.result.md)

## Unbestätigte historische Ledger-Einträge

- `Motion.c` bhSetMotion: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function and six SetMtn routines extracted for push; original decoded keys/flags/attributes, Hokan and fractional exit sampled; full other actor/player binding pending
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
- `ps2_NaSystem.c` njColorBlendingModeSys: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originale alpha_tbl[13][12] + Ninja-Indextabellen generiert; native/chunk_blend.cpp RGB=(A-B)*C+D, normalisierte Alpha-Floats; kein vollständiger GS-Zustands-/Quantisierungsport
- `sub1.c` GetItem: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original GetItem extrahiert inkl. Zweig 153, danach originale ItemSort-Phase; StatusCancel/Sprite-Ein-/Ausfahren und ganzer StatusMain nicht vollständig gebunden
- `sub1.c` ItemBox: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original ItemBoxChange generiert, Tauschen/Stack/Restriktionen und Security-Raumskript geprüft; Host-Liste statt originaler Fenster/Sprites
- `sub1.c` ItemSort: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalfunktion extrahiert, pip-ILP32-Zugriff hostgerecht; Standardinventar nach Pickup und Mutationen sortiert/geprüft; Sonder-flgchk des ganzen StatusMain noch unvollständig

Erzeugt mit `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
