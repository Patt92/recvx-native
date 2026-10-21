#include "audio_pan.h"
#include <algorithm>
#include <cstdlib>
namespace recvx {
SonyPan sony_pan_factors(int pan){
    SonyPan result;
    if(pan>=127)result.left=0;
    else if(pan>64)result.left=std::uint8_t((127-pan)*128/63);
    else if(pan<=1)result.right=0;
    else if(pan<64)result.right=std::uint8_t((pan-1)*128/63);
    return result;
}
int sony_bank_pan(int program,int split,int sample,int key_pan){
    return std::clamp(std::abs(program)+std::abs(split)+std::abs(sample)-128+key_pan,0,127);
}
unsigned sony_controller_scale(unsigned value){return (std::min(value,127u)<<7)/127;}
SonyLevels sony_voice_levels(std::uint32_t bank_volume,unsigned velocity,unsigned channel_volume,unsigned expression,int bank_pan,unsigned pan_controller){
    // Note-on 0x38c8..0x3920: +22 = ((prog+6 * split+16 * sample+16 * velcurve(velocity)) >> 14) * crossfade >> 16;
    // velocity curve 0 returns the velocity (0x86b4), crossfade 0x4680 returns 0x10000 without crossfade flags.
    const std::uint32_t note=std::uint16_t((bank_volume*std::min(velocity,127u))>>14);
    // 0x426c: +24 = (channel +43 * voice +84 * +22) >> 14.
    const std::uint32_t base=std::uint16_t((channel_volume*expression*note)>>14);
    // 0x40f0: ((+24 + +222) * port master) >> 8, +222 = 0 from key-on (0xa088), master 256; the amplitude
    // LFO term ((+176 * depth) >> 7) + 16384 stays 16384 because +176 is 0 without an amplitude LFO.
    const std::uint32_t level=(base*256u)>>8;
    // CC10 handler 0x0c18: value - 64, below -63 -> -63; 0x4534 clamps to -63..63 and adds voice +30 - 64.
    // p >= 63 silences left, p <= -63 right (0x45f0..0x4674); that is sony_pan_factors(p + 64).
    const int offset=std::clamp(int(pan_controller)-64,-63,63);
    const auto pan=sony_pan_factors(std::clamp(bank_pan+offset,0,127));
    SonyLevels out;
    out.left=std::uint16_t(std::min<std::uint32_t>(16383u,(pan.left*level)>>7));
    out.right=std::uint16_t(std::min<std::uint32_t>(16383u,(pan.right*level)>>7));
    return out;
}
}
