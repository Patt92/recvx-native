#pragma once
#include <stdint.h>

// Original EE libc rand/srand from SLUS_201.84 (rand 0x00125320,
// srand 0x00125310). One process-wide state, as in the original _reent.
#ifdef __cplusplus
extern "C" {
#endif
void recvx_core_random_seed(uint32_t seed);
int recvx_core_random_next(void);
#ifdef __cplusplus
}
#endif
