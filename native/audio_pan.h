#pragma once
#include <cstdint>
namespace recvx {
struct SonyPan {std::uint8_t left=128,right=128;};
SonyPan sony_pan_factors(int pan);
int sony_bank_pan(int program,int split,int sample,int key_pan);
}
