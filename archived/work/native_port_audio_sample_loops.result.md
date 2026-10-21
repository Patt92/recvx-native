# Sony-Sampleloops — 2026-10-11

## Befund und begrenzter Umfang

`decode_ps_adpcm` verwarf die ADPCM-Loopadressen; `decode_system_sound` ignorierte Vagi +6. `AudioMixer::mix` entschied auch bei SQ-Noten ausschließlich nach dem äußeren Slotloop. Deshalb endeten gehaltene Loopnoten nach einem Waveformdurchlauf, während ein BG-Slotloop auch One-Shots wiederholen konnte. Geändert: native/audio.cpp, tests/native/audio_movie_test.cpp, ausschließlich die Audio-CTest-Registrierung in CMakeLists.txt sowie docs/AUDIO.md. Keine world/runtime/combat/Gegneränderungen durch diese Arbeit.

## Semantikbelege

- Sony HD/BD: Prog → Sset → Smpl → Vagi, Vagi +6 = One-Shot 0 / Loop 1. Referenz: https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/SonyPS2/SonyPS2InstrSet.cpp
- ADPCM: Bit 2 setzt die Loopadresse vor dem aktuellen 28-Frame-Block; Bit 0 beendet, Bit 1 wiederholt. End-Repeat-Block gehört zum Loop. Referenzen: https://psx-spx.consoledev.net/soundprocessingunitspu/ und https://github.com/vgmstream/vgmstream/blob/master/src/coding/psx_decoder.c
- Original MULTSPQ1/0002, Typ 1/Bank 2, Programm 6/Key 60: 32000 Hz, Vagi 7, Startblock 3 = PCM-Frame 84, Ende nach Block 2158 = Frame 60452. SQ Bank 2/Liste 6: Tempo 500000, PPQN 480, NoteOn Tick 7 = Outputframe 350, NoteOff Tick 7687 = Outputframe 384350, Velocity 127. Original NRPN-Loop startet die Note danach erneut.
- Original Bank 2/Liste 0 verwendet eine One-Shot-Wellenform. NoteOn Tick 5 = Outputframe 250, Velocity 118. Test vergleicht PCM mit und ohne äußeres Slotloopbit.
- Terminalflag 7: vorhandene Decoderbehandlung (stummer Block) erhalten; Start/End/Repeat ergeben dessen expliziten Selbstloop. Original MULTSPQ1/0022 Typ 1/Bank 2/Vagi 5: sieben Blöcke bei 43000 Hz, Start 168/Ende 196. Keine Rückkehr zum Vorlauf.

Keine Originalfunktionskörper, Generatoren oder Hardwareemulation übernommen. Vorhandene PcmClip-Grenzen und Mixer-/Sequenzbesitzer wiederverwendet. Repeat ohne expliziten Start sowie unbekannte Vagi-Attribute bleiben gemeldete Format-/Portierungslücken, keine geratenen Vollschleifen.

## Prüfungen

Belege unter `build/validation/audio-small-gaps-20261011`:

