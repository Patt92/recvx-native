# Offene Punkte (aus dem aufgelösten `work/`)

Stand 2026-10-29. `work/` ist leer; erledigte Aufträge, Ergebnisse und Pläne liegen unter
`archived/work/` (nur lokal, nicht versioniert). Hier stehen nur die Punkte, die in diesen
Dateien als offen markiert waren und nicht nachweislich erledigt sind. Laufende Verträge und
Arbeitsstände: [Gegnerboard](enemies/README.md), [Inventarvertrag](INVENTORY_CONTRACT.md),
[AI-Playbook](AI_PLAYBOOK.md), [Quarantänemanifest](forbidden_cleanup_manifest.json).

## Gegner

- **Alle Identitäten:** nativ verdrahtet; Audit und Fensterabnahme je Gegner offen
  ([Gegnerboard](enemies/README.md)).
- ~~**Skriptgesteuerte Gegner vor ihrer Initialisierung**~~ (aus `hunter7110.md`): 2026-11-03
  widerlegt. bhInitEvent (event.c:352) ruft bhControlEnemy vor scd0; nativ `EventVm::initialize` ->
  `initialize_enemies`. Test `scripted_enemy_init`: EN11 in RM_7110 vor scd0 initialisiert, Skript-mode0 5
  erreicht bhEne_Event; alle 200 Raeume ohne uninitialisiertes Work ausser den Originalwaechtern
  (EN29 type 8, EN03/EN06 im Spielerpfad). Ledger `event.c:bhInitEvent:bhControlEnemy`.
- **Disc 2, dynamisch** (aus `gap-audit-disc2.md`): Griffabläufe ab Stufe 6 ohne Spielstände in den
  Räumen nicht im Spiel geprüft. `bhItmSetCk` in RM_7030 (Ereignis 0, Offsets 34/40/46) mit
  Interaktionsindex hinter `rom->etcp`: Original liest unkontrolliert, nativ wird nur der
  ATR-Teil übersprungen und gemeldet. `njSetBackColor` mit Alpha >128 in RM_7160/9091/9301/9340/9360
  außerhalb der nativen Zieldomäne (Renderer).

## Audio (aus `audio-port.md`, Details [AUDIO.md](AUDIO.md))

1. ADX-Decoder mit Float-Prädiktoren (`ADX_GetCoefficient` 0x2f11b8, `ADX_DecodeMonoFloat` /
   `ADX_DecodeSteFloatAsMono` 0x2f1798/0x2f1bb0); nativ Integer-Prädiktor und vorab erzeugtes PCM.
2. Soundbank-Synthese: Phasenpan (MODHSYN 0x4534/0x4214), ADSR-Key-Follow, exklusive Gruppen
   (0x6900), Velocitykurven (0x8794), Crossfade (0x4680), Amplituden-LFO, implizite Loopadressen.
3. Sequencer: F0-SysEx über MODMIDI (0xc0c) bis MODHSYN, F7-Skip-Pfad (0xa10), restliche
   CC-/NRPN-/CC32-Handler.
4. Übertragung/Lebensdauer: EE-Transmission, Callback, PREP, TSNDDRV-Load-Thread.
5. Ausgabe: Originalinterpolation, ADPCM-Loophistorie, Referenz-PCM für Hall, vom Renderstau
   unabhängige Ausgabe.
6. Film/Ranking: Pre-play-/Movie-Tasks, Drive-Tray/Reset, Abbruch vor PLAYING; Ranking ohne
   nativen Gameplay-Besitzer.

## Renderer und Präsentation

- **Direct3D 12** (aus `d3d12_raster_backend.result.md`): auf Windows-Hardware und WARP nicht
  ausgeführt; Bitgleichheit, Fensterbild und Parität offen.
- **FSR 4 / D3D12** (aus `fsr_present_d3d12.result.md`): Lauf auf Windows-GPU (Bild, Reset bei
  Schnitten, FP8/INT8, FSR-3.1-Fallback, Tearing), Objekt-Bewegungsvektoren, Subpixel-Jitter,
  Frame Generation, Mip-Bias. Vulkan-FSR braucht die FSR-3.1-Quellen des SDK.
- **Vulkan** (aus `vulkan_raster_backend.result.md`): Windows-/Linux-Treiber (Interlock,
  Bitgleichheit) ungeprüft; Fenster/Controller nicht geprüft. Header und SPIR-V seit 2026-11-02 eingecheckt
  (alle Builds inkl. Windows-Crossbuild/CI mit Vulkan), Mac-Pakete mit MoltenVK.
- **Energie** (aus `performance_optimization_plan_20261020.md`): Phase B3 (Präsentationsschleife,
  ereignisgesteuertes Schlafen, keine doppelten Draw-Seiteneffekte), Phase C (Asset-Aufbereitung
  bei Bedarf) und die Windows-Abnahme des GPU-Pfads offen. A, B1/B2, D1 und D2 (macOS) sind umgesetzt.
