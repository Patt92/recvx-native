# SPU2-ADSR-Hüllkurven und Hardware-Hall — 2026-10-12

## Befund und Umfang

Bisher wurden Sony-HD-Klänge und SQ-Noten ohne SPU2-Hüllkurven und Hall wiedergegeben. Stimmen verstummten bei NoteOff oder Stop schlagartig, und Raumhall (Reverb) fehlte vollständig. Umgesetzt:
- `native/audio_spu.h`, `native/audio_spu.cpp`: Plattformunabhängige SPU2-ADSR-Hüllkurve (`SpuEnvelope`) und Dual-Core Hardware-Hall DSP (`SpuReverb`).
- `native/spu_reverb_presets.inc`: Die 10 originalen LIBSD.IRX-Hardwarepresets und Arbeitsbereichsgrößen.
- `native/audio.h`, `native/audio.cpp`: Auslesen von ADSR1/ADSR2 und SpuAttr aus den Sony-HD-Sample-Headern; Dual-Core-Zuordnung (Core 0 / Core 1); SPU2-Envelope-Tick pro 48-kHz-Sample; CC 120 (`All Sound Off`, `ADSR2 = 0xC021`) beim Stoppen mit Auskling-Queue (`releasing_`); Wet-Reverb-Mischung (VMIXEL/VMIXER) in beide SPU2-Kerne; 16-VSync-Lautstärkerampe für `RoomFxLevel`.
- `CMakeLists.txt`: `native/audio_spu.cpp` zum `recvx_game`-Target hinzugefügt.
- `tests/native/audio_movie_test.cpp`: Testsuite `spu_envelope_reverb` zur Verifikation der ADSR-Phasen, CC-120-Schnellabfall, Preset-Register, Dual-Core-Zuteilung und des Raumhalls.
- `docs/AUDIO.md`: Dokumentation der SPU2-Hüllkurven- und Hall-Architektur.

## Semantikbelege aus den Original-IOP-Treibern

- **`MODHSYN.IRX`**:
  - Key-On (.text 0x3af4..0x3e00): `adsr1` (+0x12) und `adsr2` (+0x14) stammen direkt aus den 20-Byte-Sample-Einträgen der Sony-HD-Banken. Mischschalter aus `sampleSpuAttr` (+0x29): Bit 0 VMIXL, Bit 1 VMIXR, Bit 2 VMIXEL, Bit 3 VMIXER.
  - Stimmenwahl (0x64f8): `attr & 0x30 == 0x10` erzwingt Kern 0, `0x20` Kern 1, sonst wird der Kern mit mehr freien Stimmen gewählt (je 24 Stimmen pro Kern, bei Gleichstand Kern 0).
  - CC 120 (0x5bf4): `ADSR2 = 0xC021`, danach Key-Off aller Stimmen des Kanals; `TSNDDRV.IRX` `tsnd_tqcancel`/`port_stop` senden CC 120 für `sdShotStop`/`sdMidiStop`.
- **`LIBSD.IRX`**:
  - Presets (.data+0x38 und .data+0x64): 10 Hardware-Registerkonfigurationen (32 Register x uint16) und Puffergrößen. Modus 5 (HALL, ESA..EEA = 0x15BC 8-Byte-Einheiten / 0xADE0 Halbworte) wird von den Raumpaketen verwendet.
  - `TSNDDRV.IRX`: `spucore_init`, `treq_ALL_REV`.
- **Raumhall-Pegelrampe (`RoomFxLevel`)**:
  - Raumpaket-Chunk Typ 5 liefert `RoomFxLevel`. Im Treiber wird eine 16-VSync-Rampe (16 * 1/60s) gestartet: `CurrentFxLevel += (target - current) / 16` bis `RoomFxLevel * 256`, skaliert EVOLL/EVOLR.
- **SPU2-Arithmetik (psx-spx "SPU Volume and ADSR Generator" & "SPU Reverb Formula")**:
  - 48 kHz Hüllkurventakt: Attack (linear oder exponentiell bis 0x7FFF), Decay (exponentiell bis Sustain-Level), Sustain (linear/exponentiell oder Halt bei 0x7F), Release (exponentiell bis 0). Skalierung `sample * ENVX >> 15`.
  - Reverb DSP: 39-Tap-FIR-Resampling zwischen 48 kHz und 24 kHz mit abwechselnder Phase links/rechts; Wandreflexion (`vWALL`, `vIIR`), vier Kammfilter (`vCOMB1..4`), zwei Allpassfilter (`vAPF1..2`), Sättigung/Clamping. Dirty-Tracking (`nonzero_`) überspringt Stille-Zyklen exakt ohne Rechenaufwand.

## Prüfungen

- **CTest**: 68/68 Testgruppen bestanden (100 %).
  - `audio_movie` (Test `spu_envelope_reverb`): Attack-, Decay-, Sustain- und Release-Level, CC 120-Schnellabfall (Verstummen nach 32 Ticks), LIBSD-Registerabgleich, Kernbalance und Nachhallverifikation (Wet-Signal weicht von Dry ab und klingt nach Voice-Stop natürlich aus).
  - Frühere Sampleloop- und Alarmtests angepasst: NoteOff/Stop schaltet nicht sofort auf harte Stille, sondern durchläuft den SPU2-ADSR-Release und klingt durch den Raumhall ab.
- **Runtime- und Audiozustandsprüfung**:
  - `tools/verify_audio_state.py`: Bestanden.
  - `tools/verify_combat.py`: Bestanden.
- **Audio-Vergleichsdateien**:
  - `build/validation/audio-spu-20261012/hall_rm0000_mit_hall.wav` und `hall_rm0000_trocken.wav` exportiert und verifiziert.
- **Windows x64 Cross-Build**:
  - `recvx-runtime.exe` und `recvx-audio-tests.exe` über Ninja kompiliert und gelinkt, `check_no_original_core.py` PASS.

## Offen

- Mehrschichtige HD-Programme, phaseninvertiertes Programm-Pan und SPU-Interpolation (Gaussian/Hermite).
- SysEx in SQ und Live-Controller-Modulation laufender Noten.
- Vollständiger Anschluss aller verbleibenden räumlichen Soundkategorien.
- Asynchroner SPQ-Streaming-Transport.
- Kein Starter-/Paketaustausch, kein Git-Commit/Push ohne gesonderten Auftrag.
