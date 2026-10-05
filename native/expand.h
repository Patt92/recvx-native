#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace recvx {
// Bounded host implementation of the PS2 Expand stream format.
// Throws std::runtime_error for malformed streams or an exceeded output limit.
std::vector<std::uint8_t> expand(const std::vector<std::uint8_t>& source,
                               std::size_t output_limit);
}
