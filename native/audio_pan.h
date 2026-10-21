#pragma once
#include <cstdint>
namespace recvx {
struct SonyPan {std::uint8_t left=128,right=128;};
SonyPan sony_pan_factors(int pan);
int sony_bank_pan(int program,int split,int sample,int key_pan);
// MODHSYN.IRX CC7/CC11 handlers (.text 0x978/0x9d4): channel +43/+44 = (value << 7) / 127, 0..128.
unsigned sony_controller_scale(unsigned value);
// MODHSYN voice output register VOLL/VOLR (0..16383, SPU2 fixed volume; the voice plays at value/16384).
struct SonyLevels {std::uint16_t left=0,right=0;};
// bank_volume = program +6 * split +0x10 * sample +0x10 (note-on 0x38c8..0x3920 with velocity curve 0 and
// no crossfade, both the only values in the game's banks); channel_volume/expression are the scaled CC7/CC11
// (sony_controller_scale); bank_pan is the combined bank pan 0..127 (sony_bank_pan); pan_controller the
// channel CC10 0..127. Port master volume 256 (TSNDDRV sets every port to 256, sceHSyn_SetVolume), no
// amplitude LFO (sample LFO attribute & 0x70 is 0 in every bank).
SonyLevels sony_voice_levels(std::uint32_t bank_volume,unsigned velocity,unsigned channel_volume,unsigned expression,int bank_pan,unsigned pan_controller);
// Same with the voice's pan offset (MODHSYN 0x4534 argument: CC10 - 64 or HsMsg pan - 64, clamped to -63..63).
SonyLevels sony_voice_levels_offset(std::uint32_t bank_volume,unsigned velocity,unsigned channel_volume,unsigned expression,int bank_pan,int pan_offset);
// MODMIDI.IRX 0xf30: CC7 sent to MODHSYN = min(127, (sequence CC7 * channel volume 128 * port volume) >> 14);
// the port volume is the EE MIDI volume (sceMidi_MidiSetVolume(port, 255, vol), 0x1d64).
unsigned sony_midi_cc7(unsigned sequence_cc7,unsigned port_volume);
}
