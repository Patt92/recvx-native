# Audio and movies

The runtime needs only SDL2 and prepared loose files. It does not invoke
FFmpeg, open ISO/AFS containers, or emulate PS2 sound hardware. All formats
are decoded natively and mixed in a 48 kHz stereo mixer driven by the 30 Hz
game clock (1,600 frames per tick).

## ADX music and voices

- The decoder supports unencrypted ADX (encoding 3, version 3, 18-byte blocks,
  mono/stereo) with embedded loops. It rejects other variants instead of
  guessing.
- BGM and VOICE come from `data/archives/<archive>/<id>.bin` in the current
  disc set; replacements go under `mods/audio/<archive>/<id>.adx`.
- BGM control follows `sdfunc.c`: `bhBgmOn`/`bhBgmOnEx` start immediately;
  `bhBgmOn2`/`bhBgmOff2` request a change executed by the door sequence
  (SoundCommand 2) and `Com_ExecRoomFadeIn`. The same track does not restart.
  Volumes follow `AdxVolTbl` → the original 1,000-entry `ps2rna_dbtbl`
  (8-bit amplitude / 256), rather than a continuous exponential curve.
  Evidence: ELF 0x3432e0, `ps2rna_sndcbf` 0x2fe7c4..0x2fe7fc;
  TSNDDRV `psmmix_calc_vol` 0xd18c..0xd194. Fades follow `RequestAdxFadeFunction2`.
- Resources without a loop marker end naturally.
- ADX block scale is `(header & 0x1fff) + 1`, as in the original decoder
  (ELF 0x2f1820/0x2f1830). Play time counts consumed source samples across
  loop jumps; natural end retains total time, explicit stop resets it.
  `ADXT_GetTimeSfreq2` 0x2f6ce0 and `adxsjd_decexec_end` 0x2f5088..0x2f50b0
  establish that the sample counter is independent of the decoder's loop address.

`recvx-audio <input.adx> <new.wav>` exports PCM for comparison.

## Sony sound banks

- `COMMON.MLT` (system sounds) and room packets from `MULTSPQ1/2` are read as
  Sony HD/BD (Prog → Sset → Smpl → Vagi) and decoded as SPU ADPCM.
- Explicit ADPCM loop addresses remain PCM frame boundaries: start before the
  start block, end after the end-repeat block. Sony's Vagi attribute (+6)
  distinguishes one-shot (0) and loop (1). Repeat without a known start
  address and unknown attributes are reported, not assumed to mean a full
  loop.
- SQ notes repeat only their marked sample region, independently of the outer
  slot/BG loop bit and NRPN transport loops. NoteOff, Stop, Fade, and room
  replacement release the note. The decoder treats terminal flag 7 as a
  silent block; a self-loop there does not repeat the lead-in. PCM is still
  decoded once in advance, so predictor history across the loop jump and
  bit-exact SPU synthesis remain unproven.
- MIDI notes follow `MODHSYN.IRX` program/split selection (program change
  `.text` 0x75c, Note-On 0x5f90). An empty program entry (0xffffffff), program
  index past the last entry, program without a split, or a key covered by no
  split (limits +2/+4 without bit 7) starts no voice; the sequence continues.
  The split loop (0x6098..0x64a0) does not stop at the first match: each
  covering split starts its own voice in table order (`decode_system_note`,
  with per-split volume/pan from program and split; the sequencer gives each
  voice a separate note layer with Key-On/Key-Off/core selection). Splits
  whose Sset index is past the last entry, Ssets without samples, Smpl indices
  past the last entry, or Vagi 0xffffffff are skipped as in the original.
  Each Sset sample index is visited in order (0x615c..0x646c). Inclusive
  Sset velocity limits (+1/+2) precede this loop; each Smpl has its own
  limits (+2/+4 & 0x7f). The productive resolver includes velocity in its
  cache key. Synthetic bank/sequence tests exercise multiple layers, boundary
  velocities and repeated keys with different velocities through MODMIDI/MODHSYN.
  Voice allocation and stealing are handled by the IOP model described below.
