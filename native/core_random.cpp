#include "core_random.h"

namespace {
uint32_t random_state = 1;
}

extern "C" void recvx_core_random_seed(uint32_t seed) {
    random_state = seed;
}

extern "C" int recvx_core_random_next(void) {
    random_state = random_state * 0x41c64e6du + 0x3039u;
    return int(random_state & 0x7fffffffu);
}
