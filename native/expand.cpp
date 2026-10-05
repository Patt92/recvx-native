#include "expand.h"
#include <stdexcept>

namespace recvx {
std::vector<std::uint8_t> expand(const std::vector<std::uint8_t>& source,
                               std::size_t output_limit) {
    // Adapted from Clownacy's C translation in src/ps2/veronica/prog/expand.c.
    // Descriptor bytes and payload bytes share one cursor. Preserve the
    // original pre-decrement refresh behavior, including initial count of 9.
    std::size_t cursor = 0;
    auto byte = [&]() -> unsigned {
        if (cursor == source.size()) throw std::runtime_error("truncated Expand stream");
        return source[cursor++];
    };
    unsigned descriptor = byte();
    unsigned remaining = 9;
    auto bit = [&]() -> unsigned {
        if (--remaining == 0) {
            descriptor = byte();
            remaining = 8;
        }
        const unsigned value = descriptor & 1;
        descriptor >>= 1;
        return value;
    };
    std::vector<std::uint8_t> result;
    for (;;) {
        if (bit()) {
            const auto literal = static_cast<std::uint8_t>(byte());
            if (result.size() >= output_limit) throw std::runtime_error("Expand output limit exceeded");
            result.push_back(literal);
            continue;
        }
        unsigned count;
        unsigned distance;
        if (!bit()) {
            const unsigned high = bit();
            const unsigned low = bit();
            count = 2 + high * 2 + low;
            distance = 256 - byte();
        } else {
            const unsigned lower = byte();
            const unsigned whole = lower | (byte() << 8);
            if (whole == 0) return result;
            distance = 8192 - (whole >> 3);
            count = whole & 7;
            count = count ? count + 2 : byte() + 1;
        }
        if (distance > result.size()) throw std::runtime_error("invalid Expand back-reference");
        if (count > output_limit - result.size()) throw std::runtime_error("Expand output limit exceeded");
        // Bytewise copying is essential for overlapping back-references.
        for (unsigned i = 0; i < count; ++i)
            result.push_back(result[result.size() - distance]);
    }
}
}