- Room packet key: `stage * 1000 + room * 10 + case`. The order Event_init →
  `RoomSoundCase` → `RequestRoomSoundBank` follows `system.c`.
- System sounds use `CallSystemSe`; player voices use
  `RequestPlayerVoiceSoundBank` (bank 4); weapons use
  `RequestArmsSoundBank`; enemies use `RequestEnemySeBasic`/
  `ExecEnemySeManager` (six voices, priority, same-SE limit).
- Footsteps follow `PlFootSnd`/`FootSeCall` with ground material from ATR
  surfaces.
- Door sounds come from the door sound bank (`0x8000 | bank`) with status
  requests from `door.c`; the closing phase follows `FadeProc1`.
- Ambient/BG sounds follow `bhBGSeOn/Off`, `bhBGSeOn2/Off2`, and
  `CheckSpecialBank`; `bhSoundVolumeSet` affects every sound group.

Missing programs are reported as `audio_unavailable` or
`sequence_unavailable`; no replacement sounds are inserted. Replacement bank:
`mods/audio/system/COMMON.MLT`; replacement packet:
`mods/audio/MULTSPQ1/<packet>.bin`.

All SE callers can resolve bank 0 from COMMON independently of room packets;
`SetupSeGenericParm` (`sdfunc.c:913`) selects it for numbers below 256.
The original player-voice request 2 now reaches this bank in the native
RM_7250 enemy run, without a special room or resource branch.

## Sound output mode

OPTION exposes the original Stereo/Mono setting (`sdc.c:97`). It is global
configuration in `audio.ini` under the shared user-data directory;
`--audio-config` overrides that path for tests. Room loads and snapshots do
not replace it. The native UI remains the documented host OPTION screen.

- EE mode 0 selects driver mode 1 (stereo); EE mode 1 selects driver mode 0
  (mono), confirmed by ELF 0x28ef40/0x28ef5c. TSNDDRV command 0x42
  (0x7da8..0x7de4) reaches MODHSYN's output mode (0x1f88).
- MODHSYN mono sets both voice pan factors to 128 when pan is next calculated
  (0x4534..0x454c). The existing calculated factors survive the mode setter;
  new notes, changed CC10 and HsMsg pan apply the new mode. EE SE changes force
  pan 64 (`ps2_sg_sd.c:1737`); the requested pan state remains intact.
- ADX stereo-to-mono averages the two source channels before output pan
  (`ADX_DecodeSteFloatAsMono`, ELF 0x2f1da0..0x2f1ddc). Native PCM mixing
  carries this routing; the original float predictor/rounding remains an open
  decoder dependency, so bit-exact ADX mono output is not certified.
- Movie mono duplicates the left PCM channel (`ps2_MovieFunc.c:602`),
  independently of the ADX average. Reverb is not folded after the mix.

## Spatial SE parameters

`AudioMixer::sound_parameter_3d` computes distance, camera angle, pan, and
volume following `Get3DSoundParameter`. The three original tables, including
verified edge bytes, come from original data. The listener uses
`cam.wpx/wpy/wpz` (camera pivot plus rotated `-ln`), not the pivot itself. The
monitor boundary takes camera/player position and then processes the enemy SE
queue; new requests remain pending until the next monitor pass.

- `CallPlayerWeaponSeEx`: SlotNo 0 alternates SE 8/9; any other SlotNo uses
  SE 19. Combat and event callers pass the actual muzzle/joint position.
  Syukan and `VolWeaponSe` remain separate steps.
- Player voices use PlayerPos. Enemy requests retain their position,
  `VolEnemySe`, `VolDownTbl`, and the original exchange-sort algorithm.
  Occlusion follows `CheckCollision4Sound` with the 0.1 capsule, room flag 2,
  and `bhCheckC2Wall` against room/mobile walls; a hit subtracts 28.
