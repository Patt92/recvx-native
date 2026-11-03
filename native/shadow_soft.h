#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace recvx {
// Host Ultra quality only (video.ini quality = ultra), NOT original behaviour: the
// bhDrawMdfEffect mask (shadow_raster.h) is rastered at the render resolution and its
// edge is softened before the original Ps2ShadowDraw composite. The shadow geometry,
// depth test, saturation and darkening stay the original ones; only the mask value
// at a pixel becomes a weighted mean of its neighbourhood. Neighbours count only when
// their receiver lies on the same surface (transported depth within 1/8 of the centre
// plus one unit), so the edge does not bleed across depth steps onto walls or casters.
// Fixed-point weights keep the CPU rasterer and the GPU shaders identical.
constexpr int shadow_soft_radius=3;                          // taps -3..3 per axis
constexpr unsigned shadow_soft_weights[shadow_soft_radius+1]={64,51,26,9}; // Gauss, sigma 1.5 taps

// Tap spacing in mask pixels: about 2/3 of an original pixel, so the penumbra spans about
// two original 640x480 pixels on each side of the edge at any supersampling factor.
inline unsigned shadow_soft_step(unsigned scale){return std::max(1u,2u*scale/3u);}
// Extension of the composite rectangle in original pixels: the blur reaches this far out.
inline float shadow_soft_reach(unsigned scale){return float(unsigned(shadow_soft_radius)*shadow_soft_step(scale))/float(scale);}

inline bool shadow_soft_same_surface(float centre,float other){
    return other>0.f&&std::fabs(other-centre)<=centre*0.125f+1.f;
}

// mask(x,y): original 5-bit-expanded mask value (0..248) of the mask pixel; depth(x,y): its
// transported receiver depth (0 = nothing drawn). Returns the softened value 0..248.
template<class Mask,class Depth>
unsigned shadow_soft_value(Mask mask,Depth depth,int x,int y,int width,int height,unsigned scale){
    const float centre=depth(x,y);
    if(!(centre>0.f))return mask(x,y);
    const int step=int(shadow_soft_step(scale));
    std::uint32_t sum=0,weight=0;
    for(int j=-shadow_soft_radius;j<=shadow_soft_radius;++j){
        const int py=y+j*step;if(py<0||py>=height)continue;
        for(int i=-shadow_soft_radius;i<=shadow_soft_radius;++i){
            const int px=x+i*step;if(px<0||px>=width)continue;
            if(!shadow_soft_same_surface(centre,depth(px,py)))continue;
            const unsigned w=shadow_soft_weights[std::abs(i)]*shadow_soft_weights[std::abs(j)];
            sum+=w*mask(px,py);weight+=w;
        }
    }
    return (sum+weight/2u)/weight;
}

// Ps2ShadowDraw darkening (shadow_mask.h) for any mask value 0..248, also softened ones.
inline std::uint32_t shadow_mask_reduce(std::uint32_t destination,unsigned mask,unsigned fog){
    if(mask>248u||fog>255u)throw std::runtime_error("shadow: invalid mask or fog channel");
    if(mask==0)return destination;
    const unsigned reduction=(mask*((fog*3u)/16u))/128u;
    std::uint32_t result=destination&0xff000000u;
    for(unsigned shift=0;shift<24;shift+=8){
        const unsigned channel=(destination>>shift)&255u;
        result|=(channel>reduction?channel-reduction:0u)<<shift;
    }
    return result;
}
}
