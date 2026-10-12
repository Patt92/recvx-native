#include "audio_sq.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using recvx::decode_sony_sq;
using Bytes = std::vector<std::uint8_t>;

namespace {
void check(bool ok, const char* reason) {
    if (!ok) throw std::runtime_error(reason);
}

void put16(Bytes& b, std::size_t at, unsigned value) {
    b.at(at) = std::uint8_t(value);
    b.at(at + 1) = std::uint8_t(value >> 8);
}

void put32(Bytes& b, std::size_t at, unsigned value) {
    put16(b, at, value);
    put16(b, at + 2, value >> 16);
}

Bytes plain(Bytes events, unsigned offset = 6) {
    Bytes b(offset);
    put32(b, 0, offset);
    put16(b, 4, 480);
    b.insert(b.end(), events.begin(), events.end());
    return b;
}

Bytes dictionary(unsigned bound, Bytes events) {
    const unsigned offset = 10 + (bound & ~1u) + 2;
    Bytes b(offset);
    put32(b, 0, offset);
    put16(b, 4, 480);
    put16(b, 6, 1);
    put16(b, 8, bound);
    for (unsigned i = 0; i <= bound; i += 2) {
        b[10 + i] = 0x91;
        b[11 + i] = std::uint8_t(40 + i / 2);
    }
    b.insert(b.end(), events.begin(), events.end());
    return b;
}

void reject(const Bytes& b, const char* reason) {
    bool rejected = false;
    try { decode_sony_sq(b, 0, b.size()); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, reason);
}

void plain_events() {
    // Sony NoteOff has ONE byte, unlike standard MIDI. C/D likewise have one;
    // A remains two-byte poly-pressure when dictionary is disabled.
    auto b = plain({0, 0x90, 60, 100, 0x81, 0x00, 0x80, 60,
                    3, 0xc2, 9, 4, 0xd2, 17, 5, 0xa2, 60, 29,
                    6, 0xe2, 1, 2, 0, 0xff, 0x2f, 0});
    const auto e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 7, "plain event count/NoteOff width");
    check(e[1].delta == 128 && e[1].status == 0x80 && e[1].key == 60 && e[1].value == 0,
          "ordinary VLQ/short NoteOff");
    check(e[2].status == 0xc2 && e[2].key == 9 && e[2].value == 0,
          "program-change width");
    check(e[3].status == 0xd2 && e[3].key == 17 && e[3].value == 0,
          "channel-pressure width");
    check(e[4].status == 0xa2 && e[4].key == 60 && e[4].value == 29,
          "plain A channel width");
    check(e[5].status == 0xe2 && e[5].key == 1 && e[5].value == 2,
          "pitch-bend width");

    // Ordinary marker can be in either byte; both outgoing bytes lose bit7.
    b = plain({7, 0x90, 0xbc, 0xe4, 0x80, 0xbc, 0xc0, 5,
               3, 0x90, 61, 0x81, 62, 2, 0, 0xff, 0x2f, 0});
    const auto m = decode_sony_sq(b, 0, b.size());
    check(m.size() == 6 && m[0].delta == 7 && m[0].key == 60 && m[0].value == 100,
          "both raw marker bytes must be stripped");
    check(m[1].delta == 0 && m[1].status == 0x80 && m[1].key == 60,
          "NoteOff marker/delta omission");
    check(m[2].delta == 0 && m[2].status == 0xc0 && m[2].key == 5,
          "one-byte marker must not consume next status");
    check(m[3].delta == 3 && m[3].value == 1 && m[4].delta == 0 && m[4].key == 62,
          "second-byte marker and running status");

    // FF must not overwrite the ENCODED channel running status. Unknown meta
    // payload is bounded/preserved and has a normal VLQ length.
    Bytes meta_stream{0, 0x90, 60, 100, 2, 0xff, 0x51, 3, 7, 0xa1, 0x20,
                      3, 61, 90, 4, 0xff, 1, 0x81, 0};
    meta_stream.insert(meta_stream.end(), 128, 0x80);
    meta_stream.insert(meta_stream.end(), {5, 62, 80, 0, 0xff, 0x2f, 0, 0xfe});
    b = plain(meta_stream);
    const auto t = decode_sony_sq(b, 0, b.size());
    check(t.size() == 6 && t[1].meta == Bytes({7, 0xa1, 0x20}) &&
          t[2].status == 0x90 && t[2].key == 61 && t[3].meta.size() == 128 &&
          t[4].status == 0x90 && t[4].key == 62,
          "meta payload/VLQ length/running preservation/end padding");
}

