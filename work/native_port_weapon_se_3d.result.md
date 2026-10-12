# Waffen-SE-Vertrag und gemeinsamer 3D-Parameterkern

Stand: 2026-10-12. Teilport, nicht Abschluss des gesamten 3D-Audios.

## Originalbelege

- `sdfunc.c:819/847/890`: wadGetAngle, Get3DSoundParameter, SetupSeGenericParm.
- `sdfunc.c:940/1088/1123`: 3D-Flags, Reset und Lautstaerkegates.
- `sdfunc.c:1406/1416/1546`: Syukan, PlayerVoice, CallPlayerWeaponSeEx.
- `sdfunc.c:1708/2301/2371/3002`: EnemyInfo-Anforderungen, Wiedergabe, Manager und gemeinsamer Kamera-/Spieler-Monitor.
- `camera.c:47-57`: cam.wpx = Pivot + RotY/RotX/RotZ * (0,0,-ln).
- `sdfunc.c:824`, `pwksub.c:1891`: Verdeckung mit Radius 0.1, Raumflag2, Raum- und mobile Waende.
- `sdc.c:542/726/778`, `sync.c:26`: SE-Fadezustand, Fortschreibung pro 60-Hz-VSync, Ende erst Count<0, Stop bei -127.
- Original-ELF SLUS_201.84: ThreeDVolTbl 0x32D930, PanTbl360 0x32DB30, PanTbl360Vol 0x32DB80; Clamp bei 0x2936BC. Distanzindex510 und Panindizes68..71 lesen belegte Nullbytes jenseits der deklarierten Datenfelder, keine geratenen Tabellenwerte.

## Native Aenderungen

`audio.cpp/.h` enthalten den gemeinsamen Parameterkern und dieselbe Mixerqueue, keine zweite Enginewelt. SE8/9 alternieren bei SlotNo0, SE19 gilt sonst. Die Hoststimmen30/31/32 sind nur die konfliktfreie Abbildung dieser drei Originalslots. Kampf, bhWeaponSeCall und bhEasySESet reichen die tatsaechliche Mündungs-/owP-Gelenkposition durch. Syukan und Raumlautstaerke bleiben getrennte Originalschritte.

Spielerstimmen verwenden PlayerPos; Gegnerdienste reichen ihre Positionen durch, erhalten VolDownTbl/RoomEnv und den originalen Austauschsortieralgorithmus (Prio> oder Dist>, kein lexikographischer Ersatz). Listener-Snapshot und Verarbeitung der bereits angeforderten Gegner-SE liegen gemeinsam am Monitor vor dem naechsten Gamepass. `RoomWorld::tick` verarbeitet nicht mehr eine neue Queue mit einem alten Snapshot.

`wall_capsule.*` bestimmt bhCheckC2Wall und die benoetigten Ninja-Primitiven eigenstaendig; kein vereinfachter Schussstrahl als Audioverdeckung. Typ0/2/4/7, Flags/attr0x10 und Raum-vor-Mobile-Reihenfolge bleiben erhalten. Keine raumspezifischen Klangregeln.

SE-Lautstaerkezustand lebt unabhaengig vom Playback. Bei 48kHz entspricht ein Sound-VSync800 Samples; Volume/VolSpeed/FadeCntMax werden nur dort fortgeschrieben, nicht interpoliert. Fades auf inaktiven Slots gehen nicht verloren. MIDI-/ADX-Fades wurden nicht als vollstaendig portiert ausgegeben.

## Pruefungen

Belege: `build/validation/weapon-se-20261012`.

- macOS arm64 Release: voller Build; CTest65/66. Audio, Capsule, Camera-Listener und Continuity gruen.
- Ein roter Test: native-gameplay-bindings, `enemy gap changed the original HP`. Derselbe Test im policy-geprueften nativen Vorher-Binary `build/eventgaps-claude-20261011/recvx-gameplay-bindings-tests` scheitert ebenfalls: Hund0 HP60 vor, HP90 nach Tick. Aktueller Lauf: Hund1 HP90 vor, HP60 nach Tick. Keine Testerwartung gelockert, Ursache nicht von dieser Audioarbeit behoben.
- Neue Audio-Pruefungen: Tabellen/Randbytes, Kameraausrichtung, RoomFxLevel, Syukan, 3D-Flags, Slots8/9/19, PlayerVoice-Distanz, Gegnerdistanz/Pan/VolDown, Originalsortierung, CallFlag0-Panupdate, Initqueue, Verdeckung-28, SE-Fade800-Sample-Grenze, Count0 bleibt aktiv und Count<0 stoppt, inaktiver Slot behaelt Fadezustand.
- Sortier-Mutation ohne Distanzzweig wurde rot, nach Wiederherstellung gruen. Keine behauptete vollständige Baseline-Abnahme aller neuen Tests.
- Nativer Runtime-Replay RM_0030: originales Steve-Geschenk, M93R, zwei Schuesse, Munition12->10, Klanganforderungen ARMS/0001/0005/slot8 dann slot9, Leerlaufrueckkehr. Kein blockierter Eventbefehl. Vorhandene Gegner-/Kampfluecken bleiben gemeldet.
- RM_0130 nativer200-Tick-Lauf: originale Fledermaus-SE MULTSPQ1/0017/3/0003, kein unavailable, kein blockierter Befehl; keine neue visuelle/Hoer-Abnahme.
- verify_audio_state: Original-RoomSoundCase0/1 bestanden; Indoor-Regen nicht nachgewiesen behoben.
- Windows x64 Runtime, Audio- und Collisiontests kompiliert/gelinkt. Ninja-Metadaten und Vergleichslink/PE-Policy PASS. Windows nicht ausgefuehrt. Erster Makefiles-Aufbau scheiterte am fehlenden Ninja-Linknachweis; neuer eigener Ninja-Build ohne Bypass bestand. Audit verwendet projektlokalen Zigcache.
- Quellen-, macOS-Build-/Runtime- und Windows-Binary-Policy PASS; git diff --check sauber.

