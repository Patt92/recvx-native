# Ausstehende Event-Lücken — Quellenabgleich 2026-10-11

## Umfang und Nachweisgrenzen

Rechercheauftrag: ausdrücklich benannte, noch nicht portierte und in Work Items erwähnte Event-Lücken finden. Keine Implementierung, kein Build, kein neuer Runtime-/205-Raum-Lauf, kein Packaging, kein Commit/Push. Vorhandene lokale Änderungen bleiben erhalten.

AGENTS.md vollständig gelesen; `python3 tools/check_no_original_core.py` bestand vor der Recherche. Originalquellen außerhalb der Quarantäne wurden ausschließlich als Semantikreferenz verwendet. Keine Unteragenten.

Verglichen wurden der aktuelle Arbeitsbaum, Original-Dispatcher/ausgewählte Originalzweige, Roadmap, Übergabe und Paketberichte. Der gespeicherte Scan `build/validation/room-effects-20261011/scan.json` wurde ausgewertet, nicht neu ausgeführt. Er stammt aus einem früheren Binary und beweist nicht, dass später geänderter Arbeitsbaum identisch läuft.

**Gespeicherter Scan:** 205 Räume, 0 fehlerhafte Prozessenden, 0 blockierte Befehle; **130 Räume mit pending_commands, 28 unterschiedliche Opcodewerte**. Eingabefreier Scan: Untergrenze, kein Durchspielnachweis. Einzelne Pending-Einträge können Daten-/Ressourcenprobleme oder inzwischen geänderte Anschlüsse sein, nicht automatisch fehlende Routinen.

`EventVm::world` (native/events.cpp:42–47) schaltet auch bei fehlender Wirkung weiter und protokolliert pending. Daher sind „kein Blocker“ und „vollständig portiert“ ausdrücklich verschieden. Direkte VM-Anforderungsflags wie 0x89/0xa0 erzeugen darüber hinaus keinen pending-Eintrag, obwohl ihr Verbraucher fehlt.

## 1. Vorrangige, aktuell bestätigte Anschluss-/Semantiklücken

