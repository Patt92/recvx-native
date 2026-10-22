#pragma once
// Decoded Sony HD/BD and ADX sound data shared by the EE-side mixer (audio.h) and the IOP model (audio_iop.h).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace recvx {
// volume/pan: original Sony HD program (+6/+7) and split (+0x10/+0x11) values,
// 0..127 each; ADX clips keep 127/64 (unity, centre).
struct SonyPitch {int root=60,note=60,fine=0;unsigned bend_down=0,bend_up=0;};
// spu: Sony-HD-Sample (MODHSYN-Stimme) mit ADSR1/ADSR2 (+0x12/+0x14), sampleSpuAttr (+0x29) und
// Stimmenprioritaet Smpl +0x0F (MODHSYN 0x6228: Prioritaet = Smpl +0x0F + Port +0).
struct PcmClip {unsigned rate=0,channels=0;std::optional<SonyPitch> sony_pitch;bool spu=false;std::uint16_t adsr1=0,adsr2=0;std::uint8_t spu_attr=0,priority=0;std::size_t loop_start=0,loop_end=0;std::vector<std::int16_t> samples;float volume=1;unsigned pan=64;std::uint32_t sony_volume=0;bool se_shot=false;std::size_t frames()const{return channels?samples.size()/channels:0;}};
PcmClip decode_ps_adpcm(const std::vector<std::uint8_t>& bytes,unsigned rate);
// key<0 selects the first split; otherwise the split whose key range holds `key`. For key>=0 (MODHSYN
// note-on) a missing/empty program or a key no split covers yields an empty clip: no voice, as in the driver.
// A key several splits cover throws: use decode_system_note.
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bank,unsigned program,int key=-1);
// MODHSYN note-on (.text 0x5f90, split loop 0x6098..0x64a0): one clip per voice the note keys, i.e. per
// split whose key range holds `key`, in split-table order; empty when no voice starts.
std::vector<PcmClip> decode_system_note(const std::vector<std::uint8_t>& bank,unsigned program,int key,unsigned velocity=127);
PcmClip decode_adx(const std::vector<std::uint8_t>& bytes);
}