## Offene Grenzen

Der Auftrag ist insgesamt **noch nicht vollstaendig abgeschlossen**. Die gemeinsame Berechnung ist fuer Waffen, PlayerVoice und Gegner angeschlossen, aber Schritt-/Aktions-/Hülsenklänge, Objekt-SE, räumliche ADX-Aufrufe und MIDI-Pan bleiben nicht vollständig portiert. EN11 bhEne_SetSEPan besitzt einen portierten Flag0-Mixerpfad, aber seine fehlende komplette Zusatzdienstbindung bleibt offen. Die geschlossene IOP-Pan-/Pegelsynthese ist ungeklärt; vorhandene lineare Ausgabeskalierung ist kein Originalnachweis. Pitch/ADSR/Hall und weitere Syntheseteile bleiben offen. Asynchrone SPQ-Transportflags sind nicht vollständig nachgebildet; native Waffenbanken laden synchron.

VolDownTbl-Indizes8..15 sind kein bekannter Originalwert und werden gemeldet statt erfunden. Keine Gesamt-Audio-, Hoer-/Hardware-, Durchspiel- oder Windows-Laufzeitbehauptung.

Kein Commit/Push. Keine Starter-App oder Standalone-Pakete ersetzt: deren Austausch braucht seit der Orchestrator-Uebergabe einen ausdruecklichen Auftrag. Start.command bevorzugt aktuell `build/start/macos-arm64/RECVX Native.app`; diese Arbeit liegt in `build/weapon-se-claude-20261012/recvx-runtime`, nicht im Benutzerstarter.

## Fortsetzung 2026-10-12: weitere Verbraucher

Gemeinsamer Zustand (`SoundControl`) für SE/MIDI-Lautstärke und -Pan; ExecSoundFadeManager/ExecSoundPanManager im 60-Hz-Takt. Fuss-/Aktionsklänge (`CallPlayerFootStepSeEx`), `CallYakkyouSe`, `CallEnemySe`, `CallNativeEventSe` (Mode 0/1), Objekt-SE-Verwaltung (Regist/Free/Exec, ObjectInfo[16], 3 Slots, Init-Reset) und Event-Opcodes 0x17/0x79/0x7f/0x86/0x8b/0x45 mit Originalpositionen. Die Aufrufer von `footstep` in combat/world wurden auf PlayerPos bzw. Gelenkposition umgestellt.

Prüfungen: `audio_movie` (neue Fuss-/Pan-/Objekt-SE-Fälle), `core_continuity` (Erwartung auf den originalen VSync-Zeitpunkt des MIDI-Fades umgestellt, nicht abgeschwächt), voller macOS-Build, Windows-Ninja-Build mit PE-Audit PASS (nicht ausgeführt), Runtime-Replays Steve/M93R und RoomSoundCase. Eine Messung zur Kontrolle: CTest nun 65/66, nur `native-gameplay-bindings` (HP-Test, gemessen auch im Vorher-Binary) rot.

Offen: Hülsen-Producer `bhEff000`, SE-Pitch-Synthese, ADX-Objekt-SE und räumliche ADX, EN11 `se_pan`-Bindung, geschlossene IOP-Pegel-/Pan-Kurve, Pitch/ADSR/Hall. Kein Starter-/Paketaustausch, kein Commit.

## Fortsetzung 2 (2026-10-12): Verbraucher, Objekt-SE, ADX-Voice, Huelsen

Zusaetzlich zu oben:

- `CallPlayerFootStepSeEx` mit Original-Slots 11..16/6, Walk-/Run-Pitch aus dem gemeinsamen
  EE-Zufall, PlayerPos (CallPlayerFootStepSe) bzw. Gelenkposition (Event 0x48), Syukan,
  `VolPlayerAction` und Flag 0. Die Aufrufer in `combat.cpp`/`world.cpp` wurden
  entsprechend nachgezogen; der vorherige Signaturbruch ist behoben.
