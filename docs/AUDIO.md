# Audio und Filme

Die Laufzeit braucht nur SDL2 und die vorbereiteten losen Dateien. Sie ruft
kein FFmpeg auf, öffnet keine ISO/AFS-Container und emuliert keine
PS2-Soundhardware. Alle Formate werden nativ dekodiert und in einem
48-kHz-Stereo-Mixer zusammengeführt, der im 30-Hz-Spieltakt läuft
(1600 Frames pro Tick).

## ADX-Musik und -Stimmen

- Dekoder für unverschlüsseltes ADX (Codierung 3, Version 3, 18-Byte-Blöcke,
  Mono/Stereo) mit eingebetteten Schleifen. Andere Varianten werden
  abgelehnt statt geraten.
- BGM und VOICE kommen aus `data/archives/<archiv>/<id>.bin` des aktuellen
  Disc-Satzes; Ersatz unter `mods/audio/<archiv>/<id>.adx`.
- BGM-Steuerung nach `sdfunc.c`: `bhBgmOn`/`bhBgmOnEx` starten sofort,
  `bhBgmOn2`/`bhBgmOff2` fordern einen Wechsel an, den der Türablauf
  (SoundCommand 2) und `Com_ExecRoomFadeIn` ausführen. Gleiche Titel starten
  nicht neu. Lautstärken über `AdxVolTbl`, Fades über
  `RequestAdxFadeFunction2`.
- Ressourcen ohne Schleifenmarke enden natürlich.

`recvx-audio <eingabe.adx> <neu.wav>` exportiert PCM zum Vergleich.

## Sony-Soundbanken

- `COMMON.MLT` (Systemklänge) und die Raumpakete aus `MULTSPQ1/2` werden als
  Sony-HD/BD gelesen (Prog → Sset → Smpl → Vagi) und als SPU-ADPCM dekodiert.
- Explizite ADPCM-Loopadressen werden als PCM-Framegrenzen erhalten: Start
  vor dem Startblock, Ende hinter dem End-Repeat-Block. Das Sony-Vagi-Attribut
  (+6) unterscheidet One-Shot (0) und Loop (1). Repeat ohne belegte Startadresse
  sowie unbekannte Attribute werden gemeldet, nicht als Vollschleife geraten.
- SQ-Noten wiederholen nur ihren markierten Samplebereich, unabhängig vom
  äußeren Slot-/BG-Loopbit und den NRPN-Transportloops. NoteOff, Stop, Fade
  und Raumersetzung geben die Note frei. Der vorhandene Decoder behandelt
  Terminalflag 7 als stummen Block; dessen Selbstloop wiederholt keinen Vorlauf.
  PCM wird weiterhin einmal vorab dekodiert: Predictor-Historie über den
  Looprücksprung und bitgenaue SPU-Synthese sind damit nicht nachgewiesen.
- Raumpakete: Schlüssel `stage * 1000 + room * 10 + case`; die Reihenfolge
  Event_init → `RoomSoundCase` → `RequestRoomSoundBank` folgt `system.c`.
- Systemklänge über `CallSystemSe`, Spielerstimmen über
  `RequestPlayerVoiceSoundBank` (Bank 4), Waffen über
  `RequestArmsSoundBank`, Gegner über `RequestEnemySeBasic`/
  `ExecEnemySeManager` (sechs Stimmen, Priorität, Gleich-SE-Grenze).
- Schritte nach `PlFootSnd`/`FootSeCall` mit Bodenmaterial aus den ATR-Flächen.
- Türklänge aus der Türsoundbank (`0x8000 | bank`) mit den Statusanforderungen
  aus `door.c`; die Schließphase folgt `FadeProc1`.
- Ambient/BG-Klänge über `bhBGSeOn/Off`, `bhBGSeOn2/Off2` und
  `CheckSpecialBank`; `bhSoundVolumeSet` wirkt auf alle Klanggruppen.

Fehlende Programme werden als `audio_unavailable` bzw. `sequence_unavailable`
gemeldet; es werden keine Ersatzklänge eingesetzt. Ersatzbank:
`mods/audio/system/COMMON.MLT`, Ersatzpaket:
`mods/audio/MULTSPQ1/<paket>.bin`.

## Räumliche SE-Parameter

`AudioMixer::sound_parameter_3d` bestimmt Distanz, Kamerawinkel, Pan und
Lautstärke nach `Get3DSoundParameter`. Die drei Originaltabellen einschließlich
der belegten Randbytes stammen aus den Originaldaten. Der Listener verwendet
`cam.wpx/wpy/wpz` (Kamerapivot plus gedrehtes `-ln`), nicht den Pivot selbst.
Die Monitorgrenze übernimmt Kamera-/Spielerposition und verarbeitet danach die
Gegner-SE-Queue; neue Anforderungen verbleiben bis zum nächsten Monitor.

