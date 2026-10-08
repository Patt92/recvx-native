// Test-only VU micro-mode interpreter; see vu_oracle.h.
#include "vu_oracle.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

namespace vu_oracle {

int Program::label(const std::string& name) const
{
    for (const auto& l : labels) if (l.first == name) return l.second;
    return -1;
}

static bool hexword(const std::string& s, uint32_t& value)
{
    if (s.size() != 8) return false;
    for (char c : s) if (!isxdigit(static_cast<unsigned char>(c))) return false;
    uint32_t bytes = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
    // The dump lists memory bytes in order; words are little endian.
    value = ((bytes & 0xff) << 24) | ((bytes & 0xff00) << 8) | ((bytes >> 8) & 0xff00) | (bytes >> 24);
    return true;
}

bool load_vsm(const std::string& path, Program& program, std::string& error)
{
    std::ifstream in(path);
    if (!in) { error = "cannot open " + path; return false; }
    static const std::regex raw(R"(/\*\s*([0-9A-F]{4})\s+([0-9A-F-]{16})\s*\*/(.*))");
    static const std::regex lab(R"(^\s*([A-Za-z_]\w*):\s*$)");
    std::string line;
    int address = -1, remaining = 0;
    std::vector<std::string> pending_labels;
    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_search(line, m, lab)) { pending_labels.push_back(m[1]); continue; }
        if (!std::regex_search(line, m, raw)) continue;
        std::string bytes = m[2], rest = m[3];
        uint32_t lo = 0, hi = 0;
        bool has_lo = hexword(bytes.substr(0, 8), lo), has_hi = hexword(bytes.substr(8, 8), hi);
        if (has_lo != has_hi) {
            // VIF/DMA code in one half: MPG sets the load address.
            uint32_t code = has_lo ? lo : hi;
            if ((code >> 24) == 0x4a) {
                address = static_cast<int>(code & 0xffff);
                remaining = static_cast<int>((code >> 16) & 0xff);
                if (remaining == 0) remaining = 256;
            }
            continue;
        }
        if (!has_lo || remaining <= 0 || rest.find("DMA") != std::string::npos) continue;
        if (static_cast<int>(program.code.size()) <= address) program.code.resize(address + 1, 0);
        program.code[address] = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
        for (const auto& name : pending_labels) program.labels.emplace_back(name, address);
        pending_labels.clear();
        ++address;
        --remaining;
    }
    if (program.code.empty()) { error = "no microcode in " + path; return false; }
    return true;
}

Vu::Vu(int memory_qwords) : mem(memory_qwords)
{
    vf[0].f[0] = vf[0].f[1] = vf[0].f[2] = 0.0f;
    vf[0].f[3] = 1.0f;
}

enum { PEND_MAC, PEND_CLIP, PEND_Q };

void Vu::advance(uint64_t to)
{
    if (to > cycles) cycles = to;
    for (size_t n = 0; n < pending.size();) {
        if (pending[n].at <= cycles) {
            if (pending[n].kind == PEND_MAC) mac_visible = pending[n].value;
            else if (pending[n].kind == PEND_CLIP) clip_visible = pending[n].value;
            else q = pending[n].fvalue;
            pending.erase(pending.begin() + n);
        } else {
            ++n;
        }
    }
}

void Vu::settle()
{
    uint64_t last = cycles;
    for (const auto& p : pending) if (p.at > last) last = p.at;
    advance(last);
}

