#ifndef RECVX_NATIVE_NINJA_COLLISION_H
#define RECVX_NATIVE_NINJA_COLLISION_H
#include "combat_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ps2_NaColi.c:346 njCollisionCheckCC (die Ninja-Routine, nicht njplus
   npCollisionCheckCC) und :1032 njCollisionCheckBC2 (Quader aus acht
   beliebigen Eckpunkten gegen Kapsel). Koerper in native/wall_capsule.cpp. */
int recvx_nj_check_cc(const RECVX_CB_CAPSULE* a, const RECVX_CB_CAPSULE* b);
int recvx_nj_check_bc2(const RECVX_CB_VEC box[8], const RECVX_CB_CAPSULE* capsule);
#ifdef __cplusplus
}
#endif
#endif
