#include "audio_spu.h"

#include <algorithm>
#include <stdexcept>

namespace recvx {
namespace {
#include "spu_reverb_presets.inc"

std::int16_t saturate(std::int32_t value) {
    return std::int16_t(std::clamp<std::int32_t>(value, -0x8000, 0x7fff));
}

// psx-spx "SPU Reverb Formula": 39-Tap-Filter fuer das Herunter- und Hochtasten.
constexpr std::int32_t reverb_fir[39] = {
    -0x0001, 0, 0x0002, 0, -0x000a, 0, 0x0023, 0, -0x0067, 0, 0x010a, 0, -0x0268, 0,
    0x0534, 0, -0x0b90, 0, 0x2806, 0x4000, 0x2806, 0, -0x0b90, 0, 0x0534, 0, -0x0268, 0,
    0x010a, 0, -0x0067, 0, 0x0023, 0, -0x000a, 0, 0x0002, 0, -0x0001,
};

// Registerreihenfolge 1F801DC0h.. (psx-spx "SPU Reverb Registers").
enum Reg : unsigned {
    dAPF1, dAPF2, vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL, vAPF1, vAPF2,
    mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2, dLSAME, dRSAME,
    mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4, dLDIFF, dRDIFF,
    mLAPF1, mRAPF1, mLAPF2, mRAPF2, vLIN, vRIN,
};
} // namespace

std::uint32_t spu_reverb_size(unsigned mode) {
    if (mode >= 10) throw std::runtime_error("SPU reverb mode outside libsd table");
    return spu_reverb_sizes[mode];
}
const std::uint16_t* spu_reverb_preset(unsigned mode) {
    if (mode >= 10) throw std::runtime_error("SPU reverb mode outside libsd table");
    return spu_reverb_registers[mode];
}

void SpuEnvelope::key_on(std::uint16_t a1, std::uint16_t a2) {
    adsr1 = a1; adsr2 = a2; phase = Attack; level = 0; counter = 0;
}

void SpuEnvelope::key_off() {
    if (phase == Stopped) return;
    phase = Release; counter = 0;
}

bool SpuEnvelope::tick() {
    if (phase == Stopped) return false;
    bool decrease = false, exponential = false;
    int shift = 0, step_field = 0, step = 0;
    std::int32_t target = 0;
    switch (phase) {
    case Attack:
        exponential = adsr1 >> 15; shift = (adsr1 >> 10) & 0x1f; step_field = (adsr1 >> 8) & 3;
        step = 7 - step_field; target = 0x7fff; break;
    case Decay:
        decrease = exponential = true; shift = (adsr1 >> 4) & 0xf; step = -8;
        target = ((adsr1 & 0xf) + 1) << 11; break;
    case Sustain:
        exponential = adsr2 >> 15; decrease = (adsr2 >> 14) & 1; shift = (adsr2 >> 8) & 0x1f;
        step_field = (adsr2 >> 6) & 3; step = decrease ? ~(7 - step_field) : 7 - step_field; break;
    case Release:
        decrease = true; exponential = (adsr2 >> 5) & 1; shift = adsr2 & 0x1f; step = -8; break;
    case Stopped: return false;
    }
    // Schrittwert und Schrittweite 11111b/11b: die Huellkurve steht und saettigt nicht.
    if (((shift << 2) | step_field) != 0x7f) {
        std::int32_t level_step = step * (1 << std::max(0, 11 - shift));
        std::uint32_t increment = 0x8000u >> std::max(0, shift - 11);
        if (exponential && !decrease && level > 0x6000) {
            if (shift < 10) level_step >>= 2;
            else if (shift >= 11) increment >>= 2;
            else { level_step >>= 1; increment >>= 1; }
        } else if (exponential && decrease) {
            level_step = (level_step * level) >> 15;
        }
        counter += std::max<std::uint32_t>(increment, 1);
        if (counter & 0x8000) {
            counter = 0;
            level += level_step;
            level = decrease ? std::max<std::int32_t>(level, 0) : std::min<std::int32_t>(level, 0x7fff);
        }
    }
    switch (phase) {
    case Attack: if (level >= target) phase = Decay; break;
    case Decay: if (level <= target) phase = Sustain; break;
    case Release: if (level <= 0) { phase = Stopped; level = 0; return false; } break;
    default: break;
    }
    return true;
}

void SpuReverb::set_effect(unsigned mode, std::int16_t depth, bool clear) {
    const auto* preset = spu_reverb_preset(mode);
    std::copy(preset, preset + 32, reg_.begin());
    // sceSdSetEffectAttr: ESA = EEA - (Groesse*8 - 2) Byte, also Groesse*4 Halbworte.
    const std::size_t halfwords = std::size_t(spu_reverb_size(mode)) * 4;
    if (clear) work_.assign(halfwords, 0);
    else work_.resize(halfwords, 0);
    nonzero_ = std::size_t(std::count_if(work_.begin(), work_.end(), [](std::int16_t x) { return x != 0; }));
    for (const auto* history : {&in_left_, &in_right_, &out_left_, &out_right_})
        nonzero_ += std::size_t(std::count_if(history->begin(), history->end(), [](std::int16_t x) { return x != 0; }));
    if (current_ >= work_.size()) current_ = 0;
    mode_ = mode;
    evol_ = depth;
}

std::int16_t& SpuReverb::at(std::int32_t offset) {
    const auto size = std::int64_t(work_.size());
    auto index = std::int64_t(current_) + offset;
    if (index >= size) index -= size;
    else if (index < 0) index += size;
    if (index < 0 || index >= size) { index %= size; if (index < 0) index += size; }
    return work_[std::size_t(index)];
}

// Only taps with a non-zero coefficient contribute (even k and the centre k = 19); skipping the
// zero products leaves every sum unchanged.
std::int16_t SpuReverb::down_sample(const std::array<std::int16_t, 64>& history) const {
    const unsigned base = position_ + 64 - 38;
    std::int64_t sum = std::int64_t(reverb_fir[19]) * history[(base + 19) & 63];
    for (unsigned k = 0; k < 39; k += 2) sum += std::int64_t(reverb_fir[k]) * history[(base + k) & 63];
    return saturate(std::int32_t(sum >> 15));
}

// The output history is zero-stuffed (one side per 48-kHz cycle). For the side computed in this
// cycle the samples sit on even k (20 outer taps, the centre product is zero); for the other side
// they sit on odd k, where only the centre tap 0x4000 is non-zero: (x * 0x4000) >> 14 == x.
std::int32_t SpuReverb::up_sample(const std::array<std::int16_t, 64>& history, bool computed) const {
    const unsigned base = position_ + 64 - 38;
    if (!computed) return history[(base + 19) & 63];
    std::int64_t sum = 0;
    for (unsigned k = 0; k < 39; k += 2) sum += std::int64_t(reverb_fir[k]) * history[(base + k) & 63];
    return saturate(std::int32_t(sum >> 14));
}

void SpuReverb::process(std::int16_t in_left, std::int16_t in_right, std::int32_t& out_left, std::int32_t& out_right) {
    const bool right = phase_ != 0;
    if (!nonzero_ && !in_left && !in_right) {
        out_left = out_right = 0;
        if (right) current_ = (current_ + 1) % work_.size();
        phase_ ^= 1;
        position_ = (position_ + 1) & 63;
        return;
    }
    store(in_left_[position_], in_left);
    store(in_right_[position_], in_right);
    auto v = [&](Reg r) { return std::int32_t(std::int16_t(reg_[r])); };
    auto a = [&](Reg r) { return std::int32_t(reg_[r]) * 4; }; // 8-Byte-Einheiten -> Halbworte
    auto mul = [](std::int64_t x, std::int64_t y) { return std::int32_t((x * y) >> 15); };
    // Eine Seite pro 48-kHz-Takt: links im geraden, rechts im ungeraden Takt.
    const std::int32_t input = mul(v(right ? vRIN : vLIN), down_sample(right ? in_right_ : in_left_));
    const Reg same = right ? mRSAME : mLSAME, diff = right ? mRDIFF : mLDIFF;
    const Reg same_src = right ? dRSAME : dLSAME, diff_src = right ? dLDIFF : dRDIFF;
    std::int32_t previous = at(a(same) - 1);
    store(at(a(same)), saturate(mul(input + mul(at(a(same_src)), v(vWALL)) - previous, v(vIIR)) + previous));
    previous = at(a(diff) - 1);
    store(at(a(diff)), saturate(mul(input + mul(at(a(diff_src)), v(vWALL)) - previous, v(vIIR)) + previous));
    const Reg comb[4] = {right ? mRCOMB1 : mLCOMB1, right ? mRCOMB2 : mLCOMB2, right ? mRCOMB3 : mLCOMB3, right ? mRCOMB4 : mLCOMB4};
    std::int32_t out = saturate(std::int32_t((std::int64_t(v(vCOMB1)) * at(a(comb[0])) + std::int64_t(v(vCOMB2)) * at(a(comb[1])) +
                                             std::int64_t(v(vCOMB3)) * at(a(comb[2])) + std::int64_t(v(vCOMB4)) * at(a(comb[3]))) >> 15));
    const Reg apf1 = right ? mRAPF1 : mLAPF1, apf2 = right ? mRAPF2 : mLAPF2;
    std::int32_t delayed = at(a(apf1) - a(dAPF1));
    out = saturate(out - mul(v(vAPF1), delayed));
    store(at(a(apf1)), std::int16_t(out));
    out = saturate(mul(out, v(vAPF1)) + delayed);
    delayed = at(a(apf2) - a(dAPF2));
    out = saturate(out - mul(v(vAPF2), delayed));
    store(at(a(apf2)), std::int16_t(out));
    out = saturate(mul(out, v(vAPF2)) + delayed);
    store((right ? out_right_ : out_left_)[position_], std::int16_t(out));
    store((right ? out_left_ : out_right_)[position_], 0);
    out_left = mul(up_sample(out_left_, !right), evol_);
    out_right = mul(up_sample(out_right_, right), evol_);
    if (right) current_ = (current_ + 1) % work_.size();
    phase_ ^= 1;
    position_ = (position_ + 1) & 63;
}

} // namespace recvx
