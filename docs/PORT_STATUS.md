# Nativer Portstatus

Dieser Bericht wertet nur explizit belegte native Mechanismen. Die vollständige Originaldecompilation ist absichtlich weder Nenner noch Fehlermenge.

Expliziter Belegumfang: **83.3 %** gewichtet, **66.7 %** voll belegt (20/30 Mechanismen; kein Gesamtfortschritt).

Event-Befehle (bhScenarioJmpT): 149/223 gebunden, 45 nur konsumiert, 29 fehlen.

| Bereich | Mechanismen | Belegumfang | voll belegt | teilweise | offen |
|---|---:|---:|---:|---:|---:|
| Gegner/KI | 0/1 | 50.0 % | 0.0 % | 1 | 0 |
| Effekte | 0/1 | 50.0 % | 0.0 % | 1 | 0 |
| Events/Skript | 20/24 | 91.7 % | 83.3 % | 4 | 0 |
| Spieler/Kampf | 0/1 | 50.0 % | 0.0 % | 1 | 0 |
| Menü/Items/Text | 0/1 | 50.0 % | 0.0 % | 1 | 0 |
| Ablauf/Speichern | 0/2 | 50.0 % | 0.0 % | 2 | 0 |
| Plattform (Host-ersetzt) | 0/3 | 50.0 % | 0.0 % | 3 | 0 |

Nur Ledger-Einträge mit eigenständiger nativer Evidenz zählen. Namensgleichheit, Originalquellen und quarantänierte Generatoren zählen nie als Portbeleg.

## Größte offene Funktionen

- `effect.c` bhDrawTrsEffect3D (partial, Billboard/Atlas/additive Tiefe für Eff200/201/218; übrige Blendarten/Emitter/Linktyp 5 nicht vollständig)
- `en04.c` bhEne04_MV00 (partial, Originale Bereichs-/Trap-Routinen und Warte-/Lauf-MTN an RoomWorld gebunden; Verfolgung/Angriffe/gesamte MV-Zustandsmaschine fehlen)
- `event.c` bhNext (partial, Gemeinsamer WHILE/NEXT-Stack, signierter FOR-Zähler, Nullcountdown/Task-Reuse geprüft; ursprünglicher Loop/Sleep-Stack nicht vollständig übernommen)
- `event.c` bhPlayerPoisonCk (partial, Bedingung auf gemeinsamen Inventory.poisoned; alle Modi geprüft; vollständiger plp/ply_stflg-Pfad fehlt)
- `event.c` bhWalAtariSet2 (partial, Originale Attribute/Parameter/Typ im gemeinsamen CollisionWall gespeichert; Kollisionsalgorithmen bleiben native Nachbauten)
- `hitchk.c` bhCheckFloorP (partial, native/events.cpp check_floor: Original-ATR_WORK und bhControlPlayer sp_flg-Bit-0-Pause; Metalldetektor-Replay mit echter Ansage/Kamera geprüft; übriger Spieler-Hitpfad unvollständig)
- `sp_evt.c` bhDrawSpEvtComCharacter (partial, Werk, Transform (0/0.1/-1.4, cax 16384 + Skriptschritt, Skalierung 0.05), njSetScreen(600,608,416,528,336) und die Fensterbeschneidung sind übertragen; die Figur kommt über RoomWorld::computer_character aus dem Actor-Besitzer (Easy-Light 1.0/0.2 aus (4,6,10)). Offen: das Original zeichnet den stehenden Objektbaum, der Host sampelt mtn_no/frm_no neu; geprüft in tools/verify_computer.py (Testskript, Gegner 0 in RM_0020))
- `sub1.c` ItemCombination (partial, Regeln über Original Combi_00/01 und combidata; Menüablauf/Animation nativ)
- `system.c` bhSysCall (partial, cb_flg 0x40000 führt über GameplayInput::synchronize zur Box; übrige Systemzustände nicht vollständig)
- `system.c` bhSysCallMonitor (partial, Raumladen/-wechsel nativ in runtime/world, nicht als mn_md-Zustandsmaschine)

## Unbestätigte historische Ledger-Einträge