| Mechanismus | Aktueller Befund | Beleg / Zuordnung |
|---|---|---|
| **Spielerwechsel 0x89 — bhPlayerChangeSet** | VM setzt change_player und cb_flg 0x80; im nativen Baum kein Leser von change_player. Der originale Wechsel-/Neuladeablauf konsumiert diese Anforderung. CLI-/Debug-Spielerwahl ist kein Eventanschluss. | native/events.cpp:256, native/events.h:22; Original system.c:1725–1820. G02 / Player-Lifecycle. |
| **Event-Karte 0xa0 — bhMapSystemOn** | IMPLEMENTIERT (2026-10-11): VM setzt map_parameters und cb_flg 0x10000; Runtime konsumiert Anforderung mit MP_SET_EVENT, setzt/löscht st_flg 0x40000 / gm_flg 0x80000, leitet map_parameters als mp_prm weiter, korrigiert enum-Reihenfolge (MP_SET_GAME=0, MP_SET_EVENT=1, MP_SET_SUB=2) und kehrt ohne Statusmenü zur Gameplay-/Eventebene zurück. | native/events.cpp:258; native/runtime.cpp:961,1058; native/map_tables.h:47; Original system.c:783–790, 824. D10/G02. |
| **CommonCtr 0x69 / Unterbefehl 0x08** | Operandengenau geparst, aber kein Bindungszweig. Original schreibt Position auf den ausgewählten Modellknoten. | native/events.cpp:287; native/world.cpp:558–639; Original event.c:12099–12134. Im gespeicherten Scan 11 Räume, u. a. RM_4010. F01/G02. |
| **CommonCtr 0x69 / 0x29 (dezimal 41)** | Bindung gibt ausdrücklich false zurück. Original setzt/löscht Work-flg 0x40000, nicht allgemeine Sichtbarkeit. | native/world.cpp:636; Original event.c:10346–10356. Scan: 41 Räume. Gemeinsamer Work-/Model-Link-Vertrag. |
| **CommonCtr 0x69 / 0x2a (dezimal 42)** | Geparst, kein Bindungszweig; Original verändert owP[node].flg 2. | native/events.cpp:293; native/world.cpp:558–639; Original event.c:10324–10344. F01 / Modelldraw-Gates. |
| **PlayerCtr 0x64** | Parser kennt nur Teilmenge; Bindung implementiert davon nur 7/0x80/0x34. 0x83 ist z. B. parsed, aber ungebunden, im Scan RM_7010. | native/events.cpp:228; native/world.cpp:539. Player-/Event-Lifecycle. |
| **SubCtr 0x67** | Parser erkennt alle neun originalen äußeren Unterbefehle; Bindung ist dagegen besitzerabhängig unvollständig. Für native Gegner ohne en01/subplayer-Zweig fehlen u. a. 0x8f/0x92/0x93; 0x94/0x95 haben keinen Wirkungszweig. Original: 0x94 EN11-Lichtsteuerung, 0x95 flg 0x10000. | native/events.cpp:229; native/world.cpp:540–557; Original event.c:9804–9944. C02/C05/C07/E04. |
| **SubCtr Release-/State-Semantik** | Für Subplayer löschen 0x80/0x90 nicht die originalen Bits stflg 0x10000/flg 0x200000 und setzen nicht beide Task-Modi zurück. 0x93 wird zusammen mit Releasefällen behandelt und nullt ct2; im Original 0x93 werden nur vier Modi und type geschrieben, ct2 bleibt stehen. | native/world.cpp:544–546; Original event.c:9762–9802,9889–9910; archived/work/native_port_T5_subplayer.result.md:297–324. C05. |
| **Common-Motion/Subplayer-Interpolation** | Bei übernommenen Common-Motionzweigen fehlt hokan_count=t1 für t1>8. mtn_md Bit 3 wird aus dem Geschwindigkeitsbyte statt aus dem dafür vorgesehenen Zweig/Taskzustand abgeleitet. | native/world.cpp:618–630; Original event.c:11001–11065; T5-Bericht P7. C05/F01. |
| **Zombie-Aufsteh-/Totprüfung 0x8e** | Parsed, kein Bindungszweig. Original prüft EN01-exp1-Bit 2 und löscht die angegebene Gruppe-4-Flag. Nicht bloß ein Grafikeffekt. | native/events.cpp:192; Original event.c:6438–6462. Scan: 5 Räume, u. a. RM_0070. C02/C03/Eventzustand. |
| **Gesichts-Wiederholung 0x9a — bhFaceRep** | Gebunden. `face_flags` Bit 2 wird unter 0x9a gesetzt/gelöscht; 0x91 bleibt bhEffModeSet und wird weiterhin vom Effektzweig abgefangen. | native/world.cpp; Original event.c:6567–6595. Scan 0x9a: 9 Räume. F04 erledigt. |
| **Event-Morph 0x4f/0x55/0x84/0x85** | Gebunden: 0x4f wählt obj_a/obj_b (mbp-Indizes), 0x55/0x85/0x84 setzen Zähler und Gate; der Draw verbraucht den Zustand über npCalcMorphing/npTransform für alle Actor-Besitzer. Die portierten Gegner-Zustandsmaschinen wählen dasselbe Paar über ihre Dienste (en01 select_morph_pair, en12/en14/en30 morph_sources/morph_models, en18 model_root/select_root/morph_pair, en30 Knotendienste). en14s Morphzweig und en18s Rootpaar hängen zusätzlich an noch fehlenden Erzeugern (EN13-Elternteil bzw. EN18-Roots). | native/morph.cpp; native/world.cpp; native/world_enemies.cpp; Original event.c:4541–4600,6040–6115, njplus.c:677–870, en01.c:2749, en12.c:800, en14.c:457, en18.c:425–427, en30.c:176. F03. |
| **Script-Waffeneffekt 0x49 — bhWeaponSeCall** | Spielt nur den Ton, gibt false zurück; Skript-Mündungsfeuer/bhSetGunFire nicht angeschlossen. Vorhandenes CombatController-Mündungsfeuer ersetzt diesen Eventpfad nicht. | native/world.cpp:536–538; native/combat.cpp:187 ff. A0x/E03/F01. |

### Noch fehlende Controller-Parserfälle

Mit den äußeren Original-switch-Fällen verglichen; verschachtelte Motion-/Modus-switches nicht mitgezählt:

