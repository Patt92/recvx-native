#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace recvx {
// Original NJD_CB_BA/material head bits -> GS ALPHA. Invalid pairs diagnose
// the same unsupported mode as njColorBlendingModeSys, rather than substituting.
std::uint64_t chunk_blend_register(unsigned source,unsigned destination);
// GS RGB = (A - B) * C + D; framebuffer alpha is the incoming fragment alpha.
std::uint32_t blend_gs(std::uint64_t mode,std::uint32_t source,std::uint32_t destination,float source_alpha);
// ps2_NaSystem.c:169-425 njColorBlendingMode/njColorBlendingModeSys. The state is
// Ps2_nj_save_current.mode_bk[0..1]/set_last plus the ALPHA register and the
// mode pair of Ps2_gs_save: index 0 and 1 write 0x01/0x44 without a table
// lookup, a pair holding an index below two leaves the register unchanged, and
// the register therefore persists across draws (the original never reads a
// register back, but every primitive draws with the one set last).
void color_blending_mode(int target,int mode);
void color_blending_mode_sys(int source,int destination);
std::uint64_t color_blending_alpha();
// Pairs the original ends in njColorBlendingModeSys' "ALPHA ERROR" abort. The
// port reports them and keeps the register instead of substituting a mode.
const std::vector<std::string>& color_blending_unavailable();
}
