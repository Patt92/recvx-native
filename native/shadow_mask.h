#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace recvx {
// Ps2ShadowMain0/1: FIX=128, source RGB=32, PSMCT16S target.
// Store the expanded 5-bit channel, not a count of covering triangles.
// Saturation occurs after EACH fragment, before the next polygon.
inline unsigned shadow_mask_fragment(unsigned mask,bool subtract){
    const int value=int(mask)+(subtract?-32:32);
    return unsigned(std::clamp(value,0,255))&0xf8u;
}

// Ps2ShadowDraw: grayscale texture MODULATE, RGB=(unsigned(fog)*3)/16,
// then the retained Main1 blend subtracts source from destination.
// TEXA AEM discards the zero mask. Destination alpha is retained here:
// the native scene buffer represents an opaque presented RGB surface.
inline std::uint32_t shadow_mask_composite(
    std::uint32_t destination,unsigned mask,unsigned fog){
    if(mask>248u||(mask&7u)||fog>255u)
        throw std::runtime_error("shadow: invalid mask or fog channel");
    if(mask==0)return destination;
    const unsigned shade=(fog*3u)/16u;
    const unsigned reduction=(mask*shade)/128u;
    std::uint32_t result=destination&0xff000000u;
    for(unsigned shift=0;shift<24;shift+=8){
        const unsigned channel=(destination>>shift)&255u;
        result|=(channel>reduction?channel-reduction:0u)<<shift;
    }
    return result;
}
}
