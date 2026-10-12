#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace recvx {

// Installed rendering state, owned by RoomWorld and copied to the scene. No
// second camera/work owner: sync.c:143-255 decides when to generate and enable.
// Color is installed separately by that owner (njSetFogColor); this algorithm
// only supplies the 0..255 vertex power used by njCalcFogPower.
struct FogState {
    bool enabled = false;
    std::array<float,128> table{};
    float density = 0.f;
};

namespace fog_detail {
static_assert(sizeof(float)==sizeof(std::uint32_t) &&
              std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits==24,
              "fog requires IEEE binary32 storage");

inline std::uint32_t bits(float value)
{
    std::uint32_t result;
    std::memcpy(&result,&value,sizeof(result));
    return result;
}

inline float from_bits(std::uint32_t value)
{
    float result;
    std::memcpy(&result,&value,sizeof(result));
    return result;
}
} // namespace fog_detail

// Independent native implementation of the algorithm in ps2_NaFog.c:55-152,
// checked against ELF njGenerateFogTable3 0x002d6110..0x002d63cc. The original
// ignores its fog pointer and writes the installed global table; here the
// caller provides the native owner explicitly. Generation does NOT enable fog.
//
// n/f are the signed camera fog parameters, in the same units as view_z. No
// absolute-value/sign correction, near/far ordering repair or rescaling occurs
// here. In particular n>=f and n>=f/256 are real original branches, not errors.
// Nonfinite parameters and an unusable density exponent are diagnosed rather
// than replaced by an invented table. The original has no validation for them:
// frexpf/fptosi and 1<<count are outside the portable camera-input contract.
inline void fog_generate(FogState& state,float n,float f)
{
    if(!std::isfinite(n)||!std::isfinite(f))
        throw std::runtime_error("fog generation: nonfinite camera parameters");

    float density;
    if(n < -65535.f) {
        density=65536.f;
    } else {
        int exponent=0;
        const float fraction=std::frexp(-f,&exponent);
        const int power_exponent=exponent-1;
        if(!(f<0.f)||power_exponent<0||power_exponent>30)
            throw std::runtime_error("fog generation: unsupported density exponent/sign");

        // ELF 0x2d6170..0x2d61cc: truncate the frexp mantissa, keep FF00,
        // OR in exponent-1, then reconstruct. Do not round f, use fabs(f),
        // or replace this quantization with the unquantized far distance.
        const auto mantissa=static_cast<std::uint32_t>(fraction*65536.f);
        const auto packed=(mantissa&0xff00u)|
                          static_cast<std::uint32_t>(power_exponent);
        const auto power=std::uint32_t{1}<<power_exponent;
        const float fractional=static_cast<float>(packed&0x7f00u)/32768.f;
        const auto integral=(packed>>15)*power;
        const volatile float product=fractional*static_cast<float>(power);
        density=static_cast<float>(integral)+product;
    }

    // Keep the original reciprocal-Z knot lattice (8 groups of 16), not a
    // linear-in-distance fog ramp. The fallback seed 1 is intentional.
    const float seed=(f/256.f)>n ? -f : 1.f;
    std::array<float,128> table;
    std::size_t active=0;
    for(std::size_t i=0;i<table.size();++i) {
        const float power=static_cast<float>(std::uint32_t{1}<<(i/16));
        const float mantissa=static_cast<float>(i&15u)+16.f;
        const float lattice=(power*mantissa)/16.f;
        table[i]=seed/lattice;
        if(table[i]>=-n)++active;
    }

    if(seed==-f) {
        if(active>1) {
            const float denominator=f+table[active-1];
            for(std::size_t i=0;i<active;++i)
                table[i]=1.f-((f+table[i])/denominator);
        } else if(active==1) {
            table[0]=1.f;
        }
        for(std::size_t i=active;i<table.size();++i)table[i]=0.f;
    }

    for(float& value:table) {
        // Separate MUL and SUB as at 0x2d63a0/0x2d63a4; host FMA changes bits.
        const volatile float product=255.f*value;
        value=255.f-product;
        if(!std::isfinite(value))
            throw std::runtime_error("fog generation: nonfinite table entry");
    }
    // Invalid input leaves the installed state untouched, never disables it.
    state.table=table;
    state.density=density;
}

// ps2_NaFog.c:213-276; ELF njCalcFogPower 0x002d6520..0x002d66a4.
// Positive view_z is forward distance in the ORIGINAL camera units, not 1/z.
// Negative view_z and +0 reach table[127]; -0 reaches table[0]. These are the
// original sign test / division / saturation semantics, not clipping rules.
// Nonfinite native inputs or an enabled-but-uninstalled state are diagnosed;
// neither is silently treated as disabled. Disabled alone bypasses sampling.
inline float fog_sample(const FogState& state,float view_z)
{
    if(!state.enabled)return 255.f;
    if(!std::isfinite(view_z)||!std::isfinite(state.density)||state.density<=0.f)
        throw std::runtime_error("fog sample: invalid depth or uninstalled density");

    constexpr float maximum=0x1.fffff2p+7f; // ELF literal 0x437ffff9
    float coordinate;
    if(view_z<0.f) {
        coordinate=maximum;
    } else if(view_z==0.f) {
        // Native IEEE infinities and EE finite division saturation both reach
        // these same bounds, including the signed-zero distinction.
        coordinate=std::signbit(view_z) ? 1.f : maximum;
    } else {
        coordinate=state.density/view_z;
        if(coordinate<1.f)coordinate=1.f;
        else if(coordinate>maximum)coordinate=maximum;
    }

    const auto encoded=fog_detail::bits(coordinate);
    int index=static_cast<int>((encoded>>19)&15u)|
              ((static_cast<int>(encoded>>23)-127)*16);
    if(index<0)index=0;
    else if(index>127)index=127;
    if(index==127)return state.table[127]; // Never read nonexistent entry 128.

    const auto group=static_cast<unsigned>(index)/16;
    const auto knot_bits=((group+127u)<<23)|
                         ((static_cast<unsigned>(index)&15u)<<19);
    const float knot=fog_detail::from_bits(knot_bits);
    constexpr std::array<float,8> inverse_step{{16.f,8.f,4.f,2.f,1.f,.5f,.25f,.125f}};
    const float fraction=inverse_step[group]*(coordinate-knot);
    // ELF 0x2d6660..0x2d6698: subtract, multiply weight, subtract from 1,
    // weighted pair and add. Preserve the operation order without host FMA.
    const volatile float left=(1.f-fraction)*state.table[static_cast<unsigned>(index)];
    const volatile float right=fraction*state.table[static_cast<unsigned>(index)+1];
    return left+right;
}

} // namespace recvx