- `CallPlayerWeaponSeEx`: SlotNo 0 alterniert zwischen SE 8/9; jeder andere
  SlotNo verwendet SE 19. Kampf und Eventaufrufe übergeben die tatsächliche
  Mündungs-/Gelenkposition. Syukan und `VolWeaponSe` bleiben getrennte Schritte.
- Spielerstimmen nutzen PlayerPos. Gegneranforderungen erhalten ihre Position,
  `VolEnemySe`, `VolDownTbl` und den originalen Austauschsortieralgorithmus.
  Verdeckung folgt `CheckCollision4Sound` mit der 0,1-Kapsel, Raumflag 2 und
  `bhCheckC2Wall` über Raum- und mobile Wände; Treffer vermindern den Wert um 28.
- SE-Lautstärkefades halten Zustand unabhängig vom Playback. Sie laufen in
  60-Hz-Schritten und enden erst bei negativem Zähler. MIDI-/ADX-Fades sind
  davon getrennt und noch nicht vollständig originalgetreu.

- Schritt-/Aktionsklänge (`CallPlayerFootStepSeEx`): SE-Slots 11..16 alternierend
  beziehungsweise 6, Walk-/Run-Pitch über den gemeinsamen EE-Zufall, PlayerPos
  beziehungsweise Gelenkposition, Syukan, `VolPlayerAction` und Flag 0.
  `CallYakkyouSe` (Slot 10, `-RoomFxLevel/12`, `VolCartridgeSe`) ist im Mixer
  vorhanden; der Producer `bhEff000` fehlt nativ noch.
- Objekt-SE (`RegistObjectSe`/`FreeObjectSe`/`ExecObjectSeManager`): Anforderungsliste,
  Slotsuche, Raumreset bei Init, Fade-/Pan-Aufträge, Verdeckung; läuft nach der
  Gegner-SE am Monitor. Die originalen Schleifengrenzen (`MaxSlotObjectSe` statt
  `MaxObjectReqList`) bleiben erhalten.
- Event-Befehle `bhSeOn`, `bhEneSeOn`, `bhSoundPanSet`, `bhAddObjSe`, `bhDelObjSe` und
  `bhEasySESet` Typ 0..7 reichen ihre Quellposition/Parameter durch;
  MIDI-Lautstärke/-Pan laufen im 60-Hz-Fade-/Pan-Manager.

- `bhVoiceOn`/`bhVoiceWait`/`bhVoiceOff`/`bhVoiceStart` (`PlayVoiceEx2`, ADX-Slot 1,
  VOICE-Archiv): Modus 0 räumlicher Start, 1 zentrierter Fade-in, 2 reine
  Parameterübernahme; `VoiceWait` startet pausiert, `VoiceStart` setzt fort.
  Ein spielender Voice wird vor dem Switch gestoppt, wie im Original.
- `bhEneSeOn` (Objektindex/Knoten aus dem Gegnerwork) und `bhSeOn`
  (Spieler/Gegner/Objekt über `owP[0]`) reichen Position und Modus durch.
- Hülsenarbeit `bhEff000` (Patronenhülse, Typen 0..3 und 10) mit Startwerten aus
  dem gemeinsamen EE-Zufall, Boden-/Wandabprall, Wasserübergang und
  `CallYakkyouSe` auf SE-Slot 10.
- EN11 `bhEne_SetSEPan` als Flag-0-Anforderung ohne neuen Klang.

BGM-/ADX-Fades laufen nicht mehr als Host-Samplerampe: `RequestAdxFadeFunction2`/`Ex`
(Timer→Zählerformel, 0→1, Zähler<0 beendet, Stopp bei int(Volume)==-127),
`SetVolumeAdx`/`Ex`, `PlayBgmEx2`, `Com_ExecRoomFadeIn/Out` und
`RequestSoundFade` schreiben den gemeinsamen Zustand, `ExecSoundFadeManager`/
`ExecSoundPanManager`/`ExecAdxFadeManager` schalten ihn bei 60 Hz weiter
(nativ alle 800 Ausgabeframes). `ADXT_Pause` ist im Original-ELF leer
(`jr ra; nop`), eine native Pause gibt es daher nicht.

