# Native audio and movies 0.5

The gameplay runtime needs SDL2 and the packaged loose assets. It does not invoke
FFmpeg, open ISO/AFS containers or emulate PS2 sound hardware. Movie conversion
uses FFmpeg/ffprobe only at build time on already dumped regular PSS files.

## ADX

`native/audio.cpp` independently decodes unencrypted ADX coding type 3, version 3,
with 18-byte four-bit blocks, one/two channels and the original high-pass predictor.
Sample counts, header offsets and block bounds are validated. Coefficients use
the same floating-point rounding/scaling as the reference decoder. Version 4,
encrypted ADX and other codecs are rejected rather than guessed.

Original BGM1 and VOICE1 IDs map to `data/archives/<name>/<four-digit-id>.bin`.
An ADX replacement can be placed at `mods/audio/<name>/<id>.adx`. The eight-slot
mixer emits 48 kHz signed 16-bit stereo: linear resampling, volume, fades, summed
clipping and whole-clip looping. Original ADX loop points are not yet applied.
BGM uses slot 0, dialogue slot 1, and movie PCM slot 7. GUI audio is queued to SDL;
headless runs mix deterministic ticks for playback conditions. A missing output
device is reported and simulation can continue.

`recvx-audio <input.adx> <new.wav>` exports PCM for comparison. VOICE1/0000 decoded
504,141 mono samples at 48 kHz and matched FFmpeg PCM byte-for-byte through the
original declared sample count (FFmpeg's final block padding is excluded).
SHA256 of that native PCM: `ab2d49e1807a8e98f03e7a79caa524cbdeb6bc8b32c37d760644bb5017779422`.

References for ADX structure and predictor:
[FFmpeg adx.c](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adx.c),
[adxdec.c](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adxdec.c).

## System sound bank

The native decoder reads COMMON.MLT's loose Sony HD/BD wrapper, validates the
Prog → Sset → Smpl → Vagi mapping and decodes the original mono SPU-ADPCM frames.
All eleven local system programs produce non-silent PCM at 32 kHz. Menu cursor,
confirm/cancel, inventory opening, pickup choices and bhCallSysSe use these
programs, with cached clips on mixer slots 2/3. A bank replacement can be placed
at `mods/audio/system/COMMON.MLT`; no generated beep is substituted.

Format references: [vgmstream HD/BD parser](https://github.com/vgmstream/vgmstream/blob/master/src/meta/hd_bd.c)
and [SPU-ADPCM decoder](https://github.com/vgmstream/vgmstream/blob/master/src/coding/psx_decoder.c).
This native decoder uses integer predictor rounding; exact PS2 SPU interpolation,
envelopes, pitch, pan, layered programs and waveform loop points are pending.
Dialog confirmation stops slot 1 and clears queued output so the next script
voice wait can finish. BGM and movie slots are independent.

## PSS films

All 18 local films are converted to `data/movies/<id>.rvv`, with JSON provenance
containing source SHA256, frame count/rate and audio count. The Python cooker
extracts the private Sony SShd/SSbd stereo PCM stream and deinterleaves 512-byte
channel blocks without dropping samples. FFmpeg converts video frames to JPEG
with their original frame count/rate; the runtime stretches the original image
to the game's display aspect. JPEG recompression is lossy.

RVV1 stores a 36-byte little-endian header (magic, dimensions, frame-rate numerator/
denominator, video frame count, PCM rate, channel count, PCM frame count), followed
by interleaved PCM16 and length-prefixed JPEG frames. Native reading validates
sizes, rates, dimensions, JPEG boundaries and trailing bytes. It decodes JPEG
using vendored stb_image pinned in native/third_party/stb/PROVENANCE.json.
The runtime advances on simulation ticks and holds the final video frame until
both video and PCM durations finish. The original Movie/MovieCk commands control
playback and the event wait. Replacement RVV files use `mods/movies/<id>.rvv`.

Movie 0000 contains 6,306 frames at 30000/1001 fps and 10,100,480 stereo PCM frames
at 48 kHz. Picture duration is 210.4102 seconds; audio is 210.4267 seconds.
Longer original audio tails in other movies are also retained. This is clocked
native playback; drift against a physical audio device needs further profiling.

```sh
python3 tools/cook_movies.py --data data
python3 tools/prepare_native_data.py --output data --refresh
python3 tools/prepare_native_data.py --output data --verify
```

The original PCM header/block layout was checked against
[vgmstream sshd.c](https://github.com/vgmstream/vgmstream/blob/master/src/meta/sshd.c).
The JPEG dependency is [stb_image](https://github.com/nothings/stb); its license is
included in each package. FFmpeg is an external conversion dependency, not linked
into or required by the packaged runtime.

## Limits and tests

Footsteps, world action/weapon sounds and ambient MULTSPQ/EasySE bank routing are
not yet implemented. Volume mapping and procedural light/audio coordination
remain approximate. Voice behavior beyond the opening requires checking against
the original CRI command paths, including language/stereo archive variants.

The audio_movie unit test exercises SPU signed nibble order, shift and malformed
frames; its data mode checks all original system programs and bounds. It also exercises ADX signed stereo output, truncation/header
rejection, resampling, mixing/clipping/fades, invalid clip rejection and native
JPEG/PCM film completion. Real-data game tests assert the Rodrigo voice dispatch
and event playback conditions. A test passing does not imply all game sound
systems have been ported.