- SE volume fades retain state independently of playback. They advance at
  60 Hz and end only when the counter turns negative. MIDI and ADX use their
  own fade state, described below.
- Footstep/action sounds (`CallPlayerFootStepSeEx`) use alternating SE slots
  11..16 or slot 6, walk/run pitch from the shared EE random state, PlayerPos
  or joint position, Syukan, `VolPlayerAction`, and flag 0. `CallYakkyouSe`
  (slot 10, `-RoomFxLevel/12`, `VolCartridgeSe`) is called by the native
  `bhEff000` casing producer.
- Object SE (`RegistObjectSe`/`FreeObjectSe`/`ExecObjectSeManager`) uses a
  request list, slot search, room reset on init, fade/pan requests, and
  occlusion. It runs after enemy SE at the monitor. Original loop limits
  (`MaxSlotObjectSe` instead of `MaxObjectReqList`) are preserved.
- Event commands `bhSeOn`, `bhEneSeOn`, `bhSoundPanSet`, `bhAddObjSe`,
  and `bhDelObjSe` pass through source position and parameters.
  `bhEasySESet` has a partial binding; see the port ledger for its remaining
  cases. MIDI volume/pan run in the 60 Hz fade/pan manager.
- `bhVoiceOn`/`bhVoiceWait`/`bhVoiceOff`/`bhVoiceStart` (`PlayVoiceEx2`, ADX
  slot 1, VOICE archive): mode 0 starts spatially, 1 is a centered fade-in,
  and 2 only adopts parameters. `VoiceWait` passes the pause request and
  `VoiceStart` calls ContinueAdx; both reach `ADXT_Pause`, which is empty in
  the original ELF and in the native port. A playing voice stops before
  switching, as in the original.
- `bhEneSeOn` (object index/node from enemy work) and `bhSeOn`
  (player/enemy/object through `owP[0]`) pass through position and mode.
- `bhEff000` casing work (types 0..3 and 10) has initial values from shared
  EE random, floor/wall bounce, water transition, and `CallYakkyouSe` on SE
  slot 10.
- EN11 `bhEne_SetSEPan` is a flag-0 request without a new sound.

### Sound flags, fades, and stopping

`Set3dSoundFlag` selects the original player voice, footstep/action, weapon,
casing, enemy, background MIDI, event MIDI, object, or ADX control slot. Bit
0x2 enables explicit volume control; bit 0x4 enables explicit pan control.
`SetUserSoundVolume` applies SE fades for types 0..5, MIDI fades for types
6/7, a deferred object MIDI fade for type 8, and an ADX fade for type 10.
An object consumes its pending fade on its next `ExecObjectSeManager` pass.
`SetUserSoundPan` likewise gates pan requests by bit 0x4. The event VM binds
these operations through `bh3dSoundFlagSet`, `bhSoundFlagReSet`,
`bhSoundVolumeSet`, and `bhSoundPanSet` (`native/world.cpp`,
`native/audio.cpp`; `audio_gaps`).

`AllStopEnemySe` fades registered enemy voices over 30 sound-manager steps,
stops free slots, clears their request records, and stops vibration at the
status/map call sites (`native/runtime.cpp`; `audio_movie`).
`RequestAllStopSoundEx` stops or fades ADX, SE, and MIDI groups according to
its flags and resets object sound requests (`native/audio.cpp`). The original
OPTION path for `AllStopEnemySe` has no native owner because OPTION opens the
documented host settings screen.

BGM/ADX fades no longer use a host sample ramp. `RequestAdxFadeFunction2`/`Ex`
(timer-to-counter formula, 0→1, counter < 0 ends, stop at int(Volume) == -127),
`SetVolumeAdx`/`Ex`, `PlayBgmEx2`, `Com_ExecRoomFadeIn/Out`, and
`RequestSoundFade` write shared state. `ExecSoundFadeManager`/
`ExecSoundPanManager`/`ExecAdxFadeManager` advance it at 60 Hz (every 800
output frames natively). `ADXT_Pause` is empty in the original ELF (`jr ra;
nop`), so there is no native pause.