namespace {

constexpr int X = 8, Y = 4, Z = 2, W = 1;  // dest field bits as encoded (x is bit 24)
inline bool has(int dest, int c) { return dest & (8 >> c); }

struct Write {
    bool vf = false; int reg = 0; int dest = 0; Reg value{};
    bool acc = false;
    bool mac = false; uint32_t mac_value = 0;
};

uint32_t mac_of(int dest, const Reg& r)
{
    uint32_t f = 0;
    for (int c = 0; c < 4; ++c) {
        if (!has(dest, c)) continue;
        if (std::signbit(r.f[c]) && r.f[c] != 0.0f) f |= 0x80u >> c;
        if (r.f[c] == 0.0f) f |= 0x08u >> c;
    }
    return f;
}

int32_t ftoi(float v)
{
    if (std::isnan(v)) return 0;
    if (v >= 2147483647.0f) return 2147483647;
    if (v <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(v);
}

}  // namespace

bool Vu::run(const Program& program, int start, std::string& error, int limit)
{
    int pc = start;
    int delayed_target = -1;   // branch taken, jumps after the delay slot
    bool end_after = false;
    for (int guard = 0; guard < limit; ++guard) {
        if (pc < 0 || pc >= static_cast<int>(program.code.size())) {
            std::ostringstream s; s << "pc out of range " << pc; error = s.str(); return false;
        }
        const uint64_t pair = program.code[pc];
        const uint32_t lo = static_cast<uint32_t>(pair), up = static_cast<uint32_t>(pair >> 32);
        const bool ibit = up & 0x80000000u, ebit = up & 0x40000000u;
        const int dest = (up >> 21) & 0xf, ft = (up >> 16) & 0x1f, fs = (up >> 11) & 0x1f, fd = (up >> 6) & 0x1f;
        const int op = up & 0x3f;
        // Lower fields.
        const int lop = lo >> 25, ldest = (lo >> 21) & 0xf, lit = (lo >> 16) & 0x1f, lis = (lo >> 11) & 0x1f;
        const int lid = (lo >> 6) & 0x1f;
        const int imm11 = (lo & 0x400) ? static_cast<int>(lo & 0x7ff) - 0x800 : static_cast<int>(lo & 0x7ff);

        // ---- decode upper into a small description
        enum UKind { U_NONE, U_ARITH, U_ITOF, U_FTOI, U_ABS, U_CLIP } ukind = U_NONE;
        char uop = 0;          // '+','-','*','M'(max),'m'(mini)
        bool uacc_dst = false, uacc_src = false, uacc_sub = false;
        int ubc = -1;          // broadcast field of ft, -1 = vector
        char uscalar = 0;      // 'q','i' or 0
        int ushift = 0;        // ITOF/FTOI fraction bits
        if (op < 0x3c) {
            int base = op & 0x3c;
            if (op <= 0x1b) {
                ubc = op & 3;
                switch (base) {
                case 0x00: uop = '+'; break;
                case 0x04: uop = '-'; break;
                case 0x08: uop = '*'; uacc_src = true; break;
                case 0x0c: uop = '*'; uacc_src = true; uacc_sub = true; break;
                case 0x10: uop = 'M'; break;
                case 0x14: uop = 'm'; break;
                case 0x18: uop = '*'; break;
                }
            } else {
                switch (op) {
                case 0x1c: uop = '*'; uscalar = 'q'; break;
                case 0x1d: uop = 'M'; uscalar = 'i'; break;
                case 0x1e: uop = '*'; uscalar = 'i'; break;
                case 0x1f: uop = 'm'; uscalar = 'i'; break;
                case 0x20: uop = '+'; uscalar = 'q'; break;
                case 0x21: uop = '*'; uscalar = 'q'; uacc_src = true; break;
                case 0x22: uop = '+'; uscalar = 'i'; break;
                case 0x23: uop = '*'; uscalar = 'i'; uacc_src = true; break;
                case 0x24: uop = '-'; uscalar = 'q'; break;
                case 0x26: uop = '-'; uscalar = 'i'; break;
                case 0x28: uop = '+'; break;
                case 0x29: uop = '*'; uacc_src = true; break;
                case 0x2a: uop = '*'; break;
                case 0x2b: uop = 'M'; break;
                case 0x2c: uop = '-'; break;
                case 0x2d: uop = '*'; uacc_src = true; uacc_sub = true; break;
                case 0x2f: uop = 'm'; break;
                default: {
                    std::ostringstream s; s << "upper op " << std::hex << op << " at " << std::dec << pc;
                    error = s.str(); return false;
                }
                }
            }
            if (uop) ukind = U_ARITH;
        } else {
            const int sop = ((up >> 6) & 0x1f) << 2 | (up & 3);
            if (sop <= 0x0f) {
                ubc = sop & 3; uacc_dst = true;
                switch (sop & 0x0c) {
                case 0x00: uop = '+'; break;
                case 0x04: uop = '-'; break;
                case 0x08: uop = '*'; uacc_src = true; break;
                case 0x0c: uop = '*'; uacc_src = true; uacc_sub = true; break;
                }
                ukind = U_ARITH;
            } else if (sop >= 0x10 && sop <= 0x13) { ukind = U_ITOF; ushift = (int[]){0, 4, 12, 15}[sop - 0x10]; }
            else if (sop >= 0x14 && sop <= 0x17) { ukind = U_FTOI; ushift = (int[]){0, 4, 12, 15}[sop - 0x14]; }
            else if (sop >= 0x18 && sop <= 0x1b) { ubc = sop & 3; uop = '*'; uacc_dst = true; ukind = U_ARITH; }
            else if (sop == 0x1c) { uop = '*'; uscalar = 'q'; uacc_dst = true; ukind = U_ARITH; }
            else if (sop == 0x1d) ukind = U_ABS;
            else if (sop == 0x1e) { uop = '*'; uscalar = 'i'; uacc_dst = true; ukind = U_ARITH; }
            else if (sop == 0x1f) ukind = U_CLIP;
            else if (sop == 0x28) { uop = '+'; uacc_dst = true; ukind = U_ARITH; }
            else if (sop == 0x29) { uop = '*'; uacc_dst = true; uacc_src = true; ukind = U_ARITH; }
            else if (sop == 0x2a) { uop = '*'; uacc_dst = true; ukind = U_ARITH; }
            else if (sop == 0x2c) { uop = '-'; uacc_dst = true; ukind = U_ARITH; }
            else if (sop == 0x2f) ukind = U_NONE;
            else {
                std::ostringstream s; s << "upper special " << std::hex << sop << " at " << std::dec << pc;
                error = s.str(); return false;
            }
        }

        // ---- operand fields read by this pair (for interlock stalls)
        uint64_t issue = cycles;
        auto need = [&](int reg, int mask) {
            if (reg == 0) return;
            for (int c = 0; c < 4; ++c) if (mask & (8 >> c)) issue = std::max(issue, ready[reg][c]);
        };
        if (ukind == U_ARITH) {
            need(fs, dest);
            if (!uscalar) need(ft, ubc >= 0 ? (8 >> ubc) : dest);
        } else if (ukind == U_ITOF || ukind == U_FTOI || ukind == U_ABS) {
            need(fs, dest);
        } else if (ukind == U_CLIP) {
            need(fs, X | Y | Z); need(ft, W);
        }
        bool lower_is_loi = ibit;
        int special = -1;
        if (!lower_is_loi && lop == 0x40) special = ((lo & 0x3c) == 0x3c) ? (((lo >> 6) & 0x1f) << 2 | (lo & 3)) + 0x100 : static_cast<int>(lo & 0x3f);
        if (!lower_is_loi) {
            if (lop == 0x01) need(lis, ldest);                        // SQ reads vf[fs]
            if (special == 0x100 + 0x30 || special == 0x100 + 0x31) need(lis, 0xf);  // MOVE/MR32
            if (special == 0x100 + 0x35 || special == 0x100 + 0x37) need(lis, ldest); // SQI/SQD
            if (special == 0x100 + 0x38 || special == 0x100 + 0x3a) { need(lis, 0xf); need(lit, 0xf); }
            if (special == 0x100 + 0x3b || special == 0x100 + 0x38 || special == 0x100 + 0x3a) issue = std::max(issue, q_ready);
        }
        advance(issue);

        // ---- execute upper
        Write uw;
        const Reg S = vf[fs], T = vf[ft];
        if (ukind == U_ARITH) {
            Reg r = uacc_dst ? acc : vf[fd];
            for (int c = 0; c < 4; ++c) {
                if (!has(dest, c)) continue;
                float a = S.f[c];
                float b = uscalar == 'q' ? q : uscalar == 'i' ? i : T.f[ubc >= 0 ? ubc : c];
                float v = 0.0f;
                switch (uop) {
                case '+': v = a + b; break;
                case '-': v = a - b; break;
                case '*': { float p = a * b; v = uacc_src ? (uacc_sub ? acc.f[c] - p : acc.f[c] + p) : p; break; }
                case 'M': v = a > b ? a : b; break;
                case 'm': v = a < b ? a : b; break;
                }
                r.f[c] = v;
            }
            if (uacc_dst) uw.acc = true; else { uw.vf = true; uw.reg = fd; }
            uw.dest = dest; uw.value = r;
            if (uop != 'M' && uop != 'm') { uw.mac = true; uw.mac_value = mac_of(dest, r); }
        } else if (ukind == U_ITOF || ukind == U_FTOI || ukind == U_ABS) {
            Reg r = vf[ft];
            for (int c = 0; c < 4; ++c) {
                if (!has(dest, c)) continue;
                if (ukind == U_ITOF) r.f[c] = static_cast<float>(S.i[c]) / static_cast<float>(1 << ushift);
                else if (ukind == U_FTOI) r.i[c] = ftoi(S.f[c] * static_cast<float>(1 << ushift));
                else r.f[c] = std::fabs(S.f[c]);
            }
            uw.vf = true; uw.reg = ft; uw.dest = dest; uw.value = r;
        } else if (ukind == U_CLIP) {
            const float w = std::fabs(T.f[3]);
            uint32_t j = 0;
            for (int c = 0; c < 3; ++c) {
                if (S.f[c] > w) j |= 1u << (c * 2);
                if (S.f[c] < -w) j |= 1u << (c * 2 + 1);
            }
            clip_logical = ((clip_logical << 6) | j) & 0xffffff;
            pending.push_back({issue + 4, PEND_CLIP, clip_logical, 0.0f});
        }

        // ---- execute lower
        Write lw;
        uint16_t vi_new[16];
        std::memcpy(vi_new, vi, sizeof(vi));
        bool vi_changed = false;
        int branch = -1;
        bool i_load = false; float i_value = 0.0f;
        auto setvi = [&](int r, int v) { if (r) { vi_new[r] = static_cast<uint16_t>(v); vi_changed = true; } };
        const int memmask = static_cast<int>(mem.size()) - 1;
        if (lower_is_loi) {
            i_load = true; std::memcpy(&i_value, &lo, 4);
        } else if (lop == 0x40) {
            switch (special) {
            case 0x30: setvi(lid, vi[lis] + vi[lit]); break;                       // IADD
            case 0x31: setvi(lid, vi[lis] - vi[lit]); break;                       // ISUB
            case 0x32: { int imm5 = (lo >> 6) & 0x1f; if (imm5 & 0x10) imm5 -= 32; setvi(lit, vi[lis] + imm5); break; }  // IADDI
            case 0x34: setvi(lid, vi[lis] & vi[lit]); break;                       // IAND
            case 0x35: setvi(lid, vi[lis] | vi[lit]); break;                       // IOR
            case 0x100 + 0x2f: break;                                              // NOP
            case 0x100 + 0x30: { Reg r = vf[lit]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) r.u[c] = vf[lis].u[c]; lw.vf = true; lw.reg = lit; lw.dest = ldest; lw.value = r; break; }  // MOVE
            case 0x100 + 0x31: { Reg r = vf[lit]; const Reg s = vf[lis]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) r.u[c] = s.u[(c + 1) & 3]; lw.vf = true; lw.reg = lit; lw.dest = ldest; lw.value = r; break; }  // MR32
            case 0x100 + 0x34: { Reg r = vf[lit]; const Reg& m = mem[vi[lis] & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) r.u[c] = m.u[c]; lw.vf = true; lw.reg = lit; lw.dest = ldest; lw.value = r; setvi(lis, vi[lis] + 1); break; }  // LQI
            case 0x100 + 0x35: { Reg& m = mem[vi[lit] & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) m.u[c] = vf[lis].u[c]; setvi(lit, vi[lit] + 1); break; }  // SQI
            case 0x100 + 0x37: { int a = (vi[lit] - 1) & 0xffff; Reg& m = mem[a & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) m.u[c] = vf[lis].u[c]; setvi(lit, a); break; }  // SQD
            case 0x100 + 0x38: case 0x100 + 0x3a: {                                // DIV / RSQRT
                const int fsf = (lo >> 21) & 3, ftf = (lo >> 23) & 3;
                float num = vf[lis].f[fsf], den = vf[lit].f[ftf], value;
                if (special == 0x100 + 0x38) value = num / den;
                else value = num / std::sqrt(std::fabs(den));
                const uint64_t at = issue + (special == 0x100 + 0x38 ? 7 : 13);
                pending.push_back({at, PEND_Q, 0, value});
                q_ready = at;
                break;
            }
            case 0x100 + 0x3b: break;                                              // WAITQ (stall handled above)
            case 0x100 + 0x3e: { const Reg& m = mem[vi[lis] & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) setvi(lit, m.u[c] & 0xffff); break; }  // ILWR
            case 0x100 + 0x3f: { Reg& m = mem[vi[lis] & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) m.u[c] = vi[lit]; break; }  // ISWR
            case 0x100 + 0x68: setvi(lit, static_cast<int>(top)); break;           // XTOP
            case 0x100 + 0x6c: {                                                   // XGKICK
                Kick k;
                int a = vi[lis];
                for (bool eop = false; !eop;) {
                    const Reg& tag = mem[a & memmask];
                    const uint64_t t0 = static_cast<uint64_t>(tag.u[0]) | static_cast<uint64_t>(tag.u[1]) << 32;
                    k.words.push_back(t0);
                    k.words.push_back(static_cast<uint64_t>(tag.u[2]) | static_cast<uint64_t>(tag.u[3]) << 32);
                    ++a;
                    const int nloop = t0 & 0x7fff, flg = (t0 >> 58) & 3;
                    int nreg = (t0 >> 60) & 0xf; if (!nreg) nreg = 16;
                    eop = t0 & 0x8000;
                    int qwords = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
                    for (int n = 0; n < qwords; ++n, ++a) {
                        const Reg& d = mem[a & memmask];
                        k.words.push_back(static_cast<uint64_t>(d.u[0]) | static_cast<uint64_t>(d.u[1]) << 32);
                        k.words.push_back(static_cast<uint64_t>(d.u[2]) | static_cast<uint64_t>(d.u[3]) << 32);
                    }
                }
                kicks.push_back(std::move(k));
                break;
            }
            default: {
                std::ostringstream s; s << "lower special " << std::hex << special << " at " << std::dec << pc;
                error = s.str(); return false;
            }
            }
        } else {
            switch (lop) {
            case 0x00: { Reg r = vf[lit]; const Reg& m = mem[(vi[lis] + imm11) & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) r.u[c] = m.u[c]; lw.vf = true; lw.reg = lit; lw.dest = ldest; lw.value = r; break; }  // LQ
            case 0x01: { Reg& m = mem[(vi[lit] + imm11) & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) m.u[c] = vf[lis].u[c]; break; }  // SQ
            case 0x04: { const Reg& m = mem[(vi[lis] + imm11) & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) setvi(lit, m.u[c] & 0xffff); break; }  // ILW
            case 0x05: { Reg& m = mem[(vi[lis] + imm11) & memmask]; for (int c = 0; c < 4; ++c) if (has(ldest, c)) m.u[c] = vi[lit]; break; }  // ISW
            case 0x08: case 0x09: {                                                // IADDIU / ISUBIU
                const int imm15 = static_cast<int>(((lo >> 10) & 0x7800) | (lo & 0x7ff));
                setvi(lit, lop == 0x08 ? vi[lis] + imm15 : vi[lis] - imm15);
                break;
            }
            case 0x12: setvi(1, (clip_visible & (lo & 0xffffff)) != 0); break;    // FCAND
            case 0x13: setvi(1, ((clip_visible | (lo & 0xffffff)) & 0xffffff) == 0xffffff); break;  // FCOR
            case 0x1a: setvi(lit, vi[lis] & mac_visible); break;                  // FMAND
            case 0x1c: setvi(lit, clip_visible & 0xfff); break;                   // FCGET
            case 0x20: branch = pc + 1 + imm11; break;                             // B
            case 0x21: setvi(lit, pc + 2); branch = pc + 1 + imm11; break;         // BAL
            case 0x24: branch = vi[lis]; break;                                    // JR
            case 0x25: setvi(lit, pc + 2); branch = vi[lis]; break;                // JALR
            case 0x28: if (vi[lit] == vi[lis]) branch = pc + 1 + imm11; break;     // IBEQ
            case 0x29: if (vi[lit] != vi[lis]) branch = pc + 1 + imm11; break;     // IBNE
            case 0x2d: if (static_cast<int16_t>(vi[lis]) > 0) branch = pc + 1 + imm11; break;   // IBGTZ
            case 0x2e: if (static_cast<int16_t>(vi[lis]) <= 0) branch = pc + 1 + imm11; break;  // IBLEZ
            default: {
                std::ostringstream s; s << "lower op " << std::hex << lop << " at " << std::dec << pc;
                error = s.str(); return false;
            }
            }
        }

        // ---- commit
        auto commit = [&](const Write& w) {
            if (w.acc) { for (int c = 0; c < 4; ++c) if (has(w.dest, c)) acc.u[c] = w.value.u[c]; }
            if (w.vf && w.reg) {
                for (int c = 0; c < 4; ++c) if (has(w.dest, c)) { vf[w.reg].u[c] = w.value.u[c]; ready[w.reg][c] = issue + 4; }
            }
            if (w.mac) pending.push_back({issue + 4, PEND_MAC, w.mac_value, 0.0f});
        };
        commit(uw);
        commit(lw);
        if (vi_changed) std::memcpy(vi, vi_new, sizeof(vi));
        vi[0] = 0;
        if (i_load) i = i_value;
        cycles = issue + 1;
        ++executed_pairs;

        if (end_after) return true;
        if (ebit) end_after = true;
        if (delayed_target >= 0) { pc = delayed_target; delayed_target = -1; }
        else ++pc;
        if (branch >= 0) delayed_target = branch;
    }
    error = "instruction limit";
    return false;
}

}  // namespace vu_oracle
