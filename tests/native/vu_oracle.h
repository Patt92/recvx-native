// Test-only VU micro-mode interpreter.
//
// It executes the original VU0/VU1 microcode words (archived/ps2/vsm/*.vsm,
// byte-identical to the SLUS_201.84 ELF) so that the native ports in
// native/core_chunk_vu.c can be compared against the real program. It is never
// linked into the runtime: the game uses the native C ports only.
//
// Timing is modelled only as far as it decides results: FMAC/load results become
// readable after 4 cycles (the hardware interlocks, so reads simply stall), MAC and
// CLIP flags become visible 4 cycles after issue (no interlock), Q after 7 (DIV) or
// 13 (RSQRT) cycles (no interlock; WAITQ and a second DIV stall). Branches have a
// delay slot; the [E] bit ends the program after the following pair.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vu_oracle {

union Reg {
    float f[4];
    int32_t i[4];
    uint32_t u[4];
};

struct Kick {
    std::vector<uint64_t> words;  // GIF packet as kicked by XGKICK, 64-bit words
};

struct Program {
    std::vector<uint64_t> code;                // micro memory, index = micro address
    std::vector<std::pair<std::string, int>> labels;
    int label(const std::string& name) const;  // -1 if unknown
};

// Loads one or more `ps2_vuNsubM:` blocks of a .vsm file. MPG VIF codes in the
// raw stream place each block at its micro address.
bool load_vsm(const std::string& path, Program& program, std::string& error);

struct Vu {
    explicit Vu(int memory_qwords);
    Reg vf[32]{};
    Reg acc{};
    uint16_t vi[16]{};
    float q = 0.0f, i = 0.0f;
    std::vector<Reg> mem;
    uint32_t top = 0;              // XTOP value
    uint32_t mac_visible = 0;      // MAC flags as seen by FMAND/FMEQ
    uint32_t clip_visible = 0;     // CLIP flags as seen by FC* and cfc2 vi18
    uint32_t clip_logical = 0;     // CLIP register in program order
    std::vector<Kick> kicks;
    uint64_t cycles = 0;
    int executed_pairs = 0;

    // Runs from micro address `start` until the [E] pair. Returns false on an
    // unsupported encoding or runaway program; `error` names the instruction.
    bool run(const Program& program, int start, std::string& error, int limit = 2000000);

    // Settles all pending flag/Q updates as the EE observes them after the
    // microprogram (cfc2.i interlocks on program end).
    void settle();
private:
    struct Pending { uint64_t at; int kind; uint32_t value; float fvalue; };
    std::vector<Pending> pending;
    uint64_t ready[32][4]{};       // cycle when vfN.field is readable
    uint64_t q_ready = 0;
    void advance(uint64_t to);
};

}  // namespace vu_oracle
