#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace recvx {

// Channel events carry outgoing status/key/value, with the Sony marker bits
// removed. For FF, key is the meta type and meta contains its uninterpreted
// payload (51: three-byte big-endian tempo; 2F: empty end-of-track).
struct SonySqEvent {
    std::uint64_t delta = 0;
    std::uint8_t status = 0;
    std::uint8_t key = 0;
    std::uint8_t value = 0;
    std::vector<std::uint8_t> meta;
};

// Decode one selected SQ block, bounded by [block,end); event_offset is relative
// to block, not to the containing SQ bank. The block header starts with LEu32
// event_offset and LEu16 PPQN. Dictionary fields are LEu16 type at +6, inclusive
// byte-index bound at +8, then two-byte status/key pairs at +10.
//
// Based on the inspected MODMIDI.IRX event-reader semantics, independently
// implemented here. The decoder owns no playback/loop/synth state. Delta ticks
// of dictionary tokens outside the inclusive bound accrue to the next emitted
// event. NRPN loop execution belongs to the caller over these decoded events;
// replaying output must not run dictionary/running-status parsing again.
//
// Throws std::runtime_error for malformed/truncated input, invalid PPQN/status,
// overflowing VLQ, or unsupported F0/F7 SysEx (semantics not yet fully inspected).
// Stops at FF/2F; trailing block padding is not parsed as another event.
std::vector<SonySqEvent> decode_sony_sq(const std::vector<std::uint8_t>& bytes,
                                      std::size_t block, std::size_t end);

} // namespace recvx