void dictionary_events() {
    Bytes stream;
    for (unsigned velocity = 0; velocity < 16; ++velocity)
        stream.insert(stream.end(), {std::uint8_t(velocity), 0xa0,
                                     std::uint8_t(0x10 | velocity)});
    stream.insert(stream.end(), {0, 0xff, 0x2f, 0});
    auto b = dictionary(32, stream); // index 32 is INCLUDED
    auto e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 17, "inclusive bound/all velocities");
    for (unsigned velocity = 0; velocity < 16; ++velocity)
        check(e[velocity].status == 0x91 && e[velocity].key == 56 &&
              e[velocity].value == ((velocity << 3) | 7) && e[velocity].delta == velocity,
              "dictionary velocity expansion/high nibble index");

    // Low channel nibble chooses the pair; subsequent running A tokens must
    // use encoded A3, not mapped 93. Pair key bit7 is NOT a delta marker.
    b = dictionary(38, {2, 0xa3, 0x12, 3, 0x1f, 4, 0x10, 0, 0xff, 0x2f, 0});
    b[10 + 38] = 0x93;
    b[11 + 38] = 0xbc;
    e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 4 && e[0].status == 0x93 && e[0].key == 60 && e[0].value == 23 &&
          e[1].delta == 3 && e[1].value == 127 && e[2].delta == 4 && e[2].value == 7,
          "encoded running A3 must survive dictionary mapping/key marker stripping");

    // Bit7 participates in index and RAW payload marker simultaneously. The
    // following event has no delta byte; a masked index would hit key11 instead.
    b = dictionary(256, {9, 0xa0, 0x8e, 0x90, 61, 99, 0, 0xff, 0x2f, 0});
    b[11] = 11;
    b[10 + 256] = 0x95;
    b[11 + 256] = 77;
    e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 3 && e[0].status == 0x95 && e[0].key == 77 && e[0].value == 119 &&
          e[1].delta == 0 && e[1].status == 0x90 && e[1].key == 61,
          "payload bit7/index/raw skip marker");

    // Invalid dictionary indices emit NOTHING but preserve cumulative delay,
    // including marker bookkeeping when the discarded token has payload bit7.
    b = dictionary(2, {7, 0xa2, 0, 8, 1, 9, 0xa0, 0x80,
                        0x90, 60, 100, 3, 0x80, 60, 0, 0xff, 0x2f, 0});
    e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 3 && e[0].delta == 24 && e[0].status == 0x90 && e[1].delta == 3,
          "discarded dictionary tokens must retain delta and raw marker");

    // High bytes at the event boundary are ordinary explicit status bytes,
    // even when the preceding encoded running status was A.
    b = dictionary(2, {0, 0xa0, 1, 0, 0xa1, 2, 0, 0xff, 0x2f, 0});
    e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 3 && e[0].key == 40 && e[1].key == 41 && e[1].value == 23,
          "explicit A status detection");

    // Every encoded channel nibble and every high payload nibble, including
    // the highest byte-index 510; bit7 forces immediate end meta with no delta.
    for (unsigned channel = 0; channel < 16; ++channel) {
        for (unsigned high = 0; high < 16; ++high) {
            Bytes tokens{5, std::uint8_t(0xa0 | channel),
                         std::uint8_t((high << 4) | 6)};
            if (high < 8) tokens.push_back(0);
            tokens.insert(tokens.end(), {0xff, 0x2f, 0});
            b = dictionary(510, tokens);
            const auto index = 2 * ((high << 4) | channel);
            b[10 + index] = std::uint8_t(0x90 | channel);
            b[11 + index] = std::uint8_t(20 + high);
            e = decode_sony_sq(b, 0, b.size());
            check(e.size() == 2 && e[0].status == (0x90 | channel) &&
                  e[0].key == 20 + high && e[0].value == 55 && e[0].delta == 5 &&
                  e[1].delta == 0, "all dictionary high/channel nibbles");
        }
    }

    b = dictionary(2, {0, 0xa1, 1, 2, 0xff, 0x51, 3, 7, 0xa1, 0x20,
                        3, 2, 0, 0xff, 0x2f, 0});
    e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 4 && e[2].status == 0x91 && e[2].key == 41 &&
          e[2].value == 23 && e[2].delta == 3,
          "meta must preserve encoded dictionary running status");

    // Odd inclusive bound is legal; it still bounds even byte-index pairs.
    b = dictionary(3, {0, 0xa1, 0, 0, 0xff, 0x2f, 0});
    e = decode_sony_sq(b, 0, b.size());
    check(e.size() == 2 && e[0].key == 41, "odd byte-index bound");
}