- **Player_controll:** 0x81, 0x82, 0x84, 0x8c, 0x8d, 0x8e, 0x8f, 0x91, 0x96.
- **Common_controll:** 0x00, 0x0a, 0x16, 0x23, 0x2b, 0x33, 0x34, 0x35.
- **Obj_controll:** beide Originalfälle 0x01/0x14 vorhanden; kein fehlender äußerer Parserfall festgestellt.
- **Sub_controll:** alle neun Originalfälle parsed; fehlende Wirkung/Seiteneffekte bleiben separat offen.

Belege: native/events.cpp:101,228–229,287–293; Original event.c:9271–12706. Ein fehlender Controllerfall blockiert bei Erreichen; der gespeicherte eingabefreie Scan hat diese fehlenden Fälle nicht erreicht.

## 2. Weitere parsed, aber ungebundene Wirkungsgruppen

Diese Befehle werden weitergeschaltet; in RoomWorld::bind und der Runtime-Bindung fehlt ihre Wirkung ganz oder teilweise.

| Gruppe | Befehle | Work-/Abhängigkeit |
|---|---|---|
| Kamera/Motion | 0x29 CamPauseSet, 0x5a CamYureSet; 0x2d InitMotionPause, 0x2e MotionPauseSetPly, 0x30 InitMotionPauseEx | G03/P04/F01. Clip-Installationsfix ist bereits vorhanden und schließt diese Mechanismen nicht. |
| Schatten | 0x2f InitSetKage, 0x7e KageSwSet | F05. Scan: 51 bzw. 8 Räume. |
| Objekt-/Itemsicht | 0x42 ItmAspdSet, 0x6c ObjAlphaSet | Objekt-/Itemdispatcher und Zeichenbesitzer. Nicht mit bereits gebundenem ObjAspdSet 0x9d verwechseln. |
| Nebel | 0x4c FogColorSet | G03/Renderer. Scan: 12 Räume. |
| Objekt-/Raumaudio | 0x8b AddObjSe, 0x45 DelObjSe; 0x46 NextRoomBgm, 0x47 NextRoomBgSe; 0x76 3dSoundFlagSet, 0x7f SoundPanSet, 0x87 SoundFlagReSet | E04/Audio/G02. EasySESet 0x86 bindet nur Typen 3/5/6/7; Scan zeigt auch ungebundenen Typ 1. |
| Effektanforderung/-lebenszyklus | 0x4a YakkyouSet, 0x6d CyodanSet, 0x6e HEffectSet, 0x58 BloodPoolSet2, 0xca EffClearEvt | E02/E03. 0xca muss originale selektive Freigabe aus effect.c:159 ff. portieren, nicht pauschal sämtliche Effekte löschen. |
| Effektparameter | 0x88 EffUVSet, 0xd1 EffSSizeSet; 0x91/0xad/0xd2 nur für supported_effect_sprite gebunden | E03/F01. Native Liste: IDs 9,122,123,125,126,181,182,218,230. Vorhandene Parameterbindung ist kein vollständiger Port aller Effektarten. |
| Weitere System-/Player-Wirkungen | 0x3a MutekiSetPl, 0xa1 TrapDamageSet, 0xce SyukanModeSet, 0xcf ExGameItemInit, 0xc5 VoiceStart, 0x80 InitPonySet | Player-/Audio-/Objektbesitzer; vorhandene Haarphysik schließt InitPonySet nicht automatisch. |
| Controller-Vibration | 0x9e PuruPuruFlagSet, 0x9f PuruPuruStart | Plattformanpassung mit originaler Semantik; kein Hardwaretest in dieser Recherche. |

Belege: native/events.cpp:94–100,190–220,259–265; native/world.cpp:404–639; native/runtime.cpp:355–361; native/effect_sprites.cpp:290–294.

