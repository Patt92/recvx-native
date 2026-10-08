#ifndef RECVX_CORE_NJPLUS_COLLISION_H
#define RECVX_CORE_NJPLUS_COLLISION_H

/*
 * Namespace boundary for the native port of njplus.c.  Original enemy sources
 * are compiled with this mapping, so they never bind directly to a ps2_* or
 * unprefixed host replacement.
 */
#define npPlusInit core_np_plus_init
#define npCollisionCheckCC core_np_collision_check_cc
#define npCollisionCheckCCEx core_np_collision_check_cc_ex
#define npCollisionCheckSC core_np_collision_check_sc
#define npDistanceP2C core_np_distance_p2c
#define npSetMemory core_np_set_memory
#define npSetMemoryL core_np_set_memory_l

#define njTranslateEx core_nj_translate_ex
#define njRotateEx core_nj_rotate_ex
#define njScaleEx core_nj_scale_ex

#endif