Offen: die geschlossene IOP-Pegelkurve (`se_volume_gain` ist eine lineare
Hostskalierung), ADSR/Hall, das Gate `ss_flg & 0x4000000` (kein nativer Besitzer
des Flags).

SE-Note: `Tsnd_tqreq` legt die Note aus dem Programmkopf-Byte +0xB der HD-Datei ab (TSNDDRV 0x10D4,
statisch belegt), Velocity ist das Literal 127. Dieses Byte ist in COMMON, Waffenbank und allen
Raumbanken 60; `decode_system_sound` liest es jetzt aus der Bank statt eine Konstante anzunehmen.

Der gemeinsame Parameterkern ist kein Abschluss aller 3D-Verbraucher:
Schritt-/Aktions-/Hülsenklänge, Objekt-SE und räumliche ADX-Aufrufe sind noch
nicht vollständig angeschlossen. Die geschlossene IOP-Pan-/Pegelsynthese bleibt
ungeklärt; die bisherige native Ausgabeskalierung ist kein bitgenauer Nachweis.

## SPU2-Hüllkurve und Hall

Die SPU2-Stimmenhüllkurve (ADSR) und der Hardware-Hall laufen plattformunabhängig
im 48-kHz-Mixer (`native/audio_spu.*`, `native/spu_reverb_presets.inc`):

- **ADSR-Hüllkurve (`SpuEnvelope`)**:
  - `adsr1` (+0x12) und `adsr2` (+0x14) werden beim Dekodieren der Sony-HD-Sample-Einträge
    gelesen (`decode_system_sound`).
  - Phasen: Attack (linear oder exponentiell bis 0x7FFF), Decay (exponentiell bis Sustain-Level
    `((adsr1 & 0xF) + 1) << 11`), Sustain (linear/exponentiell steigend/fallend oder stehend bei
    Rate 0x7F), Release (exponentiell bis 0).
  - Schritt- und Zählerarithmetik mit 48 kHz (Mixertakt) nach psx-spx und MODHSYN.IRX;
    Multiplikation `sample * ENVX >> 15`.
  - Stoppen über `sdShotStop`/`sdMidiStop` sendet CC 120 (`All Sound Off`, `ADSR2 = 0xC021`)
    für schnellen Release-Abfall; Stimmen klingen in einer Auskling-Queue (`releasing_`)
    über das SPU2-Release natürlich aus.

- **Dual-Core SPU2-Hall (`SpuReverb`)**:
  - Hardware-DSP-Emulation für beide SPU2-Kerne (Core 0 und Core 1, je 24 Stimmen).
  - Dynamische Kernzuteilung nach `MODHSYN.IRX` (0x64f8): `attr & 0x30 == 0x10` wählt Kern 0,
    `0x20` Kern 1, sonst der Kern mit mehr freien Stimmen (Gleichstand Kern 0).
  - Reverb-Send-Routing nach `sampleSpuAttr` (+0x29): Wet-Signale VMIXEL (Bit 2) und VMIXER (Bit 3)
    werden in den Effektringpuffer des zugewiesenen Kerns gemischt.
  - 10 LIBSD.IRX-Hardware-Presets (`.data+0x38`/`.data+0x64`), Mode 5 (HALL) in Raumpaketen.
  - 39-Tap-FIR-Resampling zwischen 48 kHz und 24 kHz mit abwechselnder linker/rechter Phase,
    Wandreflexion (`vWALL`, `vIIR`), vier Kammfilter (`vCOMB1..4`) und zwei Allpassfilter (`vAPF1..2`).
  - Pegelrampe (`RoomFxLevel`): Typ-5-Paketblock setzt 16-VSync-Rampe (16 * 1/60s)
    `CurrentFxLevel += (target - current) / 16` bis `RoomFxLevel * 256`, skaliert EVOLL/EVOLR.

## SQ-Sequenzen

Der Sequenz-Transport liest Sony-SQ-Daten (unkomprimiert und komprimiert) vollständig: PPQN,
Tempo, Running Status, verkürzte NoteOffs, Notenzeitpunkte und Polyphonie,
Programm/Key/Velocity, CC7/CC11/Pan sowie die NRPN-Schleifen
CC99/6/38. MIDI-Slots und ihre Grenzen folgen `CustomMidiSlotDef` und
`CallNativeEventSe` aus `sdfunc.c`.

Komprimierte SQ (`audio_sq.*`) wird über das Wörterbuch aus MODMIDI dekodiert
(nur Tokens A0..AF; Index `2*((Payload&0xF0)|(Status&15))` einschließlich
Bit 7, inklusive Bytegrenze, Velocity `((Payload&15)<<3)|7`, Überspringmarke aus
Bit 7 der Rohdaten). SysEx wird nicht unterstützt und gemeldet.