Zusätzliche bestätigte offene Work-Grenzen, nicht als neue fehlende Opcodeparser gezählt:
- F04a: berechnete tooth/tang/eye-Winkel auf echte Skelettknoten übernehmen. FaceAsset::deform verändert bisher lokale Vertices (native/face.cpp:68–72).
- F04b: Blickrichtung durch eyemat transformieren; native/face.cpp:72 verwendet param.eye direkt.
- E03: Effekt-Scheduler führt neu erzeugte Arbeiten nicht nach originaler Index-/Aufrufreihenfolge im selben Pass aus (archived/work/native_port_E03b_room_effects.result.md:131–134).
- G03/P04: voller Kameraautomat und Eff137-abhängige Monitorabläufe nicht vollständig portiert (archived/work/native_port_P04_camera_invocations.result.md:62–71).

## 3. 29 Original-Opcodes ohne EventVm-Parserfall

Aktueller statischer Abgleich der 223 nicht-dm-Einträge von bhScenarioJmpT. Alle unten genannten Originalroutinen haben tatsächlich Verhalten; kein bhSoundFadeOut-artiger Original-No-op wurde hier als fehlende Wirkung aufgenommen.

| Gruppe | Opcode / Originalname |
|---|---|
| Audio/Zeit/Voice | 0x1e bhAdxTimeCk; 0xc4 bhVoiceWait |
| Allgemeine Effekte | 0x2c bhEffectSet; 0x70 bhEffPush; 0x71 bhEffPop; 0xb0 bhHEffectSet2 |
| Blut/Treffer/Explosion/Sand | 0x4e bhEffBloodSet; 0x56 bhEffBloodPoolSet; 0x96 bhEffectSensyaSet; 0x97 bhEffectKokuenSet; 0x98 bhEffectSandSet; 0xc0 bhCyodanSetEx; 0xc3 bhEffectSandSetMatsumoto; 0xc8 bhEffBakuDrmSet |
| Effekttextur | 0xaa bhEffUVSet2 |
| Licht/Nebel | 0x73 bhLightParameterCSet; 0x74 bhLightParameterStart; 0xa8 bhFogParameterCSet; 0xa9 bhFogParameterStart |
| Player/Modell/Verknüpfung | 0x40 bhLookGsetPlStart; 0x41 bhLookGsetPlStop; 0x6f bhObjLinkSetObjPly; 0xaf bhPlyHandChange; 0xb1 bhObjDposCk; 0xb5 bhLoadWorkEx |
| Dynamische Interaktionsbereiche | 0xb3 bhEtcAtariEnePosSet; 0xb4 bhEtcAtariEvtPosSet |
| Kamera/Debug/Endablauf | 0x82 bhSetDebugLoopEx; 0xd3 bhRankingCall |

Belege: Original src/ps2/veronica/prog/event.c, bhScenarioJmpT und jeweilige Definition; native/events.cpp:49–295. Erreichen eines solchen Befehls landet im Exception-/Blocked-Pfad (native/events.cpp:294–299). Im gespeicherten Scan nicht erreicht heißt nicht in Originaldaten generell unbenutzt.

## 4. Gespeicherte pending-Wirkungen als Diagnosepriorisierung

Anzahl verschiedener Räume, keine Zahl natürlicher Storyabläufe. Namen aus der Original-Dispatchtabelle, nicht aus historischen Berichten:

| Opcode | Name | Räume |
|---|---|---:|
| 0x23 | bhItmSetCk | 3 |
| 0x29 | bhCamPauseSet | 9 |
| 0x2d | bhInitMotionPause | 3 |
| 0x2f | bhInitSetKage | 51 |
| 0x30 | bhInitMotionPauseEx | 1 |
| 0x42 | bhItmAspdSet | 7 |
| 0x45 | bhDelObjSe | 3 |
| 0x4c | bhFogColorSet | 12 |
| 0x4f | bhCyoutenHenkeiSet | 4 |
| 0x58 | bhEffBloodPoolSet2 | 1 |
| 0x5a | bhCamYureSet | 9 |
| 0x64 | bhPlCtr | 1 |
| 0x67 | bhSubCtr | 16 |
| 0x69 | bhCommonCtr | 49 |
| 0x6c | bhObjAlphaSet | 2 |
| 0x7e | bhKageSwSet | 8 |
| 0x80 | bhInitPonySet | 4 |
| 0x86 | bhEasySESet | 2 |
| 0x88 | bhEffUVSet | 4 |
| 0x8b | bhAddObjSe | 29 |
| 0x8e | bhZombieUpDieCk | 5 |
| 0x91 | bhEffModeSet | 26 |
| 0x9a | bhFaceRep | 9 |
| 0x9e | bhPuruPuruFlagSet | 8 |
| 0x9f | bhPuruPuruStart | 8 |
| 0xad | bhEffTypeSet | 18 |
| 0xd1 | bhEffSSizeSet | 3 |
| 0xd2 | bhEffLinkOffsetSet | 2 |

