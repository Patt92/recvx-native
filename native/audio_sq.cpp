#include "audio_sq.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace recvx {
namespace {

[[noreturn]] void malformed(const char* reason) {
    throw std::runtime_error(reason);
}

class BlockReader {
public:
    BlockReader(const std::vector<std::uint8_t>& bytes, std::size_t begin,
                std::size_t end) : bytes_(bytes), begin_(begin), end_(end) {
        if (begin > end || end > bytes.size())
            malformed("Sony SQ: invalid block bounds");
    }

    std::size_t size() const { return end_ - begin_; }

    void require(std::size_t relative, std::size_t count) const {
        if (relative > size() || count > size() - relative)
            malformed("Sony SQ: truncated block");
    }

    std::uint8_t byte(std::size_t relative) const {
        require(relative, 1);
        return bytes_[begin_ + relative];
    }

    std::uint16_t u16(std::size_t relative) const {
        require(relative, 2);
        return std::uint16_t(byte(relative)) |
               (std::uint16_t(byte(relative + 1)) << 8);
    }

    std::uint32_t u32(std::size_t relative) const {
        require(relative, 4);
        return std::uint32_t(u16(relative)) |
               (std::uint32_t(u16(relative + 2)) << 16);
    }

    std::uint8_t take(std::size_t& cursor) const { return byte(cursor++); }

    std::uint32_t vlq(std::size_t& cursor) const {
        std::uint32_t value = 0;
        for (unsigned count = 0; count < 4; ++count) {
            const auto b = take(cursor);
            value = (value << 7) | (b & 0x7f);
            if (!(b & 0x80)) return value;
        }
        malformed("Sony SQ: oversized VLQ");
    }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t begin_, end_;
};

// All three fields affect parsing after an NRPN loop rewind in the original
// reader. This eager decoder parses only once; the playback consumer replays
// decoded events instead, so encoded status/marker state never changes on a loop.
struct ParseState {
    std::size_t cursor = 0;
    std::uint8_t encoded_running = 0;
    bool skip_next_delta = false;
};

bool channel_status(std::uint8_t status) {
    return status >= 0x80 && status < 0xf0;
}

} // namespace

std::vector<SonySqEvent> decode_sony_sq(const std::vector<std::uint8_t>& bytes,
                                      std::size_t block, std::size_t end) {
    const BlockReader input(bytes, block, end);
    input.require(0, 6);
    const auto event_offset = input.u32(0);
    if (event_offset < 6 || event_offset > input.size())
        malformed("Sony SQ: invalid event offset");
    if (!input.u16(4)) malformed("Sony SQ: zero PPQN");

    // Small/non-six-byte event offsets are plain streams. Only blocks whose
    // events start at +12 or later can have the dictionary discriminator.
    bool dictionary = false;
    std::uint16_t bound = 0;
    if (event_offset >= 12) {
        input.require(6, 4);
        bound = input.u16(8);
        dictionary = input.u16(6) == 1 && bound != 0;
    }
    if (dictionary) {
        // The bound compares against an even BYTE index, inclusively; it is
        // neither a pair count nor an exclusive upper limit. Odd bounds are
        // legal and refer to the same last pair as the preceding even bound.
        const std::size_t pair_end = 10 + (bound & ~std::size_t(1)) + 2;
        input.require(10, pair_end - 10);
        if (pair_end > event_offset)
            malformed("Sony SQ: dictionary overlaps events");
    }

    ParseState state{event_offset, 0, false};
    std::uint64_t pending_delta = 0;
    std::vector<SonySqEvent> events;
    while (state.cursor < input.size()) {
        if (!state.skip_next_delta) {
            const auto delta = input.vlq(state.cursor);
            if (pending_delta > std::numeric_limits<std::uint64_t>::max() - delta)
                malformed("Sony SQ: delta overflow");
            pending_delta += delta;
        }
        state.skip_next_delta = false;

        auto status = input.byte(state.cursor);
        if (status & 0x80) {
            ++state.cursor;
            if (channel_status(status)) state.encoded_running = status;
        } else {
            status = state.encoded_running;
            if (!status) malformed("Sony SQ: missing running status");
        }

        SonySqEvent event;
        event.delta = pending_delta;
        event.status = status;
        if (status == 0xf0 || status == 0xf7)
            malformed("Sony SQ: SysEx unsupported (uninspected semantics)");
        if (status == 0xff) {
            event.key = input.take(state.cursor);
            if (event.key & 0x80) malformed("Sony SQ: invalid meta type");
            const auto length = input.vlq(state.cursor);
            input.require(state.cursor, length);
            if ((event.key == 0x51 && length != 3) ||
                (event.key == 0x2f && length != 0))
                malformed("Sony SQ: invalid tempo/end meta length");
            event.meta.reserve(length);
            for (std::uint32_t i = 0; i < length; ++i)
                event.meta.push_back(input.take(state.cursor));
            if (event.key == 0x51 && !(event.meta[0] | event.meta[1] | event.meta[2]))
                malformed("Sony SQ: zero tempo");
        } else {
            if (!channel_status(status)) malformed("Sony SQ: unsupported system status");
            const auto raw1 = input.take(state.cursor);
            std::uint8_t raw2 = 0;
            if (dictionary && (status & 0xf0) == 0xa0) {
                // The payload's bit7 participates in BOTH the dictionary index
                // and the marker. A tokens consume exactly one payload byte.
                state.skip_next_delta = (raw1 & 0x80) != 0;
                const std::size_t index = 2 * ((raw1 & 0xf0) | (status & 0x0f));
                if (index > bound) continue; // retain delta even though no output
                event.status = input.byte(10 + index);
                if (!channel_status(event.status))
                    malformed("Sony SQ: invalid dictionary status");
                event.key = input.byte(11 + index) & 0x7f;
                event.value = std::uint8_t(((raw1 & 0x0f) << 3) | 7);
            } else {
                const auto kind = status & 0xf0;
                if (kind != 0x80 && kind != 0xc0 && kind != 0xd0)
                    raw2 = input.take(state.cursor);
                state.skip_next_delta = ((raw1 | raw2) & 0x80) != 0;
                event.key = raw1 & 0x7f;
                event.value = raw2 & 0x7f;
            }
        }

        const bool ended = event.status == 0xff && event.key == 0x2f;
        events.push_back(std::move(event));
        pending_delta = 0;
        if (ended) break;
    }
    return events;
}

} // namespace recvx