- Baseline-Audio mit Originaldaten grün (`baseline-audio.log`). Vorher-Runtime und natives Vorher-Archiv gesichert.
- Kontrollierte vier ADPCM-Blöcke: Start, Endblock-Einschluss, Vorlauf einmalig, normaler Endmarker, Repeat ohne Start abgelehnt. Gegenprobe rot (`red-audio.log`, `counter-check.log`).
- Neue Originaldaten-Prüfung gegen gespeichertes Vorher-Archiv rot: `original Vagi loop bounds missing/wrong`, Exit 1 (`original-loop-counter.log`). Keine fremden Quellen dafür zurückgesetzt.
- Original-SQ-PCM entspricht der separaten explizit geloopten Waveformreferenz einschließlich 350 Frames Vorlaufzeit. Nicht-null PCM nach Waveformende; NoteOff-Stille, NRPN-Wiedereinstieg, Stop, Fade und Raumersetzung bestanden. Vagi-One-Shot mit identischem PCM und gelöschten Loopgrenzen; unbekanntes Attribut abgelehnt. Beide äußeren Slotloopvarianten für Original-One-Shot bestanden (`original-loop-green.log`).
- Audio-CTest jetzt mit Datenpfad: bestanden (`final-audio-ctest.log`). Früher in dieser Arbeit kompletter CTest-Stand 61/61 grün (`full-ctest.log`); das ist nicht die Abnahme sämtlicher späterer Peer-Änderungen.
- `verify_audio_state.py`: Init → RoomSoundCase → Originalbank für Cases 0/1 bestanden, sowohl eigener Build als auch installierte Runtime. Kein Indoor-Regennachweis.
- Windows x64 isoliert kompiliert/gelinkt; Quellen-/Build-/PE-Policy PASS (`windows-build.log`, `windows-policy.log`), **nicht ausgeführt**. Erster Auditversuch traf den nicht schreibbaren globalen Zig-Cache; Wiederholung mit bereits beim Build verwendetem projektlokalem ZIG_GLOBAL_CACHE_DIR bestand, keine Sandboxänderung.
- Portierungscheck ausgeführt: `archived/work/audio_port_status_20261011.md`. Kein Fertigstellungsprozent für diese Formatportierung behauptet.

Zwischenfehler offen protokolliert: erster Build traf einen kurzzeitig unvollständigen Peer-Header (`effect_flags_`); Besitzer vervollständigte ihn, erneuter Build bestand. Erste Runtime-Fixtures hatten falschen Slotparameter (6 statt 0) bzw. wiederverwendeten Statepfad; korrigierte Fixture bestand. Diese Fehlläufe sind keine Engine-Regressionsbehauptung.

## Auslieferung

Mit Effektbesitzer abgestimmt: `tools/build_native.py --platform macos --jobs 6` aktualisierte `build/macos-arm64` und `build/start/macos-arm64/RECVX Native.app`. Runtime und SDL ausschließlich arm64, Codesign deep/strict verifiziert, installierter Binary-Policycheck PASS. Vorherige App automatisch erhalten in `build/start/macos-arm64/.previous-p0kw51vs/RECVX Native.app`.

`Start.command` mit isolierter Raum-Eventfixture und echten RM_0030-/MULTSPQ1/0004-Daten ausgeführt: Original SQ6 korrekt auf Eventslot 0, nach 120 Ticks aktiv, keine blockierten oder nicht verfügbaren Sequenzen (`starter-sampleloop-verified.json/.log`). Dasselbe Routing bestand vorher: Runtimezustand allein belegt **nicht** die PCM-Korrektur; dafür steht der Originaldaten-Komponententest. Keine natürliche Gameplay-/Hör- oder Hardwareprüfung behauptet. Projekt-INIs vor/nach identisch; Snapshots nicht bearbeitet. Die App enthält den gemeinsamen Arbeitsbaum, keine isolierte Freigabe aller parallel bearbeiteten Mechanismen. Das alte v0.5-Standalone-Paket und Windows-Paket wurden nicht erneuert.

## Offen

PCM wird vorab dekodiert; Predictor-Historie über den Looprücksprung, Pitch, ADSR/Release, Hall, Layering, exaktes Pan/Interpolation, komprimierte SQ und 3D-Audio bleiben unvollständig. NoteOff entfernt wie bisher sofort statt einer noch unportierten ADSR-Releasephase. Terminal-7-Stummschaltung ist vorhandene Formatdecodersemantik, kein neuer bitgenauer SPU-Nachweis. Keine pauschale Aussage „Audio originalgetreu“.

Konkreter nächster Audio-Kandidat aus Peer-Befund: CallPlayerWeaponSeEx in sdfunc.c:1546 wählt SlotNo 0 → MIDI 8/9 alternierend, sonst 19, und übergibt pPos an SetupSeGenericParm; vorhandenes play_weapon_sound hat nur Hostslot 6. Slotwahl und gemeinsamer Positions-/3D-Pfad sind hier nicht geändert.

Kein Commit/Push.
