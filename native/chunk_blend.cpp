#include "chunk_blend.h"
#include <algorithm>
#include <stdexcept>
namespace recvx {
#include "chunk_blend.inc"
std::uint64_t ninja_blend_register(unsigned source,unsigned destination){
    if(source>12||destination>11)throw std::runtime_error("Ninja blend index outside platform table");
    const auto mode=alpha_tbl[source][destination];if(mode==0xffffffffull)throw std::runtime_error("unsupported original Ninja blend pair");return mode;
}
std::uint64_t chunk_blend_register(unsigned source,unsigned destination){
    if(source>7||destination>7)throw std::runtime_error("chunk blend index outside Ninja table");
    const auto mode=alpha_tbl[lNaCnkSrcAlphaMode[source]][lNaCnkDstAlphaMode[destination]];
    if(mode==0xffffffffull)throw std::runtime_error("unsupported original Ninja chunk blend pair");
    return mode;
}
std::uint32_t blend_gs(std::uint64_t mode,std::uint32_t source,std::uint32_t destination,float source_alpha){
    const unsigned a=mode&3,b=(mode>>2)&3,c=(mode>>4)&3,d=(mode>>6)&3;
    const float alpha=std::clamp(source_alpha,0.f,1.f);
    const float factor=c==0?alpha:c==1?float(destination>>24)/255.f:float((mode>>32)&255)/128.f;
    std::uint32_t result=std::uint32_t(alpha*255.f+.5f)<<24;
    for(unsigned shift:{0u,8u,16u}){
        auto color=[&](unsigned selector){return selector==0?float((source>>shift)&255):selector==1?float((destination>>shift)&255):0.f;};
        result|=std::uint32_t(std::clamp((color(a)-color(b))*factor+color(d)+.5f,0.f,255.f))<<shift;
    }
    return result;
}
}