void fallback_and_bounds() {
    // Offset != 6 alone does not enable dictionary decoding. All three
    // predicates (offset >= 12, type == 1, bound != 0) are required.
    for (unsigned offset : {6u, 7u, 10u, 12u, 20u}) {
        auto b = plain({0, 0xa0, 60, 99, 0, 0xff, 0x2f, 0}, offset);
        if (offset >= 10) { put16(b, 6, 1); put16(b, 8, 0); }
        const auto e = decode_sony_sq(b, 0, b.size());
        check(e.size() == 2 && e[0].status == 0xa0 && e[0].key == 60 && e[0].value == 99,
              "non-six-byte plain fallback");
    }
    auto b = plain({0, 0xa0, 60, 99, 0, 0xff, 0x2f, 0}, 12);
    put16(b, 6, 2); put16(b, 8, 0xffff);
    check(decode_sony_sq(b, 0, b.size())[0].value == 99,
          "non-one dictionary type fallback");

    reject({}, "empty header");
    reject(Bytes(5), "short header");
    b = plain({}); put32(b, 0, 5); reject(b, "offset inside base header");
    b = plain({}); put32(b, 0, 7); reject(b, "offset beyond block");
    b = plain({}); put16(b, 4, 0); reject(b, "zero PPQN");
    b = dictionary(2, {0, 0xa0, 0}); put16(b, 8, 4); reject(b, "pair/events overlap");
    b = dictionary(2, {}); b.pop_back(); reject(b, "truncated pair/header range");
    b = dictionary(2, {0, 0xa0, 0}); b[10] = 0xf1; reject(b, "malformed mapped status");
    reject(plain({0, 60, 100}), "uninitialized running status");
    reject(plain({0, 0xf1}), "unsupported system status");
    reject(plain({0, 0xf0, 0}), "SysEx F0 must be explicit gap");
    reject(plain({0, 0xf7, 0}), "SysEx F7 must be explicit gap");
    reject(plain({0, 0x90, 60}), "missing second data byte");
    reject(plain({0, 0x80}), "missing shortened NoteOff key");
    reject(dictionary(2, {0, 0xa0}), "missing compressed payload");
    reject(plain({0x81}), "unterminated delta VLQ");
    reject(plain({0xff, 0xff, 0xff, 0xff, 0, 0x90, 60, 100}), "oversized delta VLQ");
    reject(plain({0, 0xff, 1, 0x81}), "unterminated meta VLQ");
    reject(plain({0, 0xff, 1, 0xff, 0xff, 0xff, 0xff, 0}), "oversized meta VLQ");
    reject(plain({0, 0xff, 1, 3, 1, 2}), "truncated meta payload");
    reject(plain({0, 0xff, 0x80, 0}), "invalid meta type");
    reject(plain({0, 0xff, 0x51, 2, 1, 2}), "wrong tempo length");
    reject(plain({0, 0xff, 0x51, 3, 0, 0, 0}), "zero tempo");
    reject(plain({0, 0xff, 0x2f, 1, 0}), "wrong end length");

    // Selected block is independent of bank prefixes and following blocks.
    const auto body = plain({0, 0x90, 60, 100, 0, 0xff, 0x2f, 0});
    Bytes bank(19, 0xff);
    bank.insert(bank.end(), body.begin(), body.end());
    bank.insert(bank.end(), 20, 0xff);
    check(decode_sony_sq(bank, 19, 19 + body.size()).size() == 2, "relative selected-block offsets");
    bool rejected = false;
    try { decode_sony_sq(bank, 19, 19 + 9); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "must not borrow missing event bytes from the next block");
    rejected = false;
    try { decode_sony_sq(bank, 20, 19); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "reversed block bounds");
    rejected = false;
    try { decode_sony_sq(bank, 0, bank.size() + 1); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "end outside source vector");
}
} // namespace

int main() {
    try {
        plain_events();
        dictionary_events();
        fallback_and_bounds();
        std::cout << "Sony SQ bounded event decoder passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Sony SQ decoder: " << e.what() << '\n';
        return 1;
    }
}