The `ss_flg & 0x4000000` gate before `PlayBgmEx2`/`ExecAdxFadeManager` is
always open: no original writer sets this bit in the checked source.

SE note: `Tsnd_tqreq` takes the note from program header byte +0xB of the HD
file (TSNDDRV 0x10D4, statically verified); velocity is literal 127. That
byte is 60 in COMMON, the weapon bank, and every room bank.
`decode_system_sound` now reads it from the bank instead of assuming a
constant. All eight original `Get3DSoundParameter` callers (SetupSeGenericParm,
footsteps, enemies, event SE mode 0, object SE, voices) are connected
(`audio_gaps` test).

### SE shot levels (TSNDDRV/MODHSYN)

Based on independent disassembly of `TSNDDRV.IRX` and `MODHSYN.IRX` (read-only
references), every SE shot voice (`play_pcm` with a Sony bank sound: room,
enemy, weapon, casing, footstep, or COMMON SE) computes its SPU2 VOLL/VOLR
registers (`sony_voice_levels`, `native/audio_pan.cpp`). The voice plays at
register/16384:

- EE: `sdShotSetVol` stores `(Sint8)Volume + 128` as u8. `SdrSeReq`/`SdrSeChg`
  pass it as `char` and mark it valid only at `>= 0` (`CheckCmdReq/Chg`).
  `sdShotSetPan` computes `(Sint8)Pan / 2 + 64`.
- TSNDDRV: invalid volume on Req becomes 127 (0xdc4); on Chg it retains the
  prior value (0xca0/0x1508). Chg with Pan 64 or Bend 8192 stores without
  CC10/Pitchbend (0x1588/0x1670); the channel retains its value.
  `tsnd_tq_chgreq`: CC7 = min(127, 127·Vol/127), CC10 = clamp(Pan), Bend =
  Pitch. Port volume is 256 for every port (`sceHSyn_SetVolume`). Note 60
  comes from program +0xB, with velocity 127.
- MODHSYN: CC7/CC11 (0x978/0x9d4) = `(v << 7) / 127`; note level (0x38c8)
  = `(program +6 · split +0x10 · sample +0x10 · velocity) >> 14`; voice
  (0x426c) = `(CC7 · CC11 · note) >> 14`; output (0x40f0) =
  `min(16383, (pan factor · level) >> 7)`; pan (0x0c18/0x4534) is
  `CC10 - 64` in -63..63 plus bank pan - 64; Bend (0x4098) is
  `(Bend · split +6/+8) >> 13`.
- Velocity curve (Sset +0), split/sample crossfade (bit 7 of +2/+4), and
  amplitude LFO (sample +0x28 & 0x70) are 0 in all 735 HD banks; other values
  are reported while decoding. The previously omitted volume of 100 is
  present in 251 samples.
- Request state applies until the next `sound_sync` (`sdSysServer` runs before
  the fade managers, `sdfunc.c:465`).

Test `audio_levels` checks register values and Req/Chg rules, including sample
volume 100 in the RM_0021 bank 2.

### MIDI levels (TSNDDRV/MODMIDI/MODHSYN)

Sequence notes compute the same VOLL/VOLR registers; host scaling is removed:

- EE: `sdMidiSetVol` stores vol + 128 in 0..127; `sdMidiSetPan` uses
  pan / 2 + 64.
- `SdrBgmReq` (treq_BGM 0x20): `sceMidi_MidiSetLocation` sets MODMIDI
  sequence CC7 to 127 on all 16 channels; `sceMidi_MidiSetVolume(port, 255,
  vol)` stores the port volume.
- `SdrBgmChg`: a changed volume updates CC7 on every channel. Pan 64 sends
  nothing; any other pan goes as HsMsg to all voices on the port each ATick
  (240 Hz, hardware timer 0x25800 at 36.864 MHz) without changing channel
  CC10. New notes use channel CC10 until the next ATick; after Pan 64, voices
  retain their last HsMsg pan.
