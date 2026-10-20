#pragma once
#include "ninja_math.h"
#include <cstdint>

namespace recvx {
// One bhSetEasyDirLight invocation, not a frame step. Reference: light.c:279-328;
// SLUS_201.84 0x2810e0-0x28126c. Borrow the existing room light zero's fields;
// no stored work, colour, time base, draw count, pause gate or RNG lives here.
// clock/speed are bits because RoomWorld::Light stores ct0/aspd as unsigned;
// the original adds modulo 32 bits and tests the resulting ct0 as signed.
// The caller applies the returned intensity to BOTH Easy and Simple, with
// white colour, ambient .2 and camera-space direction (0,.3,.7). Neither the
// light's RGB nor its cached working colour is changed by this operation.
inline constexpr float step_easy_dir_light(float intensity,std::uint32_t type,
                                           std::uint32_t speed_bits,std::uint32_t& flags,
                                           std::uint32_t& mode,std::uint32_t& clock_bits){
    if(!(flags&2u))return intensity;
    if(type==100u){
        if(mode==0u){++mode;return intensity*3.0f;}
        if(mode==1u)flags&=~2u;
        return intensity;
    }
    if(type!=101u)return intensity;
    // Explicit, defined signed interpretation of the owner bits (also INT_MIN).
    const auto angle=clock_bits<=0x7fffffffu?std::int32_t(clock_bits):
        std::int32_t(std::int64_t(clock_bits)-0x100000000LL);
    const float pulse=3.0f*njCos(angle);
    clock_bits+=speed_bits<<8;
    if(!(clock_bits&0x80000000u)&&clock_bits>16383u)flags&=~2u;
    return pulse; // type 101 intentionally ignores the caller's intensity
}
}
