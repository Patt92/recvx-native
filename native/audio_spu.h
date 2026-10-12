#pragma once
// SPU2-Stimmenhuellkurve (ADSR) und Hall-Einheit, plattformunabhaengig.
//
// Originalbelege (Details in docs/AUDIO.md, Abschnitt "SPU2-Huellkurve und Hall"):
// - MODHSYN.IRX Key-On (.text 0x3af4..0x3e00): ADSR1/ADSR2 = Sample-Parameter +0x12/+0x14
//   (Key-Follow-Tiefen +0x16..+0x1e sind in allen Originalbanken 0), Mischschalter aus
//   sampleSpuAttr +0x29: Bit0 VMIXL, Bit1 VMIXR, Bit2 VMIXEL, Bit3 VMIXER.
// - MODHSYN.IRX Stimmenwahl 0x64f8: attr&0x30 == 0x10 -> Kern 0, == 0x20 -> Kern 1, sonst
//   der Kern mit mehr freien Stimmen (je 24 bei Init 0x3170), Gleichstand Kern 0.
// - MODHSYN.IRX CC 120 (0x5bf4): ADSR2 := 0xC021, danach Key-Off aller Stimmen des Kanals;
//   TSNDDRV Tsnd_tqcancel/port_stop senden CC 120 fuer sdShotStop/sdMidiStop.
// - Huellkurvenarithmetik: SPU-ADSR-Generator (psx-spx "SPU Volume and ADSR Generator",
//   PCSX2 SPU2/ADSR.cpp: Phasenwechsel nach dem Schritt, Decay endet bei Level <= Ziel).
//   Die SPU2 fuehrt die Huellkurve mit 48 kHz, dem Mixertakt.
// - Hall: TSNDDRV spucore_init/treq_ALL_REV, LIBSD.IRX sceSdSetEffectAttr und Presets
//   (spu_reverb_presets.inc), Hallformel und 39-Tap-Resampling nach psx-spx "SPU Reverb
//   Formula"; die Halleinheit rechnet mit halber Ausgaberate (24 kHz), links und rechts
//   abwechselnd.
#include <array>
#include <cstdint>
#include <vector>

namespace recvx {

struct SpuEnvelope {
    enum Phase : std::uint8_t { Attack, Decay, Sustain, Release, Stopped };
    std::uint16_t adsr1 = 0, adsr2 = 0;
    Phase phase = Stopped;
    std::int32_t level = 0;   // ENVX, 0..0x7fff
    std::uint32_t counter = 0;
    void key_on(std::uint16_t a1, std::uint16_t a2);
    void key_off();
    // Ein 48-kHz-Schritt. false, sobald Release 0 erreicht hat (Stimme aus).
    bool tick();
    bool sounding() const { return phase != Stopped; }
};

// Ein SPU2-Kern-Halleffekt (Arbeitsbereich ESA..EEA als Ringpuffer in Halbworten).
class SpuReverb {
public:
    // sceSdSetEffectAttr(core, {mode, depth, depth, 0, 0}) mit mode < 10; clear = Modusbit 0x100.
    void set_effect(unsigned mode, std::int16_t depth, bool clear);
    // treq_ALL_REV bei gleichem Modus: nur EVOLL/EVOLR.
    void set_depth(std::int16_t depth) { evol_ = depth; }
    unsigned mode() const { return mode_; }
    std::int16_t depth() const { return evol_; }
    // Ein 48-kHz-Schritt: Effekteingang des Kerns (gesaettigt), Hallausgang * EVOL >> 15.
    void process(std::int16_t in_left, std::int16_t in_right, std::int32_t& out_left, std::int32_t& out_right);

private:
    std::int16_t& at(std::int32_t offset);
    // Every write goes through store() so nonzero_ counts the non-zero samples of the work area
    // and the four filter histories; at zero state each step provably yields zero (exact skip).
    void store(std::int16_t& slot, std::int16_t value) { nonzero_ += std::size_t(value != 0) - std::size_t(slot != 0); slot = value; }
    std::size_t nonzero_ = 0;
    std::int16_t down_sample(const std::array<std::int16_t, 64>& history) const;
    std::int32_t up_sample(const std::array<std::int16_t, 64>& history, bool computed) const;
    std::array<std::uint16_t, 32> reg_{};
    std::vector<std::int16_t> work_ = std::vector<std::int16_t>(8, 0);
    std::size_t current_ = 0;
    unsigned mode_ = 0, phase_ = 0, position_ = 0;
    std::int16_t evol_ = 0;
    std::array<std::int16_t, 64> in_left_{}, in_right_{}, out_left_{}, out_right_{};
};

// Feste Werte aus LIBSD.IRX (.data+0x38/+0x64), siehe spu_reverb_presets.inc.
std::uint32_t spu_reverb_size(unsigned mode);
const std::uint16_t* spu_reverb_preset(unsigned mode);

} // namespace recvx