- `CallYakkyouSe` (Waffenbank 1, SE-Slot 10, `-RoomFxLevel/12`, `VolCartridgeSe`).
- Objekt-SE vollstaendig (`RegistObjectSe`/`FreeObjectSe`/`ExecObjectSeManager`) auf
  `ObjectInfo[16]`, drei Slots, Init-Reset, Fade-/Pan-Auftraege, Verdeckung und den
  originalen Schleifengrenzen; Lauf am Monitor nach der Gegner-SE.
  `CallObjectSe2` Typ != 0 ist im Original unerreichbar und nicht nachgebaut.
- Event-Befehle 0x17 (bhSeOn), 0x79 (bhEneSeOn), 0x7f (bhSoundPanSet), 0x86
  (bhEasySESet Typ 0..7), 0x8b/0x45 (bhAddObjSe/bhDelObjSe) mit Originalpositionen und
  Parametern; 0xc4 (bhVoiceWait) neu angebunden.
- `PlayVoiceEx2`/`StopVoice`/`ContinuePlayVoice` auf ADX-Slot 1 (VOICE-Archiv) fuer
  0x19/0x1a/0xc4/0xc5; Pausezustand in der Stimme, ADX-Lautstaerke im 60-Hz-Manager.
- Huelsenproduzent `bhEff000` (Typen 0..3/10). Die Combat-Anforderung filterte vorher auf
  `type==10`; `bhSetYakkyou` nutzt `WpnEffTab[wno][3]` mit id 0 und je Waffe wechselndem
  Typ, `bhSetGunFire` die [wno][0]. Die Verteilung entscheidet jetzt die id.
- EN11 `bhEne_SetSEPan` gebunden; uebrige EN11-Dienste bleiben NULL (benannte Luecken).

Belege: `build/validation/weapon-se-20261012` (combat-final, bats2, audio-state-3,
windows-audio3). 66/66 lauffaehige CTest-Gruppen; `original_map_screen` ist unfertige
Fremdarbeit (map_screen_test.cpp) und baut nicht — nicht angefasst. Zwei Erwartungen
wurden auf Originalverhalten korrigiert statt abgeschwaecht: der MIDI-Fade wirkt erst am
naechsten Sound-VSync, und `PlayVoiceEx2` stoppt eine spielende Stimme auch in Modus 2.
Die Huelsen-Luecke in `effect_debris_test.cpp` wurde auf den neuen Stand gebracht
(bhEff000 ist gebunden, geprueft in `effect_room_sprites`).

Windows x64 Runtime, audio-tests und effect-room-tests kompiliert/gelinkt, PE-Audit PASS,
nicht ausgefuehrt. Kein Starter-/Paketaustausch, kein Commit.

Weiter offen: SE-Pitch-Synthese (`sdShotSetPitch` transportiert nur), geschlossene
IOP-Pegel-/Pan-Kurve, BGM-Fades (weiterhin Host-Samplerampe), ADSR/Hall, komprimiertes SQ,
uebrige nicht portierte Effektprozeduren.

## Fortsetzung 3 (2026-10-12): BGM/ADX-Fades, Sony-Pitch/-Pan, komprimiertes SQ

- ADX-/BGM-Fades auf Originalzustand: `RequestAdxFadeFunction2/Ex`, `SetVolumeAdx/Ex`,
  `PlayBgmEx2`, `Com_ExecRoomFadeIn/Out`, `RequestSoundFade`, Fortschreibung im 60-Hz-Manager.
  Die Host-Samplerampe entfaellt. `ADXT_Pause` ist im ELF leer — keine native Pause.
- Sony-Pitch (`audio_pitch.*`, Tabellen aus LIBSD.IRX 0x4680/0x4698, Routine 0x33a0) in der Mixerschleife;
  Walk-/Run-Pitch der Schritte ist damit hoerbar.
- Sony-Integer-Pan (`audio_pan.*`) fuer Bank-/Samplepan.
- Komprimiertes SQ (`audio_sq.*`) aus dem MODMIDI-Woerterbuch (nur Tokens A0..AF); Test `sony_sq_dictionary`.

Pruefung: Policy-Check PASS (Quellen, Build, Binary macOS und Windows); macOS arm64: 67/67 CTest
(ohne `original_map_screen`), `verify_combat --runtime-only` und `verify_audio_state` bestanden;
Windows x64 Ninja kompiliert/gelinkt, nicht ausgefuehrt. Logs: `build/validation/audio-synthesis-20261012`.
Frueherer HP-Rotstand (1 von 66) tritt nicht mehr auf.

SE-Note: Programmkopf-Byte +0xB der HD-Datei (TSNDDRV 0x10D4), Velocity 127; Byte ist in allen Banken 60,
nativ jetzt aus der Bank gelesen (Teilstrecke tsndReqFunc->Tsnd_tqreq nicht nachverfolgt).

Weiter offen (nicht belegt, nicht ersetzt): geschlossene
IOP-Pegelkurve, ADSR/Hall, phaseninvertiertes Programm-Pan, SQ-SysEx, `ss_flg & 0x4000000`-Gate,
Live-Controller-Modulation. Kein Starter-/Paketaustausch, kein Commit.
