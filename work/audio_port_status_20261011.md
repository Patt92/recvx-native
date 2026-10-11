# Nativer Portstatus

Dieser Bericht wertet nur explizit belegte native Mechanismen. Die vollständige Originaldecompilation ist absichtlich weder Nenner noch Fehlermenge.

Expliziter Belegumfang: **80.6 %** gewichtet, **61.2 %** voll belegt (30/49 Mechanismen; kein Gesamtfortschritt).

Event-Befehle (bhScenarioJmpT): 149/223 gebunden, 45 nur konsumiert, 29 fehlen.

| Bereich | Mechanismen | Belegumfang | voll belegt | teilweise | offen |
|---|---:|---:|---:|---:|---:|
| Gegner/KI | 0/1 | 50.0 % | 0.0 % | 1 | 0 |
| Effekte | 1/4 | 62.5 % | 25.0 % | 3 | 0 |
| Events/Skript | 21/24 | 93.8 % | 87.5 % | 3 | 0 |
| Spieler/Kampf | 0/6 | 50.0 % | 0.0 % | 6 | 0 |
| Raum/Kamera/Tür | 1/4 | 62.5 % | 25.0 % | 3 | 0 |
| Menü/Items/Text | 7/8 | 93.8 % | 87.5 % | 1 | 0 |
| Ablauf/Speichern | 0/2 | 50.0 % | 0.0 % | 2 | 0 |
| Plattform (Host-ersetzt) | 0/3 | 50.0 % | 0.0 % | 3 | 0 |

Nur Ledger-Einträge mit eigenständiger nativer Evidenz zählen. Namensgleichheit, Originalquellen und quarantänierte Generatoren zählen nie als Portbeleg.

## Größte offene Funktionen (Audio)


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

Erzeugt mit `python3 tools/port_check.py --markdown work/audio_port_status_20261011.md`.
