#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace recvx {
// Only the vertex transport contract, not GS fragment interpolation.
// References:
// ps2_NaView.c:142 njClipZ: signed negative near/far.
// ps2_NaDraw2D.c:10 CalcPs2ZbuffAB.
// CvVMod ELF 0x002D1DF8 / 0x002D1E30 produces positive 1/view_z.
// Ps2AddPrim3DMod ELF 0x002CC7AC..0x002CC7C4:
// VADDA replaces ACC.z with -a (the guard accumulator does not carry over),
// VMADDX writes -a + reciprocal-Z * -b to the vertex, then clamp / vftoi4.
// This is NOT the scalar Ps2AddPrim clip_3d_on branch selector.
// PACKED XYZF2 takes bits 68..91: lane Z bits 4..27.
struct ShadowDepthCoefficients {
    float offset;
    float scale;
};

inline ShadowDepthCoefficients shadow_depth_coefficients(
    float signed_near,float signed_far)
{
    if(!std::isfinite(signed_near)||!std::isfinite(signed_far))
        throw std::runtime_error("shadow depth: nonfinite clip input");

    // Preserve the two original setter limits, not symmetric clamping.
    if(signed_near>-1.f)signed_near=-1.f;
    if(signed_far<-65535.f)signed_far=-65535.f;

    // Invalid ordering is diagnosed, never replaced by a guessed far plane.
    if(!(signed_far<signed_near))
        throw std::runtime_error("shadow depth: invalid clip ordering");

    const float denominator=signed_far-signed_near;
    const float a=(-65536.f*signed_near)/denominator;
    const float b=((65536.f*signed_near)*signed_far)/denominator;
    return {-a,-b};
}

inline std::uint32_t shadow_packed_vertex_depth(float depth)
{
    if(!std::isfinite(depth))
        throw std::runtime_error("shadow depth: nonfinite vertex value");
    const float bounded=std::min(std::max(depth,0.f),65534.f);
    const auto lane=static_cast<std::uint32_t>(std::trunc(bounded*16.f));
    return (lane>>4)&0x00ffffffu;
}

inline std::uint32_t shadow_vertex_depth(
    const ShadowDepthCoefficients& coefficients,float reciprocal_z)
{
    if(!std::isfinite(reciprocal_z)||reciprocal_z<=0.f)
        throw std::runtime_error("shadow depth: invalid reciprocal Z");
    // A native compiler must not contract the two original operations.
    const volatile float product=reciprocal_z*coefficients.scale;
    const float value=coefficients.offset+product;
    return shadow_packed_vertex_depth(value);
}

// Ps2ShadowStart / Ps2AddPrim3DMod use GEQUAL and never write receiver Z.
// Both operands must already be transported/interpolated depth, not raw
// reciprocal Z. Quantization ties and saturation are intentionally accepted.
inline bool shadow_depth_pass(std::uint32_t modifier,std::uint32_t receiver)
{
    return modifier>=receiver;
}

// Ps2AddPrim else-branch (ps2_dummy.c:432): sz = Ps2_zbuff_a + Ps2_zbuff_b / z.
// This scalar transport is NOT a selectable Ps2AddPrim3DMod branch. The ELF
// confirms that Mod always uses -a-b*fQ, where CvVMod produces positive 1/z.
// Keep this separate helper for the scalar caller's distinct input contract.
inline std::uint32_t shadow_vertex_depth_linear(
    float raw_a,float raw_b,float view_z)
{
    if(!std::isfinite(raw_a)||!std::isfinite(raw_b))
        throw std::runtime_error("shadow depth: nonfinite linear coefficients");
    if(!std::isfinite(view_z)||view_z==0.f)
        throw std::runtime_error("shadow depth: invalid linear view Z");
    return shadow_packed_vertex_depth(raw_a+(raw_b/view_z));
}
}