- MODMIDI (0xf30): CC7 to MODHSYN =
  `min(127, sequence CC7 · 128 · port volume >> 14)`.
- MODHSYN: CC7/CC11 = `(v << 7) / 127`; CC10 = `v - 64` (clamped at -63);
  Pitchbend = `(byte2 << 7) + byte1 - 8192` for all channel voices. Channels
  initialize only when the module starts and persist across sequences per
  port (`MidiPort`).
- Data: CC7 precedes the first note in every channel; no sequence uses
  CC10/CC11. There are 3,369 pitchbends (previously ignored); four sequences
  end with Bend +204.
- MidiHandle *i* is port *i*. MidiHandle[2] (background slot 2, system MIDI,
  door) rotates among ports 2 → 8 → 9 (host voices 27/28/29); each
  `sdMidiPlay` stops the previous port. Host door skip stops only the ports
  in the door list.
- Background timers, `StopBackGroundSeEx`, and `Com_ExecRoomFadeIn` use
  MidiInfo fades through the 60 Hz fade manager, not host ramps.

The phase between ATick and EE VSync is not fixed in the original. Natively,
every fourth ATick aligns with the `sdSysServer` frame. Tests: `audio_levels`
(MIDI portion), `audio_movie`, `audio_gaps`.

### IOP timing and voice pool

`native/audio_iop.*` ports the IOP side: the TSNDDRV command thread (one EE
stack per `sdSysServer`; `DelayATick` on BgmReq/BgmStop), TQ SE queue (runtime
timer 420·bytes/rate ATicks, pitch >> 9 per tick, infinite loop; flag 4 → CC7/
program/Note-On 127 on MODHSYN port 10+bank; Cancel → CC 120; only TQ ports
0..4 advance), MODMIDI sequencer (4167·128 per ATick, step
`((tempo·128)/PPQN·256)/256`, NRPN loops, CC7 × port volume, stop via CC
64/123), and MODHSYN (16 ports × 16 channels, 2 × 24 voices: free list from
the +4 end, Key-On stack until ENVX < 1000, release at ENVX 0 or ENDX with a
three-tick guard, stealing by class/priority/ENVX snapshot, port limit 48).
The 48 SPU2 voices compute ADSR, VOLL/VOLR, and pitch per voice.

The EE side keeps original handles (`SND_WORK` vol/pan/pitch/req,
`use_se_info`, `req_se_info`, `sdShotPlay` with channel ^ 8,
MidiHandle[2] rotation). `active()` is the EE view: a pending request or busy
bit from `get_iop_snddata`. Stop becomes visible two `sdSysServer` frames
later; stopping before the first `sdSysServer` has no effect, as in the
original. An SE fade sends the handle's old volume with Req
(`VolumeDelayTime -1`).

Host policy, not original behavior: replacing a bank buffer stops its TQ port
or bound BGM ports (the original loader thread is not ported); a program
beyond the TSNDDRV table is reported. Tests `audio_levels` (pool, stealing,
Key-On stack on `IopSound`), `audio_movie`, and `audio_gaps` cover latency and
registers.

## SPU2 envelope and reverb

SPU2 voice envelopes (ADSR) and hardware reverb run independently of platform
in the 48 kHz mixer (`native/audio_spu.*`, `native/spu_reverb_presets.inc`):

- **ADSR envelope (`SpuEnvelope`)**:
  - `adsr1` (+0x12) and `adsr2` (+0x14) are read from Sony HD sample entries
    by `decode_system_sound`.
  - Phases: Attack (linear or exponential to 0x7FFF), Decay (exponential to
    sustain level `((adsr1 & 0xF) + 1) << 11`), Sustain (linear/exponential
    rise/fall or steady at rate 0x7F), Release (exponential to 0).
  - Step and counter arithmetic run at 48 kHz following psx-spx and
    MODHSYN.IRX; multiplication is `sample * ENVX >> 15`.
  - `sdShotStop`/`sdMidiStop` send CC 120 (`All Sound Off`, `ADSR2 = 0xC021`)
    for a fast Release fall. Voices finish naturally in `releasing_` through
    SPU2 Release.
