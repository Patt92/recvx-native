#pragma once
#include <cstdint>
namespace recvx {
// Original NJD_CB_BA/material head bits -> GS ALPHA. Invalid pairs diagnose
// the same unsupported mode as njColorBlendingModeSys, rather than substituting.
// Direct njColorBlendingMode indices, e.g. effect 8/10.
std::uint64_t ninja_blend_register(unsigned source,unsigned destination);
std::uint64_t chunk_blend_register(unsigned source,unsigned destination);
// GS RGB = (A - B) * C + D; framebuffer alpha is the incoming fragment alpha.
std::uint32_t blend_gs(std::uint64_t mode,std::uint32_t source,std::uint32_t destination,float source_alpha);
}
