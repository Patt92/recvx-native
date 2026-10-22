# Audio: verbleibende Vollportierung

Verbindlich: AGENTS.md und QUALITY.md. Keine Unteragenten, kein Originalkern,
keine Quarantänequellen, keine Ersatzlogik. Arbeitsbaum und Build sind von
installierter App/Paket getrennt; dieser Auftrag ersetzt keine App.

## Bearbeitete Mechanismen

- COMMON-Bank 0 für allgemeine SE-Aufrufer; `CallPlayerVoice(2)` mit PCM-Wirkung
  und nativem RM_7250-Lauf über originale Gegnerinitialisierung/-KI geprüft.
- Stereo/Mono: EE-/IOP-Modus, MODHSYN-Panfaktoren mit ursprünglichem
  Aktualisierungszeitpunkt, SE-Chg-Pan, Film-Linksduplikation, globale
  Konfiguration und OPTION-Bindung. ADX-Routing vorhanden; dessen bestehender
  Integerdecoder ist weiterhin eine offene Originalabhängigkeit.
- Film: StopBgm/StopVoice/SleepAdxStream, Wakeup mit FadeFunc-Reset,
  MovieTypeDef-/Override-Gates, gemeinsamer 30er-Screen-Fade und lineare
  Blocklautstärke. Film-PCM-Freigabe erhaelt den getrennten Spieler-SE-Platz7.
  Keine eigene zweite Fade-Arbeit.
- ADX: originale quantisierte Ausgangspegel, Residualskala `(header & 0x1fff)+1`
  und von der Loopadresse unabhängige Spielzeit. Expliziter Stopp/Ende getrennt.
- HD: sämtliche Sset-Samples je Split, inklusive Velocity-Grenzen und
  Velocity im produktiven Resolver-Cache. Datenfixtures sind Diagnoseprüfungen.

Belegstellen und Grenzen: [Audio](../docs/AUDIO.md), `docs/port_ledger.tsv`.
Prüfartefakte: `build/validation/audio-port/` (gitignoriert).

## Offen — kein 100-%-Nachweis

1. **ADX-Decoder:** `ADX_GetCoefficient` (ELF 0x2f11b8) und
   `ADX_DecodeMonoFloat`/`ADX_DecodeSteFloatAsMono` (0x2f1798/0x2f1bb0)
   arbeiten mit Float-Prädiktoren und eigener Koeffizienten-/Ausgaberundung.
   Nativ verbleiben Integer-Prädiktor und vorab erzeugtes PCM. Decoder-Aufrufgrenzen,
   Historienübergabe, Mono vor Quantisierung und PCM-Vergleich am Original klären.
   Die korrigierte Blockskala und Pegeltabelle schließen diese Lücke nicht.
2. **Soundbank-Synthese:** Phasenpan (MODHSYN 0x4534/0x4214), ADSR-Key-Follow,
   exklusive Gruppen (0x6900), Velocitykurven (0x8794), Crossfade (0x4680),
   Amplituden-LFO und implizite Loopadressen/Vagi-Attribute. Ausgelieferte
   ungenutzte Zweige bleiben Mechanismenlücken, auch bei fehlerfreiem Datenscan.
3. **Sequencer:** F0-SysEx muss über den originalen MODMIDI-Ausgangspfad
   (0xc0c) bis MODHSYN verstanden werden. F7 hat einen getrennten Skip-Pfad
   (0xa10). Nur das Parsen oder Verwerfen von SysEx wäre keine vollständige
   Portierung. Verbliebene CC-/NRPN-/CC32-Routing-Handler nachführen.
4. **Übertragung/Lebensdauer:** EE-Transmission, Abschluss/Callback, PREP und
   TSNDDRV-Load-Thread portieren. Synchrones Laden und Stoppen ersetzter Ports
   bleiben bestehende Host-Abweichungen. Kein SPU-RAM-/Hardwaremodell einführen.
5. **Ausgabe/Verifikation:** Originalinterpolation und ADPCM-Loophistorie,
   Referenz-PCM für Hall sowie unabhängig vom Renderstau laufende Ausgabe.
   SDL wird mit erlaubten Formatänderungen 0 geöffnet; die frühere Behauptung
   eines automatisch an die Engine durchgereichten 44,1-kHz-Formats war falsch.
6. **Film/Ranking:** Pre-play- und Movie-Tasks einschließlich Startreihenfolge,
   Drive-Tray/Reset und Abbruch vor PLAYING. Ranking braucht seinen nativen
   Gameplay-Besitzer; BATTLE GAME nicht eigenmächtig aktivieren.

Abnahme bleibt pro Mechanismus: Originaldaten/Disassembly, Zustandsübergänge,
gezielte Wirkungstests, produktiver Aufrufer; Abbruch, erneuter Eintritt,
Freigabe und Raumwechsel einschließen. Keine Vollständigkeit aus Prozentwerten
oder einem Raumlauf ableiten. Windows x64 und echte Hör-/Hardwareabnahme separat.

## Geprüfter Teilstand

- macOS arm64: Runtime und SDL rein arm64, Policy-/Binärprüfung PASS;
  `audio_movie`, `audio_gaps`, `audio_levels`, `sony_sq_dictionary` und
  `main_menu_settings_video_skip` PASS (5/5).
- Windows x64: Crossbuild der Runtime und dieser Testprogramme, Policy-/Binärprüfung
  PASS. Unter Wine die vier Audiotests PASS; OPTION mit Persistenz, RM_7250
  COMMON/0002 sowie Filmstart/-Skip jeweils über die Windows-Runtime PASS.
- Die Frontend-Nachbarsuite scheitert unter Wine bei `hint confirmation failed`
  mit verbleibender SDL-Meldung `Unexpected controller element crc`. Ein separater
  Build des echten HEAD-Vorstands `47b4a22d1c135face3ed3b890f552fccdf783499`
  scheitert identisch (`frontend-wine-baseline.json`); keine Testabschwächung.
- Fünf isolierte Rückmutationen werden abgewiesen: COMMON-Gate, kontinuierliche
  statt quantisierter ADX-Kurve, Loopadresse als Spielzeit, alte Blockskala und
  allgemeines Stop(7) statt Freigabe allein des Film-PCM.
- `runtime-checks.json` und `runtime-checks-wine.json` halten Befehle und die
  jeweiligen Binär-Hashes; Bilder liegen im selben Prüfverzeichnis. Diagnoseeinstieg
  und Replays, kein Durchspiel-, Hörvergleichs- oder echter Windows-Hardwarenachweis.
  Installierte App/Pakete blieben unangetastet.