- **Dual-core SPU2 reverb (`SpuReverb`)**:
  - Native implementation of the original reverb signal path for both SPU2
    cores, each with 24 voices.
  - Dynamic core selection following `MODHSYN.IRX` (0x64f8):
    `attr & 0x30 == 0x10` selects core 0, `0x20` selects core 1, otherwise
    the core with more free voices (core 0 on a tie).
  - Reverb-send routing follows `sampleSpuAttr` (+0x29): wet VMIXEL (bit 2)
    and VMIXER (bit 3) are mixed into the assigned core's effect ring buffer.
  - Ten LIBSD.IRX hardware presets (`.data+0x38`/`.data+0x64`); mode 5
    (HALL) appears in room packets.
  - 39-tap FIR resampling between 48 and 24 kHz with alternating left/right
    phase, wall reflection (`vWALL`, `vIIR`), four comb filters
    (`vCOMB1..4`), and two all-pass filters (`vAPF1..2`).
  - Level ramp (`RoomFxLevel`): a type-5 packet block sets a 16-VSync ramp
    (16 × 1/60 s), `CurrentFxLevel += (target - current) / 16`, toward
    `RoomFxLevel * 256`, scaling EVOLL/EVOLR.

## SQ sequences

The sequence transport reads Sony SQ data (uncompressed and compressed): PPQN,
tempo, running status, shortened NoteOffs, note timing and polyphony,
program/key/velocity, CC7/CC11/pan, and NRPN loops CC99/6/38. MIDI slots and
limits follow `CustomMidiSlotDef` and `CallNativeEventSe` in `sdfunc.c`.

Compressed SQ (`audio_sq.*`) uses the MODMIDI dictionary (tokens A0..AF only;
index `2*((Payload&0xF0)|(Status&15))` including bit 7, byte-boundary cases,
velocity `((Payload&15)<<3)|7`, skip marker from raw-data bit 7). SysEx is
unsupported and reported.

Format evidence: original SQ resources and
[VGMTrans SonyPS2Seq.cpp](https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/SonyPS2/SonyPS2Seq.cpp).
The Sony IOP sequencer itself is unavailable as source.

## Movies

All 18 PSS movies are converted during data export to `data/movies/<id>.rvv`:
Sony SShd/SSbd stereo PCM is unchanged; frames are JPEG at the original count
and rate. RVV1 consists of a 36-byte header, interleaved PCM16, and
length-prefixed JPEG frames decoded by embedded `stb_image`. Original Movie
and MovieCk commands control playback and waits. Replacement movies go under
`mods/movies/<id>.rvv`.

```sh
python3 tools/cook_movies.py --data data
```

FFmpeg is used only for this conversion, not by the runtime.

Film start stops BGM, stops VOICE, then sleeps both ADX streams, following
`PlayStartMovieEx` (`sdfunc.c:3170`). Film stop/end wakes the streams and
clears their fade functions (`adxwrap.c:465/497`); stopped tracks do not resume.
Stopping movie PCM preserves the separate player SE handle that shares host
number 7; the movie owner releases only its native PCM channel.
`MovieTypeDef[22]` controls cancellation (bit 0x80) and cancel fade (bit 0x40),
with the original MovieType override bits. Repeated cancel does not restart
the fade. `PlayMovieMain` uses the one shared ScreenFade and cb_flg bit 2:
check completion before control, decrement audio volume each pass, stop in
the following pass. `SetMwVolume` maps to the linear block level
`(unsigned)(32.8 * (volume + 999)) & 0xffff`, /32768, not ADX decibels
(`ps2_sfd_mw.c:263`, `ps2_MovieFunc.c:642`). Pre-play loading/task timing
and drive-tray/reset handling remain open.