0x23 ist ein Sonderbefund zur Reichweite der ETC-Raumtabelle; das bestehende Itemverhalten wird ausgeführt, während der ATR-Zugriff als pending gemeldet wird. Kein Beleg, dass bhItmSetCk vollständig fehlt. Native Stelle: events.cpp im 0x23-Zweig.

## 5. Historische Offenmeldungen, die nicht erneut beauftragt werden sollten

- **D06 Inventarmutationen:** sieben Befehle sind produktiv an Inventory gebunden. Der D06-Anschluss-Patch ist erledigt, siehe archived/work/native_port_inventory_tests.result.md:38–43.
- **Computerterminal einschließlich Figur:** historische „fehlt“-Notiz durch D04-Neuimplementierung überholt. Keine neue Stilllegung oder Neuentwicklung.
- **Gameover/ReturnTitle/RetryPoint:** aktuelle Runtime-Bindungen für 0xc6/0xcd/0xbd existieren (runtime.cpp:358–360). Das beweist nicht sämtliche natürlichen Todesszenen, aber diese Eventanschlüsse fehlen nicht.
- **bhEneRenderSet 0xa5:** wirkt auf WorldEntity.hidden (world.cpp:405–407). Der pending-Kommentar in events.cpp:261 ist veraltet.
- **D10 Karte:** Modul und MP_SET_SUB-Anschluss existieren im aktuellen Arbeitsbaum. Die Aussage „Nichts produktiv angeschlossen“ im früheren D10-Ergebnis ist überholt. Offen bleibt konkret der Event-/Gameplay-Einstieg, nicht ein komplett fehlender Kartenbildschirm.
- **C05:** Common 0x84/0x8c/0x8d und native Subplayer-Work sind inzwischen im Arbeitsbaum angeschlossen (world.cpp:560–588). Nicht pauschal den ganzen Subplayer-Port als leer melden; die oben benannten State-/Release-/Motionlücken bleiben.
- **Clip/Gouraud/Lighter/Raumeffekte:** aktuelle Portierungen erhalten; nicht aus älteren Übergabelisten erneut als komplett fehlend übernehmen.
- **bhEneLookFlgSet 0xcc:** der alte pending-Kommentar alleine reicht nicht; world_enemies.cpp:275–290 übernimmt e.look inzwischen in native Work-Flags.
- **bhSoundFadeOut 0x83:** Original überspringt nur Operanden; kein fehlender Fade allein aus dem No-op ableiten (events.cpp:255).

## 6. Empfehlung für die nächste Fixreihenfolge

1. Gemeinsame Controllerverträge schließen: Common 0x08/0x29/0x2a, SubCtr am nativen Gegnerbesitzer, Releaseflags und Interpolation (C02/C05/F01).
2. Story-/Systemanforderungen konsumieren: Spielerwechsel 0x89, Event-Karte 0xa0, Zombie-Flagprüfung 0x8e.
3. Gesichts-Wiederholung 0x9a korrekt binden; dann F04a/F04b und F03-Morph gemeinsam mit Pose-/Zeichenbesitzer.
4. Kamera-/Motionpause, Schatten, Objekt-SE/-Alpha, Nebel und Effektparameter nach ihren jeweiligen Besitzern portieren.
5. 29 fehlende Parserbefehle mit vollständiger Wirkung schließen, nicht nur Operandenvorschub hinzufügen.

Das ist eine Recherchepriorisierung, keine Nutzerfreigabe zur Implementierung und keine vollständige Event-Semantikabnahme. Statische Werte aus port_check (149 als „opcode“, 45 als „pending“, 29 als „fehlt“) sind nicht als 149 vollständig funktionierende Befehle zu interpretieren: der Prüfer erkennt weder alle map-basierten Bindungen noch fehlende Unterbefehle, Feldverbraucher oder sämtliche Seiteneffekte.
