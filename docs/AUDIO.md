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

## SQ-Sequenzen

Der Sequenz-Transport liest unkomprimierte Sony-SQ-Daten vollständig: PPQN,
Tempo, Running Status, verkürzte NoteOffs, Notenzeitpunkte und Polyphonie,
Programm/Key/Velocity, CC7/CC11/Pan sowie die NRPN-Schleifen
CC99/6/38. MIDI-Slots und ihre Grenzen folgen `CustomMidiSlotDef` und
`CallNativeEventSe` aus `sdfunc.c`.

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

- Tonhöhe und Transposition, ADSR-Hüllkurven, Hall, mehrschichtige
  HD-Programme, bitgenaues SPU-Panning und SPU-Interpolation.
- Komprimierte SQ-Daten und Live-Controller-Modulation laufender Noten.
- Räumliches 3D-Mischen und originale Sony-Programm-/Samplelautstärken;
  die Gesamtmischung ist dadurch noch nicht originalgetreu.
- Ambient in Innenräumen: Das Original stoppt BG-Klänge nur über
  `bhBGSeOff`/`bhBGSeOff2` oder `CheckSpecialBank`. Ob der IOP-Treiber
  (`TSNDDRV.IRX`) weiteres Verhalten hat, ist ungeklärt.

## Referenzen

- [FFmpeg adx.c](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adx.c),
  [vgmstream ADX](https://github.com/vgmstream/vgmstream/blob/master/src/meta/adx.c)
- [vgmstream HD/BD](https://github.com/vgmstream/vgmstream/blob/master/src/meta/hd_bd.c),
  [SPU-ADPCM](https://github.com/vgmstream/vgmstream/blob/master/src/coding/psx_decoder.c)
- [vgmstream SShd](https://github.com/vgmstream/vgmstream/blob/master/src/meta/sshd.c),
  [stb_image](https://github.com/nothings/stb)