## Open work

The audio count in [PORT_STATUS.md](PORT_STATUS.md) covers the explicitly
listed ledger mechanisms; it does not certify complete audio equivalence.
Remaining gaps are tracked in [the open items](OPEN_ITEMS.md#audio-aus-audio-portmd-details-audiomd):

- Phase-inverted program pan, ADSR key-follow, exclusive groups, velocity
  curves/crossfade, amplitude LFO and unrepresented Vagi/loop-address cases.
- SQ SysEx and the remaining live controller/NRPN/output-routing handlers.
  CC7/CC11/pan/pitch updates already reach active voices; this is not a blanket
  absence of all live controller modulation.
- The TSNDDRV loader thread remains open. Replacing a bank during active
  voices follows host policy: stop the port.
- ADX original float predictor/coefficient/rounding and stream scheduling.
  The previous integer predictor is still used; the original residual scale
  and output gain tables are now corrected. `GetAdxStatus` PREP has no native
  equivalent because clips load synchronously.
- Linear SPU interpolation and predecoded ADPCM predictor history at loop
  jumps remain unverified against the original output. Reverb lacks reference
  PCM acceptance. Audio production still depends on the 30-Hz host loop;
  late game frames can underrun the output queue.
- Film pre-play/cancel task scheduling and ranking audio's missing ranking
  owner. BATTLE GAME stays disabled under the existing user decision.
- Indoor ambient: the original stops BG sounds only through `bhBGSeOff`/
  `bhBGSeOff2` or `CheckSpecialBank`. Any further behavior of `TSNDDRV.IRX`
  remains unclear.

## References

- [FFmpeg adx.c](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adx.c),
  [vgmstream ADX](https://github.com/vgmstream/vgmstream/blob/master/src/meta/adx.c)
- [vgmstream HD/BD](https://github.com/vgmstream/vgmstream/blob/master/src/meta/hd_bd.c),
  [SPU ADPCM](https://github.com/vgmstream/vgmstream/blob/master/src/coding/psx_decoder.c)
- [VGMTrans SonyPS2InstrSet.cpp](https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/SonyPS2/SonyPS2InstrSet.cpp)
  (Vagi attribute and one-shot/loop),
  [PSX-SPX ADPCM flags](https://psx-spx.consoledev.net/soundprocessingunitspu/)
  (End/Repeat/Start)
- [vgmstream SShd](https://github.com/vgmstream/vgmstream/blob/master/src/meta/sshd.c),
  [stb_image](https://github.com/nothings/stb)

## Pitch, ADSR, and reverb evidence

Sony pitch (root note, fine tuning, pitchbend ranges, sample-rate scaling)
is connected through `audio_pitch.*` and the mixer loop and checked by
`audio_movie`. SPU2 ADSR envelopes and dual-core reverb are connected
through `audio_spu.*`, `spu_reverb_presets.inc`, and the mixer loop. Registers,
bit fields, and presets are supported by `LIBSD.IRX` (`.data+0x38/+0x64`),
`MODHSYN.IRX` (Key-On 0x3af4, voice selection 0x64f8, CC 120 0x5bf4),
`TSNDDRV.IRX` (`spucore_init`, `treq_ALL_REV`), and psx-spx (“SPU Volume and
ADSR Generator”, “SPU Reverb Formula”).

Integer Sony pan (`audio_pan.*`: `sony_pan_factors`, `sony_bank_pan`) and pitch
(`audio_pitch.*`, tables from LIBSD.IRX) are supported by permitted read-only
IOP references (LIBSD 0x4680/0x4698, pitch routine 0x33a0, MODHSYN
pitch/gain/pan structure). Verification: `audio_movie`
(`spu_envelope_reverb`), `sony_sq_dictionary`
(`tests/native/audio_sq_test.cpp`).