Formatbeleg: die originalen SQ-Ressourcen und
[VGMTrans SonyPS2Seq.cpp](https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/SonyPS2/SonyPS2Seq.cpp).
Der Sony-IOP-Sequencer selbst liegt nicht als Quelle vor.

## Filme

Alle 18 PSS-Filme werden beim Datenexport in `data/movies/<id>.rvv`
umgewandelt: Sony-SShd/SSbd-Stereo-PCM unverändert, Bilder als JPEG mit
originaler Bildanzahl und -rate. RVV1 besteht aus einem 36-Byte-Kopf,
verschachteltem PCM16 und längenpräfixierten JPEG-Bildern; dekodiert wird mit
dem eingebetteten `stb_image`. Wiedergabe und Wartebedingung steuern die
originalen Befehle Movie/MovieCk. Ersatzfilme: `mods/movies/<id>.rvv`.

```sh
python3 tools/cook_movies.py --data data
```

FFmpeg wird nur bei dieser Umwandlung benutzt, nicht von der Laufzeit.

## Offen

- Mehrschichtige HD-Programme, phaseninvertiertes Programm-Pan und SPU-Interpolation (ADSR und Hall sind portiert).
- SysEx in SQ und Live-Controller-Modulation laufender Noten.
- Vollständiger Anschluss aller räumlichen Soundkategorien und originale
  Sony-Programm-/Samplelautstärken; die Gesamtmischung ist noch nicht originalgetreu.
- MIDI-/ADX-Lautstärke→Ausgabekurve und die Velocity-/Bereichsfade-Pfade
  anderer Bankarten; `GetAdxStatus`-PREP hat keine native Entsprechung
  (Clips laden synchron).
- Ambient in Innenräumen: Das Original stoppt BG-Klänge nur über
  `bhBGSeOff`/`bhBGSeOff2` oder `CheckSpecialBank`. Ob der IOP-Treiber
  (`TSNDDRV.IRX`) weiteres Verhalten hat, ist ungeklärt.

## Referenzen

- [FFmpeg adx.c](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adx.c),
  [vgmstream ADX](https://github.com/vgmstream/vgmstream/blob/master/src/meta/adx.c)
- [vgmstream HD/BD](https://github.com/vgmstream/vgmstream/blob/master/src/meta/hd_bd.c),
  [SPU-ADPCM](https://github.com/vgmstream/vgmstream/blob/master/src/coding/psx_decoder.c)
- [VGMTrans SonyPS2InstrSet.cpp](https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/SonyPS2/SonyPS2InstrSet.cpp)
  (Vagi-Attribute und One-Shot/Loop),
  [PSX-SPX ADPCM-Flags](https://psx-spx.consoledev.net/soundprocessingunitspu/)
  (End/Repeat/Start)
- [vgmstream SShd](https://github.com/vgmstream/vgmstream/blob/master/src/meta/sshd.c),
  [stb_image](https://github.com/nothings/stb)


## Stand Tonhöhe, ADSR und Hall (Kontrolle 2026-10-12)

Sony-Tonhöhe (Wurzelnote, Feinstimmung, Pitchbend-Bereiche, Abtastratenskalierung) ist über
`audio_pitch.*` und die Mixerschleife angeschlossen und in `audio_movie` geprüft.
SPU2-ADSR-Hüllkurven und Dual-Core-Hall sind über `audio_spu.*`, `spu_reverb_presets.inc`
und die Mixerschleife angeschlossen. Die Register, Bitfelder und Presets sind aus
`LIBSD.IRX` (.data+0x38/+0x64), `MODHSYN.IRX` (Key-On 0x3af4, Stimmenwahl 0x64f8, CC 120 0x5bf4),
`TSNDDRV.IRX` (spucore_init, treq_ALL_REV) und psx-spx ("SPU Volume and ADSR Generator",
"SPU Reverb Formula") belegt.

Ganzzahliges Sony-Pan (`audio_pan.*`: `sony_pan_factors`, `sony_bank_pan`) und
Tonhöhe (`audio_pitch.*`, Tabellen aus LIBSD.IRX) sind aus den zulässigen
IOP-Modulen belegt (LIBSD 0x4680/0x4698, Pitchroutine 0x33a0, MODHSYN
Pitch/Gain/Pan-Struktur). Prüfung: `audio_movie` (`spu_envelope_reverb`), `sony_sq_dictionary`
(`tests/native/audio_sq_test.cpp`).