- `Motion.c` bhSetMotion: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function and six SetMtn routines extracted for push; original decoded keys/flags/attributes, Hokan and fractional exit sampled; full other actor/player binding pending
- `effsub1.c` bhEff029: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original lifecycle extracted verbatim; 32-frame wrap, linked wall-work type-bit side effect and slot release tested; hitchk.c clears wall attr 0x40 only after successful shared-slot allocation
- `event.c` bhArmsItemSet: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Ersttreffer/Taschenbereich/Equip-Slot; komplette WeaponSet/Monitor-Zustandsmaschine fehlt
- `event.c` bhItemGetGet: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Inventarbank/Flags/Menge/Equip-Slot geprüft; unabhängiges ply_wno/Monitorverhalten im Host nicht vollständig
- `event.c` bhItemGetGetEx: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; freie Slots/Shotgun/Equip geprüft; unabhängiges ply_wno/Monitorverhalten nicht vollständig
- `event.c` bhPlItemChangeM: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Ersttreffer, gepackte ID und originale Mengen/Flag-Löschung geprüft
- `event.c` bhPlItemLost: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; feste Slotbereiche/Ersttreffer/Kompaktierung; komplette WeaponSet/Monitor-Zustandsmaschine fehlt
- `event.c` bhPlItemLostEx: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Transfer in Bank 14 und feste Kompaktierung; komplette WeaponSet/Monitor-Zustandsmaschine fehlt
- `event.c` bhPlItemTamaSet: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalroutine extrahiert; Ersttreffer und u16 Little-Endian-Menge geprüft
- `hitchk.c` bhCheckWall: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Generated original routine bound to the player candidate through persistent player work/room/mobile ATR tables; type branches, filters, vertical/floor side effects and temporary Dansa geometry tested. Original bhSetEffect(29) allocates in shared eff[512], preserves linked wall ATR work, and bhEff029 applies type |= 0x40 after 32 frames. General bhControlPlayer dispatch and other callers remain open.
- `hitchk.c` bhCheckWall2Box: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function and geometry helpers extracted, static/mobile ATR walls and per-object old position bound; wall/second crate tests
- `hitchk.c` bhCheckWallEx: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function extracted and push contact/body/front block bound; full bhControlPlayer state dispatch and non-player callers remain open
- `hitchk.c` bhGetGroundPosition: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Generated original query; room-owned htp retains room/mobile index and live ATR work; player.c:1743-1762 bound only where push/stair state exists; player shot caller creates linked bhEff000 with ground/water/wall/audio dispatch, ef_pol state and SYSTEM model/texture draw; EE libc rand/srand and bhRandTest/RandamSet are bound from executable disassembly and bhSetEffectTb first-free eff[512] allocation is mapped; other random consumers, exact EasyLight, full effect lifecycle and general player work remain open
- `objitm.c` bhObj001: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function extracted; mobile walls/side probes/work indices, original speed/box collision tested; auxiliary metc/mflr consumers for crate climbing not fully bound
- `player.c` bhCPM2_act_psh: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original function extracted verbatim (packed mode-byte ABI write adapted); MTN49/50/reverse49, original Hokan and curve9 bound and tested; physical vibration callback not bound
- `ps2_NaSystem.c` njColorBlendingModeSys: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originale alpha_tbl[13][12] + Ninja-Indextabellen generiert; native/chunk_blend.cpp RGB=(A-B)*C+D, normalisierte Alpha-Floats; kein vollständiger GS-Zustands-/Quantisierungsport
- `sub1.c` GetItem: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original GetItem extrahiert inkl. Zweig 153, danach originale ItemSort-Phase; StatusCancel/Sprite-Ein-/Ausfahren und ganzer StatusMain nicht vollständig gebunden
- `sub1.c` ItemBox: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Original ItemBoxChange generiert, Tauschen/Stack/Restriktionen und Security-Raumskript geprüft; Host-Liste statt originaler Fenster/Sprites
- `sub1.c` ItemSort: Historischer Extraktions-/Generatorbeleg ist im nativen Port nicht zulaessig: Originalfunktion extrahiert, pip-ILP32-Zugriff hostgerecht; Standardinventar nach Pickup und Mutationen sortiert/geprüft; Sonder-flgchk des ganzen StatusMain noch unvollständig

Erzeugt mit `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
