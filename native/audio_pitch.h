#pragma once
#include <cstdint>
namespace recvx {
std::uint16_t sony_note_pitch(std::uint16_t root,std::uint16_t root_fine,std::uint16_t note,std::int16_t fine);
std::uint16_t sony_sample_pitch(unsigned rate,std::uint16_t root,int note,int fine);
}
